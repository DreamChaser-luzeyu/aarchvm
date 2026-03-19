    .text
    .global _start
_start:
    movz x20, #0x0000
    movk x20, #0x0900, lsl #16
    movz x25, #0x0030, lsl #16
    msr  cpacr_el1, x25
    isb

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

    movi v5.4s, #0x34, lsl #8
    str  q5, [x21, #64]
    adr  x22, exp_movi32_8
    add  x23, x21, #64
    ldr  x0, [x23]
    ldr  x1, [x22]
    cmp  x0, x1
    b.ne fail
    ldr  x0, [x23, #8]
    ldr  x1, [x22, #8]
    cmp  x0, x1
    b.ne fail

    mvni v6.4s, #0x34, lsl #8
    str  q6, [x21, #80]
    adr  x22, exp_mvni32_8
    add  x23, x21, #80
    ldr  x0, [x23]
    ldr  x1, [x22]
    cmp  x0, x1
    b.ne fail
    ldr  x0, [x23, #8]
    ldr  x1, [x22, #8]
    cmp  x0, x1
    b.ne fail

    movi v7.16b, #0
    orr  v7.4s, #0x34, lsl #8
    str  q7, [x21, #96]
    adr  x22, exp_orr32_8
    add  x23, x21, #96
    ldr  x0, [x23]
    ldr  x1, [x22]
    cmp  x0, x1
    b.ne fail
    ldr  x0, [x23, #8]
    ldr  x1, [x22, #8]
    cmp  x0, x1
    b.ne fail

    movi v8.16b, #0
    orr  v8.8h, #0xf, lsl #8
    str  q8, [x21, #112]
    adr  x22, exp_orr16_8
    add  x23, x21, #112
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
    .space 128
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
exp_movi32_8:
    .quad 0x0000340000003400
    .quad 0x0000340000003400
exp_mvni32_8:
    .quad 0xffffcbffffffcbff
    .quad 0xffffcbffffffcbff
exp_orr32_8:
    .quad 0x0000340000003400
    .quad 0x0000340000003400
exp_orr16_8:
    .quad 0x0f000f000f000f00
    .quad 0x0f000f000f000f00
