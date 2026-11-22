#include "stm32f4xx.h"
#include "clock.h"
#include "cli.h"
#include "fmt.h"
#include "spi.h"
#include "uart.h"

/*
 * Step 8a: SPI1 master talking to itself. A jumper from MOSI to MISO makes
 * every byte sent come straight back, so the SPI setup can be checked without
 * any device on the bus.
 *
 *   SCK  PB3  AF5  Arduino D3   (CN9 pin 4)
 *   MOSI PB5  AF5  Arduino D4   (CN9 pin 5)  --jumper--+
 *   MISO PB4  AF5  Arduino D5   (CN9 pin 6)  <---------+
 *   CS   PB6  GPIO Arduino D10  (software chip select, for the analyser)
 *
 * Pins: datasheet table 11 (AF5), UM1724 table 19. PA5 is also SPI1_SCK but
 * belongs to the LED and lib/core/pwm.c. PB3 and PB4 are JTAG pins after reset
 * (TRACESWO, NJTRST); SWD only needs PA13/PA14, so taking them is safe.
 *
 * SPI1 runs from APB2 = 90 MHz after clock_init(). SCK = 90 MHz / 2^(BR+1):
 *   BR 0: 45 MHz (the SPI1 master maximum, datasheet table 63) ... BR 7: 351.5 kHz
 *
 *   loop <0-7>      send 0..255 at that BR, compare what came back
 *   xfer <byte>     one byte with CS low, for an analyser capture
 *   mode <0-3>      CPOL/CPHA (a loopback passes in every mode)
 */

#define LOOP_BYTES      256u

static uint32_t g_br = SPI_BR_SLOWEST;      /* start slow: an 8-channel analyser can follow it */
static uint32_t g_mode;                     /* bit 1 = CPOL, bit 0 = CPHA */

static void print_sck(void)
{
    uart_puts("SCK ");
    print_dec32(uart_putc, spi_sck_hz(g_br));
    uart_puts(" Hz, mode ");
    print_dec32(uart_putc, g_mode);
    uart_puts("\r\n");
}

static void cmd_loop(char *args[])
{
    uint32_t br;

    if (!parse_u32(args[0], &br) || br > SPI_BR_SLOWEST) {
        uart_puts("usage: loop <0-7>  (0 = 45 MHz ... 7 = 351 kHz)\r\n");
        return;
    }
    g_br = br;
    spi_configure(g_br, g_mode);

    /* 0..255: every bit pattern once, so a stuck or swapped bit shows up. */
    uint32_t bad = 0u;
    uint8_t first_sent = 0u;
    uint8_t first_got = 0u;

    spi_cs_low();
    for (uint32_t i = 0u; i < LOOP_BYTES; i++) {
        uint8_t got = spi_xfer((uint8_t)i);
        if (got != (uint8_t)i) {
            if (bad == 0u) {
                first_sent = (uint8_t)i;
                first_got = got;
            }
            bad++;
        }
    }
    spi_cs_high();

    print_sck();
    print_dec32(uart_putc, LOOP_BYTES - bad);
    uart_puts("/256 bytes matched");
    if (bad != 0u) {
        uart_puts(", first bad: sent ");
        print_hex8(uart_putc, first_sent);
        uart_puts(" got ");
        print_hex8(uart_putc, first_got);
    }
    uart_puts("\r\n");
}

static void cmd_xfer(char *args[])
{
    uint32_t v;

    if (!parse_u32(args[0], &v) || v > 0xFFu) {
        uart_puts("usage: xfer <0-255 or 0x00-0xFF>\r\n");
        return;
    }

    spi_cs_low();
    uint8_t got = spi_xfer((uint8_t)v);
    spi_cs_high();

    uart_puts("sent ");
    print_hex8(uart_putc, (uint8_t)v);
    uart_puts(" got ");
    print_hex8(uart_putc, got);
    uart_puts("\r\n");
}

static void cmd_mode(char *args[])
{
    uint32_t mode;

    if (!parse_u32(args[0], &mode) || mode > 3u) {
        uart_puts("usage: mode <0-3>  (bit 1 = CPOL, bit 0 = CPHA)\r\n");
        return;
    }
    g_mode = mode;
    spi_configure(g_br, g_mode);
    print_sck();
}

static const cli_cmd_t g_commands[] = {
    { "loop", 1u, "loop <0-7>",  cmd_loop },
    { "xfer", 1u, "xfer <byte>", cmd_xfer },
    { "mode", 1u, "mode <0-3>",  cmd_mode },
};

int main(void)
{
    clock_init();
    uart_init();
    spi_init(g_br, g_mode);

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
