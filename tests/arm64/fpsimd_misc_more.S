    .text
    .global _start
_start:
    movz x20, #0
    movk x20, #0x0900, lsl #16
    adr  x21, dst

    // DUP (general), byte elements
    mov  w1, #0x5a
    dup  v0.16b, w1
    str  q0, [x21]
    adr  x22, exp_dup_b
    ldr  x0, [x21]
    ldr  x1, [x22]
    cmp  x0, x1
    b.ne fail
    ldr  x0, [x21, #8]
    ldr  x1, [x22, #8]
    cmp  x0, x1
    b.ne fail

    // DUP (general), doubleword elements
    ldr  x2, =0x1122334455667788
    dup  v2.2d, x2
    str  q2, [x21, #16]
    adr  x22, exp_dup_d
    add  x23, x21, #16
    ldr  x0, [x23]
    ldr  x1, [x22]
    cmp  x0, x1
    b.ne fail
    ldr  x0, [x23, #8]
    ldr  x1, [x22, #8]
    cmp  x0, x1
    b.ne fail

    // ADDP (vector, byte elements)
    adr  x22, addp_src
    ld1  {v5.16b-v6.16b}, [x22]
    addp v4.16b, v5.16b, v6.16b
    str  q4, [x21, #32]
    adr  x22, exp_addp
    add  x23, x21, #32
    ldr  x0, [x23]
    ldr  x1, [x22]
    cmp  x0, x1
    b.ne fail
    ldr  x0, [x23, #8]
    ldr  x1, [x22, #8]
    cmp  x0, x1
    b.ne fail

    // SHRN
    adr  x22, shrn_src
    ldr  q8, [x22]
    shrn v7.8b, v8.8h, #8
    str  d7, [x21, #48]
    adr  x22, exp_shrn
    add  x23, x21, #48
    ldr  x0, [x23]
    ldr  x1, [x22]
    cmp  x0, x1
    b.ne fail

    movz x0, #'G'
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
exp_dup_b:
    .quad 0x5a5a5a5a5a5a5a5a
    .quad 0x5a5a5a5a5a5a5a5a
exp_dup_d:
    .quad 0x1122334455667788
    .quad 0x1122334455667788
addp_src:
    .byte 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16
    .byte 17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32
exp_addp:
    .byte 3,7,11,15,19,23,27,31,35,39,43,47,51,55,59,63
shrn_src:
    .hword 0x1234,0x5678,0x9abc,0xdef0,0x00ff,0xff00,0x0f0f,0xf0f0
exp_shrn:
    .byte 0x12,0x56,0x9a,0xde,0x00,0xff,0x0f,0xf0
