#include "stm32f4xx.h"
#include "pwm.h"

/*
 * TIM2 runs from APB1. At reset the APB1 prescaler is /1, so the timer clock is
 * 16 MHz (HSI) (RM0390 6.2: TIMxCLK = PCLKx when the APB prescaler is 1).
 *
 *   tick   = 16 MHz / (PSC + 1) = 16 MHz / 16 = 1 MHz     (1 us per tick)
 *   period = (ARR + 1) ticks    = 1000 ticks              = 1 ms -> 1 kHz
 *   duty   = CCR1 / (ARR + 1)   = CCR1 / 1000             (1000 steps)
 *
 * For a visible 1 Hz blink instead, use PSC = 15999.
 */
#define TIM2_PSC        15u
#define TIM2_ARR        (PWM_STEPS - 1u)
#define PA5_AF_TIM2_CH1 1u                  /* datasheet table 11 */

static void pwm_pin_init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;

    GPIOA->MODER &= ~GPIO_MODER_MODER5;
    GPIOA->MODER |= GPIO_MODER_MODER5_1;
    GPIOA->AFR[0] &= ~GPIO_AFRL_AFSEL5;
    GPIOA->AFR[0] |= (PA5_AF_TIM2_CH1 << GPIO_AFRL_AFSEL5_Pos);
}

void pwm_init(uint32_t duty)
{
    pwm_pin_init();
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;
    DBGMCU->APB1FZ |= DBGMCU_APB1_FZ_DBG_TIM2_STOP;   /* stop TIM2 while the core is halted */


    /* PSC, ARR and CCR1 are preloaded: these writes reach the live registers
       only on an update event (forced below). */
    TIM2->PSC = TIM2_PSC;
    TIM2->ARR = TIM2_ARR;
    TIM2->CCR1 = duty;

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

void pwm_set_duty(uint32_t duty)
{
    TIM2->CCR1 = duty;      /* CCR1 above ARR keeps the output high */
}
