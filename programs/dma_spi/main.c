#include "stm32f4xx.h"
#include "clock.h"
#include "cli.h"
#include "delay.h"
#include "fmt.h"
#include "spi.h"
#include "uart.h"

/*
 * Step 10b: SPI1 driven by DMA. See visualizations/dma/bus.html (scene 3).
 *
 *   command        what it shows                                   wiring
 *   -------------  ----------------------------------------------  ---------------------
 *   show <byte>    one byte to the 595 by DMA                      595 as in sr595/,
 *   digits         0-9 on the display, one DMA run each            QH' unconnected
 *   burst          the latch trap: digits 0-5 in ONE run
 *   busy <n>       n bytes by DMA while the CPU counts, then by
 *                  polling: the CPU is free during DMA
 *   loop <br>      256 bytes out and back, rate br 0-7:            jumper PB5 -> PB4,
 *                  rx[i] == tx[i]. Tests the DMA path alone.       QH' off PB4
 *   chain <br>     the same through the 595's QH' (8 cells =       QH' -> PB4,
 *                  one byte of delay): rx[i+1] == tx[i]            no jumper
 *   regs           stream 3 / stream 0 / SPI1 registers after the last run
 *
 * Requests (RM0390 Table 29, p.204): SPI1_TX is DMA2 stream 3, SPI1_RX is DMA2
 * stream 0, both channel 3. A TX request comes each time TXE sets and an RX
 * request each time RXNE sets, once TXDMAEN / RXDMAEN are set (p.840).
 *
 * Start order is the manual's (p.840): RXDMAEN, enable the streams, TXDMAEN, SPE.
 * End order: stream TCIF, then TXE = 1, then BSY = 0, because TCIF only means the
 * DMA wrote the last byte into DR, not that it left the pin.
 *
 * Chip select stays low for the whole DMA run, so the 595's RCLK rises once, at
 * the end: only the last byte of a run reaches its pins (`burst` shows this).
 *
 * A TX-only run leaves received bytes unread, so OVR sets and RXNE stays 1 with a
 * stale byte. dma_end() reads DR then SR to clear both, or the next polled
 * spi_xfer() would return that stale byte at once.
 */

/* ---------- constants ---------- */

#define SR_BR           SPI_BR_SLOWEST      /* the 595 and the display are fine at 351 kHz */
#define SR_MODE         0u
#define BUF_BYTES       256u
#define DIGIT_MS        500u
#define POLL_LIMIT      5000000u            /* bounds a stuck wait: far above any run here */
#define TIMER_RELOAD    0x00FFFFFFu         /* SysTick is 24 bits */
#define RX_FILL         0xEEu               /* marks a receive byte the DMA never wrote */

#define SPI1_CHANNEL    (DMA_SxCR_CHSEL_0 | DMA_SxCR_CHSEL_1)       /* channel 3 */
/* memory to peripheral, memory address goes up, both sides bytes (SPI frame is 8 bit) */
#define TX_CONFIG       (SPI1_CHANNEL | DMA_SxCR_DIR_0 | DMA_SxCR_MINC)
/* peripheral to memory (DIR = 00) */
#define RX_CONFIG       (SPI1_CHANNEL | DMA_SxCR_MINC)

#define TX_FLAGS        (DMA_LIFCR_CTCIF3 | DMA_LIFCR_CHTIF3 | DMA_LIFCR_CTEIF3 | \
                         DMA_LIFCR_CDMEIF3 | DMA_LIFCR_CFEIF3)
#define RX_FLAGS        (DMA_LIFCR_CTCIF0 | DMA_LIFCR_CHTIF0 | DMA_LIFCR_CTEIF0 | \
                         DMA_LIFCR_CDMEIF0 | DMA_LIFCR_CFEIF0)
#define TX_ERRORS       (DMA_LISR_TEIF3 | DMA_LISR_DMEIF3)          /* no FIFO in direct mode */
#define RX_ERRORS       (DMA_LISR_TEIF0 | DMA_LISR_DMEIF0)

enum { RUN_OK = 0, RUN_STREAM_ERR = 1, RUN_TIMEOUT = 2 };

