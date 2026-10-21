#include "stm32f4xx.h"

/*
 * TIM2 channel 1 PWM on PA5 (the green user LED, LD2).
 *
 * TIM2 runs from APB1. At reset the APB1 prescaler is /1, so the timer clock is
 * 16 MHz (HSI) (RM0390 6.2: TIMxCLK = PCLKx when the APB prescaler is 1).
 *
 *   tick   = 16 MHz / (PSC + 1) = 16 MHz / 6400 = 2500 Hz   (0.4 ms per tick)
 *   period = (ARR + 1) ticks    = 50 ticks                 = 20 ms -> 50 Hz
 *   duty   = CCR1 / (ARR + 1)   = 25 / 50                  = 50 %
 *
 * Slow motion: PSC = 63999 gives 250 Hz ticks, a 5 Hz period, and a visible blink.
 */

#define TIM2_PSC   6399u                    /* divide 16 MHz by 6400 */
#define TIM2_ARR   49u                      /* 50 ticks per period, 50 duty steps */
#define TIM2_CCR1  25u                      /* high for 25 of the 50 ticks */
#define PA5_AF_TIM2_CH1 1u                  /* datasheet table 11 */

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
    pwm_pin_init();
    pwm_init();

    for (;;) {
        __WFI();                            /* the timer drives the pin by itself */
    }
}
