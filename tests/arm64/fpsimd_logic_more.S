    .text
    .global _start
_start:
    movz x20, #0x0000
    movk x20, #0x0900, lsl #16

    adr  x21, dst

    movi v2.16b, #0xaa
    movi v3.16b, #0x55
    movi v4.16b, #0xf0
    bit  v2.16b, v3.16b, v4.16b
    str  q2, [x21]
    adr  x22, exp_bit
    ldr  x0, [x21]
    ldr  x1, [x22]
    cmp  x0, x1
    b.ne fail
    ldr  x0, [x21, #8]
    ldr  x1, [x22, #8]
    cmp  x0, x1
    b.ne fail

    adr  x22, cmhs_lhs
    ld1  {v5.16b}, [x22]
    adr  x22, cmhs_rhs
    ld1  {v6.16b}, [x22]
    cmhs v7.16b, v5.16b, v6.16b
    str  q7, [x21, #16]
    adr  x22, exp_cmhs
    add  x23, x21, #16
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
    str  q9, [x21, #32]
    adr  x22, exp_umaxp
    add  x23, x21, #32
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
    movz x0, #'
'
    str  x0, [x20]
    brk  #0

fail:
    movz x0, #'F'
    str  x0, [x20]
    brk  #0

    .data
    .balign 16
dst:
    .space 48
exp_bit:
    .quad 0x5a5a5a5a5a5a5a5a
    .quad 0x5a5a5a5a5a5a5a5a
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
