#ifndef DELAY_H
#define DELAY_H

#include <stdint.h>

/*
 * Blocking delays timed by SysTick (polled COUNTFLAG, no interrupt), so they
 * last the same at any clock: the count is derived from clock_sysclk_hz().
 *
 * Owns SysTick while it waits and leaves it disabled afterwards, so do not use
 * it in a program that runs the SysTick interrupt. The wait is at least the
 * requested time (it starts at the next count), never shorter.
 */
void delay_us(uint32_t us);
void delay_ms(uint32_t ms);

#endif
