#include "stm32f4xx.h"
#include "clock.h"
#include "cli.h"
#include "fmt.h"
#include "uart.h"

/*
 * Step 10a: memory-to-memory DMA, measured against a CPU copy loop.
 * See visualizations/dma/bus.html. No wiring: both buffers are in SRAM.
 *
 *   copy <words>   copy 1-1024 words (4 bytes each) both ways, time both in CPU
 *                  cycles, check the result
 *   sweep          the same for 1, 4, 16, 64, 256 and 1024 words
 *   info           clock, timer overhead, buffer addresses
 *   regs           the DMA2 stream 0 registers after the last run
 *
 * DMA2 stream 0 is used. Memory-to-memory needs no request line: the stream
 * starts as soon as EN is set (RM0390 p.208). Only DMA2 can do it, and neither
 * direct nor circular mode is allowed (p.208), so the FIFO is on. DIR = 10
 * (Table 30 p.206): source in S0PAR, destination in S0M0AR.
 *
 * Timing is SysTick, free running at the CPU clock (counts down from 0xFFFFFF,
 * PM0214), so one reading is one cycle. It wraps after 16.7M cycles: far more
 * than any copy here. The cost of the timer reads is measured and subtracted.
 *
 * The CPU loop is compiled with the Makefile's -O0, so it is the slow case.
 * Both buffers sit in SRAM1: the stream reads through its peripheral port and
 * writes through its memory port, and the two take turns at SRAM1
 * (RM0390 p.54-55).
 */

#define BUF_WORDS       1024u
#define GUARD           0xDEADBEEFu             /* fills dst before a copy: shows an overrun */
#define TIMER_RELOAD    0x00FFFFFFu             /* SysTick is 24 bits */
#define POLL_LIMIT      2000000u                /* generous: bounds a stuck stream */

/* Stream 0 flags in LISR/LIFCR: TEIF0, DMEIF0, FEIF0 and TCIF0 (SVD). */
#define S0_ERRORS       (DMA_LISR_TEIF0 | DMA_LISR_DMEIF0 | DMA_LISR_FEIF0)
#define S0_ALL_FLAGS    (DMA_LIFCR_CTCIF0 | DMA_LIFCR_CHTIF0 | DMA_LIFCR_CTEIF0 | \
                         DMA_LIFCR_CDMEIF0 | DMA_LIFCR_CFEIF0)

/* word size on both sides, both addresses go up by one word, memory to memory */
#define S0_CONFIG       (DMA_SxCR_DIR_1 | DMA_SxCR_PINC | DMA_SxCR_MINC | \
                         DMA_SxCR_PSIZE_1 | DMA_SxCR_MSIZE_1)

static uint32_t g_src[BUF_WORDS];
static uint32_t g_dst[BUF_WORDS];
static uint32_t g_overhead;                     /* cycles taken by two timer reads */

enum { COPY_OK = 0, COPY_BAD_DATA = 1, COPY_STREAM_ERR = 2, COPY_TIMEOUT = 3 };

typedef struct {
    uint32_t cpu;                               /* cycles */
    uint32_t dma_total;
    uint32_t dma_setup;                         /* until EN is set */
    int cpu_ok;
    int dma_ok;
} result_t;

/* ---------- timer ---------- */

static void timer_init(void)
{
    SysTick->LOAD = TIMER_RELOAD;
    SysTick->VAL = 0u;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk;
}

static inline uint32_t now(void)
{
    return SysTick->VAL;
}

static uint32_t since(uint32_t t0)
{
    return (t0 - now()) & TIMER_RELOAD;         /* counts down, so t0 is the larger value */
}

/* ---------- the two copies ---------- */

static void fill(void)
{
    for (uint32_t i = 0u; i < BUF_WORDS; i++) {
        g_src[i] = 0xA5000000u | i;             /* every word differs, so a shifted copy shows */
        g_dst[i] = GUARD;
    }
}

static int check(uint32_t n)
{
    for (uint32_t i = 0u; i < n; i++) {
        if (g_dst[i] != g_src[i]) {
            return COPY_BAD_DATA;
        }
    }
    for (uint32_t i = n; i < BUF_WORDS; i++) {
        if (g_dst[i] != GUARD) {
            return COPY_BAD_DATA;               /* wrote past the end */
        }
    }
    return COPY_OK;
}

static void cpu_copy(uint32_t *dst, const uint32_t *src, uint32_t n)
{
    for (uint32_t i = 0u; i < n; i++) {
        dst[i] = src[i];
    }
}

/* Returns a COPY_ code. *setup gets the cycles until the stream is enabled. */
static int dma_copy(uint32_t *dst, const uint32_t *src, uint32_t n, uint32_t *setup)
{
    DMA_Stream_TypeDef *s = DMA2_Stream0;
    uint32_t t0 = now();

    /* The configuration bits are write-protected while EN = 1 (p.227, p.230):
       stop the stream and wait until EN reads 0 before touching them. */
    s->CR &= ~DMA_SxCR_EN;
    while ((s->CR & DMA_SxCR_EN) != 0u) {
    }
    DMA2->LIFCR = S0_ALL_FLAGS;                 /* a flag from the last run would end the wait at once */

    s->PAR = (uint32_t)src;
    s->M0AR = (uint32_t)dst;
    s->NDTR = n;
    s->FCR = DMA_SxFCR_DMDIS | DMA_SxFCR_FTH;   /* direct mode off, drain at a full FIFO (p.230) */
    s->CR = S0_CONFIG;
    s->CR = S0_CONFIG | DMA_SxCR_EN;
    *setup = since(t0);

    for (uint32_t i = 0u; i < POLL_LIMIT; i++) {
        uint32_t flags = DMA2->LISR;
        if ((flags & S0_ERRORS) != 0u) {
            return COPY_STREAM_ERR;
        }
        if ((flags & DMA_LISR_TCIF0) != 0u) {
            return COPY_OK;
        }
    }
    return COPY_TIMEOUT;
}

