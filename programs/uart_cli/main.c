#include "stm32f4xx.h"
#include "board.h"
#include "cli.h"
#include "fmt.h"
#include "uart.h"

/*
 * Step 5d: interrupt-driven UART with a tiny command line.
 *
 *   lib/ring.c, lib/fmt.c   pure logic: ring buffer, printing and parsing
 *   lib/uart.c              USART2 driver: RX and TX rings, the ISR
 *   lib/cli.c               line editor, tokenizer, help/peek/poke
 *   board.c                 LED and the 1 ms SysTick tick
 *   fault.c                 HardFault report over polled UART
 *   main.c                  init, program commands, then sleep until a byte arrives
 */

static void cmd_led(char *args[])
{
    if (streq(args[0], "on")) {
        led_set(1);
    } else if (streq(args[0], "off")) {
        led_set(0);
    } else {
        uart_puts("usage: led on|off\r\n");
    }
}

static void cmd_ticks(char *args[])
{
    (void)args;
    print_dec32(uart_putc, millis());
    uart_puts(" ms\r\n");
}

static const cli_cmd_t g_commands[] = {
    { "led",   1u, "led on|off", cmd_led   },
    { "ticks", 0u, "ticks",      cmd_ticks },
};

int main(void)
{
    led_init();
    uart_init();
    systick_init();

    cli_start(g_commands, sizeof g_commands / sizeof g_commands[0]);

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
