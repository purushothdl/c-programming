#include "stm32f4xx.h"

/*
 * SysTick timing demo (Step 3): replace the hand-rolled busy-wait loop with a
 * hardware time base.
 *
 * SysTick is a 24-bit down-counter in the Cortex-M4 core. We load it so it
 * wraps every 1 ms, then poll its COUNTFLAG ("wrapped") flag to count time.
 * The CPU still spins -- that is the point of this step -- but now the delay
 * is real milliseconds instead of a made-up instruction loop. Step 4 turns
 * this same setup into an interrupt so the CPU no longer has to spin.
 *
 * No SystemInit()/PLL is used, so the core runs on the reset default:
 * HSI = 16 MHz.
 */
#define CPU_CLK_HZ 16000000u
#define SYSTICK_HZ 1000u                    /* 1 tick per millisecond */

#define LED_PIN 5u

static volatile uint32_t g_ms_ticks;

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
                    SysTick_CTRL_ENABLE_Msk;           /* start, no IRQ (polling) */
}

static void delay_ms(uint32_t ms)
{
    uint32_t start = g_ms_ticks;

    while ((g_ms_ticks - start) < ms) {
        if ((SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) != 0u) {
            g_ms_ticks++;                             /* reading CTRL clears the flag */
        }
    }
}

int main(void)
{
    led_init();
    systick_init();

    for (;;) {
        GPIOA->BSRR = GPIO_BSRR_BS5;                  /* LED on */
        delay_ms(500u);

        GPIOA->BSRR = GPIO_BSRR_BR5;                  /* LED off */
        delay_ms(500u);
    }
}
