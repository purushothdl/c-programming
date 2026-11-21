#include "stm32f4xx.h"
#include "clock.h"
#include "cli.h"
#include "fmt.h"
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
 * belongs to the LED and lib/pwm.c. PB3 and PB4 are JTAG pins after reset
 * (TRACESWO, NJTRST); SWD only needs PA13/PA14, so taking them is safe.
 *
 * SPI1 runs from APB2 = 90 MHz after clock_init(). SCK = 90 MHz / 2^(BR+1):
 *   BR 0: 45 MHz (the SPI1 master maximum, datasheet table 63) ... BR 7: 351.5 kHz
 *
 *   loop <0-7>      send 0..255 at that BR, compare what came back
 *   xfer <byte>     one byte with CS low, for an analyser capture
 *   mode <0-3>      CPOL/CPHA (a loopback passes in every mode)
 */

#define SPI1_AF         5u
#define SCK_PIN         3u
#define MISO_PIN        4u
#define MOSI_PIN        5u
#define CS_PIN          6u
#define LOOP_BYTES      256u
#define BR_SLOWEST      7u

static uint32_t g_br = BR_SLOWEST;          /* start slow: an 8-channel analyser can follow it */
static uint32_t g_mode;                     /* bit 1 = CPOL, bit 0 = CPHA */

static void pin_af5(uint32_t pin)
{
    GPIOB->MODER &= ~(3u << (pin * 2u));
    GPIOB->MODER |= 2u << (pin * 2u);
    GPIOB->AFR[0] &= ~(0xFu << (pin * 4u));
    GPIOB->AFR[0] |= SPI1_AF << (pin * 4u);
    GPIOB->PUPDR &= ~(3u << (pin * 2u));    /* PB4 has a pull-up after reset (NJTRST): remove it */
    GPIOB->OSPEEDR &= ~(3u << (pin * 2u));
    GPIOB->OSPEEDR |= 2u << (pin * 2u);     /* "fast": the speed table 63 is specified at */
}

static void spi_pins_init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;

    pin_af5(SCK_PIN);
    pin_af5(MISO_PIN);
    pin_af5(MOSI_PIN);

    /* CS is idle high. Set the output level before switching to output mode,
       so the pin never drives low for a moment. */
    GPIOB->BSRR = GPIO_BSRR_BS6;
    GPIOB->MODER &= ~GPIO_MODER_MODER6;
    GPIOB->MODER |= GPIO_MODER_MODER6_0;
}

/* BR, CPOL and CPHA must not change while a transfer runs (RM0390 26.7.1),
   so the SPI is disabled around the change. */
static void spi_configure(uint32_t br, uint32_t mode)
{
    while ((SPI1->SR & SPI_SR_BSY) != 0u) {
    }
    SPI1->CR1 &= ~SPI_CR1_SPE;

    /* MSTR: this side drives SCK.
       SSM + SSI: NSS is managed in software and held "high" internally. With
       SSI = 0 the master would think another master selected it and fault
       (MODF clears MSTR and SPE, RM0390 26.3.13). The real chip select is the
       PB6 GPIO, driven by hand. */
    SPI1->CR1 = SPI_CR1_MSTR | SPI_CR1_SSM | SPI_CR1_SSI |
                (br << SPI_CR1_BR_Pos) |
                (((mode & 2u) != 0u) ? SPI_CR1_CPOL : 0u) |
                (((mode & 1u) != 0u) ? SPI_CR1_CPHA : 0u);
    SPI1->CR1 |= SPI_CR1_SPE;
}

static void spi_init(void)
{
    spi_pins_init();
    RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;
    SPI1->CR2 = 0u;                         /* no interrupts, no DMA, Motorola frame format */
    spi_configure(g_br, g_mode);
}

/*
 * One byte each way. Writing DR starts 8 SCK pulses; the byte shifted in at
 * the same time lands in DR when RXNE sets. Reading DR every time keeps RXNE
 * from overflowing into OVR.
 */
static uint8_t spi_xfer(uint8_t out)
{
    while ((SPI1->SR & SPI_SR_TXE) == 0u) {
    }
    SPI1->DR = out;
    while ((SPI1->SR & SPI_SR_RXNE) == 0u) {
    }
    return (uint8_t)SPI1->DR;
}

/* Wait for the last bit to leave before raising CS: TXE first, then BSY
   (RM0390 26.3.10; BSY sets 2 APB clocks after the DR write). */
static void spi_wait_idle(void)
{
    while ((SPI1->SR & SPI_SR_TXE) == 0u) {
    }
    while ((SPI1->SR & SPI_SR_BSY) != 0u) {
    }
}

static void cs_low(void)  { GPIOB->BSRR = GPIO_BSRR_BR6; }
static void cs_high(void) { spi_wait_idle(); GPIOB->BSRR = GPIO_BSRR_BS6; }

static void print_hex8(uint8_t v)
{
    static const char hex[] = "0123456789ABCDEF";

    uart_puts("0x");
    uart_putc(hex[v >> 4]);
    uart_putc(hex[v & 0xFu]);
}

static void print_sck(void)
{
    uart_puts("SCK ");
    print_dec32(uart_putc, clock_pclk2_hz() >> (g_br + 1u));
    uart_puts(" Hz, mode ");
    print_dec32(uart_putc, g_mode);
    uart_puts("\r\n");
}

static void cmd_loop(char *args[])
{
    uint32_t br;

    if (!parse_u32(args[0], &br) || br > BR_SLOWEST) {
        uart_puts("usage: loop <0-7>  (0 = 45 MHz ... 7 = 351 kHz)\r\n");
        return;
    }
    g_br = br;
    spi_configure(g_br, g_mode);

    /* 0..255: every bit pattern once, so a stuck or swapped bit shows up. */
    uint32_t bad = 0u;
    uint8_t first_sent = 0u;
    uint8_t first_got = 0u;

    cs_low();
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
    cs_high();

    print_sck();
    print_dec32(uart_putc, LOOP_BYTES - bad);
    uart_puts("/256 bytes matched");
    if (bad != 0u) {
        uart_puts(", first bad: sent ");
        print_hex8(first_sent);
        uart_puts(" got ");
        print_hex8(first_got);
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

    cs_low();
    uint8_t got = spi_xfer((uint8_t)v);
    cs_high();

    uart_puts("sent ");
    print_hex8((uint8_t)v);
    uart_puts(" got ");
    print_hex8(got);
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
    spi_init();

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
