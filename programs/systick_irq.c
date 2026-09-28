#include "stm32f4xx.h"

/*
 * Step 4a: SysTick as a real interrupt.
 *
 * Same hardware as programs/systick.c, but now the tick is counted inside the
 * SysTick exception handler instead of by polling COUNTFLAG in a loop. main()
 * never spins waiting for time to pass: it reacts when the tick crosses 500 ms
 * and otherwise sleeps with WFI, so the CPU is not spent counting.
 *
 * No SystemInit()/PLL is used, so the core runs on the reset default: HSI 16 MHz.
 */
#define CPU_CLK_HZ 16000000u
#define SYSTICK_HZ 1000u                    /* 1 tick per millisecond */

#define LED_PIN 5u

static volatile uint32_t g_ms_ticks;

/* SysTick exception (exception number 15). Runs once per millisecond, pended
   by hardware on every counter wrap -- no polling required. */
void SysTick_Handler(void)
{
    g_ms_ticks++;
}

static void led_init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;         /* GPIOA clock */

    GPIOA->MODER &= ~GPIO_MODER_MODER5;          /* clear mode field */
    GPIOA->MODER |= GPIO_MODER_MODER5_0;         /* PA5 = output */
    GPIOA->OTYPER &= ~GPIO_OTYPER_OT5;           /* push-pull */
    GPIOA->OSPEEDR &= ~GPIO_OSPEEDR_OSPEED5;     /* low speed */
    GPIOA->PUPDR &= ~GPIO_PUPDR_PUPDR5;          /* no pull */
}

static void systick_init(void)
{
    SysTick->LOAD = (CPU_CLK_HZ / SYSTICK_HZ) - 1u;   /* 16000 - 1 */
    SysTick->VAL  = 0u;                               /* clear current count */
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk |      /* count the CPU clock */
                    SysTick_CTRL_TICKINT_Msk   |      /* interrupt on wrap (NEW) */
                    SysTick_CTRL_ENABLE_Msk;          /* start */
}

int main(void)
{
    led_init();
    systick_init();

    uint32_t next_toggle = 0u;

    for (;;) {
        /* Signed-difference compare: wrap-safe even if g_ms_ticks overflows. */
        if ((int32_t)(g_ms_ticks - next_toggle) >= 0) {
            next_toggle += 500u;

            if ((GPIOA->ODR & (1u << LED_PIN)) != 0u) {
                GPIOA->BSRR = GPIO_BSRR_BR5;         /* LED off */
            } else {
                GPIOA->BSRR = GPIO_BSRR_BS5;         /* LED on  */
            }
        }

        __WFI();   /* sleep until the next interrupt (SysTick) */
    }
}
