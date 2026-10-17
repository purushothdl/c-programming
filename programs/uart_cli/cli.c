#include "board.h"
#include "cli.h"
#include "fmt.h"
#include "uart.h"

#define LINE_MAX 64u

static char g_line[LINE_MAX];
static uint32_t g_len;

static void print_prompt(void)
{
    uart_puts("> ");
}

static void cmd_peek(char *arg)
{
    uint32_t addr;

    if (!parse_u32(arg, &addr) || (addr & 3u) != 0u) {
        uart_puts("usage: peek <4-byte-aligned addr>\r\n");
        return;
    }

    /* Print the address before reading. If the read faults, the line still
       shows which address caused it. */
    print_hex32(uart_putc, addr);
    uart_puts(" = ");
    print_hex32(uart_putc, *(volatile uint32_t *)addr);
    uart_puts("\r\n");
}

static void cmd_poke(char *addr_arg, char *val_arg)
{
    uint32_t addr;
    uint32_t val;

    if (!parse_u32(addr_arg, &addr) || (addr & 3u) != 0u ||
        !parse_u32(val_arg, &val)) {
        uart_puts("usage: poke <4-byte-aligned addr> <val>\r\n");
        return;
    }

    *(volatile uint32_t *)addr = val;
    print_hex32(uart_putc, addr);
    uart_puts(" <- ");
    print_hex32(uart_putc, val);
    uart_puts("\r\n");
}

static void execute(char *line)
{
    char *argv[3];
    int argc = tokenize(line, argv, 3);

    if (argc == 0) {
        return;
    }

    if (streq(argv[0], "help")) {
        uart_puts("led on|off | ticks | peek <addr> | poke <addr> <val>\r\n");
    } else if (streq(argv[0], "led") && argc == 2) {
        if (streq(argv[1], "on")) {
            led_set(1);
        } else if (streq(argv[1], "off")) {
            led_set(0);
        } else {
            uart_puts("usage: led on|off\r\n");
        }
    } else if (streq(argv[0], "ticks") && argc == 1) {
        print_dec32(uart_putc, millis());
        uart_puts(" ms\r\n");
    } else if (streq(argv[0], "peek") && argc == 2) {
        cmd_peek(argv[1]);
    } else if (streq(argv[0], "poke") && argc == 3) {
        cmd_poke(argv[1], argv[2]);
    } else {
        uart_puts("unknown command, try 'help'\r\n");
    }
}

void cli_start(void)
{
    uart_puts("\r\nSTM32F446 CLI ready. Type 'help'.\r\n");
    print_prompt();
}

void cli_feed(char c)
{
    if (c == '\r' || c == '\n') {
        uart_puts("\r\n");
        g_line[g_len] = '\0';
        execute(g_line);
        g_len = 0u;
        print_prompt();
    } else if (c == '\b' || c == 0x7F) {
        if (g_len != 0u) {
            g_len--;
            uart_puts("\b \b");
        }
    } else if (c >= 0x20 && c < 0x7F) {
        if (g_len < LINE_MAX - 1u) {
            g_line[g_len++] = c;
            uart_putc(c);
        }
    }
}
