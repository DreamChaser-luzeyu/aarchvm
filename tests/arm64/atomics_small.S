    .text
    .global _start
_start:
    movz x1, #0
    movk x1, #0x0900, lsl #16      // UART
    adr  x20, byte_slot
    adr  x21, half_slot
    movz x28, #'a'

    // Byte acquire/release.
    movz w2, #0x5a
    stlrb w2, [x20]
    ldarb w3, [x20]
    subs wzr, w3, w2
    b.ne fail

    // Byte exclusive success/fail paths.
    movz x28, #'b'
    ldxrb w4, [x20]
    subs wzr, w4, w2
    b.ne fail
    movz w5, #0x6b
    stxrb w6, w5, [x20]
    cbnz w6, fail
    ldaxrb w7, [x20]
    subs wzr, w7, w5
    b.ne fail
    movz w8, #0x7c
    stlxrb w9, w8, [x20]
    cbnz w9, fail
    ldarb w10, [x20]
    subs wzr, w10, w8
    b.ne fail
    stxrb w11, w2, [x20]
    cbz  w11, fail

    // Halfword acquire/release.
    movz x28, #'c'
    movz w12, #0x1234
    stlrh w12, [x21]
    ldarh w13, [x21]
    subs wzr, w13, w12
    b.ne fail

    // Halfword exclusive success/fail paths.
    movz x28, #'d'
    ldxrh w14, [x21]
    subs wzr, w14, w12
    b.ne fail
    movz w15, #0x5678
    stxrh w16, w15, [x21]
    cbnz w16, fail
    ldaxrh w17, [x21]
    subs wzr, w17, w15
    b.ne fail
    movz w18, #0x9abc
    stlxrh w19, w18, [x21]
    cbnz w19, fail
    ldarh w20, [x21]
    subs wzr, w20, w18
    b.ne fail
    stxrh w23, w12, [x21]
    cbz  w23, fail

    movz x0, #'O'
    str  x0, [x1]
    movz x0, #'\n'
    str  x0, [x1]
    brk  #0

fail:
    orr  x0, x28, xzr
    str  x0, [x1]
    brk  #0

    .data
    .balign 8
byte_slot:
    .byte 0
    .byte 0
half_slot:
    .hword 0
