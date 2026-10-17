#ifndef UART_H
#define UART_H

#include <stdint.h>

/*
 * Interrupt-driven USART2 (115200 8N1 on PA2/PA3, 16 MHz clock).
 *
 *   USART2 ISR  --RX ring-->  caller (uart_rx_pop)
 *   caller      --TX ring-->  USART2 ISR (TXE interrupt drains one byte)
 *
 * This module owns both rings and USART2_IRQHandler. The TX interrupt is
 * enabled only while bytes are queued, so an idle UART does not interrupt the
 * CPU.
 */
void uart_init(void);

/* Queued output: blocks only while the TX ring is full. */
void uart_putc(char c);
void uart_puts(const char *s);

/* Received bytes: pop only when !uart_rx_empty(). */
int uart_rx_empty(void);
uint8_t uart_rx_pop(void);

/* Polled output for the fault handler, where interrupts cannot be trusted.
   fault_flush() waits until the last frame has left the pin. */
void fault_putc(char c);
void fault_flush(void);

#endif
