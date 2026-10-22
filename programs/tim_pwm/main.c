#include "stm32f4xx.h"
#include "cli.h"
#include "fmt.h"
#include "pwm.h"
#include "uart.h"

/*
 * Step 6a: 1 kHz PWM on the LED (lib/pwm.c), duty set from the CLI.
 *
 *   duty <0-1000>   0.1 % steps: 250 is 25.0 %, 1000 is always on
 */

#define DUTY_START (PWM_STEPS / 2u)

static void cmd_duty(char *args[])
{
    uint32_t duty;

    if (!parse_u32(args[0], &duty) || duty > PWM_STEPS) {
        uart_puts("usage: duty <0-1000>  (1000 = 100 %)\r\n");
        return;
    }

    pwm_set_duty(duty);
    uart_puts("duty ");
    print_dec32(uart_putc, duty);
    uart_puts("/1000\r\n");
}

static const cli_cmd_t g_commands[] = {
    { "duty", 1u, "duty <0-1000>", cmd_duty },
};

int main(void)
{
    uart_init();
    pwm_init(DUTY_START);

    cli_start(g_commands, sizeof g_commands / sizeof g_commands[0]);

    for (;;) {
        /* Same check-and-sleep pattern as uart_cli: mask interrupts while
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
