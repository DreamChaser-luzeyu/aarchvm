    .text
    .global _start
_start:
    movz x20, #0
    movk x20, #0x0900, lsl #16
    movz x25, #0x0030, lsl #16
    msr  cpacr_el1, x25
    isb

    adr  x21, dst

    adr  x22, smax_a
    ldr  d1, [x22]
    adr  x22, smax_b
    ldr  d2, [x22]
    smax v0.8b, v1.8b, v2.8b
    str  d0, [x21]
    mov  x0, x21
    adr  x1, exp_smax
    bl   check8

    adr  x22, smin_a
    ldr  q4, [x22]
    adr  x22, smin_b
    ldr  q5, [x22]
    smin v3.8h, v4.8h, v5.8h
    str  q3, [x21, #16]
    add  x0, x21, #16
    adr  x1, exp_smin
    bl   check16

    adr  x22, umax_a
    ldr  q7, [x22]
    adr  x22, umax_b
    ldr  q8, [x22]
    umax v6.4s, v7.4s, v8.4s
    str  q6, [x21, #32]
    add  x0, x21, #32
    adr  x1, exp_umax
    bl   check16

    adr  x22, umin_a
    ldr  d10, [x22]
    adr  x22, umin_b
    ldr  d11, [x22]
    umin v9.8b, v10.8b, v11.8b
    str  d9, [x21, #48]
    add  x0, x21, #48
    adr  x1, exp_umin
    bl   check8

    movz x0, #'M'
    str  x0, [x20]
    movz x0, #'X'
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
    .balign 8
smax_a:
    .byte 0x80, 0x7f, 0xff, 0x01, 0x05, 0xfb, 0x0a, 0xf6
smax_b:
    .byte 0x01, 0xff, 0x02, 0x03, 0xfa, 0x07, 0x0b, 0xf5
exp_smax:
    .byte 0x01, 0x7f, 0x02, 0x03, 0x05, 0x07, 0x0b, 0xf6

    .balign 16
smin_a:
    .hword 0, 1, 0xffff, 0x7fff, 0x8000, 100, 0xff38, 50
smin_b:
    .hword 1, 0xfffe, 2, 0x7ffe, 0xffff, 200, 0xff9c, 50
exp_smin:
    .hword 0, 0xfffe, 0xffff, 0x7ffe, 0x8000, 100, 0xff38, 50

    .balign 16
umax_a:
    .word 0, 1, 0xffffffff, 100
umax_b:
    .word 1, 0, 5, 200
exp_umax:
    .word 1, 1, 0xffffffff, 200

    .balign 8
umin_a:
    .byte 0, 10, 255, 1, 50, 100, 200, 5
umin_b:
    .byte 1, 20, 0, 250, 45, 110, 100, 5
exp_umin:
    .byte 0, 10, 0, 1, 45, 100, 100, 5

dst:
    .space 64
