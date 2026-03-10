    .text
    .global _start
_start:
    movz x1, #0
    movk x1, #0x0900, lsl #16      // UART

    // UMULH: (2^63 * 4) >> 64 = 2
    movz x2, #1
    lsl  x2, x2, #63
    movz x3, #4
    umulh x4, x2, x3
    subs xzr, x4, #2
    b.ne fail1

    // SMULH: ((-2^63) * 2) >> 64 = -1
    movz x5, #1
    lsl  x5, x5, #63
    movz x6, #2
    smulh x7, x5, x6
    movn x8, #0
    subs xzr, x7, x8
    b.ne fail2

    movz x0, #'J'
    str  x0, [x1]
    movz x0, #'\n'
    str  x0, [x1]
    brk  #0

fail1:
    movz x0, #'1'
    b out
fail2:
    movz x0, #'2'
out:
    str  x0, [x1]
    brk  #0
