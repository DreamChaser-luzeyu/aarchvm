    .text
    .global _start
_start:
    movz x20, #0
    movk x20, #0x0900, lsl #16
    adr  x21, dst
    movz x25, #0x0030, lsl #16
    msr  cpacr_el1, x25
    isb

    msr  fpcr, xzr
    msr  fpsr, xzr

    adr  x22, fmul_scalar_s_src
    ldr  s0, [x22]
    adr  x22, fmul_scalar_s_vec
    ldr  q1, [x22]
    fmul s2, s0, v1.s[2]
    str  s2, [x21]
    movz x23, #'1'
    add  x0, x21, #0
    adr  x1, exp_fmul_scalar_s
    bl   check4

    adr  x22, fmul_scalar_d_src
    ldr  d3, [x22]
    adr  x22, fmul_scalar_d_vec
    ldr  q4, [x22]
    fmul d5, d3, v4.d[1]
    str  d5, [x21, #8]
    movz x23, #'2'
    add  x0, x21, #8
    adr  x1, exp_fmul_scalar_d
    bl   check8

    adr  x22, fmul_vec_s_src
    ldr  q6, [x22]
    adr  x22, fmul_vec_s_vec
    ldr  q7, [x22]
    fmul v8.4s, v6.4s, v7.s[1]
    str  q8, [x21, #16]
    movz x23, #'3'
    add  x0, x21, #16
    adr  x1, exp_fmul_vec_s
    bl   check16

    adr  x22, fmul_vec_d_src
    ldr  q9, [x22]
    adr  x22, fmul_vec_d_vec
    ldr  q10, [x22]
    fmul v11.2d, v9.2d, v10.d[0]
    str  q11, [x21, #32]
    movz x23, #'4'
    add  x0, x21, #32
    adr  x1, exp_fmul_vec_d
    bl   check16

    adr  x22, fmla_scalar_s_acc
    ldr  s12, [x22]
    adr  x22, fmla_scalar_s_src
    ldr  s13, [x22]
    adr  x22, fmla_scalar_s_vec
    ldr  q14, [x22]
    fmla s12, s13, v14.s[3]
    str  s12, [x21, #48]
    movz x23, #'5'
    add  x0, x21, #48
    adr  x1, exp_fmla_scalar_s
    bl   check4

    adr  x22, fmla_scalar_d_acc
    ldr  d15, [x22]
    adr  x22, fmla_scalar_d_src
    ldr  d16, [x22]
    adr  x22, fmla_scalar_d_vec
    ldr  q17, [x22]
    fmla d15, d16, v17.d[1]
    str  d15, [x21, #56]
    movz x23, #'6'
    add  x0, x21, #56
    adr  x1, exp_fmla_scalar_d
    bl   check8

    adr  x22, fmla_vec_s_acc
    ldr  q18, [x22]
    adr  x22, fmla_vec_s_src
    ldr  q19, [x22]
    adr  x22, fmla_vec_s_vec
    ldr  q20, [x22]
    fmla v18.4s, v19.4s, v20.s[0]
    str  q18, [x21, #64]
    movz x23, #'7'
    add  x0, x21, #64
    adr  x1, exp_fmla_vec_s
    bl   check16

    adr  x22, fmla_vec_d_acc
    ldr  q21, [x22]
    adr  x22, fmla_vec_d_src
    ldr  q22, [x22]
    adr  x22, fmla_vec_d_vec
    ldr  q23, [x22]
    fmla v21.2d, v22.2d, v23.d[1]
    str  q21, [x21, #80]
    movz x23, #'8'
    add  x0, x21, #80
    adr  x1, exp_fmla_vec_d
    bl   check16

    adr  x22, fmls_scalar_s_acc
    ldr  s24, [x22]
    adr  x22, fmls_scalar_s_src
    ldr  s25, [x22]
    adr  x22, fmls_scalar_s_vec
    ldr  q26, [x22]
    fmls s24, s25, v26.s[1]
    str  s24, [x21, #96]
    movz x23, #'9'
    add  x0, x21, #96
    adr  x1, exp_fmls_scalar_s
    bl   check4

    adr  x22, fmls_scalar_d_acc
    ldr  d27, [x22]
    adr  x22, fmls_scalar_d_src
    ldr  d28, [x22]
    adr  x22, fmls_scalar_d_vec
    ldr  q29, [x22]
    fmls d27, d28, v29.d[0]
    str  d27, [x21, #104]
    movz x23, #'A'
    add  x0, x21, #104
    adr  x1, exp_fmls_scalar_d
    bl   check8

    adr  x22, fmls_vec_s_acc
    ldr  q30, [x22]
    adr  x22, fmls_vec_s_src
    ldr  q31, [x22]
    adr  x22, fmls_vec_s_vec
    ldr  q0, [x22]
    fmls v30.4s, v31.4s, v0.s[2]
    str  q30, [x21, #112]
    movz x23, #'B'
    add  x0, x21, #112
    adr  x1, exp_fmls_vec_s
    bl   check16

    adr  x22, fmls_vec_d_acc
    ldr  q1, [x22]
    adr  x22, fmls_vec_d_src
    ldr  q2, [x22]
    adr  x22, fmls_vec_d_vec
    ldr  q3, [x22]
    fmls v1.2d, v2.2d, v3.d[0]
    str  q1, [x21, #128]
    movz x23, #'C'
    add  x0, x21, #128
    adr  x1, exp_fmls_vec_d
    bl   check16

    movz x0, #'I'
    str  x0, [x20]
    movz x0, #'\n'
    str  x0, [x20]
    brk  #0

check4:
    ldr  w2, [x0]
    ldr  w3, [x1]
    cmp  w2, w3
    b.ne fail
    ret

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
    str  x23, [x20]
    brk  #0

    .data
    .balign 16
fmul_scalar_s_src:
    .word 0x3fc00000
fmul_scalar_s_vec:
    .word 0x3f800000, 0x40000000, 0xc0000000, 0x40400000
fmul_scalar_d_src:
    .quad 0xc010000000000000
fmul_scalar_d_vec:
    .quad 0x4010000000000000, 0x3fe0000000000000
fmul_vec_s_src:
    .word 0x3f800000, 0x40000000, 0x40400000, 0x40800000
fmul_vec_s_vec:
    .word 0x3f000000, 0xc0000000, 0x40400000, 0x40800000
fmul_vec_d_src:
    .quad 0x3ff8000000000000, 0xc000000000000000
fmul_vec_d_vec:
    .quad 0x4010000000000000, 0x3fd0000000000000

fmla_scalar_s_acc:
    .word 0x41200000
fmla_scalar_s_src:
    .word 0x3fc00000
fmla_scalar_s_vec:
    .word 0x3f800000, 0x40000000, 0x40400000, 0x40000000
fmla_scalar_d_acc:
    .quad 0xbff0000000000000
fmla_scalar_d_src:
    .quad 0x4004000000000000
fmla_scalar_d_vec:
    .quad 0x3fe0000000000000, 0x4010000000000000
fmla_vec_s_acc:
    .word 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000
fmla_vec_s_src:
    .word 0x3f800000, 0x40000000, 0x40400000, 0x40800000
fmla_vec_s_vec:
    .word 0x3f000000, 0x40000000, 0x40400000, 0x40800000
fmla_vec_d_acc:
    .quad 0x3ff0000000000000, 0xbff0000000000000
fmla_vec_d_src:
    .quad 0x4000000000000000, 0x4010000000000000
fmla_vec_d_vec:
    .quad 0x3ff0000000000000, 0x3fd0000000000000

fmls_scalar_s_acc:
    .word 0x41200000
fmls_scalar_s_src:
    .word 0x3fc00000
fmls_scalar_s_vec:
    .word 0x3f800000, 0x40000000, 0x40400000, 0x40800000
fmls_scalar_d_acc:
    .quad 0xbff0000000000000
fmls_scalar_d_src:
    .quad 0x4004000000000000
fmls_scalar_d_vec:
    .quad 0x4010000000000000, 0x3fe0000000000000
fmls_vec_s_acc:
    .word 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000
fmls_vec_s_src:
    .word 0x3f800000, 0x40000000, 0x40400000, 0x40800000
fmls_vec_s_vec:
    .word 0x3f000000, 0x40000000, 0x3f000000, 0x40800000
fmls_vec_d_acc:
    .quad 0x3ff0000000000000, 0xbff0000000000000
fmls_vec_d_src:
    .quad 0x4000000000000000, 0x4010000000000000
fmls_vec_d_vec:
    .quad 0x3fd0000000000000, 0x3ff0000000000000

exp_fmul_scalar_s:
    .word 0xc0400000
exp_fmul_scalar_d:
    .quad 0xc000000000000000
exp_fmul_vec_s:
    .word 0xc0000000, 0xc0800000, 0xc0c00000, 0xc1000000
exp_fmul_vec_d:
    .quad 0x4018000000000000, 0xc020000000000000

exp_fmla_scalar_s:
    .word 0x41500000
exp_fmla_scalar_d:
    .quad 0x4022000000000000
exp_fmla_vec_s:
    .word 0x3fc00000, 0x40000000, 0x40200000, 0x40400000
exp_fmla_vec_d:
    .quad 0x3ff8000000000000, 0x0000000000000000

exp_fmls_scalar_s:
    .word 0x40e00000
exp_fmls_scalar_d:
    .quad 0xc026000000000000
exp_fmls_vec_s:
    .word 0x3f000000, 0x00000000, 0xbf000000, 0xbf800000
exp_fmls_vec_d:
    .quad 0x3fe0000000000000, 0xc000000000000000

    .balign 16
dst:
    .space 144
