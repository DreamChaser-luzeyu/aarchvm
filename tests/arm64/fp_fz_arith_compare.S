    .text
    .global _start
_start:
    movz x20, #0
    movk x20, #0x0900, lsl #16
    adr  x21, dst
    movz x25, #0x0030, lsl #16
    msr  cpacr_el1, x25
    isb

    mov  x24, #(1 << 24)         // FPCR.FZ
    msr  fpcr, x24

    movz x23, #'1'
    msr  fpsr, xzr
    ldr  w0, =0x00000001
    fmov s0, w0
    fmov s1, wzr
    fadd s2, s0, s1
    str  s2, [x21]
    add  x0, x21, #0
    adr  x1, exp_zero_s
    bl   check4
    mrs  x2, fpsr
    cmp  x2, #0x80
    b.ne fail

    movz x23, #'2'
    msr  fpsr, xzr
    ldr  x0, =0x0000000000000001
    fmov d0, x0
    fmov d1, xzr
    fadd d2, d0, d1
    str  d2, [x21, #8]
    add  x0, x21, #8
    adr  x1, exp_zero_d
    bl   check8
    mrs  x2, fpsr
    cmp  x2, #0x80
    b.ne fail

    movz x23, #'3'
    msr  fpsr, xzr
    ldr  w0, =0x00000001
    fmov s0, w0
    fcmp s0, #0.0
    bl   check_eq_flags
    mrs  x2, fpsr
    cmp  x2, #0x80
    b.ne fail

    movz x23, #'4'
    msr  fpsr, xzr
    ldr  w0, =0x00000001
    fmov s0, w0
    fcmeq s1, s0, #0.0
    str  s1, [x21, #16]
    add  x0, x21, #16
    adr  x1, exp_true_s
    bl   check4
    mrs  x2, fpsr
    cmp  x2, #0x80
    b.ne fail

    movz x23, #'5'
    msr  fpsr, xzr
    adr  x22, vec_fadd_src
    ldr  q0, [x22]
    adr  x22, vec_zero
    ldr  q1, [x22]
    fadd v2.4s, v0.4s, v1.4s
    str  q2, [x21, #32]
    add  x0, x21, #32
    adr  x1, exp_vec_fadd
    bl   check16
    mrs  x2, fpsr
    cmp  x2, #0x80
    b.ne fail

    movz x23, #'6'
    msr  fpsr, xzr
    adr  x22, vec_cmp_src
    ldr  q0, [x22]
    fcmeq v1.4s, v0.4s, #0.0
    str  q1, [x21, #48]
    add  x0, x21, #48
    adr  x1, exp_vec_cmp
    bl   check16
    mrs  x2, fpsr
    cmp  x2, #0x80
    b.ne fail

    movz x23, #'7'
    msr  fpsr, xzr
    ldr  w0, =0x00800000
    ldr  w1, =0x3f000000
    fmov s0, w0
    fmov s1, w1
    fmul s2, s0, s1
    str  s2, [x21, #64]
    add  x0, x21, #64
    adr  x1, exp_zero_s
    bl   check4
    mrs  x2, fpsr
    cmp  x2, #0x8
    b.ne fail

    movz x23, #'8'
    msr  fpsr, xzr
    ldr  x0, =0x0010000000000000
    ldr  x1, =0x3fe0000000000000
    fmov d0, x0
    fmov d1, x1
    fmul d2, d0, d1
    str  d2, [x21, #72]
    add  x0, x21, #72
    adr  x1, exp_zero_d
    bl   check8
    mrs  x2, fpsr
    cmp  x2, #0x8
    b.ne fail

    movz x0, #'F'
    str  x0, [x20]
    brk  #0

check_eq_flags:
    mrs  x3, nzcv
    ldr  x4, =0x60000000
    cmp  x3, x4
    b.ne fail_flags
    ret

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

fail_flags:
    movz x23, #'E'
fail:
    str  x23, [x20]
    brk  #0

    .data
    .balign 16
dst:
    .space 80

exp_zero_s:
    .word 0x00000000
exp_true_s:
    .word 0xffffffff
exp_zero_d:
    .quad 0x0000000000000000

vec_zero:
    .word 0x00000000, 0x00000000, 0x00000000, 0x00000000

vec_fadd_src:
    .word 0x00000001, 0x3f800000, 0x40000000, 0x40400000
exp_vec_fadd:
    .word 0x00000000, 0x3f800000, 0x40000000, 0x40400000

vec_cmp_src:
    .word 0x00000001, 0x00000000, 0x3f800000, 0x80000001
exp_vec_cmp:
    .word 0xffffffff, 0xffffffff, 0x00000000, 0xffffffff
