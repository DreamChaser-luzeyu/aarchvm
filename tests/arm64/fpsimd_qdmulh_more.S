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
    adr  x0, vec_h_a
    ldr  d1, [x0]
    adr  x0, vec_h_b
    ldr  d2, [x0]
    sqdmulh v0.4h, v1.4h, v2.4h
    str  d0, [x21]
    mov  x0, x21
    adr  x1, exp_vec_sqdmulh_h
    bl   check8
    mrs  x2, fpsr
    tst  x2, x9
    b.ne fail

    msr  fpsr, xzr
    adr  x0, vec_h_a
    ldr  d4, [x0]
    adr  x0, vec_h_b
    ldr  d5, [x0]
    sqrdmulh v3.4h, v4.4h, v5.4h
    str  d3, [x21, #16]
    add  x0, x21, #16
    adr  x1, exp_vec_sqrdmulh_h
    bl   check8
    mrs  x2, fpsr
    tst  x2, x9
    b.ne fail

    msr  fpsr, xzr
    adr  x0, vec_s_a
    ldr  q7, [x0]
    adr  x0, vec_s_b
    ldr  q8, [x0]
    sqdmulh v6.4s, v7.4s, v8.4s
    str  q6, [x21, #32]
    add  x0, x21, #32
    adr  x1, exp_vec_sqdmulh_s
    bl   check16
    mrs  x2, fpsr
    tst  x2, x9
    b.ne fail

    msr  fpsr, xzr
    adr  x0, vec_s_a
    ldr  q10, [x0]
    adr  x0, vec_s_b
    ldr  q11, [x0]
    sqrdmulh v9.4s, v10.4s, v11.4s
    str  q9, [x21, #48]
    add  x0, x21, #48
    adr  x1, exp_vec_sqrdmulh_s
    bl   check16
    mrs  x2, fpsr
    tst  x2, x9
    b.ne fail

    msr  fpsr, xzr
    adr  x0, vec_sat_h_a
    ldr  d13, [x0]
    adr  x0, vec_sat_h_b
    ldr  d14, [x0]
    sqdmulh v12.4h, v13.4h, v14.4h
    str  d12, [x21, #64]
    add  x0, x21, #64
    adr  x1, exp_vec_sat_h
    bl   check8
    mrs  x2, fpsr
    tst  x2, x9
    b.eq fail

    msr  fpsr, xzr
    adr  x0, scalar_h_a
    ldr  d20, [x0]
    adr  x0, scalar_h_b
    ldr  d21, [x0]
    sqdmulh h22, h20, h21
    umov w3, v22.h[0]
    adr  x0, exp_scalar_sqdmulh_h
    ldrh w4, [x0]
    cmp  w3, w4
    b.ne fail
    mrs  x2, fpsr
    tst  x2, x9
    b.ne fail

    msr  fpsr, xzr
    adr  x0, scalar_h_a
    ldr  d24, [x0]
    adr  x0, scalar_h_b
    ldr  d25, [x0]
    sqrdmulh h23, h24, h25
    umov w3, v23.h[0]
    adr  x0, exp_scalar_sqrdmulh_h
    ldrh w4, [x0]
    cmp  w3, w4
    b.ne fail
    mrs  x2, fpsr
    tst  x2, x9
    b.ne fail

    msr  fpsr, xzr
    adr  x0, scalar_sat_s_a
    ldr  d26, [x0]
    adr  x0, scalar_sat_s_b
    ldr  d27, [x0]
    sqdmulh s28, s26, s27
    umov w3, v28.s[0]
    adr  x0, exp_scalar_sat_s
    ldr  w4, [x0]
    cmp  w3, w4
    b.ne fail
    mrs  x2, fpsr
    tst  x2, x9
    b.eq fail

    movz x0, #'Q'
    str  x0, [x20]
    movz x0, #'D'
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
vec_h_a:
    .hword 0x4000, 0x4000, 0x8000, 0x1234
vec_h_b:
    .hword 0x4001, 0x4000, 0x7fff, 0xf000
exp_vec_sqdmulh_h:
    .hword 0x2000, 0x2000, 0x8001, 0xfdb9
exp_vec_sqrdmulh_h:
    .hword 0x2001, 0x2000, 0x8001, 0xfdba

    .balign 16
vec_s_a:
    .word 0x40000000, 0x40000000, 0x80000000, 0x12345678
vec_s_b:
    .word 0x40000001, 0x40000000, 0x7fffffff, 0xf0000000
exp_vec_sqdmulh_s:
    .word 0x20000000, 0x20000000, 0x80000001, 0xfdb97531
exp_vec_sqrdmulh_s:
    .word 0x20000001, 0x20000000, 0x80000001, 0xfdb97531

    .balign 8
vec_sat_h_a:
    .hword 0x8000, 0x4000, 0x0000, 0x0000
vec_sat_h_b:
    .hword 0x8000, 0x4000, 0x0000, 0x0000
exp_vec_sat_h:
    .hword 0x7fff, 0x2000, 0x0000, 0x0000

    .balign 8
scalar_h_a:
    .hword 0x4000, 0, 0, 0
scalar_h_b:
    .hword 0x4001, 0, 0, 0
exp_scalar_sqdmulh_h:
    .hword 0x2000
exp_scalar_sqrdmulh_h:
    .hword 0x2001

    .balign 8
scalar_sat_s_a:
    .word 0x80000000, 0
scalar_sat_s_b:
    .word 0x80000000, 0
exp_scalar_sat_s:
    .word 0x7fffffff

tmp:
    .space 80
