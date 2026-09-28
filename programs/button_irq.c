#include "stm32f4xx.h"

/*
 * Step 4b: the button as an external interrupt.
 *
 * Same LED/button hardware as programs/button.c, but main() never polls the
 * pin. PC13 falling edges are routed by SYSCFG to EXTI line 13, and EXTI
 * pends NVIC IRQ 40 (EXTI15_10). The handler toggles the LED once per press.
 *
 * The button on this board is mechanical: a press bounces for a few ms and
 * produces several falling edges. A 20 ms guard using SysTick (1 ms) collapses
 * the burst into a single toggle.
 *
 * No SystemInit()/PLL is used, so the core runs on the reset default: HSI 16 MHz.
 */
#define CPU_CLK_HZ 16000000u
#define SYSTICK_HZ 1000u                    /* 1 tick per millisecond */

#define LED_PIN 5u

#define BUTTON_PIN 13u
#define DEBOUNCE_MS 20u

static volatile uint32_t g_ms_ticks;
static volatile uint32_t g_last_press_ms;

/* SysTick exception (15): the 1 ms time base for debouncing. */
void SysTick_Handler(void)
{
    g_ms_ticks++;
}

/* EXTI15_10 (IRQ 40) is shared by lines 10..15, so confirm the line first. */
void EXTI15_10_IRQHandler(void)
{
    if ((EXTI->PR & EXTI_PR_PR13) != 0u) {
        EXTI->PR = EXTI_PR_PR13;                 /* write 1 to clear pending */

        if ((int32_t)(g_ms_ticks - g_last_press_ms) >= (int32_t)DEBOUNCE_MS) {
            g_last_press_ms = g_ms_ticks;

            if ((GPIOA->ODR & (1u << LED_PIN)) != 0u) {
                GPIOA->BSRR = GPIO_BSRR_BR5;     /* was on  -> off */
            } else {
                GPIOA->BSRR = GPIO_BSRR_BS5;     /* was off -> on  */
            }
        }
    }
}

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

static void systick_init(void)
{
    SysTick->LOAD = (CPU_CLK_HZ / SYSTICK_HZ) - 1u;   /* 16000 - 1 */
    SysTick->VAL  = 0u;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk |
                    SysTick_CTRL_TICKINT_Msk   |
                    SysTick_CTRL_ENABLE_Msk;
}

static void exti_init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;        /* SYSCFG clock */

    /* Route EXTI line 13 to port C (PC13). */
    SYSCFG->EXTICR[3] &= ~SYSCFG_EXTICR4_EXTI13;
    SYSCFG->EXTICR[3] |= SYSCFG_EXTICR4_EXTI13_PC;

    EXTI->IMR  |= EXTI_IMR_MR13;                 /* unmask line 13 (interrupt) */
    EXTI->FTSR |= EXTI_FTSR_TR13;                /* trigger on falling edge */
    EXTI->RTSR &= ~EXTI_RTSR_TR13;               /* not on rising edge */

    NVIC_SetPriority(EXTI15_10_IRQn, 2u);        /* lower priority than SysTick */
    NVIC_EnableIRQ(EXTI15_10_IRQn);              /* NVIC ISER bit 8 (IRQ 40) */
}

int main(void)
{
    led_init();
    button_init();
    systick_init();
    exti_init();

    for (;;) {
        __WFI();   /* sleep until SysTick ticks or the button is pressed */
    }
}
