#include "stm32f4xx.h"
#include "clock.h"
#include "cli.h"
#include "delay.h"
#include "fmt.h"
#include "spi.h"
#include "uart.h"

/*
 * Step 9b: a 74HC595 shift register on SPI, driving a one-digit 7-segment display.
 * Write-only chip, so success is what you see on the LEDs. SN74HC595 datasheet
 * (Components/sn74hc595.pdf): pins p.3, behaviour p.12.
 *
 *   595 pin (DIP-16)   Nucleo / board
 *   14 SER             PB5   D4   MOSI
 *   11 SRCLK           PB3   D3   SCK     shifts on its rising edge
 *   12 RCLK            PB6   D10  CS      copies the shifted byte to the outputs on its
 *                                         rising edge: our chip select going high IS the latch
 *   13 OE              GND    (low = outputs on)
 *   10 SRCLR           3V3    (low would clear the shift register)
 *   16 VCC             3V3    (2-6 V allowed, p.4)
 *   8  GND             GND
 *   9  QH'             PB4   D5   MISO    optional, for `echo`
 *
 *   Outputs, one 330 ohm resistor each to a segment:
 *     QA (pin 15) = a   QB (1) = b   QC (2) = c   QD (3) = d
 *     QE (4) = e        QF (5) = f   QG (6) = g   QH (7) = dp
 *   Display common pin: GND for a common-cathode digit, 3V3 for common-anode
 *   (then run `invert 1`).
 *
 *   out <byte>     send a raw byte: bit 0 = QA ... bit 7 = QH (MSB goes out first
 *                  and ends up in QH after 8 clocks)
 *   walk           QA .. QH one at a time, 700 ms each: which segment lights for which pin?
 *   digit <0-9>    show a digit, using the wiring above
 *   count          0-9 every 500 ms until a key is pressed
 *   invert <0|1>   1 = common anode (segments light on a low output); redraws the last value
 *   echo <byte>    needs QH' on MISO: sends the byte, then reads it back out of QH'
 */

#define SR_SPI_MODE     0u                  /* shift on the rising edge, idle low */
#define WALK_MS         700u
#define COUNT_MS        500u

/* Segment bits a=bit0 ... g=bit6, dp=bit7. 0x3F = a+b+c+d+e+f. */
static const uint8_t g_digit[10] = {
    0x3Fu, 0x06u, 0x5Bu, 0x4Fu, 0x66u, 0x6Du, 0x7Du, 0x07u, 0x7Fu, 0x6Fu
};

static uint32_t g_invert;
static uint8_t g_last;                      /* last value passed to show(), before inversion */

static void show(uint8_t value)
{
    g_last = value;
    spi_cs_low();
    (void)spi_xfer(g_invert ? (uint8_t)~value : value);
    spi_cs_high();                          /* rising edge on RCLK: outputs update */
}

static void cmd_out(char *args[])
{
    uint32_t v;

    if (!parse_u32(args[0], &v) || v > 0xFFu) {
        uart_puts("usage: out <0-0xFF>\r\n");
        return;
    }
    show((uint8_t)v);
    uart_puts("sent ");
    print_hex8(uart_putc, (uint8_t)v);
    uart_puts("\r\n");
}

static void cmd_walk(char *args[])
{
    static const char names[8] = { 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'p' };
    (void)args;

    for (uint32_t i = 0u; i < 8u; i++) {
        show((uint8_t)(1u << i));
        uart_puts("Q");
        uart_putc((char)('A' + i));
        uart_puts(" on, expected segment ");
        uart_putc(names[i]);
        uart_puts("\r\n");
        delay_ms(WALK_MS);
    }
    show(0u);
}

static void cmd_digit(char *args[])
{
    uint32_t d;

    if (!parse_u32(args[0], &d) || d > 9u) {
        uart_puts("usage: digit <0-9>\r\n");
        return;
    }
    show(g_digit[d]);
}

static void cmd_count(char *args[])
{
    (void)args;
    uart_puts("counting, any key stops\r\n");
    for (uint32_t d = 0u; uart_rx_empty(); d = (d + 1u) % 10u) {
        show(g_digit[d]);
        delay_ms(COUNT_MS);
    }
    (void)uart_rx_pop();
    show(0u);
}

static void cmd_invert(char *args[])
{
    uint32_t v;

    if (!parse_u32(args[0], &v) || v > 1u) {
        uart_puts("usage: invert <0|1>\r\n");
        return;
    }
    g_invert = v;
    show(g_last);                           /* redraw so the change is visible */
}

/* Byte A is shifted in and latched. The next transfer clocks QH' (the 8th stage)
   back to MISO, so its reply is A, first bit first. */
static void cmd_echo(char *args[])
{
    uint32_t v;

    if (!parse_u32(args[0], &v) || v > 0xFFu) {
        uart_puts("usage: echo <0-0xFF>\r\n");
        return;
    }
    spi_cs_low();
    (void)spi_xfer((uint8_t)v);
    spi_cs_high();
    spi_cs_low();
    uint8_t back = spi_xfer(0x00u);
    spi_cs_high();
    uart_puts("sent ");
    print_hex8(uart_putc, (uint8_t)v);
    uart_puts(", read back ");
    print_hex8(uart_putc, back);
    uart_puts(back == (uint8_t)v ? "  match\r\n" : "  no match (is QH' on MISO?)\r\n");
}

static const cli_cmd_t g_commands[] = {
    { "out",    1u, "out <byte>",    cmd_out },
    { "walk",   0u, "walk",          cmd_walk },
    { "digit",  1u, "digit <0-9>",   cmd_digit },
    { "count",  0u, "count",         cmd_count },
    { "invert", 1u, "invert <0|1>",  cmd_invert },
    { "echo",   1u, "echo <byte>",   cmd_echo },
};

int main(void)
{
    clock_init();
    uart_init();
    spi_init(SPI_BR_SLOWEST, SR_SPI_MODE);
    show(0u);

    cli_start(g_commands, sizeof g_commands / sizeof g_commands[0]);

    for (;;) {
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
