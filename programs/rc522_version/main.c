#include "stm32f4xx.h"
#include "clock.h"
#include "cli.h"
#include "fmt.h"
#include "rc522.h"
#include "spi.h"
#include "uart.h"

/*
 * Step 8b: first words with a real SPI device. Reads the RC522's VersionReg,
 * the read-only chip id, so the wiring, the SPI mode and the address-byte
 * framing are all proven before any card is involved.
 *
 *   RC522 module   Nucleo
 *   3.3V           3V3   (3.3 V only: the chip is not 5 V tolerant)
 *   GND            GND
 *   SCK            PB3   D3
 *   MOSI           PB5   D4
 *   MISO           PB4   D5
 *   SDA (= NSS)    PB6   D10
 *   RST            PB10  D6
 *   IRQ            not connected
 *
 *   version        read VersionReg: 0x91 = v1.0, 0x92 = v2.0, 0x00/0xFF = no answer
 *   read <reg>     read any register (reg 0-63)
 *   write <reg> <value>
 *   speed <3-7>    SPI rate: 3 = 5.6 MHz ... 7 = 351 kHz. The RC522 allows 10 Mbit/s
 *                  (datasheet 8.1.2), so BR 0-2 (45, 22.5, 11.25 MHz) are refused.
 */

#define RC522_SPI_MODE  0u                  /* datasheet 8.1.2: data stable on the rising edge */
#define REG_MAX         0x3Fu               /* 6-bit address field */
#define BR_FASTEST      3u                  /* 90 MHz / 2^4 = 5.6 MHz, the first rate under 10 MHz */

static uint32_t g_br = SPI_BR_SLOWEST;

static void print_hex8(uint8_t v)
{
    static const char hex[] = "0123456789ABCDEF";

    uart_puts("0x");
    uart_putc(hex[v >> 4]);
    uart_putc(hex[v & 0xFu]);
}

static void cmd_version(char *args[])
{
    (void)args;
    uint8_t v = rc522_read(RC522_REG_VERSION);

    uart_puts("VersionReg = ");
    print_hex8(v);
    if (v == 0x91u) {
        uart_puts("  MFRC522 v1.0\r\n");
    } else if (v == 0x92u) {
        uart_puts("  MFRC522 v2.0\r\n");
    } else if (v == 0x00u || v == 0xFFu) {
        uart_puts("  no answer: check wiring, 3.3 V and CS\r\n");
    } else {
        uart_puts("  unknown chip id (clone?)\r\n");
    }
}

static void cmd_read(char *args[])
{
    uint32_t reg;

    if (!parse_u32(args[0], &reg) || reg > REG_MAX) {
        uart_puts("usage: read <0-0x3F>\r\n");
        return;
    }
    print_hex8((uint8_t)reg);
    uart_puts(" = ");
    print_hex8(rc522_read((uint8_t)reg));
    uart_puts("\r\n");
}

static void cmd_write(char *args[])
{
    uint32_t reg;
    uint32_t value;

    if (!parse_u32(args[0], &reg) || reg > REG_MAX || !parse_u32(args[1], &value) || value > 0xFFu) {
        uart_puts("usage: write <0-0x3F> <0-0xFF>\r\n");
        return;
    }
    rc522_write((uint8_t)reg, (uint8_t)value);
    uart_puts("wrote ");
    print_hex8((uint8_t)value);
    uart_puts(" to ");
    print_hex8((uint8_t)reg);
    uart_puts(", reads back ");
    print_hex8(rc522_read((uint8_t)reg));
    uart_puts("\r\n");
}

static void cmd_speed(char *args[])
{
    uint32_t br;

    if (!parse_u32(args[0], &br) || br < BR_FASTEST || br > SPI_BR_SLOWEST) {
        uart_puts("usage: speed <3-7>\r\n");
        return;
    }
    g_br = br;
    spi_configure(g_br, RC522_SPI_MODE);
    uart_puts("SCK ");
    print_dec32(uart_putc, spi_sck_hz(g_br));
    uart_puts(" Hz\r\n");
}

static const cli_cmd_t g_commands[] = {
    { "version", 0u, "version",              cmd_version },
    { "read",    1u, "read <reg>",           cmd_read },
    { "write",   2u, "write <reg> <value>",  cmd_write },
    { "speed",   1u, "speed <3-7>",          cmd_speed },
};

int main(void)
{
    clock_init();
    uart_init();
    spi_init(g_br, RC522_SPI_MODE);
    rc522_init();

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
