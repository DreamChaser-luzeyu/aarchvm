    .text
    .global _start
_start:
    movz x20, #0
    movk x20, #0x0900, lsl #16
    movz x25, #0x0030, lsl #16
    msr  cpacr_el1, x25
    isb

    movz x9, #0x0800
    lsl  x9, x9, #16               // FPSR.QC

    adr  x21, dst

    adr  x22, ssubl_a
    ldr  d1, [x22]
    adr  x22, ssubl_b
    ldr  d2, [x22]
    ssubl v0.8h, v1.8b, v2.8b
    str  q0, [x21]
    mov  x0, x21
    adr  x1, exp_ssubl
    bl   check16

    adr  x22, usubl2_a
    ldr  q4, [x22]
    adr  x22, usubl2_b
    ldr  q5, [x22]
    usubl2 v3.4s, v4.8h, v5.8h
    str  q3, [x21, #16]
    add  x0, x21, #16
    adr  x1, exp_usubl2
    bl   check16

    adr  x22, ssubw_a
    ldr  q7, [x22]
    adr  x22, ssubw_b
    ldr  d8, [x22]
    ssubw v6.8h, v7.8h, v8.8b
    str  q6, [x21, #32]
    add  x0, x21, #32
    adr  x1, exp_ssubw
    bl   check16

    adr  x22, usubw2_a
    ldr  q10, [x22]
    adr  x22, usubw2_b
    ldr  q11, [x22]
    usubw2 v9.2d, v10.2d, v11.4s
    str  q9, [x21, #48]
    add  x0, x21, #48
    adr  x1, exp_usubw2
    bl   check16

    msr  fpsr, xzr
    adr  x22, sqsub_vec_a
    ldr  q13, [x22]
    adr  x22, sqsub_vec_b
    ldr  q14, [x22]
    sqsub v12.2d, v13.2d, v14.2d
    str  q12, [x21, #64]
    add  x0, x21, #64
    adr  x1, exp_sqsub_vec
    bl   check16
    mrs  x0, fpsr
    tst  x0, x9
    b.eq fail

    msr  fpsr, xzr
    adr  x22, uqsub_vec_a
    ldr  q16, [x22]
    adr  x22, uqsub_vec_b
    ldr  q17, [x22]
    uqsub v15.8h, v16.8h, v17.8h
    str  q15, [x21, #80]
    add  x0, x21, #80
    adr  x1, exp_uqsub_vec
    bl   check16
    mrs  x0, fpsr
    tst  x0, x9
    b.eq fail

    msr  fpsr, xzr
    adr  x22, sqadd_scalar_a
    ldr  d19, [x22]
    adr  x22, sqadd_scalar_b
    ldr  d20, [x22]
    sqadd d18, d19, d20
    fmov x0, d18
    ldr  x1, =0x7fffffffffffffff
    cmp  x0, x1
    b.ne fail
    mrs  x0, fpsr
    tst  x0, x9
    b.eq fail

    msr  fpsr, xzr
    adr  x22, uqadd_scalar_a
    ldr  d22, [x22]
    adr  x22, uqadd_scalar_b
    ldr  d23, [x22]
    uqadd h21, h22, h23
    umov w0, v21.h[0]
    cmp  w0, #3
    b.ne fail
    mrs  x0, fpsr
    tst  x0, x9
    b.ne fail

    msr  fpsr, xzr
    adr  x22, sqsub_scalar_a
    ldr  s25, [x22]
    adr  x22, sqsub_scalar_b
    ldr  s26, [x22]
    sqsub s24, s25, s26
    fmov w0, s24
    ldr  w1, =0x80000000
    cmp  w0, w1
    b.ne fail
    mrs  x0, fpsr
    tst  x0, x9
    b.eq fail

    msr  fpsr, xzr
    adr  x22, uqsub_scalar_a
    ldr  d28, [x22]
    adr  x22, uqsub_scalar_b
    ldr  d29, [x22]
    uqsub d27, d28, d29
    fmov x0, d27
    cbnz x0, fail
    mrs  x0, fpsr
    tst  x0, x9
    b.eq fail

    movz x0, #'S'
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
ssubl_a:
    .byte 0x80, 0x7f, 0xfe, 0x05, 0x32, 0xce, 0x01, 0xff
ssubl_b:
    .byte 0x01, 0xff, 0x02, 0xfb, 0xec, 0x14, 0x02, 0x80
exp_ssubl:
    .hword 0xff7f, 0x0080, 0xfffc, 0x000a, 0x0046, 0xffba, 0xffff, 0x007f

    .balign 16
usubl2_a:
    .hword 0, 0, 0, 0, 1, 0x0100, 0x8000, 0xffff
usubl2_b:
    .hword 0, 0, 0, 0, 2, 1, 0x7fff, 1
exp_usubl2:
    .word 0xffffffff, 0x000000ff, 0x00000001, 0x0000fffe

    .balign 16
ssubw_a:
    .hword 100, -100, 0x7fff, -32768, 50, -50, 300, -300
ssubw_b:
    .byte 1, -2, 127, -128, -100, 100, -1, 1
exp_ssubw:
    .hword 99, -98, 0x7f80, 0x8080, 150, -150, 301, -301

    .balign 16
usubw2_a:
    .quad 5, 0x0000000100000000
usubw2_b:
    .word 0, 0, 10, 0xffffffff
exp_usubw2:
    .quad 0xfffffffffffffffb, 1

    .balign 16
sqsub_vec_a:
    .quad 0x8000000000000000, 0x7fffffffffffffff
sqsub_vec_b:
    .quad 1, 0xffffffffffffffff
exp_sqsub_vec:
    .quad 0x8000000000000000, 0x7fffffffffffffff

    .balign 16
uqsub_vec_a:
    .hword 0, 1, 100, 0xffff, 0x1234, 5, 0x8000, 0x10
uqsub_vec_b:
    .hword 1, 2, 50, 1, 0x0234, 10, 0x7fff, 0x10
exp_uqsub_vec:
    .hword 0, 0, 50, 0xfffe, 0x1000, 0, 1, 0

    .balign 8
sqadd_scalar_a:
    .quad 0x7fffffffffffffff
sqadd_scalar_b:
    .quad 1

    .balign 8
uqadd_scalar_a:
    .quad 1
uqadd_scalar_b:
    .quad 2

    .balign 4
sqsub_scalar_a:
    .word 0x80000000
sqsub_scalar_b:
    .word 1

    .balign 8
uqsub_scalar_a:
    .quad 5
uqsub_scalar_b:
    .quad 10

dst:
    .space 96
