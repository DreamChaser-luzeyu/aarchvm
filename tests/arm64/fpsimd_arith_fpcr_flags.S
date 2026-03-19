    .text
    .global _start
_start:
    movz x20, #0
    movk x20, #0x0900, lsl #16
    movz x25, #0x0030, lsl #16
    msr  cpacr_el1, x25
    isb

    adr  x21, dst

    movz x0, #0x40, lsl #16 // FPCR.RMode = +Inf
    msr  fpcr, x0
    isb
    msr  fpsr, xzr
    adr  x22, add_s_a
    ldr  q0, [x22]
    adr  x22, add_s_b
    ldr  q1, [x22]
    fadd v2.4s, v0.4s, v1.4s
    str  q2, [x21]
    mov  x0, x21
    adr  x1, exp_add_s
    bl   check16
    mrs  x2, fpsr
    mov  x3, #0x10
    cmp  x2, x3
    b.ne fail1

    msr  fpcr, xzr
    isb
    msr  fpsr, xzr
    adr  x22, div_d_a
    ldr  q3, [x22]
    adr  x22, div_d_b
    ldr  q4, [x22]
    fdiv v5.2d, v3.2d, v4.2d
    str  q5, [x21, #16]
    add  x0, x21, #16
    adr  x1, exp_div_d
    bl   check16
    mrs  x2, fpsr
    mov  x3, #0x2
    cmp  x2, x3
    b.ne fail2

    msr  fpsr, xzr
    adr  x22, fmla_acc
    ldr  q6, [x22]
    adr  x22, fmla_lhs
    ldr  q7, [x22]
    adr  x22, fmla_rhs
    ldr  q8, [x22]
    fmla v6.4s, v7.4s, v8.4s
    str  q6, [x21, #32]
    add  x0, x21, #32
    adr  x1, exp_fmla
    bl   check16
    mrs  x2, fpsr
    cbnz x2, fail3

    movz x0, #'N'
    str  x0, [x20]
    movz x0, #'\n'
    str  x0, [x20]
    brk  #0

check16:
    ldr  x2, [x0]
    ldr  x3, [x1]
    cmp  x2, x3
    b.ne fail0
    ldr  x2, [x0, #8]
    ldr  x3, [x1, #8]
    cmp  x2, x3
    b.ne fail0
    ret

fail0:
    movz x0, #'0'
    b out
fail1:
    movz x0, #'1'
    b out
fail2:
    movz x0, #'2'
    b out
fail3:
    movz x0, #'3'
out:
    str  x0, [x20]
    brk  #0

    .data
    .balign 16
add_s_a:
    .word 0x3f800000, 0x00000000, 0x3f800000, 0x3f800000
add_s_b:
    .word 0x33800000, 0x00000000, 0x00000000, 0x00000000
exp_add_s:
    .word 0x3f800001, 0x00000000, 0x3f800000, 0x3f800000

div_d_a:
    .quad 0x3ff0000000000000, 0x3ff0000000000000
div_d_b:
    .quad 0x0000000000000000, 0x0000000000000000
exp_div_d:
    .quad 0x7ff0000000000000, 0x7ff0000000000000

fmla_acc:
    .word 0xbf800000, 0xbf800000, 0xbf800000, 0xbf800000
fmla_lhs:
    .word 0x3f800800, 0x3f800800, 0x3f800800, 0x3f800800
fmla_rhs:
    .word 0x3f7ff000, 0x3f7ff000, 0x3f7ff000, 0x3f7ff000
exp_fmla:
    .word 0xb3800000, 0xb3800000, 0xb3800000, 0xb3800000

    .balign 16
dst:
    .space 48
