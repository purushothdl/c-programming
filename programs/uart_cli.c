#include "stm32f4xx.h"

/*
 * Step 5d: interrupt-driven UART with a tiny command line.
 *
 *   USART2 ISR  --RX ring-->  main (line editor + command parser)
 *   main        --TX ring-->  USART2 ISR (TXE interrupt drains one byte)
 *
 * Each ring has exactly one producer and one consumer, so no locks are used.
 * The TX interrupt is enabled only while bytes are queued; the ISR turns it off
 * when the ring empties, so an idle UART does not interrupt the CPU.
 *
 * Commands:
 *   help
 *   led on | off
 *   ticks                  milliseconds since reset (SysTick)
 *   peek <addr>            read a 32-bit word
 *   poke <addr> <val>      write a 32-bit word
 * Numbers may be decimal or 0x-prefixed hex. Addresses must be 4-byte aligned.
 *
 * HardFault_Handler prints CFSR/HFSR/BFAR and the stacked PC over UART using
 * polled output, because interrupts cannot be trusted inside a fault. Then the
 * board resets so the CLI comes back up.
 */
#define CPU_CLK_HZ 16000000u
#define USART2_CLK_HZ 16000000u   /* PCLK1 at the reset-default clock setup */
#define UART_BAUD 115200u
#define UART_BRR ((USART2_CLK_HZ + (UART_BAUD / 2u)) / UART_BAUD)
#define UART_AF 7u

#define RING_SIZE 64u            /* power of two: index & MASK wraps correctly */
#define RING_MASK (RING_SIZE - 1u)
#define LINE_MAX 64u

/* Keeps the compiler from moving memory accesses across the index update. */
#define COMPILER_BARRIER() __asm volatile ("" ::: "memory")

typedef struct {
    uint8_t buf[RING_SIZE];
    volatile uint32_t head;      /* written only by the producer */
    volatile uint32_t tail;      /* written only by the consumer */
    volatile uint32_t drops;     /* producer-side count of rejected bytes */
} ring_t;

static ring_t g_tx;              /* producer: main,       consumer: USART2 ISR */
static ring_t g_rx;              /* producer: USART2 ISR, consumer: main       */
static volatile uint32_t g_ms_ticks;
static volatile uint32_t g_overruns;

static int ring_empty(const ring_t *r)
{
    return r->head == r->tail;
}

static int ring_full(const ring_t *r)
{
    return (uint32_t)(r->head - r->tail) >= RING_SIZE;
}

static int ring_push(ring_t *r, uint8_t byte)
{
    if (ring_full(r)) {
        r->drops++;
        return 0;
    }
    r->buf[r->head & RING_MASK] = byte;   /* 1. write the slot */
    COMPILER_BARRIER();
    r->head = r->head + 1u;               /* 2. publish it */
    return 1;
}

static uint8_t ring_pop(ring_t *r)        /* caller checks !ring_empty first */
{
    uint8_t byte = r->buf[r->tail & RING_MASK];
    COMPILER_BARRIER();
    r->tail = r->tail + 1u;               /* release the slot */
    return byte;
}

/* ---- Output -------------------------------------------------------------- */

/* Output helpers take the character sink as a parameter, so the same code
   path prints through the interrupt-driven queue normally and through the
   polled writer inside HardFault. */
typedef void (*putc_fn)(char);

static void uart_putc(char c)
{
    while (ring_full(&g_tx)) {
        /* the TX ISR drains one byte per TXE event */
    }
    ring_push(&g_tx, (uint8_t)c);
    USART2->CR1 |= USART_CR1_TXEIE;       /* make sure the ISR is armed */
}

static void fault_putc(char c)
{
    while ((USART2->SR & USART_SR_TXE) == 0u) {
    }
    USART2->DR = (uint8_t)c;
}

static void fault_flush(void)
{
    while ((USART2->SR & USART_SR_TC) == 0u) {
        /* wait for the last frame to leave the pin before resetting */
    }
}

static void print_str(putc_fn put, const char *s)
{
    while (*s != '\0') {
        put(*s++);
    }
}

static void print_hex32(putc_fn put, uint32_t v)
{
    static const char digits[] = "0123456789ABCDEF";

    put('0');
    put('x');
    for (int shift = 28; shift >= 0; shift -= 4) {
        put(digits[(v >> shift) & 0xFu]);
    }
}

static void print_dec32(putc_fn put, uint32_t v)
{
    char buf[10];
    uint32_t n = 0u;

    do {
        buf[n++] = (char)('0' + (v % 10u));
        v /= 10u;
    } while (v != 0u);

    while (n != 0u) {
        put(buf[--n]);
    }
}

static void uart_puts(const char *s)
{
    print_str(uart_putc, s);
}

