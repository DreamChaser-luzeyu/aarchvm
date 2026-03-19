    .text
    .global _start
_start:
    movz x20, #0
    movk x20, #0x0900, lsl #16
    adr  x21, dst
    movz x25, #0x0030, lsl #16
    msr  cpacr_el1, x25
    isb

    msr  fpsr, xzr
    adr  x22, eq_qnan_a
    ldr  q0, [x22]
    adr  x22, eq_qnan_b
    ldr  q1, [x22]
    fcmeq v2.4s, v0.4s, v1.4s
    str  q2, [x21]
    mov  x0, x21
    adr  x1, exp_eq_qnan
    bl   check16
    mrs  x2, fpsr
    cbnz x2, fail1

    msr  fpsr, xzr
    adr  x22, ge_snan_a
    ldr  q3, [x22]
    adr  x22, ge_snan_b
    ldr  q4, [x22]
    fcmge v5.4s, v3.4s, v4.4s
    str  q5, [x21, #16]
    add  x0, x21, #16
    adr  x1, exp_ge_snan
    bl   check16
    mrs  x2, fpsr
    cmp  x2, #1
    b.ne fail2

    msr  fpsr, xzr
    adr  x22, gt_zero_snan
    ldr  q6, [x22]
    fcmgt v7.4s, v6.4s, #0.0
    str  q7, [x21, #32]
    add  x0, x21, #32
    adr  x1, exp_gt_zero_snan
    bl   check16
    mrs  x2, fpsr
    cmp  x2, #1
    b.ne fail3

    msr  fpsr, xzr
    adr  x22, abs_gt_snan_a
    ldr  q8, [x22]
    adr  x22, abs_gt_snan_b
    ldr  q9, [x22]
    facgt v10.2d, v8.2d, v9.2d
    str  q10, [x21, #48]
    add  x0, x21, #48
    adr  x1, exp_abs_gt_snan
    bl   check16
    mrs  x2, fpsr
    cmp  x2, #1
    b.ne fail4

    movz x0, #'G'
    str  x0, [x20]
    brk  #0

check16:
    ldr  x2, [x0]
    ldr  x3, [x1]
    cmp  x2, x3
    b.ne failm
    ldr  x2, [x0, #8]
    ldr  x3, [x1, #8]
    cmp  x2, x3
    b.ne failm
    ret

fail1:
    movz x0, #'1'
    b out
fail2:
    movz x0, #'2'
    b out
fail3:
    movz x0, #'3'
    b out
fail4:
    movz x0, #'4'
    b out
failm:
    movz x0, #'M'
out:
    str  x0, [x20]
    brk  #0

    .data
    .balign 16
eq_qnan_a:
    .word 0x7fc00000, 0x3f800000, 0x40000000, 0x80000000
eq_qnan_b:
    .word 0x3f800000, 0x3f800000, 0x40400000, 0x00000000
exp_eq_qnan:
    .quad 0xffffffff00000000, 0xffffffff00000000

ge_snan_a:
    .word 0x7f800001, 0x40000000, 0xc0400000, 0x40800000
ge_snan_b:
    .word 0x3f800000, 0x3f800000, 0xc0800000, 0x40a00000
exp_ge_snan:
    .quad 0xffffffff00000000, 0x00000000ffffffff

gt_zero_snan:
    .word 0x7f800001, 0x3f800000, 0xbf800000, 0x80000000
exp_gt_zero_snan:
    .quad 0xffffffff00000000, 0x0000000000000000

abs_gt_snan_a:
    .quad 0x7ff0000000000001, 0xc000000000000000
abs_gt_snan_b:
    .quad 0x3ff0000000000000, 0x3ff8000000000000
exp_abs_gt_snan:
    .quad 0x0000000000000000, 0xffffffffffffffff

dst:
    .space 64
