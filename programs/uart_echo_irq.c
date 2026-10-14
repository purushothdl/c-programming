#include "stm32f4xx.h"

/*
 * Step 5c: receive by interrupt into a ring buffer, echo from main.
 *
 *   USART2 RXNE interrupt -- producer: stores the byte, advances g_head
 *   main()                -- consumer: echoes the byte, advances g_tail
 *
 * Each index has exactly one writer, so no lock is needed. The indexes count
 * freely and the slot is index & RING_MASK, so RING_SIZE must be a power of
 * two. The ring is full when (head - tail) == RING_SIZE.
 */
#define CPU_CLK_HZ 16000000u
#define USART2_CLK_HZ 16000000u
#define UART_BAUD 115200u
#define UART_BRR ((USART2_CLK_HZ + (UART_BAUD / 2u)) / UART_BAUD)
#define UART_AF 7u

#define RING_SIZE 64u             /* must be a power of two */
#define RING_MASK (RING_SIZE - 1u)

/* Compiler-only ordering: enough on one core. A second core would need DMB. */
#define COMPILER_BARRIER() __asm volatile ("" ::: "memory")

static uint8_t g_rx_buf[RING_SIZE];
static volatile uint32_t g_head;           /* written only by the ISR */
static volatile uint32_t g_tail;           /* written only by main */
static volatile uint32_t g_overruns;       /* USART overruns: bytes lost in hardware */
static volatile uint32_t g_ring_drops;     /* bytes dropped because the ring was full */

/* ISR only. */
static void rb_push(uint8_t byte)
{
    uint32_t head = g_head;
    uint32_t tail = g_tail;

    if ((head - tail) >= RING_SIZE) {     /* full */
        g_ring_drops++;
        return;
    }

    g_rx_buf[head & RING_MASK] = byte;
    COMPILER_BARRIER();                   /* store the data before publishing it */
    g_head = head + 1u;
}

/* main only, and only when the ring is not empty. */
static uint8_t rb_pop(void)
{
    uint32_t tail = g_tail;
    uint8_t byte = g_rx_buf[tail & RING_MASK];
    COMPILER_BARRIER();                   /* read the data before freeing the slot */
    g_tail = tail + 1u;
    return byte;
}

static int rb_empty(void)
{
    return g_head == g_tail;
}

/* USART2 interrupt (IRQ 38). Reading DR clears RXNE and, after the SR read,
   also ORE. With ORE set, the byte in DR is still the last good one. */
void USART2_IRQHandler(void)
{
    uint32_t sr = USART2->SR;

    if ((sr & (USART_SR_RXNE | USART_SR_ORE)) == 0u) {
        return;
    }

    uint8_t byte = (uint8_t)USART2->DR;

    if ((sr & USART_SR_ORE) != 0u) {
        g_overruns++;                      /* bytes after this one were lost */
    }
    if ((sr & USART_SR_RXNE) != 0u) {
        rb_push(byte);
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

    /* TE for the echo, RE + RXNEIE to interrupt on each received byte. */
    USART2->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE | USART_CR1_UE;

    NVIC_SetPriority(USART2_IRQn, 1u);
    NVIC_EnableIRQ(USART2_IRQn);           /* ISER1 bit 6 */
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

    for (;;) {
        /* Mask IRQs so the empty check and WFI are atomic. A byte arriving in
           between stays pending, WFI returns at once, and the ISR runs at
           __enable_irq(). */
        __disable_irq();
        if (rb_empty()) {
            __WFI();
            __enable_irq();
            continue;                      /* re-check: the ring may still be empty */
        }
        __enable_irq();

        uart_write_char((char)rb_pop());
    }
}
