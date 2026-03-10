    .text
    .global _start
_start:
    movz x1, #0
    movk x1, #0x0900, lsl #16      // UART
    adr  x20, pair_slot

    movz x2, #0x1111
    movz x3, #0x2222
    stp  x2, x3, [x20]

    movz x28, #'a'
    ldxp x4, x5, [x20]
    subs xzr, x4, x2
    b.ne fail
    subs xzr, x5, x3
    b.ne fail
    movz x6, #0x3333
    movz x7, #0x4444
    stxp w8, x6, x7, [x20]
    cbnz w8, fail
    ldxp x9, x10, [x20]
    subs xzr, x9, x6
    b.ne fail
    subs xzr, x10, x7
    b.ne fail

    movz x28, #'b'
    ldaxp x11, x12, [x20]
    subs xzr, x11, x6
    b.ne fail
    subs xzr, x12, x7
    b.ne fail
    movz x13, #0x5555
    movz x14, #0x6666
    stlxp w15, x13, x14, [x20]
    cbnz w15, fail
    ldxp x16, x17, [x20]
    subs xzr, x16, x13
    b.ne fail
    subs xzr, x17, x14
    b.ne fail

    movz x28, #'c'
    clrex
    stxp w18, x2, x3, [x20]
    cbz  w18, fail

    movz x0, #'X'
    str  x0, [x1]
    movz x0, #'\n'
    str  x0, [x1]
    brk  #0

fail:
    orr  x0, x28, xzr
    str  x0, [x1]
    brk  #0

    .data
    .balign 16
pair_slot:
    .quad 0
    .quad 0
