    .text
    .global _start
_start:
    movz x20, #0x0000
    movk x20, #0x0900, lsl #16

    adr  x21, src
    adr  x22, dst

    ldr  q1, [x21]
    dup  v0.16b, v1.b[2]
    str  q0, [x22]
    adr  x23, exp_b
    ldr  x0, [x22]
    ldr  x1, [x23]
    cmp  x0, x1
    b.ne fail
    ldr  x0, [x22, #8]
    ldr  x1, [x23, #8]
    cmp  x0, x1
    b.ne fail

    ldr  q3, [x21]
    dup  v2.8h, v3.h[1]
    str  q2, [x22, #16]
    adr  x23, exp_h
    add  x24, x22, #16
    ldr  x0, [x24]
    ldr  x1, [x23]
    cmp  x0, x1
    b.ne fail
    ldr  x0, [x24, #8]
    ldr  x1, [x23, #8]
    cmp  x0, x1
    b.ne fail

    ldr  q5, [x21]
    dup  v4.4s, v5.s[0]
    str  q4, [x22, #32]
    adr  x23, exp_s
    add  x24, x22, #32
    ldr  x0, [x24]
    ldr  x1, [x23]
    cmp  x0, x1
    b.ne fail
    ldr  x0, [x24, #8]
    ldr  x1, [x23, #8]
    cmp  x0, x1
    b.ne fail

    ldr  q7, [x21]
    dup  v6.2d, v7.d[1]
    str  q6, [x22, #48]
    adr  x23, exp_d
    add  x24, x22, #48
    ldr  x0, [x24]
    ldr  x1, [x23]
    cmp  x0, x1
    b.ne fail
    ldr  x0, [x24, #8]
    ldr  x1, [x23, #8]
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
src:
    .byte 0x10, 0x11, 0x12, 0x13, 0x20, 0x21, 0x22, 0x23
    .byte 0x30, 0x31, 0x32, 0x33, 0x40, 0x41, 0x42, 0x43
dst:
    .space 64
exp_b:
    .quad 0x1212121212121212
    .quad 0x1212121212121212
exp_h:
    .quad 0x1312131213121312
    .quad 0x1312131213121312
exp_s:
    .quad 0x1312111013121110
    .quad 0x1312111013121110
exp_d:
    .quad 0x4342414033323130
    .quad 0x4342414033323130
