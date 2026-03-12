    .text
    .global _start
_start:
    movz x20, #0x0000
    movk x20, #0x0900, lsl #16

    adr  x21, dst

    movi v4.16b, #0x33
    bic  v4.8h, #0xf0
    str  q4, [x21]
    adr  x22, exp_bic_lo
    ldr  x0, [x21]
    ldr  x1, [x22]
    cmp  x0, x1
    b.ne fail
    ldr  x0, [x21, #8]
    ldr  x1, [x22, #8]
    cmp  x0, x1
    b.ne fail

    movi v2.16b, #0x33
    bic  v2.8h, #0xf, lsl #8
    str  q2, [x21, #16]
    adr  x22, exp_bic_hi
    add  x23, x21, #16
    ldr  x0, [x23]
    ldr  x1, [x22]
    cmp  x0, x1
    b.ne fail
    ldr  x0, [x23, #8]
    ldr  x1, [x22, #8]
    cmp  x0, x1
    b.ne fail

    movi v0.16b, #0x5a
    bic  v0.4s, #0x12
    str  q0, [x21, #32]
    adr  x22, exp_bic32_0
    add  x23, x21, #32
    ldr  x0, [x23]
    ldr  x1, [x22]
    cmp  x0, x1
    b.ne fail
    ldr  x0, [x23, #8]
    ldr  x1, [x22, #8]
    cmp  x0, x1
    b.ne fail

    movi v1.16b, #0x5a
    bic  v1.4s, #0x34, lsl #8
    str  q1, [x21, #48]
    adr  x22, exp_bic32_8
    add  x23, x21, #48
    ldr  x0, [x23]
    ldr  x1, [x22]
    cmp  x0, x1
    b.ne fail
    ldr  x0, [x23, #8]
    ldr  x1, [x22, #8]
    cmp  x0, x1
    b.ne fail

    movz x0, #'W'
    str  x0, [x20]
    movz x0, #'\n'
    str  x0, [x20]
    brk  #0

fail:
    movz x0, #'F'
    str  x0, [x20]
    brk  #0

    .data
    .balign 16
dst:
    .space 64
exp_bic_lo:
    .quad 0x3303330333033303
    .quad 0x3303330333033303
exp_bic_hi:
    .quad 0x3033303330333033
    .quad 0x3033303330333033
exp_bic32_0:
    .quad 0x5a5a5a485a5a5a48
    .quad 0x5a5a5a485a5a5a48
exp_bic32_8:
    .quad 0x5a5a4a5a5a5a4a5a
    .quad 0x5a5a4a5a5a5a4a5a
