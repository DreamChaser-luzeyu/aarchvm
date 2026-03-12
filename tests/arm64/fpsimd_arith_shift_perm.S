    .text
    .global _start
_start:
    movz x20, #0
    movk x20, #0x0900, lsl #16
    adr  x21, dst

    adr  x22, add_a
    ldr  q0, [x22]
    adr  x22, add_b
    ldr  q1, [x22]
    add  v2.16b, v0.16b, v1.16b
    str  q2, [x21]
    adr  x22, exp_add
    ldr  x0, [x21]
    ldr  x1, [x22]
    cmp  x0, x1
    b.ne fail
    ldr  x0, [x21, #8]
    ldr  x1, [x22, #8]
    cmp  x0, x1
    b.ne fail

    adr  x22, sub_a
    ldr  q3, [x22]
    adr  x22, sub_b
    ldr  q4, [x22]
    sub  v5.8h, v3.8h, v4.8h
    str  q5, [x21, #16]
    adr  x22, exp_sub
    add  x23, x21, #16
    ldr  x0, [x23]
    ldr  x1, [x22]
    cmp  x0, x1
    b.ne fail
    ldr  x0, [x23, #8]
    ldr  x1, [x22, #8]
    cmp  x0, x1
    b.ne fail

    adr  x22, mul_a
    ldr  q6, [x22]
    adr  x22, mul_b
    ldr  q7, [x22]
    mul  v8.4s, v6.4s, v7.4s
    str  q8, [x21, #32]
    adr  x22, exp_mul
    add  x23, x21, #32
    ldr  x0, [x23]
    ldr  x1, [x22]
    cmp  x0, x1
    b.ne fail
    ldr  x0, [x23, #8]
    ldr  x1, [x22, #8]
    cmp  x0, x1
    b.ne fail

    adr  x22, shift_h_src
    ldr  q9, [x22]
    ushr v10.8h, v9.8h, #3
    str  q10, [x21, #48]
    adr  x22, exp_ushr
    add  x23, x21, #48
    ldr  x0, [x23]
    ldr  x1, [x22]
    cmp  x0, x1
    b.ne fail
    ldr  x0, [x23, #8]
    ldr  x1, [x22, #8]
    cmp  x0, x1
    b.ne fail

    adr  x22, shift_s_src
    ldr  q11, [x22]
    sshr v12.4s, v11.4s, #5
    str  q12, [x21, #64]
    adr  x22, exp_sshr
    add  x23, x21, #64
    ldr  x0, [x23]
    ldr  x1, [x22]
    cmp  x0, x1
    b.ne fail
    ldr  x0, [x23, #8]
    ldr  x1, [x22, #8]
    cmp  x0, x1
    b.ne fail

    adr  x22, perm_a
    ldr  q13, [x22]
    adr  x22, perm_b
    ldr  q14, [x22]
    zip1 v15.16b, v13.16b, v14.16b
    str  q15, [x21, #80]
    adr  x22, exp_zip1
    add  x23, x21, #80
    ldr  x0, [x23]
    ldr  x1, [x22]
    cmp  x0, x1
    b.ne fail
    ldr  x0, [x23, #8]
    ldr  x1, [x22, #8]
    cmp  x0, x1
    b.ne fail

    mov  v22.16b, v13.16b
    str  q22, [x21, #96]
    adr  x22, exp_mov
    add  x23, x21, #96
    ldr  x0, [x23]
    ldr  x1, [x22]
    cmp  x0, x1
    b.ne fail
    ldr  x0, [x23, #8]
    ldr  x1, [x22, #8]
    cmp  x0, x1
    b.ne fail

    adr  x22, perm_h_a
    ldr  q16, [x22]
    adr  x22, perm_h_b
    ldr  q17, [x22]
    uzp1 v18.8h, v16.8h, v17.8h
    str  q18, [x21, #112]
    adr  x22, exp_uzp1
    add  x23, x21, #112
    ldr  x0, [x23]
    ldr  x1, [x22]
    cmp  x0, x1
    b.ne fail
    ldr  x0, [x23, #8]
    ldr  x1, [x22, #8]
    cmp  x0, x1
    b.ne fail

    adr  x22, perm_s_a
    ldr  q19, [x22]
    adr  x22, perm_s_b
    ldr  q20, [x22]
    trn1 v21.4s, v19.4s, v20.4s
    str  q21, [x21, #128]
    adr  x22, exp_trn1
    add  x23, x21, #128
    ldr  x0, [x23]
    ldr  x1, [x22]
    cmp  x0, x1
    b.ne fail
    ldr  x0, [x23, #8]
    ldr  x1, [x22, #8]
    cmp  x0, x1
    b.ne fail

    movz x0, #'V'
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
    .space 144
add_a:
    .byte 1,4,7,10,13,16,19,22,25,28,31,34,37,40,43,46
add_b:
    .byte 3,8,13,18,23,28,33,38,43,48,53,58,63,68,73,78
exp_add:
    .byte 4,12,20,28,36,44,52,60,68,76,84,92,100,108,116,124
sub_a:
    .hword 1000,2000,3000,4000,5000,6000,7000,8000
sub_b:
    .hword 7,11,13,17,19,23,29,31
exp_sub:
    .hword 993,1989,2987,3983,4981,5977,6971,7969
mul_a:
    .word 7,11,13,17
mul_b:
    .word 19,23,29,31
exp_mul:
    .word 133,253,377,527
shift_h_src:
    .hword 0x1234,0x5678,0x9abc,0xdef0,0x00ff,0xff00,0x0f0f,0xf0f0
exp_ushr:
    .hword 0x0246,0x0acf,0x1357,0x1bde,0x001f,0x1fe0,0x01e1,0x1e1e
shift_s_src:
    .word 0x00000100,0xfffffe00,0x7fffffff,0x80000000
exp_sshr:
    .word 0x00000008,0xfffffff0,0x03ffffff,0xfc000000
perm_a:
    .byte 0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f
perm_b:
    .byte 0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x8a,0x8b,0x8c,0x8d,0x8e,0x8f
exp_zip1:
    .byte 0x00,0x80,0x01,0x81,0x02,0x82,0x03,0x83,0x04,0x84,0x05,0x85,0x06,0x86,0x07,0x87
exp_mov:
    .byte 0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f
perm_h_a:
    .hword 0x1000,0x1001,0x1002,0x1003,0x1004,0x1005,0x1006,0x1007
perm_h_b:
    .hword 0x2000,0x2001,0x2002,0x2003,0x2004,0x2005,0x2006,0x2007
exp_uzp1:
    .hword 0x1000,0x1002,0x1004,0x1006,0x2000,0x2002,0x2004,0x2006
perm_s_a:
    .word 0x11111111,0x22222222,0x33333333,0x44444444
perm_s_b:
    .word 0xaaaaaaaa,0xbbbbbbbb,0xcccccccc,0xdddddddd
exp_trn1:
    .word 0x11111111,0xaaaaaaaa,0x33333333,0xcccccccc
