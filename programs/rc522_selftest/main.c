#include "stm32f4xx.h"
#include "clock.h"
#include "cli.h"
#include "fmt.h"
#include "rc522.h"
#include "spi.h"
#include "uart.h"

/*
 * Step 8c: the RC522's built-in digital self test. No card, no antenna. The
 * chip leaves 64 known bytes in its FIFO (datasheet 16.1.1, p.82); matching all
 * 64 proves the register writes, a command, and a 64-byte FIFO burst read.
 *
 * Wiring is the same as rc522_version (see that file).
 *
 *   selftest      run it and compare with the table for the chip version
 *   selftestraw   same, but without clearing the internal buffer first
 *   version       VersionReg (0x91 = v1.0, 0x92 = v2.0)
 *   speed <3-7>   SPI rate, 3 = 5.6 MHz ... 7 = 351 kHz
 *
 * The 25-byte internal buffer (see docs/mfrc522.md, datasheet p.71 and p.73):
 *   memread       copy the buffer out through the FIFO and print it
 *   memfill <n>   write n, n+1, ... n+24 into the buffer
 *   memtest       read after reset, write a pattern, read back, soft reset,
 *                 read again (should survive), clear, read again
 */

#define RC522_SPI_MODE  0u
#define BR_FASTEST      3u                  /* first rate under the chip's 10 Mbit/s */
#define ROW_BYTES       8u                  /* printed like the datasheet table */
#define MEM_ROW         5u                  /* the 25-byte buffer as 5 x 5 */

/* Expected FIFO contents, datasheet p.82. v2.0 prints two entries with an
   extra leading zero ("061h", "03Bh"); they are read as 0x61 and 0x3B. */
static const uint8_t g_expect_v1[RC522_FIFO_SIZE] = {
    0x00, 0xC6, 0x37, 0xD5, 0x32, 0xB7, 0x57, 0x5C,
    0xC2, 0xD8, 0x7C, 0x4D, 0xD9, 0x70, 0xC7, 0x73,
    0x10, 0xE6, 0xD2, 0xAA, 0x5E, 0xA1, 0x3E, 0x5A,
    0x14, 0xAF, 0x30, 0x61, 0xC9, 0x70, 0xDB, 0x2E,
    0x64, 0x22, 0x72, 0xB5, 0xBD, 0x65, 0xF4, 0xEC,
    0x22, 0xBC, 0xD3, 0x72, 0x35, 0xCD, 0xAA, 0x41,
    0x1F, 0xA7, 0xF3, 0x53, 0x14, 0xDE, 0x7E, 0x02,
    0xD9, 0x0F, 0xB5, 0x5E, 0x25, 0x1D, 0x29, 0x79,
};

static const uint8_t g_expect_v2[RC522_FIFO_SIZE] = {
    0x00, 0xEB, 0x66, 0xBA, 0x57, 0xBF, 0x23, 0x95,
    0xD0, 0xE3, 0x0D, 0x3D, 0x27, 0x89, 0x5C, 0xDE,
    0x9D, 0x3B, 0xA7, 0x00, 0x21, 0x5B, 0x89, 0x82,
    0x51, 0x3A, 0xEB, 0x02, 0x0C, 0xA5, 0x00, 0x49,
    0x7C, 0x84, 0x4D, 0xB3, 0xCC, 0xD2, 0x1B, 0x81,
    0x5D, 0x48, 0x76, 0xD5, 0x71, 0x61, 0x21, 0xA9,
    0x86, 0x96, 0x83, 0x38, 0xCF, 0x9D, 0x5B, 0x6D,
    0xDC, 0x15, 0xBA, 0x3E, 0x7D, 0x95, 0x3B, 0x2F,
};

static uint32_t g_br = SPI_BR_SLOWEST;

/* ---------- self test: run it, compare with the datasheet table ---------- */

