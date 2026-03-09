    .text
    .global _start
_start:
    movz x1, #0x0000
    movk x1, #0x0900, lsl #16      // UART

    movn w2, #0
    tbz  x2, #0, fail

    movz x3, #5
    movz x4, #7
    subs xzr, x3, #5
    csel x5, x3, x4, eq
    subs xzr, x5, #5
    b.ne fail

    movz x7, #0x00f0
    movz x8, #0x000f
    bic  x9, x7, x8
    subs xzr, x9, #0x00f0
    b.ne fail

    ands x11, x7, x8
    b.ne fail

    tbnz x2, #31, pass

fail:
    movz x0, #'F'
    str  x0, [x1]
    movz x0, #'\n'
    str  x0, [x1]
    brk  #0

pass:
    movz x0, #'L'
    str  x0, [x1]
    movz x0, #'\n'
    str  x0, [x1]
    brk  #0
