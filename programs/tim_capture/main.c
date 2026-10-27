#include "stm32f4xx.h"
#include "fmt.h"
#include "uart.h"

/*
 * Step 6c: input capture. On every edge of the user button, TIM5 copies its
 * counter into CCR1 by itself. The ISR turns those timestamps into presses, and
 * main prints the time between two presses.
 *
 * Wiring: PC13 (button B1) has no timer function, so a jumper carries the
 * button signal to a timer pin:
 *   CN7 pin 23 (PC13)  ->  CN7 pin 28 (PA0 = TIM5_CH1 on AF2, datasheet table 11)
 * The button pulls the line low while pressed.
 *
 *   tick = 16 MHz / (PSC + 1) = 16 MHz / 16 = 1 MHz    (1 us per tick)
 *   ARR  = 0xFFFFFFFF: the 32-bit counter wraps after 2^32 us, about 71.6 min.
 *   An interval is new - old in unsigned arithmetic, so one wrap in between
 *   still gives the right answer. Gaps above 71.6 min do not.
 *
 * Why TIM5 and not TIM2: lib/pwm.c owns TIM2. TIM5 is the other 32-bit timer,
 * so the next step can run PWM and capture at the same time.
 *
 *   TIM5 ISR  --(g_presses, g_interval_us, g_bounces)-->  main prints
 */

#define TIM5_PSC        15u
#define TIM5_ARR        0xFFFFFFFFu
#define PA0_AF_TIM5_CH1 2u
#define IC1F_DTS32_N8   0xFu        /* 8 equal samples at 16 MHz / 32: 16 us */
#define DEBOUNCE_US     20000u      /* line must be quiet 20 ms before an edge counts */

static volatile uint32_t g_presses;         /* accepted presses */
static volatile uint32_t g_interval_us;     /* time between the last two presses */
static volatile uint32_t g_bounces;         /* edges rejected as bounce */
static volatile uint32_t g_overcaptures;    /* captures lost before the ISR read them */

static void capture_pin_init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;

    GPIOA->MODER &= ~GPIO_MODER_MODER0;
    GPIOA->MODER |= GPIO_MODER_MODER0_1;
    GPIOA->AFR[0] &= ~GPIO_AFRL_AFSEL0;
    GPIOA->AFR[0] |= (PA0_AF_TIM5_CH1 << GPIO_AFRL_AFSEL0_Pos);

    /* Pull-up so PA0 reads high, not floating, if the jumper is off. */
    GPIOA->PUPDR &= ~GPIO_PUPDR_PUPD0;
    GPIOA->PUPDR |= GPIO_PUPDR_PUPD0_0;
}

static void capture_init(void)
{
    capture_pin_init();
    RCC->APB1ENR |= RCC_APB1ENR_TIM5EN;
    DBGMCU->APB1FZ |= DBGMCU_APB1_FZ_DBG_TIM5_STOP;   /* stop TIM5 while the core is halted */

    TIM5->PSC = TIM5_PSC;
    TIM5->ARR = TIM5_ARR;

    /* CC1S = 01: channel 1 is an input, and CCR1 is fed from TI1 (PA0).
       From here on CCR1 is read-only. CC1S can only be written while CC1E = 0
       (RM0390 17.4.7), which is why the channel is enabled last.
       IC1F: the hardware filter only rejects glitches shorter than 16 us. A
       button bounces for milliseconds, so the ISR still has to debounce. */
    TIM5->CCMR1 &= ~(TIM_CCMR1_CC1S | TIM_CCMR1_IC1F | TIM_CCMR1_IC1PSC);
    TIM5->CCMR1 |= TIM_CCMR1_CC1S_0 | (IC1F_DTS32_N8 << TIM_CCMR1_IC1F_Pos);

    /* CC1NP = 1, CC1P = 1: capture both edges (RM0390 17.4.9). The ISR needs
       the releases too, or it cannot tell a press from release bounce. */
    TIM5->CCER |= TIM_CCER_CC1NP | TIM_CCER_CC1P;
    TIM5->CCER |= TIM_CCER_CC1E;

    /* PSC is always preloaded: UG moves it into the live prescaler now.
       UG also sets UIF; clear it so a stale flag is never mistaken later. */
    TIM5->EGR = TIM_EGR_UG;
    TIM5->SR = 0u;

    TIM5->DIER |= TIM_DIER_CC1IE;
    NVIC_EnableIRQ(TIM5_IRQn);

    TIM5->CR1 |= TIM_CR1_CEN;
}

