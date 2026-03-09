    .text
    .global _start
_start:
    movz x1, #0x0000
    movk x1, #0x0900, lsl #16      // UART

    movz x2, #0x0000
    movk x2, #0x0a00, lsl #16      // TIMER

    adr  x10, vectors
    msr  vbar_el1, x10

    // Program compare, but keep timer disabled.
    mrs  x5, cntvct_el0
    add  x5, x5, #20
    str  x5, [x2, #8]
    movz x6, #0
    str  x6, [x2, #16]

    // Delay loop
    movz x4, #200
wait_loop:
    subs x4, x4, #1
    b.ne wait_loop

    movz x0, #'D'
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
    movz x0, #'F'
    str  x0, [x1]
    brk  #0