/* Segment bits a=bit0 ... g=bit6 (same table as sr595/). */
static const uint8_t g_digit[10] = {
    0x3Fu, 0x06u, 0x5Bu, 0x4Fu, 0x66u, 0x6Du, 0x7Du, 0x07u, 0x7Fu, 0x6Fu
};

/* ---------- state ---------- */

static uint8_t g_tx[BUF_BYTES];
static uint8_t g_rx[BUF_BYTES];
static int g_rx_used;                       /* the run in progress also receives by DMA */
static int g_ovr;                           /* OVR was set at the end of the last run */

/* ---------- timer ---------- */

static void timer_init(void)
{
    SysTick->LOAD = TIMER_RELOAD;
    SysTick->VAL = 0u;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk;
}

static uint32_t since(uint32_t t0)
{
    return (t0 - SysTick->VAL) & TIMER_RELOAD;  /* counts down, so t0 is the larger value */
}

/* ---------- DMA on SPI1 ---------- */

/* Configuration bits are write-protected while EN = 1 (RM0390 p.227): stop the
   stream and wait until EN reads 0. */
static void stream_stop(DMA_Stream_TypeDef *s)
{
    s->CR &= ~DMA_SxCR_EN;
    while ((s->CR & DMA_SxCR_EN) != 0u) {
    }
}

/* Starts sending n bytes of tx. If rx is not NULL the n bytes shifted in are
   stored there. Returns at once; the caller waits with dma_end(). */
static void dma_begin(const uint8_t *tx, uint8_t *rx, uint32_t n)
{
    DMA_Stream_TypeDef *txs = DMA2_Stream3;
    DMA_Stream_TypeDef *rxs = DMA2_Stream0;

    while ((SPI1->SR & SPI_SR_BSY) != 0u) {
    }
    SPI1->CR1 &= ~SPI_CR1_SPE;              /* SPE goes last (p.840) */

    stream_stop(txs);
    stream_stop(rxs);
    DMA2->LIFCR = TX_FLAGS | RX_FLAGS;      /* flags of the last run would end the wait at once */

    txs->PAR = (uint32_t)&SPI1->DR;
    txs->M0AR = (uint32_t)tx;
    txs->NDTR = n;
    txs->FCR = 0u;                          /* direct mode: bytes go straight to DR */
    txs->CR = TX_CONFIG;

    g_rx_used = (rx != 0);
    if (g_rx_used) {
        rxs->PAR = (uint32_t)&SPI1->DR;
        rxs->M0AR = (uint32_t)rx;
        rxs->NDTR = n;
        rxs->FCR = 0u;
        rxs->CR = RX_CONFIG;
        SPI1->CR2 |= SPI_CR2_RXDMAEN;       /* 1 */
        rxs->CR = RX_CONFIG | DMA_SxCR_EN;  /* 2 */
    }
    txs->CR = TX_CONFIG | DMA_SxCR_EN;      /* 2 */
    SPI1->CR2 |= SPI_CR2_TXDMAEN;           /* 3 */
    SPI1->CR1 |= SPI_CR1_SPE;               /* 4: the first TXE request fires here */
}

/* The DMA has written the last byte to DR (and, with RX, read the last one). */
static int dma_done(void)
{
    int tx_done = (DMA2->LISR & DMA_LISR_TCIF3) != 0u;
    int rx_done = !g_rx_used || (DMA2->LISR & DMA_LISR_TCIF0) != 0u;

    return tx_done && rx_done;
}

/* Waits for the run to finish and leaves SPI1 ready for the polled driver. */
static int dma_end(void)
{
    int rc = RUN_OK;
    uint32_t i;

    for (i = 0u; i < POLL_LIMIT && !dma_done(); i++) {
        if ((DMA2->LISR & (TX_ERRORS | RX_ERRORS)) != 0u) {
            rc = RUN_STREAM_ERR;
            break;
        }
    }
    if (rc == RUN_OK && i == POLL_LIMIT) {
        rc = RUN_TIMEOUT;
    }

    for (i = 0u; i < POLL_LIMIT && (SPI1->SR & SPI_SR_TXE) == 0u; i++) {
    }
    for (i = 0u; i < POLL_LIMIT && (SPI1->SR & SPI_SR_BSY) != 0u; i++) {
    }

    SPI1->CR2 &= ~(SPI_CR2_TXDMAEN | SPI_CR2_RXDMAEN);
    g_ovr = (SPI1->SR & SPI_SR_OVR) != 0u;
    (void)SPI1->DR;                         /* DR then SR clears OVR (RM0390 26.3.13) */
    (void)SPI1->SR;
    return rc;
}

