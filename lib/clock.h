#ifndef CLOCK_H
#define CLOCK_H

#include <stdint.h>

/*
 * System clock setup: 16 MHz HSI at reset -> 180 MHz from the main PLL.
 *
 *   HSI 16 MHz --/M(8)--> 2 MHz --xN(180)--> VCO 360 MHz --/P(2)--> SYSCLK 180 MHz
 *
 *   AHB  /1 = 180 MHz   CPU, memory, GPIO
 *   APB1 /4 =  45 MHz   USART2, TIM2, TIM5   (timers run at 2x: 90 MHz)
 *   APB2 /2 =  90 MHz   TIM1, USART1, SPI1
 *
 * Call clock_init() once, first thing in main(), before any other init: the
 * manual says no peripheral clock may be enabled during the over-drive switch
 * (RM0390 5.1.3). It leaves flash wait states, regulator and bus dividers
 * ready for 180 MHz before it switches the CPU over.
 *
 * The getters report the clock the hardware is running now. They return the
 * reset-default 16 MHz until clock_init() has run, so programs that never call
 * it (blink, uart_cli, ...) keep their 16 MHz timing unchanged. Drivers that
 * depend on a bus clock (uart.c, pwm.c) must call a getter instead of
 * hard-coding a frequency, and be initialised after clock_init().
 */
void clock_init(void);

uint32_t clock_sysclk_hz(void);         /* CPU / AHB clock */
uint32_t clock_pclk1_hz(void);          /* APB1 bus clock: USART2 baud generator */
uint32_t clock_tim_apb1_hz(void);       /* clock the APB1 timers (TIM2..TIM7, TIM12..14) count from */

#endif
