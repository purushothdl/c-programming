#include "stm32f4xx.h"
#include "fmt.h"
#include "uart.h"

/*
 * HardFault_Handler prints CFSR/HFSR/BFAR and the stacked PC over UART using
 * polled output, because interrupts cannot be trusted inside a fault. Then the
 * board resets so the CLI comes back up.
 */

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