/* ---------- the three ways to run it ---------- */

/* 595 update: CS low, n bytes by DMA, CS high is the latch. */
static int send_dma(const uint8_t *tx, uint32_t n)
{
    spi_cs_low();
    dma_begin(tx, 0, n);
    int rc = dma_end();
    spi_cs_high();
    return rc;
}

/* 256 bytes out and 256 in, at SPI rate br. Puts the polled driver back at the
   595's rate afterwards, so the other commands keep working. */
static int exchange(uint32_t br)
{
    for (uint32_t i = 0u; i < BUF_BYTES; i++) {
        g_tx[i] = (uint8_t)(i * 37u + 11u);     /* 37 is odd: all 256 values appear once */
        g_rx[i] = RX_FILL;
    }

    spi_configure(br, SR_MODE);
    spi_cs_low();
    dma_begin(g_tx, g_rx, BUF_BYTES);
    int rc = dma_end();
    spi_cs_high();
    spi_configure(SR_BR, SR_MODE);
    return rc;
}

/* Counts the bytes where rx[i + lag] != tx[i]. lag is how many bytes the wiring
   delays the data: 0 for a jumper, 1 for the 595 (its shift row is 8 cells).
   *first gets the index of the first bad byte. */
static uint32_t count_mismatches(uint32_t lag, uint32_t *first)
{
    uint32_t bad = 0u;

    for (uint32_t i = 0u; i + lag < BUF_BYTES; i++) {
        if (g_rx[i + lag] != g_tx[i]) {
            if (bad == 0u) {
                *first = i;
            }
            bad++;
        }
    }
    return bad;
}

/* ---------- printing ---------- */

static const char *verdict(int rc)
{
    switch (rc) {
    case RUN_OK:         return "ok";
    case RUN_STREAM_ERR: return "STREAM ERROR flag";
    default:             return "TIMEOUT";
    }
}

static void print_num(const char *label, uint32_t value)
{
    uart_puts(label);
    print_dec32(uart_putc, value);
}

/* ---------- commands: the 595 ---------- */

static void cmd_show(char *args[])
{
    uint32_t v;

    if (!parse_u32(args[0], &v) || v > 0xFFu) {
        uart_puts("usage: show <0-0xFF>\r\n");
        return;
    }
    g_tx[0] = (uint8_t)v;
    int rc = send_dma(g_tx, 1u);
    uart_puts("sent ");
    print_hex8(uart_putc, (uint8_t)v);
    uart_puts(" by DMA: ");
    uart_puts(verdict(rc));
    uart_puts("\r\n");
}

static void cmd_digits(char *args[])
{
    (void)args;
    for (uint32_t d = 0u; d < 10u; d++) {
        g_tx[0] = g_digit[d];
        (void)send_dma(g_tx, 1u);
        delay_ms(DIGIT_MS);
    }
    g_tx[0] = 0u;
    (void)send_dma(g_tx, 1u);
}

/* Six bytes in one run: all six are shifted through the 595, but RCLK rises only
   once, after the last. Expected: the display shows 5 (0x6D) and never 0-4. */
static void cmd_burst(char *args[])
{
    (void)args;
    for (uint32_t i = 0u; i < 6u; i++) {
        g_tx[i] = g_digit[i];
    }
    int rc = send_dma(g_tx, 6u);
    uart_puts("6 bytes (digits 0-5) in one run: ");
    uart_puts(verdict(rc));
    uart_puts("\r\nwatch the display: 0-4 never appear, only the last byte latches\r\n");
}

/* ---------- commands: the CPU is free ---------- */

