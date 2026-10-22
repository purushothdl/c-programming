#include "stm32f4xx.h"
#include "pwm.h"

/*
 * Step 6b: the LED fades up from off (lib/pwm.c), then starts again.
 *
 * Raises the duty by 25 steps every 50 ms: 0 to 1000 in about 2 s.
 */

#define CPU_CLK_HZ 16000000u
#define FADE_STEP  25u
#define FADE_MS    50u

static void systick_init(void)
{
    /* No interrupt here: delay_ms() polls COUNTFLAG. */
    SysTick->LOAD = (CPU_CLK_HZ / 1000u) - 1u;
    SysTick->VAL = 0u;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk;
}

static void delay_ms(uint32_t ms)
{
    uint32_t i;
    for (i = 0; i < ms; i++) {
        while (!(SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk)) {
            /* Wait for the SysTick counter to reach zero */
        }
    }
}

int main(void)
{
    systick_init();
    pwm_init(0u);

    for (;;) {
        for (uint32_t duty = 0u; duty <= PWM_STEPS; duty += FADE_STEP) {
            pwm_set_duty(duty);
            delay_ms(FADE_MS);
        }
    }
}
