    .text
    .global _start
_start:
    movz x20, #0
    movk x20, #0x0900, lsl #16
    movz x25, #0x0030, lsl #16
    msr  cpacr_el1, x25
    isb

    adr  x21, scratch

    adr  x22, init_b
    ldr  q0, [x22]
    adr  x22, src_b
    ldr  q1, [x22]
    mov  v0.b[1], v1.b[0]
    mov  v0.b[15], v1.b[14]
    str  q0, [x21]
    adr  x22, exp_b
    ldp  x0, x1, [x21]
    ldp  x2, x3, [x22]
    cmp  x0, x2
    b.ne fail
    cmp  x1, x3
    b.ne fail

    adr  x22, init_h
    ldr  q2, [x22]
    adr  x22, src_h
    ldr  q3, [x22]
    mov  v2.h[1], v3.h[0]
    mov  v2.h[7], v3.h[6]
    str  q2, [x21, #16]
    adr  x22, exp_h
    add  x23, x21, #16
    ldp  x0, x1, [x23]
    ldp  x2, x3, [x22]
    cmp  x0, x2
    b.ne fail
    cmp  x1, x3
    b.ne fail

    adr  x22, init_s
    ldr  q4, [x22]
    adr  x22, src_s
    ldr  q5, [x22]
    mov  v4.s[1], v5.s[0]
    mov  v4.s[3], v5.s[2]
    str  q4, [x21, #32]
    adr  x22, exp_s
    add  x23, x21, #32
    ldp  x0, x1, [x23]
    ldp  x2, x3, [x22]
    cmp  x0, x2
    b.ne fail
    cmp  x1, x3
    b.ne fail

    adr  x22, init_d
    ldr  q6, [x22]
    adr  x22, src_d
    ldr  q7, [x22]
    mov  v6.d[1], v7.d[0]
    mov  v6.d[0], v7.d[1]
    str  q6, [x21, #48]
    adr  x22, exp_d
    add  x23, x21, #48
    ldp  x0, x1, [x23]
    ldp  x2, x3, [x22]
    cmp  x0, x2
    b.ne fail
    cmp  x1, x3
    b.ne fail

    movz x0, #'M'
    str  x0, [x20]
    movz x0, #'E'
    str  x0, [x20]
    brk  #0

fail:
    movz x0, #'F'
    str  x0, [x20]
    brk  #0

    .data
    .balign 16
scratch:
    .space 64

init_b:
    .byte 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07
    .byte 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f
src_b:
    .byte 0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7
    .byte 0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf
exp_b:
    .byte 0x00, 0xa0, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07
    .byte 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0xae

init_h:
    .hword 0x0001, 0x0002, 0x0003, 0x0004, 0x0005, 0x0006, 0x0007, 0x0008
src_h:
    .hword 0xa100, 0xa101, 0xa102, 0xa103, 0xa104, 0xa105, 0xa106, 0xa107
exp_h:
    .hword 0x0001, 0xa100, 0x0003, 0x0004, 0x0005, 0x0006, 0x0007, 0xa106

init_s:
    .word 0x11111111, 0x22222222, 0x33333333, 0x44444444
src_s:
    .word 0xa0a0a0a0, 0xb1b1b1b1, 0xc2c2c2c2, 0xd3d3d3d3
exp_s:
    .word 0x11111111, 0xa0a0a0a0, 0x33333333, 0xc2c2c2c2

init_d:
    .quad 0x0123456789abcdef, 0xfedcba9876543210
src_d:
    .quad 0x1111222233334444, 0xaaaabbbbccccdddd
exp_d:
    .quad 0xaaaabbbbccccdddd, 0x1111222233334444
