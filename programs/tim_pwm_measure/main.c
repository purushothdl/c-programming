#include "stm32f4xx.h"
#include "cli.h"
#include "fmt.h"
#include "pwm.h"
#include "uart.h"

/*
 * Step 6d: the board measures its own PWM.
 *
 *   TIM2 CH1 (lib/pwm.c) --PA5--jumper--PA0--> TIM5 CH1 + CH2 (PWM input mode)
 *
 * Wiring: CN10 pin 11 (PA5) -> CN7 pin 28 (PA0). Remove the PC13 jumper from
 * tim_capture first: a press would short the PWM output to GND through B1.
 *
 * PWM input mode (RM0390 17.3.6): both channels of TIM5 watch the same pin.
 *   rising edge  -> CCR1 = CNT (the period that just ended), then CNT resets to 0
 *   falling edge -> CCR2 = CNT (the high time since that reset)
 * The slave mode controller does the reset, so no interrupt and no CPU work:
 * the latest period and high time are always sitting in CCR1 and CCR2.
 *
 *   TIM5 tick = 16 MHz / (PSC + 1) = 16 MHz    (62.5 ns, 16x finer than TIM2's 1 us)
 *   expected for 1 kHz PWM: period 16000 ticks, high = duty x 16 ticks
 *
 *   duty <0-1000>   set the PWM (same as tim_pwm)
 *   measure         read CCR1/CCR2 and print period, high time and duty
 */

#define DUTY_START      (PWM_STEPS / 2u)
#define TIM5_PSC        0u
#define TIM5_ARR        0xFFFFFFFFu
#define TIM5_TICK_HZ    16000000u
#define PA0_AF_TIM5_CH1 2u                  /* datasheet table 11 */
#define NO_EDGE_TICKS   TIM5_TICK_HZ        /* no rising edge for 1 s: no signal */
#define RESET_DELAY_TICKS 2u                /* edge to counter reset, measured */

static void measure_pin_init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;

    GPIOA->MODER &= ~GPIO_MODER_MODER0;
    GPIOA->MODER |= GPIO_MODER_MODER0_1;
    GPIOA->AFR[0] &= ~GPIO_AFRL_AFSEL0;
    GPIOA->AFR[0] |= (PA0_AF_TIM5_CH1 << GPIO_AFRL_AFSEL0_Pos);
    GPIOA->PUPDR &= ~GPIO_PUPDR_PUPD0;      /* driven by PA5, no pull needed */
}

static void measure_init(void)
{
    measure_pin_init();
    RCC->APB1ENR |= RCC_APB1ENR_TIM5EN;
    DBGMCU->APB1FZ |= DBGMCU_APB1_FZ_DBG_TIM5_STOP;

    TIM5->PSC = TIM5_PSC;
    TIM5->ARR = TIM5_ARR;

    /* CC1S = 01: IC1 from TI1.  CC2S = 10: IC2 also from TI1 (the same pin).
       IC1F = 0, no filter: the filter delays both edges equally, but it also
       swallows pulses shorter than its length (16 us at IC1F = 1111), which
       would hide duties below 1.6 %. A push-pull output on the same board
       needs no filtering. */
    TIM5->CCMR1 &= ~(TIM_CCMR1_CC1S | TIM_CCMR1_IC1F | TIM_CCMR1_IC1PSC |
                     TIM_CCMR1_CC2S | TIM_CCMR1_IC2F | TIM_CCMR1_IC2PSC);
    TIM5->CCMR1 |= TIM_CCMR1_CC1S_0 | TIM_CCMR1_CC2S_1;

    /* IC1 on rising edges (CC1P = 0, CC1NP = 0), IC2 on falling (CC2P = 1). */
    TIM5->CCER &= ~(TIM_CCER_CC1P | TIM_CCER_CC1NP | TIM_CCER_CC2NP);
    TIM5->CCER |= TIM_CCER_CC2P;

    /* TS = 101: the trigger is TI1FP1, the rising edge IC1 already uses.
       SMS = 100: reset mode, every trigger restarts CNT from 0. */
    TIM5->SMCR &= ~(TIM_SMCR_TS | TIM_SMCR_SMS);
    TIM5->SMCR |= TIM_SMCR_TS_2 | TIM_SMCR_TS_0 | TIM_SMCR_SMS_2;

    TIM5->CCER |= TIM_CCER_CC1E | TIM_CCER_CC2E;

    TIM5->EGR = TIM_EGR_UG;                 /* load PSC */
    TIM5->CR1 |= TIM_CR1_CEN;
}

