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
    sqdmulh v0.4h, v1.4h, v2.h[0]
    str  d0, [x21]
    mov  x0, x21
    adr  x1, exp_vec4h
    bl   check8
    mrs  x2, fpsr
    tst  x2, x9
    b.ne fail

    msr  fpsr, xzr
    adr  x0, vec8h_src
    ldr  q16, [x0]
    adr  x0, vec_mid_rhs_h
    ldr  q15, [x0]
    sqrdmulh v17.8h, v16.8h, v15.h[7]
    str  q17, [x21, #16]
    add  x0, x21, #16
    adr  x1, exp_vec8h_round
    bl   check16
    mrs  x2, fpsr
    tst  x2, x9
    b.ne fail

    msr  fpsr, xzr
    adr  x0, vec4s_src
    ldr  q19, [x0]
    adr  x0, vec_hi_rhs_s
    ldr  q20, [x0]
    sqdmulh v21.4s, v19.4s, v20.s[3]
    str  q21, [x21, #32]
    add  x0, x21, #32
    adr  x1, exp_vec4s
    bl   check16
    mrs  x2, fpsr
    tst  x2, x9
    b.ne fail

    msr  fpsr, xzr
    adr  x0, scalar_h_src
    ldr  d22, [x0]
    adr  x0, vec_low_rhs
    ldr  d14, [x0]
    sqdmulh h24, h22, v14.h[0]
    umov w3, v24.h[0]
    adr  x0, exp_scalar_h
    ldrh w4, [x0]
    cmp  w3, w4
    b.ne fail
    mrs  x2, fpsr
    tst  x2, x9
    b.ne fail

    msr  fpsr, xzr
    adr  x0, scalar_s_src
    ldr  d25, [x0]
    adr  x0, scalar_hi_rhs_s
    ldr  q26, [x0]
    sqrdmulh s27, s25, v26.s[1]
    umov w3, v27.s[0]
    adr  x0, exp_scalar_s_round
    ldr  w4, [x0]
    cmp  w3, w4
    b.ne fail
    mrs  x2, fpsr
    tst  x2, x9
    b.ne fail

    msr  fpsr, xzr
    adr  x0, vec_sat_src
    ldr  q28, [x0]
    adr  x0, vec_sat_rhs_h
    ldr  q14, [x0]
    sqdmulh v30.8h, v28.8h, v14.h[7]
    str  q30, [x21, #48]
    add  x0, x21, #48
    adr  x1, exp_vec_sat
    bl   check16
    mrs  x2, fpsr
    tst  x2, x9
    b.eq fail

    movz x0, #'Q'
    str  x0, [x20]
    movz x0, #'I'
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
vec4h_src:
    .hword 0x4000, 0x4001, 0x1234, 0x8000
vec_low_rhs:
    .hword 0x4001, 0, 0, 0
exp_vec4h:
    .hword 0x2000, 0x2001, 0x091a, 0xbfff

    .balign 16
vec8h_src:
    .hword 0x4000, 0x4001, 0x7fff, 0x8000, 0x1234, 0xf000, 0x2000, 0x6000
vec_mid_rhs_h:
    .hword 0, 0, 0, 0, 0, 0, 0, 0x2001
exp_vec8h_round:
    .hword 0x1001, 0x1001, 0x2001, 0xdfff, 0x048d, 0xfc00, 0x0800, 0x1801

    .balign 16
vec4s_src:
    .word 0x40000000, 0x40000001, 0x12345678, 0x80000000
vec_hi_rhs_s:
    .word 0, 0, 0, 0x20000001
exp_vec4s:
    .word 0x10000000, 0x10000000, 0x048d159e, 0xdfffffff

    .balign 8
scalar_h_src:
    .hword 0x4000, 0, 0, 0
exp_scalar_h:
    .hword 0x2000

    .balign 16
scalar_s_src:
    .word 0x12345678, 0, 0, 0
scalar_hi_rhs_s:
    .word 0, 0x40000001, 0, 0
exp_scalar_s_round:
    .word 0x091a2b3c

    .balign 16
vec_sat_src:
    .hword 0x8000, 0x4000, 0x7fff, 0x0000, 0x0001, 0x0002, 0x0003, 0x0004
vec_sat_rhs_h:
    .hword 0, 0, 0, 0, 0, 0, 0, 0x8000
exp_vec_sat:
    .hword 0x7fff, 0xc000, 0x8001, 0x0000, 0xffff, 0xfffe, 0xfffd, 0xfffc

tmp:
    .space 64
