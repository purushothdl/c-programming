#include "stm32f4xx.h"
#include "clock.h"
#include "cli.h"
#include "fmt.h"
#include "pwm.h"
#include "uart.h"

/*
 * Step 7: run the chip at 180 MHz and prove it.
 *
 * clock_init() (lib/core/clock.c) builds the clock before anything else starts.
 * uart.c and pwm.c then derive their settings from lib/core/clock.h, so the same
 * 115200 baud and 1 kHz PWM come out at the new speed. The measurement part is
 * tim_pwm_measure with the tick rate read from the clock instead of assumed:
 *
 *   TIM2 CH1 (lib/core/pwm.c) --PA5--jumper--PA0--> TIM5 CH1 + CH2 (PWM input mode)
 *
 * Wiring: CN10 pin 11 (PA5) -> CN7 pin 28 (PA0).
 *
 *   TIM5 tick = 90 MHz / (PSC + 1) = 90 MHz   (11.1 ns)
 *   expected for 1 kHz PWM: period 90000 ticks, high = duty x 90 ticks
 *
 *   duty <0-1000>   set the PWM (same as tim_pwm)
 *   measure         read CCR1/CCR2 and print period, high time and duty
 *   clocks          decode the clock registers: which source, M/N/P, wait states
 *
 * TIM2 and TIM5 share one clock, so measure agrees with the duty even if that
 * clock were wrong. Only things outside the chip show the true speed: the
 * readable terminal text (the PC sets the baud reference) and a stopwatch.
 */

#define DUTY_START      (PWM_STEPS / 2u)
#define TIM5_PSC        0u
#define TIM5_ARR        0xFFFFFFFFu
#define PA0_AF_TIM5_CH1 2u                  /* datasheet table 11 */
#define HSI_HZ          16000000u
/* Edge to counter reset: 2 ticks measured at 16 MHz. Not yet measured at
   90 MHz, so check CCR1 against period - 2 with the measure command. */
#define RESET_DELAY_TICKS 2u

static uint32_t g_tick_hz;                  /* TIM5 count rate, set in measure_init */

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

    g_tick_hz = clock_tim_apb1_hz() / (TIM5_PSC + 1u);

    TIM5->PSC = TIM5_PSC;
    TIM5->ARR = TIM5_ARR;

    /* CC1S = 01: IC1 from TI1.  CC2S = 10: IC2 also from TI1 (the same pin).
       No filter: it would swallow short pulses, and a push-pull output on the
       same board needs none (see tim_pwm_measure). */
    TIM5->CCMR1 &= ~(TIM_CCMR1_CC1S | TIM_CCMR1_IC1F | TIM_CCMR1_IC1PSC |
                     TIM_CCMR1_CC2S | TIM_CCMR1_IC2F | TIM_CCMR1_IC2PSC);
    TIM5->CCMR1 |= TIM_CCMR1_CC1S_0 | TIM_CCMR1_CC2S_1;

    /* IC1 on rising edges, IC2 on falling. */
    TIM5->CCER &= ~(TIM_CCER_CC1P | TIM_CCER_CC1NP | TIM_CCER_CC2NP);
    TIM5->CCER |= TIM_CCER_CC2P;

    /* TS = 101: trigger is TI1FP1. SMS = 100: reset mode, each rising edge
       restarts CNT from 0. */
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

/* ticks -> nanoseconds, printed as microseconds with 3 decimals.
   ticks x 1000 fits in 32 bits up to 4.29 M ticks (47 ms at 90 MHz). */
