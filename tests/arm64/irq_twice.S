    .text
    .global _start
_start:
    movz x1, #0x0000
    movk x1, #0x0900, lsl #16      // UART

    movz x2, #0x0000
    movk x2, #0x0a00, lsl #16      // TIMER

    adr  x10, vectors
    msr  vbar_el1, x10

    // Route timer MMIO IRQs through GIC PPI 27.
    movz x15, #0x0000
    movk x15, #0x080b, lsl #16     // GICR_SGI base
    movz x16, #0
    movk x16, #0x0800, lsl #16
    str  w16, [x15, #0x100]
    movz x16, #0xff
    msr  icc_pmr_el1, x16
    movz x16, #1
    msr  icc_sre_el1, x16
    msr  icc_igrpen1_el1, x16

    adr  x11, irq_count
    movz x12, #0
    str  x12, [x11]

    mrs  x5, cntvct_el0
    add  x5, x5, #30
    str  x5, [x2, #8]
    movz x6, #1
    str  x6, [x2, #16]

wait_done:
    ldr  x13, [x11]
    subs x14, x13, #2
    b.ne wait_done

    movz x0, #'T'
    str  x0, [x1]
    movz x0, #'\n'
    str  x0, [x1]
    brk  #0

    .balign 2048
vectors:
    .space 0x280
irq_vector:
    b irq_handler

irq_handler:
    ldr  x13, [x11]
    add  x13, x13, #1
    str  x13, [x11]

    // if count != 2, re-arm timer
    subs x14, x13, #2
    b.eq done

    mrs  x5, cntvct_el0
    add  x5, x5, #30
    str  x5, [x2, #8]
    movz x6, #1
    str  x6, [x2, #16]
    eret

done:
    movz x6, #0
    str  x6, [x2, #16]
    eret

    .data
    .balign 8
irq_count:
    .quad 0
