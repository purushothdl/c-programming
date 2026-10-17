#include "stm32f4xx.h"
#include "board.h"

#define CPU_CLK_HZ 16000000u

static volatile uint32_t g_ms_ticks;

void led_init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    GPIOA->MODER &= ~GPIO_MODER_MODER5;
    GPIOA->MODER |= GPIO_MODER_MODER5_0;
}

void led_set(int on)
{
    GPIOA->BSRR = on ? GPIO_BSRR_BS5 : GPIO_BSRR_BR5;
}

void systick_init(void)
{
    SysTick->LOAD = (CPU_CLK_HZ / 1000u) - 1u;
    SysTick->VAL = 0u;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk |
                    SysTick_CTRL_TICKINT_Msk   |
                    SysTick_CTRL_ENABLE_Msk;
}

uint32_t millis(void)
{
    return g_ms_ticks;
}

void SysTick_Handler(void)
{
    g_ms_ticks++;
}
