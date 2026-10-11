#include "stm32f4xx.h"

/*
 * Step 5b: polling echo, deliberately too slow.
 *
 * Every received byte is read from USART2->DR and sent back. The loop also
 * waits 1 ms per iteration, so it can only handle about 1000 bytes per second,
 * while a 115200-baud link delivers about 11,500 bytes per second.
 *
 * When a new byte arrives before the old one has been read, the hardware sets
 * SR.ORE (overrun) and discards the new byte. Each overrun is marked with '!'
 * on the terminal, so the missing bytes are visible.
 */
#define CPU_CLK_HZ 16000000u
#define USART2_CLK_HZ 16000000u
#define UART_BAUD 115200u
#define UART_BRR ((USART2_CLK_HZ + (UART_BAUD / 2u)) / UART_BAUD)
#define UART_AF 7u

static volatile uint32_t g_overruns;

static void systick_init(void)
{
    /* No interrupt here: delay_ms() polls COUNTFLAG. */
    SysTick->LOAD = (CPU_CLK_HZ / 1000u) - 1u;
    SysTick->VAL = 0u;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk;
}

static void delay_ms(uint32_t ms)
{
    while (ms-- > 0u) {
        while ((SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) == 0u) {
            /* busy-wait for one 1 ms SysTick period */
        }
    }
}

static void uart_init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;

    /* PA2 = USART2_TX, PA3 = USART2_RX: alternate function (10), AF7. */
    GPIOA->MODER &= ~(GPIO_MODER_MODER2 | GPIO_MODER_MODER3);
    GPIOA->MODER |= GPIO_MODER_MODER2_1 | GPIO_MODER_MODER3_1;

    GPIOA->AFR[0] &= ~(GPIO_AFRL_AFSEL2 | GPIO_AFRL_AFSEL3);
    GPIOA->AFR[0] |= (UART_AF << GPIO_AFRL_AFSEL2_Pos) |
                     (UART_AF << GPIO_AFRL_AFSEL3_Pos);

    GPIOA->OTYPER &= ~(GPIO_OTYPER_OT2 | GPIO_OTYPER_OT3);
    GPIOA->PUPDR &= ~(GPIO_PUPDR_PUPDR2 | GPIO_PUPDR_PUPDR3);

    /* 8N1, 16x oversampling, no flow control. */
    USART2->CR1 = 0u;
    USART2->CR2 = 0u;
    USART2->CR3 = 0u;
    USART2->BRR = UART_BRR;

    /* Enable both directions: TE for echo output, RE for the incoming bytes. */
    USART2->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;
}

static void uart_write_char(char ch)
{
    while ((USART2->SR & USART_SR_TXE) == 0u) {
    }
    USART2->DR = (uint8_t)ch;
}

int main(void)
{
    uart_init();
    systick_init();

    for (;;) {
        delay_ms(1u);   /* deliberately slow: 1 ms per loop iteration */

        uint32_t sr = USART2->SR;   /* one snapshot; ORE clears on SR read then DR read */

        if ((sr & USART_SR_ORE) != 0u) {
            g_overruns++;
        }

        if ((sr & USART_SR_RXNE) != 0u) {
            uint8_t byte = (uint8_t)USART2->DR;

            uart_write_char((char)byte);
            if ((sr & USART_SR_ORE) != 0u) {
                uart_write_char('!');   /* the lost bytes came right after this one */
            }
        } else if ((sr & USART_SR_ORE) != 0u) {
            /* ORE without RXNE: the loss hit between SR and DR reads last pass.
               Read DR to finish the clear sequence, or ORE would stay set. */
            (void)USART2->DR;
            uart_write_char('!');
        }
    }
}