/* ---- Input parsing ------------------------------------------------------- */

static int hexval(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* Accepts 0x-prefixed hex or plain decimal. Rejects empty input, bad digits,
   and values that do not fit in 32 bits. */
static int parse_u32(const char *s, uint32_t *out)
{
    uint32_t base = 10u;
    uint32_t v = 0u;

    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        base = 16u;
        s += 2;
    }
    if (*s == '\0') {
        return 0;
    }

    for (; *s != '\0'; s++) {
        int d = hexval(*s);
        if (d < 0 || (uint32_t)d >= base) {
            return 0;
        }
        if (v > (0xFFFFFFFFu - (uint32_t)d) / base) {
            return 0;                       /* would overflow */
        }
        v = v * base + (uint32_t)d;
    }
    *out = v;
    return 1;
}

static int streq(const char *a, const char *b)
{
    while (*a != '\0' && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}

/* Splits s in place into up to max words; returns the word count. */
static int tokenize(char *s, char *argv[], int max)
{
    int n = 0;

    while (*s != '\0' && n < max) {
        while (*s == ' ') {
            s++;
        }
        if (*s == '\0') {
            break;
        }
        argv[n++] = s;
        while (*s != '\0' && *s != ' ') {
            s++;
        }
        if (*s != '\0') {
            *s++ = '\0';
        }
    }
    return n;
}

/* ---- Hardware ------------------------------------------------------------ */

static void led_init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    GPIOA->MODER &= ~GPIO_MODER_MODER5;
    GPIOA->MODER |= GPIO_MODER_MODER5_0;
}

static void led_set(int on)
{
    GPIOA->BSRR = on ? GPIO_BSRR_BS5 : GPIO_BSRR_BR5;
}

static void systick_init(void)
{
    SysTick->LOAD = (CPU_CLK_HZ / 1000u) - 1u;
    SysTick->VAL = 0u;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk |
                    SysTick_CTRL_TICKINT_Msk   |
                    SysTick_CTRL_ENABLE_Msk;
}

static void uart_init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;

    GPIOA->MODER &= ~(GPIO_MODER_MODER2 | GPIO_MODER_MODER3);
    GPIOA->MODER |= GPIO_MODER_MODER2_1 | GPIO_MODER_MODER3_1;

    GPIOA->AFR[0] &= ~(GPIO_AFRL_AFSEL2 | GPIO_AFRL_AFSEL3);
    GPIOA->AFR[0] |= (UART_AF << GPIO_AFRL_AFSEL2_Pos) |
                     (UART_AF << GPIO_AFRL_AFSEL3_Pos);
                     
    GPIOA->OTYPER &= ~(GPIO_OTYPER_OT2 | GPIO_OTYPER_OT3);
    GPIOA->PUPDR &= ~(GPIO_PUPDR_PUPDR2 | GPIO_PUPDR_PUPDR3);

    USART2->CR1 = 0u;
    USART2->CR2 = 0u;
    USART2->CR3 = 0u;
    USART2->BRR = UART_BRR;

    /* RXNEIE: interrupt per received byte. TXEIE is enabled later, only while
       there is something to send. */
    USART2->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE | USART_CR1_UE;

    NVIC_SetPriority(USART2_IRQn, 1u);
    NVIC_EnableIRQ(USART2_IRQn);
}

/* ---- Interrupts ---------------------------------------------------------- */

void SysTick_Handler(void)
{
    g_ms_ticks++;
}

void USART2_IRQHandler(void)
{
    uint32_t sr = USART2->SR;

    if ((sr & (USART_SR_RXNE | USART_SR_ORE)) != 0u) {
        /* Reading SR (above) then DR clears RXNE and ORE. */
        uint8_t byte = (uint8_t)USART2->DR;

        if ((sr & USART_SR_ORE) != 0u) {
            g_overruns++;
        }
        if ((sr & USART_SR_RXNE) != 0u) {
            ring_push(&g_rx, byte);
        }
    }

    /* TX: when the data register is empty and TX is armed, send the next byte,
       or disarm the interrupt if nothing is queued. */
    if (((USART2->CR1 & USART_CR1_TXEIE) != 0u) &&
        ((USART2->SR & USART_SR_TXE) != 0u)) {
        if (!ring_empty(&g_tx)) {
            USART2->DR = ring_pop(&g_tx);
        } else {
            USART2->CR1 &= ~USART_CR1_TXEIE;
        }
    }
}

/* ---- HardFault ----------------------------------------------------------- */

/* Finds the stack frame that was active when the fault happened. Bit 2 of
   EXC_RETURN says whether that was MSP or PSP. The C routine receives the
   frame pointer and EXC_RETURN. */
