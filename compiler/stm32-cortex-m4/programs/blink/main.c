#include "stm32f4xx.h"

#define LED_PIN 5u

static void delay(volatile uint32_t count)
{
    while (count-- != 0u) {
        __asm volatile ("nop");
    }
}

int main(void)
{
    /* Enable the GPIOA peripheral clock: RCC_AHB1ENR.GPIOAEN, bit 0. */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;

    /* PA5: general-purpose output, push-pull, low speed, no pull. */
    GPIOA->MODER &= ~GPIO_MODER_MODER5;            /* clear the 2-bit mode field */
    GPIOA->MODER |= GPIO_MODER_MODER5_0;           /* mode 01: general output    */
    GPIOA->OTYPER &= ~GPIO_OTYPER_OT5;             /* push-pull                  */
    GPIOA->OSPEEDR &= ~GPIO_OSPEEDR_OSPEED5;       /* low speed                  */
    GPIOA->PUPDR &= ~GPIO_PUPDR_PUPD5;            /* no pull-up / pull-down     */

    for (;;) {
        GPIOA->BSRR = GPIO_BSRR_BS5;               /* set PA5 high */
        delay(1000000u);

        GPIOA->BSRR = GPIO_BSRR_BR5;               /* reset PA5 low */
        delay(1000000u);
    }
}
