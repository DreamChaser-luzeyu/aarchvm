    .text
    .global _start
_start:
    movz x1, #0x0000
    movk x1, #0x0900, lsl #16      // UART

    movz x2, #0x0000
    movk x2, #0x0a00, lsl #16      // TIMER

    adr  x10, vectors
    msr  vbar_el1, x10

    adr  x3, phase
    movz x4, #0
    str  x4, [x3]

    // Test SP0 vector path: IRQ should enter VBAR+0x80.
    msr  spsel, #0
    mrs  x5, cntvct_el0
    add  x5, x5, #30
    str  x5, [x2, #8]
    movz x6, #1
    str  x6, [x2, #16]

    movz x7, #140
wait0:
    subs x7, x7, #1
    b.ne wait0

    ldr  x8, [x3]
    subs x9, x8, #1
    b.ne fail

    // Test SPx vector path: IRQ should enter VBAR+0x280.
    msr  spsel, #1
    mrs  x5, cntvct_el0
    add  x5, x5, #30
    str  x5, [x2, #8]
    movz x6, #1
    str  x6, [x2, #16]

    movz x7, #140
wait1:
    subs x7, x7, #1
    b.ne wait1

    ldr  x8, [x3]
    subs x9, x8, #2
    b.ne fail

    movz x0, #'P'
    str  x0, [x1]
    movz x0, #'\n'
    str  x0, [x1]
    brk  #0

fail:
    movz x0, #'F'
    str  x0, [x1]
    brk  #0

    .balign 2048
vectors:
    .space 0x80
irq_sp0_vector:
    b irq_sp0
    .space 0x1FC
irq_spx_vector:
    b irq_spx

irq_sp0:
    adr  x11, phase
    movz x12, #1
    str  x12, [x11]
    movz x6, #0
    str  x6, [x2, #16]
    eret

irq_spx:
    adr  x11, phase
    movz x12, #2
    str  x12, [x11]
    movz x6, #0
    str  x6, [x2, #16]
    eret

    .data
    .balign 8
phase:
    .quad 0
