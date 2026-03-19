    .text
    .global _start
_start:
    movz x20, #0
    movk x20, #0x0900, lsl #16
    adr  x21, dst
    movz x25, #0x0030, lsl #16
    msr  cpacr_el1, x25
    isb

    adr  x22, add_s_a
    ldr  q1, [x22]
    adr  x22, add_s_b
    ldr  q2, [x22]
    fadd v0.4s, v1.4s, v2.4s
    str  q0, [x21]
    mov  x0, x21
    adr  x1, exp_add_s
    bl   check16

    fsub v3.4s, v1.4s, v2.4s
    str  q3, [x21, #16]
    add  x0, x21, #16
    adr  x1, exp_sub_s
    bl   check16

    fmul v4.4s, v1.4s, v2.4s
    str  q4, [x21, #32]
    add  x0, x21, #32
    adr  x1, exp_mul_s
    bl   check16

    adr  x22, div_s_a
    ldr  q5, [x22]
    adr  x22, div_s_b
    ldr  q6, [x22]
    fdiv v7.4s, v5.4s, v6.4s
    str  q7, [x21, #48]
    add  x0, x21, #48
    adr  x1, exp_div_s
    bl   check16

    fmax v8.4s, v1.4s, v2.4s
    str  q8, [x21, #64]
    add  x0, x21, #64
    adr  x1, exp_max_s
    bl   check16

    fmin v9.4s, v1.4s, v2.4s
    str  q9, [x21, #80]
    add  x0, x21, #80
    adr  x1, exp_min_s
    bl   check16

    adr  x22, add_d_a
    ldr  q10, [x22]
    adr  x22, add_d_b
    ldr  q11, [x22]
    fadd v12.2d, v10.2d, v11.2d
    str  q12, [x21, #96]
    add  x0, x21, #96
    adr  x1, exp_add_d
    bl   check16

    fsub v13.2d, v10.2d, v11.2d
    str  q13, [x21, #112]
    add  x0, x21, #112
    adr  x1, exp_sub_d
    bl   check16

    fmul v14.2d, v10.2d, v11.2d
    str  q14, [x21, #128]
    add  x0, x21, #128
    adr  x1, exp_mul_d
    bl   check16

    adr  x22, div_d_a
    ldr  q15, [x22]
    adr  x22, div_d_b
    ldr  q16, [x22]
    fdiv v17.2d, v15.2d, v16.2d
    str  q17, [x21, #144]
    add  x0, x21, #144
    adr  x1, exp_div_d
    bl   check16

    fcmeq v18.4s, v1.4s, v1.4s
    str  q18, [x21, #160]
    add  x0, x21, #160
    adr  x1, exp_eq_self_s
    bl   check16

    adr  x22, cmp_s_a
    ldr  q19, [x22]
    adr  x22, cmp_s_b
    ldr  q20, [x22]
    fcmeq v21.4s, v19.4s, v20.4s
    str  q21, [x21, #176]
    add  x0, x21, #176
    adr  x1, exp_fcmeq_reg
    bl   check16

    fcmge v22.4s, v19.4s, v20.4s
    str  q22, [x21, #192]
    add  x0, x21, #192
    adr  x1, exp_fcmge_reg
    bl   check16

    fcmgt v23.4s, v19.4s, v20.4s
    str  q23, [x21, #208]
    add  x0, x21, #208
    adr  x1, exp_fcmgt_reg
    bl   check16

    adr  x22, zero_cmp_s
    ldr  q24, [x22]
    fcmeq v25.4s, v24.4s, #0.0
    str  q25, [x21, #224]
    add  x0, x21, #224
    adr  x1, exp_fcmeq_zero
    bl   check16

    fcmge v26.4s, v24.4s, #0.0
    str  q26, [x21, #240]
    add  x0, x21, #240
    adr  x1, exp_fcmge_zero
    bl   check16

    fcmgt v27.4s, v24.4s, #0.0
    str  q27, [x21, #256]
    add  x0, x21, #256
    adr  x1, exp_fcmgt_zero
    bl   check16

    fcmle v28.4s, v24.4s, #0.0
    str  q28, [x21, #272]
    add  x0, x21, #272
    adr  x1, exp_fcmle_zero
    bl   check16

    adr  x22, zero_cmp_d
    ldr  q29, [x22]
    fcmlt v30.2d, v29.2d, #0.0
    str  q30, [x21, #288]
    add  x0, x21, #288
    adr  x1, exp_fcmlt_zero_d
    bl   check16

    movz x0, #'V'
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
add_s_a:
    .word 0x3fc00000, 0xc0000000, 0x41240000, 0xbf000000
add_s_b:
    .word 0x40100000, 0x40800000, 0xbe800000, 0xbfc00000
exp_add_s:
    .word 0x40700000, 0x40000000, 0x41200000, 0xc0000000
exp_sub_s:
    .word 0xbf400000, 0xc0c00000, 0x41280000, 0x3f800000
exp_mul_s:
    .word 0x40580000, 0xc1000000, 0xc0240000, 0x3f400000

div_s_a:
    .word 0x41100000, 0xc1000000, 0x40f00000, 0xc0c00000
div_s_b:
    .word 0x40400000, 0x40000000, 0x40200000, 0xc0000000
exp_div_s:
    .word 0x40400000, 0xc0800000, 0x40400000, 0x40400000
exp_max_s:
    .word 0x40100000, 0x40800000, 0x41240000, 0xbf000000
exp_min_s:
    .word 0x3fc00000, 0xc0000000, 0xbe800000, 0xbfc00000

add_d_a:
    .quad 0x3ff8000000000000, 0xc020000000000000
add_d_b:
    .quad 0x3fe0000000000000, 0x4000000000000000
exp_add_d:
    .quad 0x4000000000000000, 0xc018000000000000
exp_sub_d:
    .quad 0x3ff0000000000000, 0xc024000000000000
exp_mul_d:
    .quad 0x3fe8000000000000, 0xc030000000000000

div_d_a:
    .quad 0x4018000000000000, 0xc020000000000000
div_d_b:
    .quad 0x4000000000000000, 0x4000000000000000
exp_div_d:
    .quad 0x4008000000000000, 0xc010000000000000

exp_eq_self_s:
    .quad 0xffffffffffffffff, 0xffffffffffffffff
cmp_s_a:
    .word 0x3f800000, 0xc0000000, 0x40400000, 0xc0800000
cmp_s_b:
    .word 0x3f800000, 0xc0400000, 0x40800000, 0xc0800000
exp_fcmeq_reg:
    .quad 0x00000000ffffffff, 0xffffffff00000000
exp_fcmge_reg:
    .quad 0xffffffffffffffff, 0xffffffff00000000
exp_fcmgt_reg:
    .quad 0xffffffff00000000, 0x0000000000000000

zero_cmp_s:
    .word 0x3f800000, 0x00000000, 0xbf800000, 0x40000000
exp_fcmeq_zero:
    .quad 0xffffffff00000000, 0x0000000000000000
exp_fcmge_zero:
    .quad 0xffffffffffffffff, 0xffffffff00000000
exp_fcmgt_zero:
    .quad 0x00000000ffffffff, 0xffffffff00000000
exp_fcmle_zero:
    .quad 0xffffffff00000000, 0x00000000ffffffff

zero_cmp_d:
    .quad 0x3ff0000000000000, 0xbff0000000000000
exp_fcmlt_zero_d:
    .quad 0x0000000000000000, 0xffffffffffffffff

dst:
    .space 304
