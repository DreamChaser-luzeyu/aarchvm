    .text
    .global _start
_start:
    movz x1, #0
    movk x1, #0x0900, lsl #16      // UART

    adr  x20, word_slot
    adr  x21, quad_slot
    movz x28, #'a'

    // PRFM variants are modelled as hints only.
    prfm pldl1keep, prfm_target
    prfm pstl1strm, [x20]
    movz x0, #0
    prfm pstl1keep, [x20, x0]

    // Release store + acquire load, 32-bit.
    movz w2, #0x1234
    stlr w2, [x20]
    ldar w3, [x20]
    subs wzr, w3, w2
    b.ne fail

    // STXR without a live monitor must fail and leave memory unchanged.
    movz x28, #'b'
    movz w4, #0x7777
    stxr w5, w4, [x20]
    cbz  w5, fail
    ldar w6, [x20]
    subs wzr, w6, w2
    b.ne fail

    // LDXR/STXR success path.
    movz x28, #'c'
    ldxr w6, [x20]
    subs wzr, w6, w2
    b.ne fail
    movz w7, #0x5678
    stxr w8, w7, [x20]
    cbnz w8, fail
    ldar w9, [x20]
    subs wzr, w9, w7
    b.ne fail

    // CLREX clears the local monitor.
    movz x28, #'d'
    ldxr w10, [x20]
    clrex
    movz w11, #0x9999
    stxr w12, w11, [x20]
    cbz  w12, fail
    ldar w13, [x20]
    subs wzr, w13, w7
    b.ne fail

    // LDAXR/STLXR success path.
    movz x28, #'e'
    ldaxr w14, [x20]
    subs wzr, w14, w7
    b.ne fail
    movz w15, #0xabcd
    stlxr w16, w15, [x20]
    cbnz w16, fail
    ldar w17, [x20]
    subs wzr, w17, w15
    b.ne fail

    // Any successful ordinary write clears the single-core monitor approximation.
    movz x28, #'f'
    ldxr w18, [x20]
    movz w19, #0x1357
    str  w19, [x20]
    movz w22, #0x2468
    stxr w23, w22, [x20]
    cbz  w23, fail
    ldar w24, [x20]
    subs wzr, w24, w19
    b.ne fail

    // 64-bit release/acquire + exclusive path.
    movz x28, #'g'
    movz x2, #0x1111
    movk x2, #0x2222, lsl #16
    movk x2, #0x3333, lsl #32
    movk x2, #0x4444, lsl #48
    stlr x2, [x21]
    ldar x3, [x21]
    subs xzr, x3, x2
    b.ne fail

    ldxr x4, [x21]
    subs xzr, x4, x2
    b.ne fail
    movz x5, #0xaaaa
    movk x5, #0xbbbb, lsl #16
    movk x5, #0xcccc, lsl #32
    movk x5, #0xdddd, lsl #48
    stxr w6, x5, [x21]
    cbnz w6, fail
    ldaxr x7, [x21]
    subs xzr, x7, x5
    b.ne fail
    movz x8, #0x0102
    movk x8, #0x0304, lsl #16
    movk x8, #0x0506, lsl #32
    movk x8, #0x0708, lsl #48
    stlxr w9, x8, [x21]
    cbnz w9, fail
    ldar x10, [x21]
    subs xzr, x10, x8
    b.ne fail

    movz x0, #'A'
    str  x0, [x1]
    movz x0, #'\n'
    str  x0, [x1]
    brk  #0

fail:
    orr  x0, x28, xzr
    str  x0, [x1]
    brk  #0

prfm_target:
    nop

    .data
    .balign 8
word_slot:
    .word 0
    .word 0
quad_slot:
    .quad 0
