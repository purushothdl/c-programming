#include "stm32f4xx.h"
#include "board.h"
#include "cli.h"
#include "uart.h"

/*
 * Step 5d: interrupt-driven UART with a tiny command line.
 *
 *   lib/ring.c, lib/fmt.c   pure logic: ring buffer, printing and parsing
 *   lib/uart.c              USART2 driver: RX and TX rings, the ISR
 *   board.c                 LED and the 1 ms SysTick tick
 *   cli.c                   line editor, tokenizer and commands
 *   fault.c                 HardFault report over polled UART
 *   main.c                  init, then sleep until a byte arrives
 */
int main(void)
{
    led_init();
    uart_init();
    systick_init();

    cli_start();

    for (;;) {
        /* Same check-and-sleep pattern as uart_echo_irq: mask interrupts while
           deciding to sleep, so a byte arriving in between is not missed. */
        __disable_irq();
        if (uart_rx_empty()) {
            __WFI();
            __enable_irq();
            continue;
        }
        __enable_irq();

        cli_feed((char)uart_rx_pop());
    }
}
