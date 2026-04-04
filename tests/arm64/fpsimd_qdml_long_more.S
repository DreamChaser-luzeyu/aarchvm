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
    adr  x0, vec4h_a
    ldr  d1, [x0]
    adr  x0, vec4h_b
    ldr  d2, [x0]
    sqdmull v0.4s, v1.4h, v2.4h
    str  q0, [x21]
    mov  x0, x21
    adr  x1, exp_vec_sqdmull
    bl   check16
    mrs  x2, fpsr
    tst  x2, x9
    b.ne fail

    msr  fpsr, xzr
    adr  x0, vec8h_upper_a
    ldr  q4, [x0]
    adr  x0, vec8h_upper_b
    ldr  q5, [x0]
    sqdmull2 v3.4s, v4.8h, v5.8h
    str  q3, [x21, #16]
    add  x0, x21, #16
    adr  x1, exp_vec_sqdmull2
    bl   check16
    mrs  x2, fpsr
    tst  x2, x9
    b.ne fail

    msr  fpsr, xzr
    adr  x0, vec_sat_a
    ldr  d7, [x0]
    adr  x0, vec_sat_b
    ldr  d8, [x0]
    sqdmull v6.4s, v7.4h, v8.4h
    str  q6, [x21, #32]
    add  x0, x21, #32
    adr  x1, exp_vec_sqdmull_sat
    bl   check16
    mrs  x2, fpsr
    tst  x2, x9
    b.eq fail

    msr  fpsr, xzr
    adr  x0, acc_small
    ldr  q10, [x0]
    adr  x0, vec4h_a
    ldr  d11, [x0]
    adr  x0, vec4h_b
    ldr  d12, [x0]
    sqdmlal v10.4s, v11.4h, v12.4h
    str  q10, [x21, #48]
    add  x0, x21, #48
    adr  x1, exp_vec_sqdmlal
    bl   check16
    mrs  x2, fpsr
    tst  x2, x9
    b.ne fail

    msr  fpsr, xzr
    adr  x0, acc_sat2
    ldr  q14, [x0]
    adr  x0, vec8h_sat_a
    ldr  q15, [x0]
    adr  x0, vec8h_sat_b
    ldr  q16, [x0]
    sqdmlal2 v14.4s, v15.8h, v16.8h
    str  q14, [x21, #64]
    add  x0, x21, #64
    adr  x1, exp_vec_sqdmlal2_sat
    bl   check16
    mrs  x2, fpsr
    tst  x2, x9
    b.eq fail

    msr  fpsr, xzr
    adr  x0, acc_zero
    ldr  q18, [x0]
    adr  x0, vec4h_a
    ldr  d19, [x0]
    adr  x0, vec4h_b
    ldr  d20, [x0]
    sqdmlsl v18.4s, v19.4h, v20.4h
    str  q18, [x21, #80]
    add  x0, x21, #80
    adr  x1, exp_vec_sqdmlsl
    bl   check16
    mrs  x2, fpsr
    tst  x2, x9
    b.ne fail

    msr  fpsr, xzr
    adr  x0, scalar_h_a
    ldr  d22, [x0]
    adr  x0, scalar_h_b
    ldr  d23, [x0]
    sqdmull s21, h22, h23
    umov w3, v21.s[0]
    adr  x0, exp_scalar_sqdmull_s
    ldr  w4, [x0]
    cmp  w3, w4
    b.ne fail
    mrs  x2, fpsr
    tst  x2, x9
    b.ne fail

    msr  fpsr, xzr
    adr  x0, scalar_sat_s_a
    ldr  d24, [x0]
    adr  x0, scalar_sat_s_b
    ldr  d25, [x0]
    sqdmull d26, s24, s25
    fmov x3, d26
    adr  x0, exp_scalar_sqdmull_d_sat
    ldr  x4, [x0]
    cmp  x3, x4
    b.ne fail
    mrs  x2, fpsr
    tst  x2, x9
    b.eq fail

    msr  fpsr, xzr
    adr  x0, scalar_acc_s
    ldr  d27, [x0]
    adr  x0, scalar_mul_h_a
    ldr  d28, [x0]
    adr  x0, scalar_mul_h_b
    ldr  d29, [x0]
    sqdmlal s27, h28, h29
    umov w3, v27.s[0]
    adr  x0, exp_scalar_sqdmlal_s
    ldr  w4, [x0]
    cmp  w3, w4
    b.ne fail
    mrs  x2, fpsr
    tst  x2, x9
    b.ne fail

    msr  fpsr, xzr
    adr  x0, scalar_acc_d
    ldr  d30, [x0]
    adr  x0, scalar_mul_s_a
    ldr  d31, [x0]
    adr  x0, scalar_mul_s_b
    ldr  d17, [x0]
    sqdmlsl d30, s31, s17
    fmov x3, d30
    adr  x0, exp_scalar_sqdmlsl_d
    ldr  x4, [x0]
    cmp  x3, x4
    b.ne fail
    mrs  x2, fpsr
    tst  x2, x9
    b.ne fail

    movz x0, #'Q'
    str  x0, [x20]
    movz x0, #'L'
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
vec4h_a:
    .hword 0x4000, 0x4001, 0x1234, 0x8000
vec4h_b:
    .hword 0x4001, 0x4000, 0xf000, 0x7fff

    .balign 16
exp_vec_sqdmull:
    .word 0x20008000, 0x20008000, 0xfdb98000, 0x80010000

    .balign 16
vec8h_upper_a:
    .hword 0x0001, 0x0002, 0x0003, 0x0004, 0x1000, 0x2000, 0x3000, 0x4000
vec8h_upper_b:
    .hword 0x0005, 0x0006, 0x0007, 0x0008, 0x0002, 0xfffe, 0x7fff, 0x8000
exp_vec_sqdmull2:
    .word 0x00004000, 0xffff8000, 0x2fffa000, 0xc0000000

    .balign 8
vec_sat_a:
    .hword 0x8000, 0x4000, 0x7fff, 0x0001
vec_sat_b:
    .hword 0x8000, 0x4000, 0x7fff, 0xffff

    .balign 16
exp_vec_sqdmull_sat:
    .word 0x7fffffff, 0x20000000, 0x7ffe0002, 0xfffffffe

    .balign 16
acc_small:
    .word 0x00000001, 0x00000002, 0x00000003, 0x00000004
exp_vec_sqdmlal:
    .word 0x20008001, 0x20008002, 0xfdb98003, 0x80010004

    .balign 16
vec8h_sat_a:
    .hword 0x0000, 0x0000, 0x0000, 0x0000, 0x8000, 0x4000, 0x7fff, 0x0001
vec8h_sat_b:
    .hword 0x0000, 0x0000, 0x0000, 0x0000, 0x8000, 0x4000, 0x7fff, 0xffff
acc_sat2:
    .word 0x7ffff000, 0x10000000, 0x80001000, 0xffffffff
exp_vec_sqdmlal2_sat:
    .word 0x7fffffff, 0x30000000, 0xfffe1002, 0xfffffffd

    .balign 16
acc_zero:
    .word 0x00000000, 0x00000000, 0x00000000, 0x00000000
exp_vec_sqdmlsl:
    .word 0xdfff8000, 0xdfff8000, 0x02468000, 0x7fff0000

    .balign 8
scalar_h_a:
    .hword 0x4000, 0, 0, 0
scalar_h_b:
    .hword 0x4001, 0, 0, 0
exp_scalar_sqdmull_s:
    .word 0x20008000

    .balign 8
scalar_sat_s_a:
    .word 0x80000000, 0
scalar_sat_s_b:
    .word 0x80000000, 0
exp_scalar_sqdmull_d_sat:
    .xword 0x7fffffffffffffff

    .balign 8
scalar_acc_s:
    .word 0x10000000, 0
scalar_mul_h_a:
    .hword 0x1234, 0, 0, 0
scalar_mul_h_b:
    .hword 0x4001, 0, 0, 0
exp_scalar_sqdmlal_s:
    .word 0x191a2468

    .balign 8
scalar_acc_d:
    .xword 0x1000000000000000
scalar_mul_s_a:
    .word 0x40000000, 0
scalar_mul_s_b:
    .word 0x40000001, 0
exp_scalar_sqdmlsl_d:
    .xword 0xefffffff80000000

tmp:
    .space 96
