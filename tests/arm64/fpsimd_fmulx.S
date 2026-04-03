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

    adr  x22, src_scalar_s
    ldr  s1, [x22]
    ldr  s2, [x22, #4]
    fmulx s0, s1, s2
    str  s0, [x21]
    movz x23, #'1'
    add  x0, x21, #0
    adr  x1, exp_scalar_s
    bl   check4
    mrs  x2, fpsr
    cbnz x2, fail

    adr  x22, src_scalar_d
    ldr  d3, [x22]
    ldr  d4, [x22, #8]
    fmulx d2, d3, d4
    str  d2, [x21, #8]
    movz x23, #'2'
    add  x0, x21, #8
    adr  x1, exp_scalar_d
    bl   check8

    adr  x22, src_vec_s_a
    ldr  q5, [x22]
    adr  x22, src_vec_s_b
    ldr  q6, [x22]
    fmulx v7.4s, v5.4s, v6.4s
    str  q7, [x21, #16]
    movz x23, #'3'
    add  x0, x21, #16
    adr  x1, exp_vec_s
    bl   check16

    adr  x22, src_vec_d_a
    ldr  q8, [x22]
    adr  x22, src_vec_d_b
    ldr  q9, [x22]
    fmulx v10.2d, v8.2d, v9.2d
    str  q10, [x21, #32]
    movz x23, #'4'
    add  x0, x21, #32
    adr  x1, exp_vec_d
    bl   check16

    adr  x22, src_lane_scalar_s
    ldr  s11, [x22]
    adr  x22, src_lane_vec_s
    ldr  q12, [x22]
    fmulx s13, s11, v12.s[3]
    str  s13, [x21, #48]
    movz x23, #'5'
    add  x0, x21, #48
    adr  x1, exp_lane_scalar_s
    bl   check4

    adr  x22, src_lane_scalar_d
    ldr  d14, [x22]
    adr  x22, src_lane_vec_d
    ldr  q15, [x22]
    fmulx d16, d14, v15.d[1]
    str  d16, [x21, #56]
    movz x23, #'6'
    add  x0, x21, #56
    adr  x1, exp_lane_scalar_d
    bl   check8

    adr  x22, src_lane_vec_mul_s_a
    ldr  q17, [x22]
    adr  x22, src_lane_vec_mul_s_b
    ldr  q18, [x22]
    fmulx v19.4s, v17.4s, v18.s[2]
    str  q19, [x21, #64]
    movz x23, #'7'
    add  x0, x21, #64
    adr  x1, exp_lane_vec_s
    bl   check16

    adr  x22, src_lane_vec_mul_d_a
    ldr  q20, [x22]
    adr  x22, src_lane_vec_mul_d_b
    ldr  q21, [x22]
    fmulx v22.2d, v20.2d, v21.d[1]
    str  q22, [x21, #80]
    movz x23, #'8'
    add  x0, x21, #80
    adr  x1, exp_lane_vec_d
    bl   check16

    movz x0, #'X'
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
src_scalar_s:
    .word 0x00000000, 0x7f800000
src_scalar_d:
    .quad 0x8000000000000000, 0x7ff0000000000000
src_vec_s_a:
    .word 0x00000000, 0x80000000, 0x3fc00000, 0xc0800000
src_vec_s_b:
    .word 0x7f800000, 0x7f800000, 0xc0000000, 0x3f000000
src_vec_d_a:
    .quad 0x0000000000000000, 0x3ff8000000000000
src_vec_d_b:
    .quad 0xfff0000000000000, 0xc000000000000000
src_lane_scalar_s:
    .word 0x00000000
src_lane_vec_s:
    .word 0x3f800000, 0x40000000, 0x40400000, 0xff800000
src_lane_scalar_d:
    .quad 0x8000000000000000
src_lane_vec_d:
    .quad 0x3ff0000000000000, 0xfff0000000000000
src_lane_vec_mul_s_a:
    .word 0x00000000, 0x80000000, 0x3fc00000, 0xc0000000
src_lane_vec_mul_s_b:
    .word 0x3f800000, 0x40000000, 0x7f800000, 0x40800000
src_lane_vec_mul_d_a:
    .quad 0x0000000000000000, 0x8000000000000000
src_lane_vec_mul_d_b:
    .quad 0x3ff0000000000000, 0x7ff0000000000000

exp_scalar_s:
    .word 0x40000000
exp_scalar_d:
    .quad 0xc000000000000000
exp_vec_s:
    .word 0x40000000, 0xc0000000, 0xc0400000, 0xc0000000
exp_vec_d:
    .quad 0xc000000000000000, 0xc008000000000000
exp_lane_scalar_s:
    .word 0xc0000000
exp_lane_scalar_d:
    .quad 0x4000000000000000
exp_lane_vec_s:
    .word 0x40000000, 0xc0000000, 0x7f800000, 0xff800000
exp_lane_vec_d:
    .quad 0x4000000000000000, 0xc000000000000000

    .balign 16
dst:
    .space 96
