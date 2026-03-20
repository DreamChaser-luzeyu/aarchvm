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
    fmov s0, w0
    fcvtns w1, s0
    cbnz w1, fail
    mrs  x2, fpsr
    cmp  x2, #0x80
    b.ne fail

    movz x23, #'2'
    msr  fpsr, xzr
    ldr  w0, =0x80000001
    fmov s0, w0
    fcvtzu w1, s0
    cbnz w1, fail
    mrs  x2, fpsr
    cmp  x2, #0x80
    b.ne fail

    movz x23, #'3'
    msr  fpsr, xzr
    ldr  x0, =0x0000000000000001
    fmov d0, x0
    fcvtzs x1, d0
    cbnz x1, fail
    mrs  x2, fpsr
    cmp  x2, #0x80
    b.ne fail

    movz x23, #'4'
    msr  fpsr, xzr
    ldr  w0, =0x00000001
    fmov s0, w0
    fcvtzs s1, s0
    str  s1, [x21, #0]
    add  x0, x21, #0
    adr  x1, exp_zero_s
    bl   check4
    mrs  x2, fpsr
    cmp  x2, #0x80
    b.ne fail

    movz x23, #'5'
    msr  fpsr, xzr
    ldr  x0, =0x0000000000000001
    fmov d0, x0
    fcvtzs d1, d0
    str  d1, [x21, #8]
    add  x0, x21, #8
    adr  x1, exp_zero_d
    bl   check8
    mrs  x2, fpsr
    cmp  x2, #0x80
    b.ne fail

    movz x23, #'6'
    msr  fpsr, xzr
    adr  x22, vec_signed_s_src
    ldr  q0, [x22]
    fcvtzs v1.4s, v0.4s
    str  q1, [x21, #16]
    add  x0, x21, #16
    adr  x1, exp_vec_zero_4s
    bl   check16
    mrs  x2, fpsr
    cmp  x2, #0x80
    b.ne fail

    movz x23, #'7'
    msr  fpsr, xzr
    adr  x22, vec_unsigned_s_src
    ldr  q0, [x22]
    fcvtzu v1.4s, v0.4s
    str  q1, [x21, #32]
    add  x0, x21, #32
    adr  x1, exp_vec_zero_4s
    bl   check16
    mrs  x2, fpsr
    cmp  x2, #0x80
    b.ne fail

    movz x23, #'8'
    msr  fpsr, xzr
    adr  x22, vec_signed_d_src
    ldr  q0, [x22]
    fcvtzs v1.2d, v0.2d
    str  q1, [x21, #48]
    add  x0, x21, #48
    adr  x1, exp_vec_zero_2d
    bl   check16
    mrs  x2, fpsr
    cmp  x2, #0x80
    b.ne fail

    movz x23, #'9'
    msr  fpsr, xzr
    adr  x22, vec_unsigned_d_src
    ldr  q0, [x22]
    fcvtzu v1.2d, v0.2d
    str  q1, [x21, #64]
    add  x0, x21, #64
    adr  x1, exp_vec_zero_2d
    bl   check16
    mrs  x2, fpsr
    cmp  x2, #0x80
    b.ne fail

    movz x0, #'T'
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
    .space 80

exp_zero_s:
    .word 0x00000000

    .balign 8
exp_zero_d:
    .quad 0x0000000000000000

    .balign 16
vec_signed_s_src:
    .word 0x00000001, 0x80000001, 0x00000001, 0x80000001
vec_unsigned_s_src:
    .word 0x00000001, 0x80000001, 0x00000001, 0x80000001
exp_vec_zero_4s:
    .word 0x00000000, 0x00000000, 0x00000000, 0x00000000

vec_signed_d_src:
    .quad 0x0000000000000001, 0x8000000000000001
vec_unsigned_d_src:
    .quad 0x0000000000000001, 0x8000000000000001
exp_vec_zero_2d:
    .quad 0x0000000000000000, 0x0000000000000000
