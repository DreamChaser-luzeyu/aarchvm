    .text
    .global _start
_start:
    movz x20, #0x0000
    movk x20, #0x0900, lsl #16
    movz x25, #0x0030, lsl #16
    msr  cpacr_el1, x25
    isb

    adr  x21, dst

    movi v0.16b, #0xaa
    movi v1.16b, #0x55
    and  v2.16b, v0.16b, v1.16b
    str  q2, [x21]
    adr  x22, exp_and
    ldr  x0, [x21]
    ldr  x1, [x22]
    cmp  x0, x1
    b.ne fail
    ldr  x0, [x21, #8]
    ldr  x1, [x22, #8]
    cmp  x0, x1
    b.ne fail

    eor  v2.16b, v0.16b, v1.16b
    str  q2, [x21, #16]
    adr  x22, exp_eor
    add  x23, x21, #16
    ldr  x0, [x23]
    ldr  x1, [x22]
    cmp  x0, x1
    b.ne fail
    ldr  x0, [x23, #8]
    ldr  x1, [x22, #8]
    cmp  x0, x1
    b.ne fail

    movi v2.16b, #0xaa
    movi v3.16b, #0x55
    movi v4.16b, #0xf0
    bit  v2.16b, v3.16b, v4.16b
    str  q2, [x21, #32]
    adr  x22, exp_bit
    add  x23, x21, #32
    ldr  x0, [x23]
    ldr  x1, [x22]
    cmp  x0, x1
    b.ne fail
    ldr  x0, [x23, #8]
    ldr  x1, [x22, #8]
    cmp  x0, x1
    b.ne fail

    movi v2.16b, #0xaa
    movi v3.16b, #0x55
    movi v4.16b, #0xf0
    bif  v2.16b, v3.16b, v4.16b
    str  q2, [x21, #48]
    adr  x22, exp_bif
    add  x23, x21, #48
    ldr  x0, [x23]
    ldr  x1, [x22]
    cmp  x0, x1
    b.ne fail
    ldr  x0, [x23, #8]
    ldr  x1, [x22, #8]
    cmp  x0, x1
    b.ne fail

    movi v2.16b, #0xf0
    movi v3.16b, #0xaa
    movi v4.16b, #0x55
    bsl  v2.16b, v3.16b, v4.16b
    str  q2, [x21, #64]
    adr  x22, exp_bsl
    add  x23, x21, #64
    ldr  x0, [x23]
    ldr  x1, [x22]
    cmp  x0, x1
    b.ne fail
    ldr  x0, [x23, #8]
    ldr  x1, [x22, #8]
    cmp  x0, x1
    b.ne fail

    adr  x22, cmhs_lhs
    ld1  {v5.16b}, [x22]
    adr  x22, cmhs_rhs
    ld1  {v6.16b}, [x22]
    cmhs v7.16b, v5.16b, v6.16b
    str  q7, [x21, #80]
    adr  x22, exp_cmhs
    add  x23, x21, #80
    ldr  x0, [x23]
    ldr  x1, [x22]
    cmp  x0, x1
    b.ne fail
    ldr  x0, [x23, #8]
    ldr  x1, [x22, #8]
    cmp  x0, x1
    b.ne fail

    adr  x22, umaxp_src
    ld1  {v8.16b}, [x22]
    umaxp v9.16b, v8.16b, v8.16b
    str  q9, [x21, #96]
    adr  x22, exp_umaxp
    add  x23, x21, #96
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
    .space 112
exp_and:
    .quad 0x0000000000000000
    .quad 0x0000000000000000
exp_eor:
    .quad 0xffffffffffffffff
    .quad 0xffffffffffffffff
exp_bit:
    .quad 0x5a5a5a5a5a5a5a5a
    .quad 0x5a5a5a5a5a5a5a5a
exp_bif:
    .quad 0xa5a5a5a5a5a5a5a5
    .quad 0xa5a5a5a5a5a5a5a5
exp_bsl:
    .quad 0xa5a5a5a5a5a5a5a5
    .quad 0xa5a5a5a5a5a5a5a5
cmhs_lhs:
    .byte 1,3,2,4,5,7,6,8,9,11,10,12,13,15,14,16
cmhs_rhs:
    .byte 0,4,2,5,4,7,7,7,10,10,11,11,13,16,13,17
exp_cmhs:
    .byte 0xff,0x00,0xff,0x00,0xff,0xff,0x00,0xff,0x00,0xff,0x00,0xff,0xff,0x00,0xff,0x00
umaxp_src:
    .byte 1,7,2,8,3,9,4,10,5,11,6,12,13,14,15,16
exp_umaxp:
    .byte 7,8,9,10,11,12,14,16,7,8,9,10,11,12,14,16