__attribute__((naked)) void HardFault_Handler(void)
{
    __asm volatile (
        "tst lr, #4        \n"
        "ite eq            \n"
        "mrseq r0, msp     \n"
        "mrsne r0, psp     \n"
        "mov r1, lr        \n"
        "b hardfault_report\n"
    );
}

typedef struct {
    uint32_t bit;
    const char *name;
} fault_bit_t;

static const fault_bit_t g_cfsr_bits[] = {
    { 1u << 0,  "IACCVIOL"   }, { 1u << 1,  "DACCVIOL"   },
    { 1u << 4,  "MSTKERR"    }, { 1u << 8,  "IBUSERR"    },
    { 1u << 9,  "PRECISERR"  }, { 1u << 10, "IMPRECISERR"},
    { 1u << 12, "STKERR"     }, { 1u << 15, "BFARVALID"  },
    { 1u << 16, "UNDEFINSTR" }, { 1u << 17, "INVSTATE"   },
    { 1u << 18, "INVPC"      }, { 1u << 24, "UNALIGNED"  },
    { 1u << 25, "DIVBYZERO"  },
};

static void print_flags(const fault_bit_t *table, uint32_t count, uint32_t value)
{
    for (uint32_t i = 0u; i < count; i++) {
        if ((value & table[i].bit) != 0u) {
            print_str(fault_putc, table[i].name);
            print_str(fault_putc, " ");
        }
    }
}

void hardfault_report(uint32_t *frame, uint32_t exc_return) __attribute__((noreturn));
void hardfault_report(uint32_t *frame, uint32_t exc_return)
{
    uint32_t cfsr = SCB->CFSR;
    uint32_t hfsr = SCB->HFSR;
    uint32_t bfar = SCB->BFAR;
    uint32_t count = sizeof g_cfsr_bits / sizeof g_cfsr_bits[0];

    print_str(fault_putc, "\r\n*** HARDFAULT ***\r\n");

    print_str(fault_putc, "CFSR  = "); print_hex32(fault_putc, cfsr);
    print_str(fault_putc, "  "); print_flags(g_cfsr_bits, count, cfsr); print_str(fault_putc, "\r\n");

    print_str(fault_putc, "HFSR  = "); print_hex32(fault_putc, hfsr);
    if ((hfsr & (1u << 30)) != 0u) print_str(fault_putc, "  FORCED");
    if ((hfsr & (1u << 1)) != 0u)  print_str(fault_putc, "  VECTTBL");
    print_str(fault_putc, "\r\n");

    print_str(fault_putc, "BFAR  = ");
    if ((cfsr & (1u << 15)) != 0u) {
        print_hex32(fault_putc, bfar);
    } else {
        print_str(fault_putc, "(not valid)");
    }
    print_str(fault_putc, "\r\n");

    /* Stacked frame: R0 R1 R2 R3 R12 LR PC xPSR. */
    print_str(fault_putc, "PC    = "); print_hex32(fault_putc, frame[6]);
    print_str(fault_putc, "  (addr2line -e build/uart_cli/uart_cli.elf -f ");
    print_hex32(fault_putc, frame[6]);
    print_str(fault_putc, ")\r\n");

    print_str(fault_putc, "LR    = "); print_hex32(fault_putc, frame[5]);
    print_str(fault_putc, "  xPSR = "); print_hex32(fault_putc, frame[7]);
    print_str(fault_putc, "  EXC_RETURN = "); print_hex32(fault_putc, exc_return);
    print_str(fault_putc, "\r\n");

    print_str(fault_putc, "resetting\r\n");
    fault_flush();
    NVIC_SystemReset();
}

/* ---- Command line -------------------------------------------------------- */

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
        print_dec32(uart_putc, g_ms_ticks);
        uart_puts(" ms\r\n");
    } else if (streq(argv[0], "peek") && argc == 2) {
        cmd_peek(argv[1]);
    } else if (streq(argv[0], "poke") && argc == 3) {
        cmd_poke(argv[1], argv[2]);
    } else {
        uart_puts("unknown command, try 'help'\r\n");
    }
}

/* Handles one received character: echo, edit, or run the line on Enter. */
static void cli_feed(char c)
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

/* ---- Main ---------------------------------------------------------------- */

int main(void)
{
    led_init();
    uart_init();
    systick_init();

    uart_puts("\r\nSTM32F446 CLI ready. Type 'help'.\r\n");
    print_prompt();

    for (;;) {
        /* Same check-and-sleep pattern as uart_echo_irq: mask interrupts while
           deciding to sleep, so a byte arriving in between is not missed. */
        __disable_irq();
        if (ring_empty(&g_rx)) {
            __WFI();
            __enable_irq();
            continue;
        }
        __enable_irq();

        cli_feed((char)ring_pop(&g_rx));
    }
}
