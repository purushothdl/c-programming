.syntax unified
.cpu cortex-m4
.thumb

.extern main
.extern _sidata
.extern _sdata
.extern _edata
.extern _sbss
.extern _ebss

.section .isr_vector, "a", %progbits
.align 2
.word _estack                /* 0x00  initial stack pointer  */
.word Reset_Handler          /* 0x04  reset                  */
.word Default_Handler        /* 0x08  NMI                    */
.word Default_Handler        /* 0x0C  HardFault              */
.word Default_Handler        /* 0x10  MemManage              */
.word Default_Handler        /* 0x14  BusFault               */
.word Default_Handler        /* 0x18  UsageFault             */
.word Default_Handler        /* 0x1C  reserved               */
.word Default_Handler        /* 0x20  reserved               */
.word Default_Handler        /* 0x24  reserved               */
.word Default_Handler        /* 0x28  reserved               */
.word Default_Handler        /* 0x2C  SVCall                 */
.word Default_Handler        /* 0x30  DebugMonitor           */
.word Default_Handler        /* 0x34  reserved               */
.word Default_Handler        /* 0x38  PendSV                 */
.word SysTick_Handler        /* 0x3C  SysTick (exception 15) */

/* EXTI lines 10..15 share one vector: IRQ 40 -> exception 56. */
.rept 40                     /* 0x40..0xDC  IRQ 0..39 (unused, default) */
.word Default_Handler
.endr
.word EXTI15_10_IRQHandler   /* 0xE0  EXTI15_10 (IRQ 40) */

.section .text.Reset_Handler, "ax", %progbits
.thumb_func
.global Reset_Handler
.type Reset_Handler, %function
Reset_Handler:
    /* Copy initialized data from Flash to SRAM. */
    ldr r0, =_sidata
    ldr r1, =_sdata
    ldr r2, =_edata
1:
    cmp r1, r2
    bcs 2f
    ldr r3, [r0], #4
    str r3, [r1], #4
    b 1b

    /* Clear the zero-initialized data section. */
2:
    ldr r1, =_sbss
    ldr r2, =_ebss
    movs r3, #0
3:
    cmp r1, r2
    bcs 4f
    str r3, [r1], #4
    b 3b

4:
    bl main
5:
    b 5b
.size Reset_Handler, . - Reset_Handler

.section .text.Default_Handler, "ax", %progbits
.thumb_func
.global Default_Handler
.type Default_Handler, %function
Default_Handler:
    b Default_Handler
.size Default_Handler, . - Default_Handler

/* A program may define a real SysTick_Handler; if it does not, this weak
   default is used so every program links. */
.weak SysTick_Handler
.thumb_set SysTick_Handler, Default_Handler

/* Same weak-default trick for EXTI15_10 (IRQ 40). */
.weak EXTI15_10_IRQHandler
.thumb_set EXTI15_10_IRQHandler, Default_Handler