/* 1234567 -> "1234.567" */
static void print_fixed3(uint32_t v)
{
    uint32_t frac = v % 1000u;

    print_dec32(uart_putc, v / 1000u);
    uart_putc('.');
    uart_putc((char)('0' + frac / 100u));
    uart_putc((char)('0' + (frac / 10u) % 10u));
    uart_putc((char)('0' + frac % 10u));
}

/* ticks of 62.5 ns -> nanoseconds, printed as microseconds with 3 decimals */
static void print_ticks_us(uint32_t ticks)
{
    print_fixed3(ticks * 125u / 2u);        /* fine up to about 34 M ticks */
}

static void cmd_duty(char *args[])
{
    uint32_t duty;

    if (!parse_u32(args[0], &duty) || duty > PWM_STEPS) {
        uart_puts("usage: duty <0-1000>  (1000 = 100 %)\r\n");
        return;
    }

    pwm_set_duty(duty);
    uart_puts("duty ");
    print_dec32(uart_putc, duty);
    uart_puts("/1000\r\n");
}

static void cmd_measure(char *args[])
{
    (void)args;

    /* Duty 0 or 1000 gives a flat line: no edges, so CNT is never reset and
       CCR1/CCR2 keep values from the last real period. */
    if (TIM5->CNT > NO_EDGE_TICKS) {
        uart_puts("no edges for 1 s: pin stuck ");
        uart_puts((GPIOA->IDR & GPIO_IDR_ID0) != 0u ? "high" : "low");
        uart_puts("\r\n");
        return;
    }

    /* The capture is taken on the edge, but the counter reset reaches CNT
       2 ticks later (trigger resynchronization, RM0390 17.3.14). CNT counts
       2 ticks past the edge before it restarts, so every span is the captured
       value + 2. Measured on the board: 1 kHz gives CCR1 = 15998, CCR2 =
       duty x 16 - 2. RM0390 figure 186 leaves this delay out. */
    uint32_t ccr1 = TIM5->CCR1;
    uint32_t ccr2 = TIM5->CCR2;
    uint32_t period = ccr1 + RESET_DELAY_TICKS;
    uint32_t high = ccr2 + RESET_DELAY_TICKS;

    uart_puts("CCR1 ");
    print_dec32(uart_putc, ccr1);
    uart_puts("  CCR2 ");
    print_dec32(uart_putc, ccr2);
    uart_puts("\r\nperiod ");
    print_ticks_us(period);
    uart_puts(" us, high ");
    print_ticks_us(high);
    uart_puts(" us, duty ");
    print_dec32(uart_putc, (high * 1000u + period / 2u) / period);
    uart_puts("/1000\r\n");
}

static const cli_cmd_t g_commands[] = {
    { "duty",    1u, "duty <0-1000>", cmd_duty    },
    { "measure", 0u, "measure",       cmd_measure },
};

int main(void)
{
    uart_init();
    pwm_init(DUTY_START);
    measure_init();

    cli_start(g_commands, sizeof g_commands / sizeof g_commands[0]);

    for (;;) {
        __disable_irq();
        if (uart_rx_empty()) {
            __WFI();
            __enable_irq();
            continue;
        }
        __enable_irq();

        cli_feed((char)uart_rx_pop());
    }
}