static void cmd_busy(char *args[])
{
    uint32_t n;

    if (!parse_u32(args[0], &n) || n == 0u || n > BUF_BYTES) {
        uart_puts("usage: busy <1-256>\r\n");
        return;
    }
    for (uint32_t i = 0u; i < n; i++) {
        g_tx[i] = (uint8_t)(i * 37u + 11u);
    }

    timer_init();                           /* delay_ms() switches SysTick off, so restart it */
    volatile uint32_t spins = 0u;
    uint32_t t0 = SysTick->VAL;
    spi_cs_low();
    dma_begin(g_tx, 0, n);
    while (!dma_done()) {
        spins++;                            /* the CPU is free: this stands for real work */
    }
    int rc = dma_end();
    spi_cs_high();
    uint32_t dma_cycles = since(t0);
    uint32_t work = spins;

    t0 = SysTick->VAL;
    spi_cs_low();
    for (uint32_t i = 0u; i < n; i++) {
        (void)spi_xfer(g_tx[i]);
    }
    spi_cs_high();
    uint32_t poll_cycles = since(t0);

    uint32_t sck = spi_sck_hz(SR_BR);
    print_num("n = ", n);
    print_num(" bytes at SCK ", sck);
    print_num(" Hz: on the wire ", n * 8u * 1000000u / sck);
    print_num(" us\r\n  DMA:     ", dma_cycles);
    uart_puts(" cycles (");
    uart_puts(verdict(rc));
    print_num("), the CPU counted ", work);
    print_num(" loop turns meanwhile\r\n  polling: ", poll_cycles);
    uart_puts(" cycles, the CPU did nothing else\r\n");
}

/* ---------- commands: receive by DMA ---------- */

/* loop and chain differ only in the wiring and so in the lag. */
static void run_check(char *args[], uint32_t lag, const char *usage)
{
    uint32_t br;
    uint32_t first = 0u;

    if (!parse_u32(args[0], &br) || br > SPI_BR_SLOWEST) {
        uart_puts(usage);
        return;
    }
    int rc = exchange(br);
    uint32_t bad = count_mismatches(lag, &first);

    print_num("SCK ", spi_sck_hz(br));
    uart_puts(" Hz, 256 bytes: ");
    uart_puts(verdict(rc));
    print_num(", ", bad);
    uart_puts(" mismatches, OVR ");
    uart_puts(g_ovr ? "set" : "clear");
    uart_puts("\r\n");
    if (bad != 0u) {
        print_num("  first at ", first);
        uart_puts(": sent ");
        print_hex8(uart_putc, g_tx[first]);
        uart_puts(", got ");
        print_hex8(uart_putc, g_rx[first + lag]);
        uart_puts("\r\n");
    }
}

static void cmd_loop(char *args[])
{
    run_check(args, 0u, "usage: loop <0-7>  (jumper PB5 to PB4, QH' off PB4)\r\n");
}

static void cmd_chain(char *args[])
{
    run_check(args, 1u, "usage: chain <0-7>  (595 QH' to PB4, no jumper)\r\n");
}

/* ---------- command: registers ---------- */

static void print_reg(const char *name, uint32_t value)
{
    uart_puts(name);
    print_hex32(uart_putc, value);
    uart_puts("\r\n");
}

static void cmd_regs(char *args[])
{
    (void)args;
    print_reg("S3CR     ", DMA2_Stream3->CR);
    print_reg("S3NDTR   ", DMA2_Stream3->NDTR);
    print_reg("S0CR     ", DMA2_Stream0->CR);
    print_reg("S0NDTR   ", DMA2_Stream0->NDTR);
    print_reg("LISR     ", DMA2->LISR);
    print_reg("SPI1 CR2 ", SPI1->CR2);
    print_reg("SPI1 SR  ", SPI1->SR);
}

static const cli_cmd_t g_commands[] = {
    { "show",   1u, "show <byte>",  cmd_show },
    { "digits", 0u, "digits",       cmd_digits },
    { "burst",  0u, "burst",        cmd_burst },
    { "busy",   1u, "busy <1-256>", cmd_busy },
    { "loop",   1u, "loop <0-7>",   cmd_loop },
    { "chain",  1u, "chain <0-7>",  cmd_chain },
    { "regs",   0u, "regs",         cmd_regs },
};

int main(void)
{
    clock_init();
    uart_init();
    spi_init(SR_BR, SR_MODE);
    RCC->AHB1ENR |= RCC_AHB1ENR_DMA2EN;
    (void)RCC->AHB1ENR;                     /* let the clock enable settle before the first access */

    g_tx[0] = 0u;
    (void)send_dma(g_tx, 1u);               /* blank display */

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
