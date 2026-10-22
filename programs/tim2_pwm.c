#include "stm32f4xx.h"

/*
 * TIM2 channel 1 PWM on PA5 (the green user LED, LD2).
 *
 * TIM2 runs from APB1. At reset the APB1 prescaler is /1, so the timer clock is
 * 16 MHz (HSI) (RM0390 6.2: TIMxCLK = PCLKx when the APB prescaler is 1).
 * 
 * Example values below give a 1 Hz PWM with 50 % duty cycle:
 *   tick   = 16 MHz / (PSC + 1) = 16 MHz / 16000 = 1 kHz    (1 ms per tick)
 *   period = (ARR + 1) ticks    = 1000 ticks               = 1 s -> 1 Hz
 *   duty   = CCR1 / (ARR + 1)   = 500 / 1000               = 50 %
 *
 * 1 Hz is slow enough to see as a blink: 0.5 s on, 0.5 s off.
 */

#define TIM2_PSC   15u                   /* divide 16 MHz by 16 */
#define TIM2_ARR   999u                  /* 1000 ticks per period, 1000 duty steps */
#define TIM2_CCR1  0u                    /* high for 0 of the 1000 ticks */
#define PA5_AF_TIM2_CH1 1u               /* datasheet table 11 */

#define CPU_CLK_HZ 16000000u

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

static void pwm_pin_init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;

    GPIOA->MODER &= ~GPIO_MODER_MODER5;
    GPIOA->MODER |= GPIO_MODER_MODER5_1;
    GPIOA->AFR[0] &= ~GPIO_AFRL_AFSEL5;
    GPIOA->AFR[0] |= (PA5_AF_TIM2_CH1 << GPIO_AFRL_AFSEL5_Pos);
}

static void pwm_init(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;

    /* PSC, ARR and CCR1 are preloaded: these writes land in buffers and reach
       the live registers only on an update event (forced below). */
    TIM2->PSC = TIM2_PSC;
    TIM2->ARR = TIM2_ARR;
    TIM2->CCR1 = TIM2_CCR1;

    /* PWM mode 1: output high while CNT < CCR1. OC1PE makes a new duty apply at
       the next wrap, never mid-period (RM0390 17.3.9 requires it). */
    TIM2->CCMR1 &= ~(TIM_CCMR1_CC1S | TIM_CCMR1_OC1M);
    TIM2->CCMR1 |= TIM_CCMR1_OC1M_2 | TIM_CCMR1_OC1M_1 | TIM_CCMR1_OC1PE;

    /* Active high, then connect to the pin last. */
    TIM2->CCER &= ~TIM_CCER_CC1P;
    TIM2->CCER |= TIM_CCER_CC1E;

    TIM2->CR1 |= TIM_CR1_ARPE;

    /* Without this the counter would start with the reset values of PSC and ARR
       and keep them until its first wrap. */
    TIM2->EGR = TIM_EGR_UG;

    TIM2->CR1 |= TIM_CR1_CEN;
}

int main(void)
{
    systick_init();
    pwm_pin_init();
    pwm_init();

    for (;;) {
        for (uint32_t i = 0; i < 41; i++) {
            TIM2->CCR1 = i * 25u;   /* 0, 25, 50, ..., 1000 */
            delay_ms(50u);
        }
    }
}
