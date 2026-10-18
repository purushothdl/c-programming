#include "stm32f4xx.h"
#include "uart.h"
#include "fmt.h"
#include "ring.h"

#define USART2_CLK_HZ 16000000u   /* PCLK1 at the reset-default clock setup */
#define UART_BAUD 115200u
#define UART_BRR ((USART2_CLK_HZ + (UART_BAUD / 2u)) / UART_BAUD)
#define UART_AF 7u

static ring_t g_tx;              /* producer: caller,     consumer: USART2 ISR */
static ring_t g_rx;              /* producer: USART2 ISR, consumer: caller     */
static volatile uint32_t g_overruns;

void uart_init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;

    GPIOA->MODER &= ~(GPIO_MODER_MODER2 | GPIO_MODER_MODER3);
    GPIOA->MODER |= GPIO_MODER_MODER2_1 | GPIO_MODER_MODER3_1;

    GPIOA->AFR[0] &= ~(GPIO_AFRL_AFSEL2 | GPIO_AFRL_AFSEL3);
    GPIOA->AFR[0] |= (UART_AF << GPIO_AFRL_AFSEL2_Pos) |
                     (UART_AF << GPIO_AFRL_AFSEL3_Pos);

    GPIOA->OTYPER &= ~(GPIO_OTYPER_OT2 | GPIO_OTYPER_OT3);
    GPIOA->PUPDR &= ~(GPIO_PUPDR_PUPDR2 | GPIO_PUPDR_PUPDR3);

    USART2->CR1 = 0u;
    USART2->CR2 = 0u;
    USART2->CR3 = 0u;
    USART2->BRR = UART_BRR;

    /* RXNEIE: interrupt per received byte. TXEIE is enabled later, only while
       there is something to send. */
    USART2->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE | USART_CR1_UE;

    NVIC_SetPriority(USART2_IRQn, 1u);
    NVIC_EnableIRQ(USART2_IRQn);
}

void uart_putc(char c)
{
    while (ring_full(&g_tx)) {
        /* the TX ISR drains one byte per TXE event */
    }
    ring_push(&g_tx, (uint8_t)c);
    USART2->CR1 |= USART_CR1_TXEIE;       /* make sure the ISR is armed */
}

void uart_puts(const char *s)
{
    print_str(uart_putc, s);
}

int uart_rx_empty(void)
{
    return ring_empty(&g_rx);
}

uint8_t uart_rx_pop(void)
{
    return ring_pop(&g_rx);
}

void fault_putc(char c)
{
    while ((USART2->SR & USART_SR_TXE) == 0u) {
    }
    USART2->DR = (uint8_t)c;
}

void fault_flush(void)
{
    while ((USART2->SR & USART_SR_TC) == 0u) {
        /* wait for the last frame to leave the pin before resetting */
    }
}

void USART2_IRQHandler(void)
{
    uint32_t sr = USART2->SR;

    if ((sr & (USART_SR_RXNE | USART_SR_ORE)) != 0u) {
        /* Reading SR (above) then DR clears RXNE and ORE. */
        uint8_t byte = (uint8_t)USART2->DR;

        if ((sr & USART_SR_ORE) != 0u) {
            g_overruns++;
        }
        if ((sr & USART_SR_RXNE) != 0u) {
            ring_push(&g_rx, byte);
        }
    }

    /* TX: when the data register is empty and TX is armed, send the next byte,
       or disarm the interrupt if nothing is queued. */
    if (((USART2->CR1 & USART_CR1_TXEIE) != 0u) &&
        ((USART2->SR & USART_SR_TXE) != 0u)) {
        if (!ring_empty(&g_tx)) {
            USART2->DR = ring_pop(&g_tx);
        } else {
            USART2->CR1 &= ~USART_CR1_TXEIE;
        }
    }
}

/* HardFault only: the TX interrupt can no longer run, so send what is still queued by polling. */
void uart_drain_polled(void)
{
    USART2->CR1 &= ~USART_CR1_TXEIE;          /* the ISR must not touch the ring any more */
    while (!ring_empty(&g_tx)) {
        fault_putc((char)ring_pop(&g_tx));
    }
}