/*
 * Debounce on timestamps: an edge counts only if the line was quiet for
 * DEBOUNCE_US before it. A press is such an edge with the pin now low.
 *
 *   PA0: ‾‾‾‾‾‾‾‾|_|‾|____________ ... ____|‾|_|‾‾‾‾‾‾
 *                ^ press: quiet before, pin low -> accepted
 *                  ^ ^ bounce: too soon -> rejected
 *                                         ^ release: quiet before, pin high -> not a press
 *                                           ^ ^ release bounce: too soon -> rejected
 */
void TIM5_IRQHandler(void)
{
    static uint32_t last_edge;
    static uint32_t last_press;
    static int have_press;

    if ((TIM5->SR & TIM_SR_CC1IF) == 0u) {
        return;
    }

    uint32_t now = TIM5->CCR1;                      /* reading CCR1 clears CC1IF */

    /* CC1OF: a second edge was captured before CCR1 was read, so one
       timestamp was overwritten. SR bits clear by writing 0; writing 1 to
       the other bits leaves them alone. */
    if ((TIM5->SR & TIM_SR_CC1OF) != 0u) {
        TIM5->SR = ~TIM_SR_CC1OF;
        g_overcaptures++;
    }

    uint32_t quiet = now - last_edge;
    last_edge = now;

    if (quiet < DEBOUNCE_US) {
        g_bounces++;
        return;
    }

    /* The capture happened at most a few microseconds ago, so the pin still
       shows which way this edge went. */
    if ((GPIOA->IDR & GPIO_IDR_ID0) != 0u) {
        return;                                     /* a clean release */
    }

    if (have_press) {
        g_interval_us = now - last_press;
    }
    last_press = now;
    have_press = 1;
    g_presses++;
}

/* 1234567 us -> "1234.567" */
static void print_ms(uint32_t us)
{
    uint32_t frac = us % 1000u;

    print_dec32(uart_putc, us / 1000u);
    uart_putc('.');
    uart_putc((char)('0' + frac / 100u));
    uart_putc((char)('0' + (frac / 10u) % 10u));
    uart_putc((char)('0' + frac % 10u));
}

int main(void)
{
    uart_init();
    capture_init();

    uart_puts("\r\nTIM5 input capture. Jumper PC13 -> PA0, then press B1.\r\n");

    uint32_t seen = 0u;

    for (;;) {
        /* Same check-and-sleep pattern as uart_cli. The four values are
           copied with interrupts masked, so they all belong to one press. */
        __disable_irq();
        if (g_presses == seen) {
            __WFI();
            __enable_irq();
            continue;
        }
        uint32_t presses = g_presses;
        uint32_t interval = g_interval_us;
        uint32_t bounces = g_bounces;
        uint32_t overcaptures = g_overcaptures;
        __enable_irq();

        uart_puts("press ");
        print_dec32(uart_putc, presses);
        if (presses == 1u) {
            uart_puts(": first press, timing starts");
        } else {
            uart_puts(": ");
            print_ms(interval);
            uart_puts(" ms since the last press");
        }
        uart_puts("  (bounces ignored: ");
        print_dec32(uart_putc, bounces);
        uart_puts(", overcaptures: ");
        print_dec32(uart_putc, overcaptures);
        uart_puts(")\r\n");

        seen = presses;
    }
}
