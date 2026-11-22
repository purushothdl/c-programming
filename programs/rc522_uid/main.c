#include "stm32f4xx.h"
#include "clock.h"
#include "cli.h"
#include "delay.h"
#include "fmt.h"
#include "rc522.h"
#include "spi.h"
#include "uart.h"

/*
 * Step 8d: read a tag's UID (visualizations/spi/rc522-uid.html). The MCU stays
 * SPI master; the tag answers by radio into the RC522 FIFO. Wiring as rc522_version.
 *
 *   reqa      one REQA, print the ATQA (00 04h = 4-byte UID, 00 44h = 7-byte)
 *   uid       REQA, then anticollision: UID + check byte
 *   scan      poll every 100 ms; prints the UID on arrival, "card gone" after 3 misses
 *   scanw     same with WUPA (52h) instead of REQA (26h)
 *   regs      dump the registers that matter; init = redo setup, test one write
 *   version   VersionReg
 *
 * Open question: does a tag answer the next REQA after anticollision, without
 * SELECT? Compare scan and scanw on a still card. 7-byte UIDs stop at 3 bytes.
 */

#define RC522_SPI_MODE  0u
#define POLL_MS         100u
#define GONE_POLLS      3u                  /* empty polls before "card gone" */

static const char *why(int rc)
{
    switch (rc) {
    case RC522_NO_TAG:    return "no tag answered";
    case RC522_ERR:       return "reply damaged (ErrorReg flagged it)";
    case RC522_NO_FLAG:   return "chip raised no flag (check wiring)";
    case RC522_BAD_FRAME: return "reply has the wrong length";
    case RC522_BAD_BCC:   return "check byte does not match";
    case RC522_CASCADE:   return "7-byte UID (cascade): only 3 bytes read";
    default:              return "ok";
    }
}

/* ---------- one attempt ---------- */

static void print_uid(const uint8_t uid[4], const uint8_t atqa[2])
{
    uart_puts("UID");
    for (uint32_t i = 0u; i < 4u; i++) {
        uart_putc(' ');
        print_hex8(uart_putc, uid[i]);
    }
    uart_puts("  ATQA ");
    print_hex8(uart_putc, atqa[0]);
    uart_putc(' ');
    print_hex8(uart_putc, atqa[1]);
    uart_puts("\r\n");
}

/* REQA then anticollision. Returns RC522_OK or CASCADE with uid filled. */
static int read_card(uint8_t uid[4], uint8_t atqa[2], int wake)
{
    int rc = rc522_request(atqa, wake);
    if (rc != RC522_OK) {
        return rc;
    }
    return rc522_read_uid(uid);
}

static void cmd_reqa(char *args[])
{
    (void)args;
    uint8_t atqa[2];
    int rc = rc522_request(atqa, 0);

    if (rc != RC522_OK) {
        uart_puts(why(rc));
        uart_puts("\r\n");
        return;
    }
    uart_puts("ATQA");
    print_bytes(uart_putc, atqa, 2u, 2u);
}

static void cmd_uid(char *args[])
{
    (void)args;
    uint8_t uid[4];
    uint8_t atqa[2];
    int rc = read_card(uid, atqa, 0);

    if (rc == RC522_OK || rc == RC522_CASCADE) {
        print_uid(uid, atqa);
    }
    if (rc != RC522_OK) {
        uart_puts(why(rc));
        uart_puts("\r\n");
    }
}

/* ---------- scanning ---------- */

