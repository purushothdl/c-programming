#ifndef PWM_H
#define PWM_H

#include <stdint.h>

/*
 * 1 kHz PWM on PA5 (LED LD2) from TIM2 channel 1.
 *
 * Duty is counted in steps of 0.1 %: 0 is always low, PWM_STEPS is always high.
 * A new duty applies at the next wrap of the counter (within 1 ms), so a period
 * is never cut in two. The timer runs by itself once started.
 *
 * This module owns TIM2 and PA5: do not use either for anything else.
 */
#define PWM_STEPS 1000u

void pwm_init(uint32_t duty);           /* configure and start; duty in 0..PWM_STEPS */
void pwm_set_duty(uint32_t duty);       /* values above PWM_STEPS act as PWM_STEPS */

#endif
