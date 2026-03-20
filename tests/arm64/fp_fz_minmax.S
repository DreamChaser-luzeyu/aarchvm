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
    isb

    movz x23, #'1'
    msr  fpsr, xzr
    ldr  w0, =0x00000001
    ldr  w1, =0xbf800000
    fmov s0, w0
    fmov s1, w1
    fmax s2, s0, s1
    str  s2, [x21, #0]
    add  x0, x21, #0
    adr  x1, exp_pos_zero_s
    bl   check4
    mrs  x2, fpsr
    cmp  x2, #0x80
    b.ne fail

    movz x23, #'2'
    msr  fpsr, xzr
    ldr  w0, =0x80000001
    ldr  w1, =0x3f800000
    fmov s0, w0
    fmov s1, w1
    fmin s2, s0, s1
    str  s2, [x21, #4]
    add  x0, x21, #4
    adr  x1, exp_neg_zero_s
    bl   check4
    mrs  x2, fpsr
    cmp  x2, #0x80
    b.ne fail

    movz x23, #'3'
    msr  fpsr, xzr
    ldr  w0, =0x7fc12345
    ldr  w1, =0x00000001
    fmov s0, w0
    fmov s1, w1
    fmaxnm s2, s0, s1
    str  s2, [x21, #8]
    add  x0, x21, #8
    adr  x1, exp_pos_zero_s
    bl   check4
    mrs  x2, fpsr
    cmp  x2, #0x80
    b.ne fail

    movz x23, #'4'
    msr  fpsr, xzr
    ldr  w0, =0x7fc12345
    ldr  w1, =0x80000001
    fmov s0, w0
    fmov s1, w1
    fminnm s2, s0, s1
    str  s2, [x21, #12]
    add  x0, x21, #12
    adr  x1, exp_neg_zero_s
    bl   check4
    mrs  x2, fpsr
    cmp  x2, #0x80
    b.ne fail

    movz x23, #'5'
    msr  fpsr, xzr
    ldr  x0, =0x0000000000000001
    ldr  x1, =0xbff0000000000000
    fmov d0, x0
    fmov d1, x1
    fmax d2, d0, d1
    str  d2, [x21, #16]
    add  x0, x21, #16
    adr  x1, exp_pos_zero_d
    bl   check8
    mrs  x2, fpsr
    cmp  x2, #0x80
    b.ne fail

    movz x23, #'6'
    msr  fpsr, xzr
    ldr  x0, =0x7ff8123456789abc
    ldr  x1, =0x8000000000000001
    fmov d0, x0
    fmov d1, x1
    fminnm d2, d0, d1
    str  d2, [x21, #24]
    add  x0, x21, #24
    adr  x1, exp_neg_zero_d
    bl   check8
    mrs  x2, fpsr
    cmp  x2, #0x80
    b.ne fail

    movz x23, #'7'
    msr  fpsr, xzr
    adr  x22, vec_fmin_src_lhs
    ldr  q0, [x22]
    adr  x22, vec_fmin_src_rhs
    ldr  q1, [x22]
    fmin v2.4s, v0.4s, v1.4s
    str  q2, [x21, #32]
    add  x0, x21, #32
    adr  x1, exp_vec_fmin
    bl   check16
    mrs  x2, fpsr
    cmp  x2, #0x80
    b.ne fail

    movz x23, #'8'
    msr  fpsr, xzr
    adr  x22, vec_fmaxnm_src_lhs
    ldr  q0, [x22]
    adr  x22, vec_fmaxnm_src_rhs
    ldr  q1, [x22]
    fmaxnm v2.4s, v0.4s, v1.4s
    str  q2, [x21, #48]
    add  x0, x21, #48
    adr  x1, exp_vec_fmaxnm
    bl   check16
    mrs  x2, fpsr
    cmp  x2, #0x80
    b.ne fail

    movz x0, #'W'
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
dst:
    .space 64

exp_pos_zero_s:
    .word 0x00000000
exp_neg_zero_s:
    .word 0x80000000
exp_pos_zero_d:
    .quad 0x0000000000000000
exp_neg_zero_d:
    .quad 0x8000000000000000

    .balign 16
vec_fmin_src_lhs:
    .word 0x00000001, 0x80000001, 0x3f800000, 0xbf800000
vec_fmin_src_rhs:
    .word 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000
exp_vec_fmin:
    .word 0x00000000, 0x80000000, 0x3f800000, 0xbf800000

    .balign 16
vec_fmaxnm_src_lhs:
    .word 0x00000001, 0x80000001, 0x7fc12345, 0x7fc12345
vec_fmaxnm_src_rhs:
    .word 0xbf800000, 0x3f800000, 0x00000001, 0x80000001
exp_vec_fmaxnm:
    .word 0x00000000, 0x3f800000, 0x00000000, 0x80000000