static void scan(int wake)
{
    uint32_t misses = 0u;
    int present = 0;
    int last_err = RC522_OK;          /* last error printed; cleared by a good read */

    uart_puts(wake ? "scanning with WUPA, any key stops\r\n" : "scanning with REQA, any key stops\r\n");
    for (;;) {
        if (!uart_rx_empty()) {
            (void)uart_rx_pop();
            uart_puts("stopped\r\n");
            return;
        }

        uint8_t uid[4];
        uint8_t atqa[2];
        int rc = read_card(uid, atqa, wake);

        if (rc == RC522_OK || rc == RC522_CASCADE) {
            if (!present) {
                print_uid(uid, atqa);
                if (rc == RC522_CASCADE) {
                    uart_puts(why(rc));
                    uart_puts("\r\n");
                }
            }
            present = 1;
            misses = 0u;
            last_err = RC522_OK;
        } else if (rc == RC522_NO_TAG) {
            misses++;
            if (present && misses >= GONE_POLLS) {
                present = 0;
                uart_puts("card gone\r\n");
            }
        } else if (rc != last_err) {
            uart_puts(why(rc));         /* a repeated fault prints once, not ten times a second */
            uart_puts("\r\n");
            last_err = rc;
        }
        delay_ms(POLL_MS);
    }
}

static void cmd_scan(char *args[])
{
    (void)args;
    scan(0);
}

static void cmd_scanw(char *args[])
{
    (void)args;
    scan(1);
}

/* ---------- chip ---------- */

static void cmd_version(char *args[])
{
    (void)args;
    uart_puts("VersionReg = ");
    print_hex8(uart_putc, rc522_read(RC522_REG_VERSION));
    uart_puts("\r\n");
}

/* Registers that decide whether a REQA can go out. Expected after rc522_picc_init:
   TxControl 0x83, TMode 0x80, TPrescaler 0xA9, TReload 0x03E8, TxASK 0x40. */
static void cmd_regs(char *args[])
{
    static const struct { const char *name; uint8_t reg; } list[] = {
        { "Command   ", 0x01 }, { "ComIrq    ", 0x04 }, { "Error     ", 0x06 },
        { "Status1   ", 0x07 }, { "FIFOLevel ", 0x0A }, { "BitFraming", 0x0D },
        { "Mode      ", 0x11 }, { "TxControl ", 0x14 }, { "TxASK     ", 0x15 },
        { "TMode     ", 0x2A }, { "TPrescaler", 0x2B }, { "TReloadH  ", 0x2C },
        { "TReloadL  ", 0x2D }, { "Version   ", 0x37 },
    };
    (void)args;
    for (uint32_t i = 0u; i < sizeof list / sizeof list[0]; i++) {
        uart_puts("  ");
        uart_puts(list[i].name);
        uart_puts(" = ");
        print_hex8(uart_putc, rc522_read(list[i].reg));
        uart_puts("\r\n");
    }
}

/* Re-run the setup by hand. The probe goes first: it leaves a test value in
   TPrescaler, which picc_init then overwrites with the real one. No 0x00 in the
   probe: that is the reset value, so reading it back proves nothing. */
static void cmd_init(char *args[])
{
    static const uint8_t probe[] = { 0xA5, 0x5A, 0xFF };
    (void)args;

    for (uint32_t i = 0u; i < sizeof probe; i++) {
        rc522_write(0x2Bu, probe[i]);
        uart_puts("  wrote ");
        print_hex8(uart_putc, probe[i]);
        uart_puts(" read ");
        print_hex8(uart_putc, rc522_read(0x2Bu));
        uart_puts("\r\n");
    }
    rc522_picc_init();
    uart_puts("picc_init done, run regs\r\n");
}

/* ---------- command table and main ---------- */

static const cli_cmd_t g_commands[] = {
    { "reqa",    0u, "reqa",    cmd_reqa },
    { "uid",     0u, "uid",     cmd_uid },
    { "scan",    0u, "scan",    cmd_scan },
    { "scanw",   0u, "scanw",   cmd_scanw },
    { "version", 0u, "version", cmd_version },
    { "regs",    0u, "regs",    cmd_regs },
    { "init",    0u, "init",    cmd_init },
};

int main(void)
{
    clock_init();
    uart_init();
    spi_init(SPI_BR_SLOWEST, RC522_SPI_MODE);
    rc522_init();
    rc522_picc_init();

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
