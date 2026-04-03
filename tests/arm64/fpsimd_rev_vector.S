    .text
    .global _start
_start:
    movz x20, #0
    movk x20, #0x0900, lsl #16
    movz x25, #0x0030, lsl #16
    msr  cpacr_el1, x25
    isb

    adr  x21, scratch

    adr  x22, src_rev16
    ldr  q1, [x22]
    rev16 v0.16b, v1.16b
    str  q0, [x21]
    mov  x0, x21
    adr  x1, exp_rev16
    bl   check16

    adr  x22, src_rev32
    ldr  q3, [x22]
    rev32 v2.8h, v3.8h
    str  q2, [x21, #16]
    add  x0, x21, #16
    adr  x1, exp_rev32
    bl   check16

    adr  x22, src_rev64
    ldr  q5, [x22]
    rev64 v4.4s, v5.4s
    str  q4, [x21, #32]
    add  x0, x21, #32
    adr  x1, exp_rev64
    bl   check16

    adr  x22, src_rev64_d
    ldr  d7, [x22]
    rev64 v6.8b, v7.8b
    str  d6, [x21, #48]
    add  x0, x21, #48
    adr  x1, exp_rev64_d
    bl   check8

    movz x0, #'R'
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
src_rev16:
    .byte 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07
    .byte 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f
exp_rev16:
    .byte 0x01, 0x00, 0x03, 0x02, 0x05, 0x04, 0x07, 0x06
    .byte 0x09, 0x08, 0x0b, 0x0a, 0x0d, 0x0c, 0x0f, 0x0e

src_rev32:
    .hword 0x1000, 0x1001, 0x1002, 0x1003, 0x1004, 0x1005, 0x1006, 0x1007
exp_rev32:
    .hword 0x1001, 0x1000, 0x1003, 0x1002, 0x1005, 0x1004, 0x1007, 0x1006

src_rev64:
    .word 0x11111111, 0x22222222, 0x33333333, 0x44444444
exp_rev64:
    .word 0x22222222, 0x11111111, 0x44444444, 0x33333333

src_rev64_d:
    .byte 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17
exp_rev64_d:
    .byte 0x17, 0x16, 0x15, 0x14, 0x13, 0x12, 0x11, 0x10

scratch:
    .space 64
