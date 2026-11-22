#include "stm32f4xx.h"
#include "clock.h"
#include "cli.h"
#include "delay.h"
#include "fmt.h"
#include "mpu6500.h"
#include "spi.h"
#include "uart.h"

/*
 * Step 9a: first words with the MPU-6500. Reads WHO_AM_I (register 117, reset
 * value 0x70, register map p.44) to prove wiring, SPI mode and framing.
 *
 *   IMU module   Nucleo
 *   VCC          3V3
 *   GND          GND
 *   SCL (SCLK)   PB3   D3
 *   SDA (SDI)    PB5   D4   MOSI
 *   AD0 (SDO)    PB4   D5   MISO
 *   NCS          PB6   D10  (same pin as the RC522: unplug the RC522 first)
 *   INT, FSYNC, EDA, ECL   not connected (FSYNC to GND if it floats, product spec p.19)
 *
 *   whoami         read register 117: 0x70 = MPU-6500
 *   reset          the SPI reset sequence (about 200 ms), then SPI-only mode
 *   read <reg>     read any register (0-0x7F)
 *   write <reg> <value>
 *   speed <6-7>    SPI rate: 6 = 703 kHz, 7 = 351 kHz. The limit is 1 MHz (p.16).
 *
 * Mode 0 is an assumption: the datasheet says data is latched on the rising
 * edge and changes on the falling edge (p.34) and gives no mode number.
 */

#define IMU_SPI_MODE    0u
#define REG_MAX         0x7Fu                   /* 7-bit address field */
#define BR_FASTEST      6u                      /* 90 MHz / 2^7 = 703 kHz, the first rate under 1 MHz */
#define POWER_UP_MS     100u                    /* register access is valid within 100 ms (p.12) */

static uint32_t g_br = SPI_BR_SLOWEST;

static void cmd_whoami(char *args[])
{
    (void)args;
    uint8_t v = mpu6500_read(MPU6500_REG_WHO_AM_I);

    uart_puts("WHO_AM_I = ");
    print_hex8(uart_putc, v);
    if (v == MPU6500_WHO_AM_I_VALUE) {
        uart_puts("  MPU-6500\r\n");
    } else if (v == 0x00u || v == 0xFFu) {
        uart_puts("  no answer: check wiring, 3.3 V and NCS\r\n");
    } else {
        uart_puts("  a different chip id (this module may not be an MPU-6500)\r\n");
    }
}

static void cmd_reset(char *args[])
{
    (void)args;
    mpu6500_reset();
    uart_puts("reset done, SPI-only mode set\r\n");
}

static void cmd_read(char *args[])
{
    uint32_t reg;

    if (!parse_u32(args[0], &reg) || reg > REG_MAX) {
        uart_puts("usage: read <0-0x7F>\r\n");
        return;
    }
    print_hex8(uart_putc, (uint8_t)reg);
    uart_puts(" = ");
    print_hex8(uart_putc, mpu6500_read((uint8_t)reg));
    uart_puts("\r\n");
}

static void cmd_write(char *args[])
{
    uint32_t reg;
    uint32_t value;

    if (!parse_u32(args[0], &reg) || reg > REG_MAX || !parse_u32(args[1], &value) || value > 0xFFu) {
        uart_puts("usage: write <0-0x7F> <0-0xFF>\r\n");
        return;
    }
    mpu6500_write((uint8_t)reg, (uint8_t)value);
    uart_puts("wrote ");
    print_hex8(uart_putc, (uint8_t)value);
    uart_puts(" to ");
    print_hex8(uart_putc, (uint8_t)reg);
    uart_puts(", reads back ");
    print_hex8(uart_putc, mpu6500_read((uint8_t)reg));
    uart_puts("\r\n");
}

static void cmd_speed(char *args[])
{
    uint32_t br;

    if (!parse_u32(args[0], &br) || br < BR_FASTEST || br > SPI_BR_SLOWEST) {
        uart_puts("usage: speed <6-7>\r\n");
        return;
    }
    g_br = br;
    spi_configure(g_br, IMU_SPI_MODE);
    uart_puts("SCK ");
    print_dec32(uart_putc, spi_sck_hz(g_br));
    uart_puts(" Hz\r\n");
}

static const cli_cmd_t g_commands[] = {
    { "whoami", 0u, "whoami",              cmd_whoami },
    { "reset",  0u, "reset",               cmd_reset },
    { "read",   1u, "read <reg>",          cmd_read },
    { "write",  2u, "write <reg> <value>", cmd_write },
    { "speed",  1u, "speed <6-7>",         cmd_speed },
};

int main(void)
{
    clock_init();
    uart_init();
    spi_init(g_br, IMU_SPI_MODE);
    delay_ms(POWER_UP_MS);

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
