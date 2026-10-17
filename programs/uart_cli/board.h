#ifndef BOARD_H
#define BOARD_H

#include <stdint.h>

void led_init(void);
void led_set(int on);               /* LD2 on PA5 */

void systick_init(void);            /* 1 ms tick */
uint32_t millis(void);              /* milliseconds since systick_init */

#endif
