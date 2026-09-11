#include "stm32f4xx.h"

#define LED_PIN 5u
#define LED_MASK (1u << LED_PIN)
#define LED_RESET_MASK (1u << (LED_PIN + 16u))

#define BUTTON_PIN 13u
#define BUTTON_MASK (1u << BUTTON_PIN)

/* Onboard B1 shorts PC13 to GND, so pressed = low, released = high.
   For an external button wired to 3.3V, flip this to 1. */
#define BUTTON_PRESSED_LEVEL 0u

static void led_init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;         /* GPIOA clock */

    GPIOA->MODER &= ~GPIO_MODER_MODER5;          /* clear mode field */
    GPIOA->MODER |= GPIO_MODER_MODER5_0;         /* PA5 = output */
    GPIOA->OTYPER &= ~GPIO_OTYPER_OT5;           /* push-pull */
    GPIOA->OSPEEDR &= ~GPIO_OSPEEDR_OSPEED5;     /* low speed */
    GPIOA->PUPDR &= ~GPIO_PUPDR_PUPDR5;          /* no pull */
}

static void button_init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;         /* GPIOC clock */

    GPIOC->MODER &= ~GPIO_MODER_MODER13;         /* PC13 = input */
    GPIOC->PUPDR &= ~GPIO_PUPDR_PUPDR13;         /* clear pull field */
    GPIOC->PUPDR |= GPIO_PUPDR_PUPDR13_0;        /* pull-up: released = high */
}

static int button_is_pressed(void)
{
    return (GPIOC->IDR & GPIO_IDR_ID13) == BUTTON_PRESSED_LEVEL;
}

int main(void)
{
    led_init();
    button_init();

    for (;;) {
        if (button_is_pressed()) {
            GPIOA->BSRR = GPIO_BSRR_BS5;         /* LED on while held */
        } else {
            GPIOA->BSRR = GPIO_BSRR_BR5;         /* LED off when released */
        }
    }
}