/* One size: a warm-up run first (the CPU loop runs from Flash, and its first
   pass also fills the instruction cache), then the timed run. */
static void measure(uint32_t n, result_t *r)
{
    uint32_t t0;
    uint32_t setup;
    int rc;

    fill();
    cpu_copy(g_dst, g_src, n);
    fill();
    t0 = now();
    cpu_copy(g_dst, g_src, n);
    r->cpu = since(t0) - g_overhead;
    r->cpu_ok = check(n);

    fill();
    (void)dma_copy(g_dst, g_src, n, &setup);
    fill();
    t0 = now();
    rc = dma_copy(g_dst, g_src, n, &setup);
    r->dma_total = since(t0) - g_overhead;
    r->dma_setup = setup;
    r->dma_ok = (rc == COPY_OK) ? check(n) : rc;
}

/* ---------- printing ---------- */

static void print_fixed2(uint32_t v100)
{
    print_dec32(uart_putc, v100 / 100u);
    uart_putc('.');
    uart_putc((char)('0' + (v100 / 10u) % 10u));
    uart_putc((char)('0' + v100 % 10u));
}

static const char *verdict(int rc)
{
    switch (rc) {
    case COPY_OK:         return "ok";
    case COPY_BAD_DATA:   return "WRONG DATA";
    case COPY_STREAM_ERR: return "STREAM ERROR flag";
    default:              return "TIMEOUT";
    }
}

static void print_result(uint32_t n, const result_t *r)
{
    uart_puts("n = ");
    print_dec32(uart_putc, n);
    uart_puts(" words (");
    print_dec32(uart_putc, n * 4u);
    uart_puts(" bytes)\r\n  CPU loop: ");
    print_dec32(uart_putc, r->cpu);
    uart_puts(" cycles, ");
    print_fixed2(r->cpu * 100u / n);
    uart_puts(" per word, ");
    uart_puts(verdict(r->cpu_ok));
    uart_puts("\r\n  DMA:      ");
    print_dec32(uart_putc, r->dma_total);
    uart_puts(" cycles, ");
    print_fixed2(r->dma_total * 100u / n);
    uart_puts(" per word, ");
    uart_puts(verdict(r->dma_ok));
    uart_puts("  (setup ");
    print_dec32(uart_putc, r->dma_setup);
    uart_puts(", transfer ");
    print_dec32(uart_putc, r->dma_total > r->dma_setup ? r->dma_total - r->dma_setup : 0u);
    uart_puts(")\r\n  DMA time / CPU time = ");
    print_fixed2(r->dma_total * 100u / (r->cpu == 0u ? 1u : r->cpu));
    uart_puts("\r\n");
}

/* ---------- commands ---------- */

static void cmd_copy(char *args[])
{
    uint32_t n;
    result_t r;

    if (!parse_u32(args[0], &n) || n == 0u || n > BUF_WORDS) {
        uart_puts("usage: copy <1-1024>\r\n");
        return;
    }
    measure(n, &r);
    print_result(n, &r);
}

static void cmd_sweep(char *args[])
{
    static const uint32_t sizes[] = { 1u, 4u, 16u, 64u, 256u, 1024u };
    result_t r;
    (void)args;

    for (uint32_t i = 0u; i < sizeof sizes / sizeof sizes[0]; i++) {
        measure(sizes[i], &r);
        print_result(sizes[i], &r);
    }
}

static void cmd_info(char *args[])
{
    (void)args;
    uart_puts("CPU clock ");
    print_dec32(uart_putc, clock_sysclk_hz());
    uart_puts(" Hz, one timer tick = one cycle\r\ntimer read overhead ");
    print_dec32(uart_putc, g_overhead);
    uart_puts(" cycles (subtracted)\r\nsrc ");
    print_hex32(uart_putc, (uint32_t)g_src);
    uart_puts("  dst ");
    print_hex32(uart_putc, (uint32_t)g_dst);
    uart_puts("  (both SRAM1: 0x20000000-0x2001BFFF)\r\n");
}

static void cmd_regs(char *args[])
{
    (void)args;
    uart_puts("S0CR   ");
    print_hex32(uart_putc, DMA2_Stream0->CR);
    uart_puts("\r\nS0NDTR ");
    print_hex32(uart_putc, DMA2_Stream0->NDTR);
    uart_puts("\r\nS0FCR  ");
    print_hex32(uart_putc, DMA2_Stream0->FCR);
    uart_puts("\r\nLISR   ");
    print_hex32(uart_putc, DMA2->LISR);
    uart_puts("\r\n");
}

static const cli_cmd_t g_commands[] = {
    { "copy",  1u, "copy <words>", cmd_copy },
    { "sweep", 0u, "sweep",        cmd_sweep },
    { "info",  0u, "info",         cmd_info },
    { "regs",  0u, "regs",         cmd_regs },
};

int main(void)
{
    clock_init();
    uart_init();
    RCC->AHB1ENR |= RCC_AHB1ENR_DMA2EN;
    (void)RCC->AHB1ENR;                         /* let the clock enable settle before the first access */
    timer_init();

    uint32_t t0 = now();                        /* cost of one start and one stop read */
    g_overhead = since(t0);

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
