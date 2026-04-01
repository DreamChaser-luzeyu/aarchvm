    .text
    .global _start
_start:
    movz x20, #0
    movk x20, #0x0900, lsl #16
    movz x25, #0x0030, lsl #16
    msr  cpacr_el1, x25
    isb

    adr  x21, scratch

    adr  x22, add16_a
    ldr  q0, [x22]
    adr  x22, add16_b
    ldr  q1, [x22]
    addhn v2.8b, v0.8h, v1.8h
    str  d2, [x21]
    adr  x22, exp_add16
    ldr  x0, [x21]
    ldr  x1, [x22]
    cmp  x0, x1
    b.ne fail

    adr  x22, radd16_a
    ldr  q3, [x22]
    adr  x22, radd16_b
    ldr  q4, [x22]
    raddhn v5.8b, v3.8h, v4.8h
    str  d5, [x21, #8]
    adr  x22, exp_radd16
    add  x23, x21, #8
    ldr  x0, [x23]
    ldr  x1, [x22]
    cmp  x0, x1
    b.ne fail

    adr  x22, sub16_a
    ldr  q6, [x22]
    adr  x22, sub16_b
    ldr  q7, [x22]
    subhn v8.8b, v6.8h, v7.8h
    str  d8, [x21, #16]
    adr  x22, exp_sub16
    add  x23, x21, #16
    ldr  x0, [x23]
    ldr  x1, [x22]
    cmp  x0, x1
    b.ne fail

    adr  x22, rsub16_a
    ldr  q9, [x22]
    adr  x22, rsub16_b
    ldr  q10, [x22]
    rsubhn v11.8b, v9.8h, v10.8h
    str  d11, [x21, #24]
    adr  x22, exp_rsub16
    add  x23, x21, #24
    ldr  x0, [x23]
    ldr  x1, [x22]
    cmp  x0, x1
    b.ne fail

    adr  x22, addhn2_init
    ldr  q12, [x22]
    adr  x22, add16_a
    ldr  q13, [x22]
    adr  x22, add16_b
    ldr  q14, [x22]
    addhn2 v12.16b, v13.8h, v14.8h
    str  q12, [x21, #32]
    adr  x22, exp_addhn2
    add  x23, x21, #32
    ldp  x0, x1, [x23]
    ldp  x2, x3, [x22]
    cmp  x0, x2
    b.ne fail
    cmp  x1, x3
    b.ne fail

    adr  x22, add32_a
    ldr  q15, [x22]
    adr  x22, add32_b
    ldr  q16, [x22]
    addhn v17.4h, v15.4s, v16.4s
    str  d17, [x21, #48]
    adr  x22, exp_add32
    add  x23, x21, #48
    ldr  x0, [x23]
    ldr  x1, [x22]
    cmp  x0, x1
    b.ne fail

    adr  x22, rsubhn2_init
    ldr  q18, [x22]
    adr  x22, rsub16_a
    ldr  q19, [x22]
    adr  x22, rsub16_b
    ldr  q20, [x22]
    rsubhn2 v18.16b, v19.8h, v20.8h
    str  q18, [x21, #64]
    adr  x22, exp_rsubhn2
    add  x23, x21, #64
    ldp  x0, x1, [x23]
    ldp  x2, x3, [x22]
    cmp  x0, x2
    b.ne fail
    cmp  x1, x3
    b.ne fail

    adr  x22, sub64_a
    ldr  q21, [x22]
    adr  x22, sub64_b
    ldr  q22, [x22]
    subhn v23.2s, v21.2d, v22.2d
    str  d23, [x21, #80]
    adr  x22, exp_sub64
    add  x23, x21, #80
    ldr  x0, [x23]
    ldr  x1, [x22]
    cmp  x0, x1
    b.ne fail

    movz x0, #'H'
    str  x0, [x20]
    movz x0, #'N'
    str  x0, [x20]
    brk  #0

fail:
    movz x0, #'F'
    str  x0, [x20]
    brk  #0

    .data
    .balign 16
scratch:
    .space 96

add16_a:
    .hword 0x1200, 0x00ff, 0xffff, 0x8000, 0x1234, 0xabcd, 0x1111, 0x0102
add16_b:
    .hword 0x3400, 0x0001, 0x0001, 0x7fff, 0x00ff, 0x0102, 0x2222, 0xfeff
exp_add16:
    .byte 0x46, 0x01, 0x00, 0xff, 0x13, 0xac, 0x33, 0x00

radd16_a:
    .hword 0x007f, 0x0080, 0x00ff, 0xff00, 0x1234, 0x5678, 0x8000, 0xffff
radd16_b:
    .hword 0x0000, 0x0000, 0x0001, 0x00ff, 0x0001, 0x1111, 0x7fff, 0x0001
exp_radd16:
    .byte 0x00, 0x01, 0x01, 0x00, 0x12, 0x68, 0x00, 0x00

sub16_a:
    .hword 0x1200, 0x0100, 0x0000, 0x8000, 0x1000, 0xabcd, 0x0001, 0x8000
sub16_b:
    .hword 0x0200, 0x0001, 0x0001, 0x7fff, 0x2000, 0x00cd, 0x0002, 0xffff
exp_sub16:
    .byte 0x10, 0x00, 0xff, 0x00, 0xf0, 0xab, 0xff, 0x80

rsub16_a:
    .hword 0x1200, 0x0100, 0x0000, 0x8000, 0x1000, 0xabcd, 0x0001, 0x8000
rsub16_b:
    .hword 0x0201, 0x0002, 0x0001, 0x7fff, 0x2001, 0x00ce, 0x0002, 0xffff
exp_rsub16:
    .byte 0x10, 0x01, 0x00, 0x00, 0xf0, 0xab, 0x00, 0x80

addhn2_init:
    .byte 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80
    .byte 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
exp_addhn2:
    .byte 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80
    .byte 0x46, 0x01, 0x00, 0xff, 0x13, 0xac, 0x33, 0x00

add32_a:
    .word 0x12340000, 0x0000ffff, 0xffffffff, 0x80000000
add32_b:
    .word 0x11110000, 0x00000001, 0x00010001, 0x7fffffff
exp_add32:
    .hword 0x2345, 0x0001, 0x0001, 0xffff

rsubhn2_init:
    .byte 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8
    .byte 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
exp_rsubhn2:
    .byte 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8
    .byte 0x10, 0x01, 0x00, 0x00, 0xf0, 0xab, 0x00, 0x80

sub64_a:
    .quad 0x1234567800000000, 0x8000000000000000
sub64_b:
    .quad 0x1111111100000000, 0x0000000000000001
exp_sub64:
    .word 0x01234567, 0x7fffffff
