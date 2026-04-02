    .text
    .global _start
_start:
    movz x20, #0
    movk x20, #0x0900, lsl #16
    movz x25, #0x0030, lsl #16
    msr  cpacr_el1, x25
    isb

    adr  x21, scratch

    adr  x22, xtn8_src
    ldr  q1, [x22]
    xtn  v0.8b, v1.8h
    str  d0, [x21]
    mov  x0, x21
    adr  x1, exp_xtn8
    bl   check8

    adr  x22, xtn2_8_init
    ldr  q2, [x22]
    adr  x22, xtn2_8_src
    ldr  q3, [x22]
    xtn2 v2.16b, v3.8h
    str  q2, [x21, #16]
    add  x0, x21, #16
    adr  x1, exp_xtn2_8
    bl   check16

    adr  x22, xtn16_src
    ldr  q4, [x22]
    xtn  v5.4h, v4.4s
    str  d5, [x21, #32]
    add  x0, x21, #32
    adr  x1, exp_xtn16
    bl   check8

    adr  x22, xtn2_16_init
    ldr  q6, [x22]
    adr  x22, xtn2_16_src
    ldr  q7, [x22]
    xtn2 v6.8h, v7.4s
    str  q6, [x21, #48]
    add  x0, x21, #48
    adr  x1, exp_xtn2_16
    bl   check16

    adr  x22, xtn32_src
    ldr  q8, [x22]
    xtn  v9.2s, v8.2d
    str  d9, [x21, #64]
    add  x0, x21, #64
    adr  x1, exp_xtn32
    bl   check8

    adr  x22, xtn2_32_init
    ldr  q10, [x22]
    adr  x22, xtn2_32_src
    ldr  q11, [x22]
    xtn2 v10.4s, v11.2d
    str  q10, [x21, #80]
    add  x0, x21, #80
    adr  x1, exp_xtn2_32
    bl   check16

    movz x0, #'N'
    str  x0, [x20]
    movz x0, #'\n'
    str  x0, [x20]
    brk  #0

check8:
    ldr  x2, [x0]
    ldr  x3, [x1]
    cmp  x2, x3
    b.ne fail
    ret

check16:
    ldr  x2, [x0]
    ldr  x3, [x1]
    cmp  x2, x3
    b.ne fail
    ldr  x2, [x0, #8]
    ldr  x3, [x1, #8]
    cmp  x2, x3
    b.ne fail
    ret

fail:
    movz x0, #'F'
    str  x0, [x20]
    brk  #0

    .data
    .balign 16
xtn8_src:
    .hword 0x0102, 0x0304, 0x0506, 0x0708, 0xff09, 0x1234, 0xabcd, 0x00ff
exp_xtn8:
    .byte 0x02, 0x04, 0x06, 0x08, 0x09, 0x34, 0xcd, 0xff

xtn2_8_init:
    .byte 0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7
    .byte 0, 0, 0, 0, 0, 0, 0, 0
xtn2_8_src:
    .hword 0x1111, 0x2222, 0x3333, 0x4444, 0x5555, 0x6666, 0x7777, 0x8888
exp_xtn2_8:
    .byte 0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7
    .byte 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88

xtn16_src:
    .word 0x12340002, 0xabcd0004, 0xffff0006, 0x00070008
exp_xtn16:
    .hword 0x0002, 0x0004, 0x0006, 0x0008

xtn2_16_init:
    .hword 0x1001, 0x1002, 0x1003, 0x1004, 0, 0, 0, 0
xtn2_16_src:
    .word 0xaaaa0005, 0xbbbb0006, 0xcccc0007, 0xdddd0008
exp_xtn2_16:
    .hword 0x1001, 0x1002, 0x1003, 0x1004, 0x0005, 0x0006, 0x0007, 0x0008

xtn32_src:
    .quad 0x1234567800000009, 0xabcdef010000000a
exp_xtn32:
    .word 0x00000009, 0x0000000a

xtn2_32_init:
    .word 0x11111111, 0x22222222, 0, 0
xtn2_32_src:
    .quad 0x555566660000000b, 0x777788880000000c
exp_xtn2_32:
    .word 0x11111111, 0x22222222, 0x0000000b, 0x0000000c

scratch:
    .space 96
