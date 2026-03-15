    .text
    .global _start
_start:
    movz x20, #0
    movk x20, #0x0900, lsl #16
    adr  x21, dst

    adr  x22, perm_a
    ldr  q0, [x22]
    adr  x22, perm_b
    ldr  q1, [x22]
    zip2 v2.16b, v0.16b, v1.16b
    str  q2, [x21]
    mov  x0, x21
    adr  x1, exp_zip2
    bl   check16

    adr  x22, perm_h_a
    ldr  q3, [x22]
    adr  x22, perm_h_b
    ldr  q4, [x22]
    uzp2 v5.8h, v3.8h, v4.8h
    str  q5, [x21, #16]
    add  x0, x21, #16
    adr  x1, exp_uzp2
    bl   check16

    adr  x22, perm_s_a
    ldr  q6, [x22]
    adr  x22, perm_s_b
    ldr  q7, [x22]
    trn2 v8.4s, v6.4s, v7.4s
    str  q8, [x21, #32]
    add  x0, x21, #32
    adr  x1, exp_trn2
    bl   check16

    adr  x22, fmla_acc
    ldr  q9, [x22]
    adr  x22, fmla_lhs
    ldr  q10, [x22]
    adr  x22, fmla_rhs
    ldr  q11, [x22]
    fmla v9.4s, v10.4s, v11.4s
    str  q9, [x21, #48]
    add  x0, x21, #48
    adr  x1, exp_fmla_s
    bl   check16

    adr  x22, fmla_acc
    ldr  q12, [x22]
    adr  x22, fmla_lhs
    ldr  q13, [x22]
    adr  x22, fmla_rhs
    ldr  q14, [x22]
    fmls v12.4s, v13.4s, v14.4s
    str  q12, [x21, #64]
    add  x0, x21, #64
    adr  x1, exp_fmls_s
    bl   check16

    adr  x22, fmla_acc_d
    ldr  q15, [x22]
    adr  x22, fmla_lhs_d
    ldr  q16, [x22]
    adr  x22, fmla_rhs_d
    ldr  q17, [x22]
    fmla v15.2d, v16.2d, v17.2d
    str  q15, [x21, #80]
    add  x0, x21, #80
    adr  x1, exp_fmla_d
    bl   check16

    adr  x22, nm_lhs
    ldr  q18, [x22]
    adr  x22, nm_rhs
    ldr  q19, [x22]
    fminnm v20.4s, v18.4s, v19.4s
    str  q20, [x21, #96]
    add  x0, x21, #96
    adr  x1, exp_fminnm
    bl   check16

    fmaxnm v21.4s, v18.4s, v19.4s
    str  q21, [x21, #112]
    add  x0, x21, #112
    adr  x1, exp_fmaxnm
    bl   check16

    adr  x22, fabs_a
    ldr  q22, [x22]
    adr  x22, fabs_b
    ldr  q23, [x22]
    fabd v24.4s, v22.4s, v23.4s
    str  q24, [x21, #128]
    add  x0, x21, #128
    adr  x1, exp_fabd
    bl   check16

    facge v25.4s, v22.4s, v23.4s
    str  q25, [x21, #144]
    add  x0, x21, #144
    adr  x1, exp_facge
    bl   check16

    facgt v26.4s, v22.4s, v23.4s
    str  q26, [x21, #160]
    add  x0, x21, #160
    adr  x1, exp_facgt
    bl   check16

    movz x0, #'M'
    str  x0, [x20]
    movz x0, #'\n'
    str  x0, [x20]
    brk  #0

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
perm_a:
    .byte 0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f
perm_b:
    .byte 0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x8a,0x8b,0x8c,0x8d,0x8e,0x8f
exp_zip2:
    .byte 0x08,0x88,0x09,0x89,0x0a,0x8a,0x0b,0x8b,0x0c,0x8c,0x0d,0x8d,0x0e,0x8e,0x0f,0x8f
perm_h_a:
    .hword 0x1000,0x1001,0x1002,0x1003,0x1004,0x1005,0x1006,0x1007
perm_h_b:
    .hword 0x2000,0x2001,0x2002,0x2003,0x2004,0x2005,0x2006,0x2007
exp_uzp2:
    .hword 0x1001,0x1003,0x1005,0x1007,0x2001,0x2003,0x2005,0x2007
perm_s_a:
    .word 0x11111111,0x22222222,0x33333333,0x44444444
perm_s_b:
    .word 0xaaaaaaaa,0xbbbbbbbb,0xcccccccc,0xdddddddd
exp_trn2:
    .word 0x22222222,0xbbbbbbbb,0x44444444,0xdddddddd

fmla_acc:
    .word 0x3f800000,0x40000000,0x40400000,0x40800000
fmla_lhs:
    .word 0x40000000,0x40400000,0x40800000,0x40a00000
fmla_rhs:
    .word 0x41200000,0x41a00000,0x41f00000,0x42200000
exp_fmla_s:
    .word 0x41a80000,0x42780000,0x42f60000,0x434c0000
exp_fmls_s:
    .word 0xc1980000,0xc2680000,0xc2ea0000,0xc3440000

fmla_acc_d:
    .quad 0x3ff0000000000000,0x4000000000000000
fmla_lhs_d:
    .quad 0x4008000000000000,0x4010000000000000
fmla_rhs_d:
    .quad 0x4024000000000000,0x4034000000000000
exp_fmla_d:
    .quad 0x403f000000000000,0x4054800000000000

nm_lhs:
    .word 0x7fc00000,0x40a00000,0xc0000000,0x3f800000
nm_rhs:
    .word 0x40400000,0x7fc00000,0x40800000,0xc1000000
exp_fminnm:
    .word 0x40400000,0x40a00000,0xc0000000,0xc1000000
exp_fmaxnm:
    .word 0x40400000,0x40a00000,0x40800000,0x3f800000

fabs_a:
    .word 0x3f800000,0xc0a00000,0x40600000,0xc1000000
fabs_b:
    .word 0x40800000,0xbfc00000,0xc0200000,0x40000000
exp_fabd:
    .word 0x40400000,0x40600000,0x40c00000,0x41200000
exp_facge:
    .word 0x00000000,0xffffffff,0xffffffff,0xffffffff
exp_facgt:
    .word 0x00000000,0xffffffff,0xffffffff,0xffffffff

dst:
    .space 176
