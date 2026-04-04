    .text
    .global _start
_start:
    movz x20, #0x0000
    movk x20, #0x0900, lsl #16
    movz x25, #0x0030, lsl #16
    msr  cpacr_el1, x25
    isb

    movz x9, #0x0800
    lsl  x9, x9, #16               // FPSR.QC

    adr  x21, tmp

    msr  fpsr, xzr
    adr  x0, vec4h_src
    ldr  d1, [x0]
    adr  x0, vec_low_rhs
    ldr  d2, [x0]
    sqdmull v0.4s, v1.4h, v2.h[0]
    str  q0, [x21]
    mov  x0, x21
    adr  x1, exp_vec_sqdmull
    bl   check16
    mrs  x2, fpsr
    tst  x2, x9
    b.ne fail

    msr  fpsr, xzr
    adr  x0, vec8h_src
    ldr  q4, [x0]
    adr  x0, vec_hi_rhs_h
    ldr  q5, [x0]
    sqdmull2 v3.4s, v4.8h, v5.h[7]
    str  q3, [x21, #16]
    add  x0, x21, #16
    adr  x1, exp_vec_sqdmull2
    bl   check16
    mrs  x2, fpsr
    tst  x2, x9
    b.ne fail

    msr  fpsr, xzr
    adr  x0, acc_idx
    ldr  q6, [x0]
    adr  x0, vec4h_src
    ldr  d7, [x0]
    adr  x0, vec_low_rhs
    ldr  d8, [x0]
    sqdmlal v6.4s, v7.4h, v8.h[0]
    str  q6, [x21, #32]
    add  x0, x21, #32
    adr  x1, exp_vec_sqdmlal
    bl   check16
    mrs  x2, fpsr
    tst  x2, x9
    b.eq fail

    msr  fpsr, xzr
    adr  x0, acc_idx
    ldr  q10, [x0]
    adr  x0, vec8h_src
    ldr  q11, [x0]
    adr  x0, vec_hi_rhs_h
    ldr  q12, [x0]
    sqdmlsl2 v10.4s, v11.8h, v12.h[7]
    str  q10, [x21, #48]
    add  x0, x21, #48
    adr  x1, exp_vec_sqdmlsl2
    bl   check16
    mrs  x2, fpsr
    tst  x2, x9
    b.ne fail

    msr  fpsr, xzr
    adr  x0, vec_sat_src
    ldr  d13, [x0]
    adr  x0, vec_sat_rhs
    ldr  d14, [x0]
    sqdmull v15.4s, v13.4h, v14.h[0]
    str  q15, [x21, #64]
    add  x0, x21, #64
    adr  x1, exp_vec_sqdmull_sat
    bl   check16
    mrs  x2, fpsr
    tst  x2, x9
    b.eq fail

    msr  fpsr, xzr
    adr  x0, scalar_h_src
    ldr  d16, [x0]
    adr  x0, vec_low_rhs
    ldr  d2, [x0]
    sqdmull s17, h16, v2.h[0]
    umov w3, v17.s[0]
    adr  x0, exp_scalar_sqdmull_s
    ldr  w4, [x0]
    cmp  w3, w4
    b.ne fail
    mrs  x2, fpsr
    tst  x2, x9
    b.ne fail

    msr  fpsr, xzr
    adr  x0, scalar_acc_s
    ldr  d18, [x0]
    adr  x0, scalar_h_mul
    ldr  d19, [x0]
    adr  x0, vec_low_rhs
    ldr  d2, [x0]
    sqdmlal s18, h19, v2.h[0]
    umov w3, v18.s[0]
    adr  x0, exp_scalar_sqdmlal_s
    ldr  w4, [x0]
    cmp  w3, w4
    b.ne fail
    mrs  x2, fpsr
    tst  x2, x9
    b.ne fail

    msr  fpsr, xzr
    adr  x0, scalar_acc_d
    ldr  d20, [x0]
    adr  x0, scalar_s_src
    ldr  d21, [x0]
    adr  x0, scalar_hi_rhs_s
    ldr  q22, [x0]
    sqdmlsl d20, s21, v22.s[1]
    fmov x3, d20
    adr  x0, exp_scalar_sqdmlsl_d
    ldr  x4, [x0]
    cmp  x3, x4
    b.ne fail
    mrs  x2, fpsr
    tst  x2, x9
    b.ne fail

    movz x0, #'Q'
    str  x0, [x20]
    movz x0, #'X'
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
    .balign 8
vec4h_src:
    .hword 0x4000, 0x4001, 0x1234, 0x8000
vec_low_rhs:
    .hword 0x4001, 0, 0, 0

    .balign 16
exp_vec_sqdmull:
    .word 0x20008000, 0x20010002, 0x091a2468, 0xbfff0000

    .balign 16
vec8h_src:
    .hword 0x0001, 0x0002, 0x0003, 0x0004, 0x4000, 0x4001, 0x1234, 0x8000
vec_hi_rhs_h:
    .hword 0, 0, 0, 0, 0, 0, 0, 0x2001
exp_vec_sqdmull2:
    .word 0x10008000, 0x1000c002, 0x048d2468, 0xdfff0000

    .balign 16
acc_idx:
    .word 0x10000000, 0xffffffff, 0x7ffff000, 0x80001000
exp_vec_sqdmlal:
    .word 0x30008000, 0x20010001, 0x7fffffff, 0x80000000
exp_vec_sqdmlsl2:
    .word 0xffff8000, 0xefff3ffd, 0x7b72cb98, 0xa0011000

    .balign 8
vec_sat_src:
    .hword 0x8000, 0x4000, 0x7fff, 0x0001
vec_sat_rhs:
    .hword 0x8000, 0, 0, 0

    .balign 16
exp_vec_sqdmull_sat:
    .word 0x7fffffff, 0xc0000000, 0x80010000, 0xffff0000

    .balign 8
scalar_h_src:
    .hword 0x4000, 0, 0, 0
exp_scalar_sqdmull_s:
    .word 0x20008000

    .balign 8
scalar_acc_s:
    .word 0x10000000, 0
scalar_h_mul:
    .hword 0x1234, 0, 0, 0
exp_scalar_sqdmlal_s:
    .word 0x191a2468

    .balign 8
scalar_acc_d:
    .xword 0x1000000000000000
scalar_s_src:
    .word 0x40000000, 0
scalar_hi_rhs_s:
    .word 0, 0x40000001, 0, 0
exp_scalar_sqdmlsl_d:
    .xword 0xefffffff80000000

tmp:
    .space 80
