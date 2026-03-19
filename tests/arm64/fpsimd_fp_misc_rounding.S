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

    adr  x22, src_abs_s
    ldr  q1, [x22]
    fabs v0.4s, v1.4s
    str  q0, [x21]
    mov  x0, x21
    adr  x1, exp_abs_s
    bl   check16

    adr  x22, src_neg_d
    ldr  q3, [x22]
    fneg v2.2d, v3.2d
    str  q2, [x21, #16]
    add  x0, x21, #16
    adr  x1, exp_neg_d
    bl   check16

    adr  x22, src_sqrt_s
    ldr  q5, [x22]
    fsqrt v4.4s, v5.4s
    str  q4, [x21, #32]
    add  x0, x21, #32
    adr  x1, exp_sqrt_s
    bl   check16

    adr  x22, src_round_s
    ldr  q7, [x22]
    frintn v6.4s, v7.4s
    str  q6, [x21, #48]
    add  x0, x21, #48
    adr  x1, exp_frintn_s
    bl   check16

    adr  x22, src_round_d_pos
    ldr  q9, [x22]
    frintp v8.2d, v9.2d
    str  q8, [x21, #64]
    add  x0, x21, #64
    adr  x1, exp_frintp_d
    bl   check16

    adr  x22, src_round_s
    ldr  q11, [x22]
    frintm v10.4s, v11.4s
    str  q10, [x21, #80]
    add  x0, x21, #80
    adr  x1, exp_frintm_s
    bl   check16

    adr  x22, src_round_d_trunc
    ldr  q13, [x22]
    frintz v12.2d, v13.2d
    str  q12, [x21, #96]
    add  x0, x21, #96
    adr  x1, exp_frintz_d
    bl   check16

    adr  x22, src_round_s
    ldr  q15, [x22]
    frinta v14.4s, v15.4s
    str  q14, [x21, #112]
    add  x0, x21, #112
    adr  x1, exp_frinta_s
    bl   check16

    mov  x3, #(1 << 22)
    msr  fpcr, x3
    isb
    adr  x22, src_round_s_mode
    ldr  q17, [x22]
    frintx v16.4s, v17.4s
    str  q16, [x21, #128]
    add  x0, x21, #128
    adr  x1, exp_frintx_s
    bl   check16

    mov  x3, #(2 << 22)
    msr  fpcr, x3
    isb
    adr  x22, src_round_d_mode
    ldr  q19, [x22]
    frinti v18.2d, v19.2d
    str  q18, [x21, #144]
    add  x0, x21, #144
    adr  x1, exp_frinti_d
    bl   check16

    movz x0, #'V'
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
src_abs_s:
    .word 0xc0200000, 0x40500000, 0x80000000, 0xbfc00000
exp_abs_s:
    .word 0x40200000, 0x40500000, 0x00000000, 0x3fc00000

src_neg_d:
    .quad 0x4004000000000000, 0xc00a000000000000
exp_neg_d:
    .quad 0xc004000000000000, 0x400a000000000000

src_sqrt_s:
    .word 0x40800000, 0x41100000, 0x40100000, 0x3f800000
exp_sqrt_s:
    .word 0x40000000, 0x40400000, 0x3fc00000, 0x3f800000

src_round_s:
    .word 0x40200000, 0xc0200000, 0x40066666, 0xc0066666
exp_frintn_s:
    .word 0x40000000, 0xc0000000, 0x40000000, 0xc0000000
exp_frintm_s:
    .word 0x40000000, 0xc0400000, 0x40000000, 0xc0400000
exp_frinta_s:
    .word 0x40400000, 0xc0400000, 0x40000000, 0xc0000000

src_round_d_pos:
    .quad 0x4000cccccccccccd, 0xc000cccccccccccd
exp_frintp_d:
    .quad 0x4008000000000000, 0xc000000000000000

src_round_d_trunc:
    .quad 0x4007333333333333, 0xc007333333333333
exp_frintz_d:
    .quad 0x4000000000000000, 0xc000000000000000

src_round_s_mode:
    .word 0x40066666, 0xc0066666, 0x40200000, 0xc0200000
exp_frintx_s:
    .word 0x40400000, 0xc0000000, 0x40400000, 0xc0000000

src_round_d_mode:
    .quad 0x4007333333333333, 0xc000cccccccccccd
exp_frinti_d:
    .quad 0x4000000000000000, 0xc008000000000000

dst:
    .space 160
