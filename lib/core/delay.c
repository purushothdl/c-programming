#include "stm32f4xx.h"
#include "clock.h"
#include "delay.h"

/* Wait for `count` periods of `ticks` core cycles each. COUNTFLAG sets when the
   counter wraps and clears when CTRL is read; LOAD is 24 bits. */
static void wait_periods(uint32_t ticks, uint32_t count)
{
    SysTick->LOAD = ticks - 1u;
    SysTick->VAL = 0u;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk;
    for (uint32_t i = 0u; i < count; i++) {
        while ((SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) == 0u) {
        }
    }
    SysTick->CTRL = 0u;
}

void delay_us(uint32_t us)
{
    wait_periods(clock_sysclk_hz() / 1000000u, us);     /* 180 at 180 MHz, 16 at 16 MHz */
}

void delay_ms(uint32_t ms)
{
    wait_periods(clock_sysclk_hz() / 1000u, ms);        /* 180000 fits in 24 bits */
}