static void run_selftest(int keep_buffer)
{
    uint8_t got[RC522_FIFO_SIZE];
    uint8_t version = rc522_read(RC522_REG_VERSION);
    const uint8_t *expect = (version == 0x91u) ? g_expect_v1 : (version == 0x92u) ? g_expect_v2 : 0;

    int rc = keep_buffer ? rc522_self_test_keep_buffer(got) : rc522_self_test(got);
    if (rc != 0) {
        uart_puts("self test failed: ");
        uart_puts(rc == -1 ? "soft reset never finished"
                : rc == -2 ? "Mem command never finished"
                           : "FIFO never reached 64 bytes");
        uart_puts("\r\n");
        return;
    }

    uart_puts("FIFO after self test:\r\n");
    print_bytes(uart_putc, got, RC522_FIFO_SIZE, ROW_BYTES);

    if (expect == 0) {
        uart_puts("version ");
        print_hex8(uart_putc, version);
        uart_puts(" has no table in the datasheet: compare by hand\r\n");
        return;
    }

    uint32_t bad = 0u;
    uint32_t first_bad = 0u;
    for (uint32_t i = 0u; i < RC522_FIFO_SIZE; i++) {
        if (got[i] != expect[i]) {
            if (bad == 0u) {
                first_bad = i;
            }
            bad++;
        }
    }

    print_dec32(uart_putc, RC522_FIFO_SIZE - bad);
    uart_puts("/64 bytes match the v");
    uart_puts(version == 0x91u ? "1.0" : "2.0");
    uart_puts(" table");
    if (bad != 0u) {
        uart_puts(", first bad at byte ");
        print_dec32(uart_putc, first_bad);
        uart_puts(": expected ");
        print_hex8(uart_putc, expect[first_bad]);
        uart_puts(" got ");
        print_hex8(uart_putc, got[first_bad]);
    }
    uart_puts("\r\n");
}

static void cmd_selftest(char *args[])
{
    (void)args;
    run_selftest(0);
}

static void cmd_selftestraw(char *args[])
{
    (void)args;
    run_selftest(1);
}

/* ---------- internal buffer: the 25 bytes behind the Mem command ---------- */

static int bytes_equal(const uint8_t *a, const uint8_t *b, uint32_t n)
{
    for (uint32_t i = 0u; i < n; i++) {
        if (a[i] != b[i]) {
            return 0;
        }
    }
    return 1;
}

static int show_mem(const char *label, const uint8_t *expect)
{
    uint8_t got[RC522_MEM_SIZE];
    int rc = rc522_mem_read(got);

    uart_puts(label);
    if (rc != 0) {
        uart_puts(": read failed (");
        uart_puts(rc == -2 ? "Mem never finished" : "FIFO did not hold 25 bytes");
        uart_puts(")\r\n");
        return 0;
    }
    uart_puts(":\r\n");
    print_bytes(uart_putc, got, RC522_MEM_SIZE, MEM_ROW);
    if (expect == 0) {
        return 1;
    }
    int ok = bytes_equal(got, expect, RC522_MEM_SIZE);
    uart_puts(ok ? "  -> as expected\r\n" : "  -> DIFFERENT from what was written\r\n");
    return ok;
}

static void cmd_memread(char *args[])
{
    (void)args;
    (void)show_mem("internal buffer", 0);
}

static void cmd_memfill(char *args[])
{
    uint32_t start;
    uint8_t data[RC522_MEM_SIZE];

    if (!parse_u32(args[0], &start) || start > 0xFFu) {
        uart_puts("usage: memfill <0-255>\r\n");
        return;
    }
    for (uint32_t i = 0u; i < RC522_MEM_SIZE; i++) {
        data[i] = (uint8_t)(start + i);
    }
    if (rc522_mem_write(data) != 0) {
        uart_puts("write failed: Mem never finished\r\n");
        return;
    }
    uart_puts("buffer written\r\n");
}

static void cmd_memtest(char *args[])
{
    (void)args;
    uint8_t pattern[RC522_MEM_SIZE];
    uint8_t zeros[RC522_MEM_SIZE];

    for (uint32_t i = 0u; i < RC522_MEM_SIZE; i++) {
        pattern[i] = (uint8_t)(i + 1u);
        zeros[i] = 0u;
    }

    if (rc522_soft_reset() != 0) {
        uart_puts("soft reset failed\r\n");
        return;
    }
    (void)show_mem("1. after soft reset (whatever the chip held)", 0);

    if (rc522_mem_write(pattern) != 0) {
        uart_puts("write failed\r\n");
        return;
    }
    (void)show_mem("2. after writing 01..19", pattern);

    (void)rc522_soft_reset();
    (void)show_mem("3. after another soft reset (p.73: unchanged)", pattern);

    if (rc522_mem_write(zeros) != 0) {
        uart_puts("write failed\r\n");
        return;
    }
    (void)show_mem("4. after writing zeros", zeros);
}

/* ---------- chip and bus ---------- */

static void cmd_version(char *args[])
{
    (void)args;
    uart_puts("VersionReg = ");
    print_hex8(uart_putc, rc522_read(RC522_REG_VERSION));
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

/* ---------- command table and main ---------- */

static const cli_cmd_t g_commands[] = {
    { "selftest",    0u, "selftest",     cmd_selftest },
    { "selftestraw", 0u, "selftestraw",  cmd_selftestraw },
    { "memread",     0u, "memread",      cmd_memread },
    { "memfill",     1u, "memfill <n>",  cmd_memfill },
    { "memtest",     0u, "memtest",      cmd_memtest },
    { "version",     0u, "version",      cmd_version },
    { "speed",       1u, "speed <3-7>",  cmd_speed },
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
