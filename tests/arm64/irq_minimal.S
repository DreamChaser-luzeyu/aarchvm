    .text
    .global _start
_start:
    // UART base
    movz x1, #0x0000
    movk x1, #0x0900, lsl #16

    // TIMER base
    movz x2, #0x0000
    movk x2, #0x0a00, lsl #16

    // VBAR_EL1 = vectors
    adr  x10, vectors
    msr  vbar_el1, x10

    // flag = 0
    adr  x3, flag
    movz x4, #0
    str  x4, [x3]

    // compare = cntvct + 40
    mrs  x5, cntvct_el0
    add  x5, x5, #40
    str  x5, [x2, #8]

    // enable timer
    movz x6, #1
    str  x6, [x2, #16]

wait_irq:
    ldr  x7, [x3]
    cbz  x7, wait_irq

    movz x0, #'M'
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
    movz x0, #'I'
    str  x0, [x1]

    adr  x3, flag
    movz x4, #1
    str  x4, [x3]

    // disable timer
    movz x6, #0
    str  x6, [x2, #16]

    eret

    .data
    .balign 8
flag:
    .quad 0