static void print_ticks_us(uint32_t ticks)
{
    print_fixed3(ticks * 1000u / (g_tick_hz / 1000000u));
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
       CCR1/CCR2 keep values from the last real period. 1 s of ticks = g_tick_hz. */
    if (TIM5->CNT > g_tick_hz) {
        uart_puts("no edges for 1 s: pin stuck ");
        uart_puts((GPIOA->IDR & GPIO_IDR_ID0) != 0u ? "high" : "low");
        uart_puts("\r\n");
        return;
    }

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

/* RCC_CFGR APB prescaler code -> divider (0xx = /1, 100 = /2 ... 111 = /16). */
static uint32_t apb_div(uint32_t code)
{
    return ((code & 4u) != 0u) ? (2u << (code & 3u)) : 1u;
}

/* RCC_CFGR HPRE code -> divider (0xxx = /1, 1000 = /2 ... 1111 = /512). */
static uint32_t ahb_div(uint32_t code)
{
    static const uint16_t div[8] = { 2u, 4u, 8u, 16u, 64u, 128u, 256u, 512u };

    return ((code & 8u) != 0u) ? div[code & 7u] : 1u;
}

static void print_field(const char *name, uint32_t v)
{
    uart_puts(name);
    print_dec32(uart_putc, v);
    uart_puts("\r\n");
}

/* Everything below is decoded from the registers, not from lib/core/clock.c's
   numbers, so it shows what the hardware is really set to. */
static void cmd_clocks(char *args[])
{
    (void)args;

    uint32_t cfgr = RCC->CFGR;
    uint32_t pllcfgr = RCC->PLLCFGR;
    uint32_t acr = FLASH->ACR;

    uint32_t sws = (cfgr & RCC_CFGR_SWS) >> RCC_CFGR_SWS_Pos;
    uint32_t m = (pllcfgr & RCC_PLLCFGR_PLLM) >> RCC_PLLCFGR_PLLM_Pos;
    uint32_t n = (pllcfgr & RCC_PLLCFGR_PLLN) >> RCC_PLLCFGR_PLLN_Pos;
    uint32_t p = 2u * (((pllcfgr & RCC_PLLCFGR_PLLP) >> RCC_PLLCFGR_PLLP_Pos) + 1u);
    uint32_t vco = (HSI_HZ / m) * n;       /* PLLSRC = HSI is the only source clock.c sets */

    uint32_t ahb = ahb_div((cfgr & RCC_CFGR_HPRE) >> RCC_CFGR_HPRE_Pos);
    uint32_t apb1 = apb_div((cfgr & RCC_CFGR_PPRE1) >> RCC_CFGR_PPRE1_Pos);
    uint32_t apb2 = apb_div((cfgr & RCC_CFGR_PPRE2) >> RCC_CFGR_PPRE2_Pos);

    uint32_t sysclk = (sws == (RCC_CFGR_SWS_PLL >> RCC_CFGR_SWS_Pos)) ? (vco / p) : HSI_HZ;
    uint32_t pclk1 = sysclk / ahb / apb1;
    uint32_t timclk = (apb1 == 1u) ? pclk1 : 2u * pclk1;

    uart_puts("source (SWS): ");
    uart_puts(sws == (RCC_CFGR_SWS_PLL >> RCC_CFGR_SWS_Pos) ? "PLL" :
              sws == (RCC_CFGR_SWS_HSI >> RCC_CFGR_SWS_Pos) ? "HSI" : "other");
    uart_puts("\r\nPLLCFGR ");
    print_hex32(uart_putc, pllcfgr);
    uart_puts("\r\n");
    print_field("  M ", m);
    print_field("  N ", n);
    print_field("  P ", p);
    print_field("VCO Hz ", vco);
    print_field("SYSCLK Hz ", sysclk);
    print_field("flash wait states ", (acr & FLASH_ACR_LATENCY) >> FLASH_ACR_LATENCY_Pos);
    uart_puts("prefetch/icache/dcache ");
    uart_putc((acr & FLASH_ACR_PRFTEN) != 0u ? '1' : '0');
    uart_putc((acr & FLASH_ACR_ICEN) != 0u ? '1' : '0');
    uart_putc((acr & FLASH_ACR_DCEN) != 0u ? '1' : '0');
    uart_puts("\r\n");
    print_field("AHB /", ahb);
    print_field("APB1 /", apb1);
    print_field("APB2 /", apb2);
    print_field("PCLK1 Hz ", pclk1);
    print_field("timer clock Hz ", timclk);
    print_field("BRR ", USART2->BRR);
}

static const cli_cmd_t g_commands[] = {
    { "duty",    1u, "duty <0-1000>", cmd_duty    },
    { "measure", 0u, "measure",       cmd_measure },
    { "clocks",  0u, "clocks",        cmd_clocks  },
};

int main(void)
{
    clock_init();                           /* first: no peripheral clock may be on yet */
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
