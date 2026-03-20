    .text
    .global _start
_start:
    movz x20, #0
    movk x20, #0x0900, lsl #16
    movz x25, #0x0030, lsl #16
    msr  cpacr_el1, x25
    isb

    adr  x21, dst

    msr  fpcr, xzr
    isb
    msr  fpsr, xzr
    adr  x22, src_scalar_s
    ldr  s1, [x22]
    fsqrt s0, s1
    str  s0, [x21]
    movz x23, #'1'
    mov  x0, x21
    adr  x1, exp_scalar_s_rne
    bl   check4
    mrs  x2, fpsr
    cmp  x2, #0x10
    b.ne fail

    mov  x24, #(1 << 22)
    msr  fpcr, x24
    isb
    msr  fpsr, xzr
    adr  x22, src_scalar_s
    ldr  s3, [x22]
    fsqrt s2, s3
    str  s2, [x21, #4]
    movz x23, #'2'
    add  x0, x21, #4
    adr  x1, exp_scalar_s_ru
    bl   check4
    mrs  x2, fpsr
    cmp  x2, #0x10
    b.ne fail

    msr  fpcr, xzr
    isb
    msr  fpsr, xzr
    adr  x22, src_scalar_d
    ldr  d5, [x22]
    fsqrt d4, d5
    str  d4, [x21, #8]
    movz x23, #'3'
    add  x0, x21, #8
    adr  x1, exp_scalar_d_rne
    bl   check8
    mrs  x2, fpsr
    cmp  x2, #0x10
    b.ne fail

    mov  x24, #(2 << 22)
    msr  fpcr, x24
    isb
    msr  fpsr, xzr
    adr  x22, src_scalar_d
    ldr  d7, [x22]
    fsqrt d6, d7
    str  d6, [x21, #16]
    movz x23, #'4'
    add  x0, x21, #16
    adr  x1, exp_scalar_d_rm
    bl   check8
    mrs  x2, fpsr
    cmp  x2, #0x10
    b.ne fail

    msr  fpcr, xzr
    isb
    msr  fpsr, xzr
    adr  x22, src_vec_s
    ldr  q9, [x22]
    fsqrt v8.4s, v9.4s
    str  q8, [x21, #24]
    movz x23, #'5'
    add  x0, x21, #24
    adr  x1, exp_vec_s_rne
    bl   check16
    mrs  x2, fpsr
    cmp  x2, #0x10
    b.ne fail

    mov  x24, #(1 << 22)
    msr  fpcr, x24
    isb
    msr  fpsr, xzr
    adr  x22, src_vec_s
    ldr  q11, [x22]
    fsqrt v10.4s, v11.4s
    str  q10, [x21, #40]
    movz x23, #'6'
    add  x0, x21, #40
    adr  x1, exp_vec_s_ru
    bl   check16
    mrs  x2, fpsr
    cmp  x2, #0x10
    b.ne fail

    msr  fpcr, xzr
    isb
    msr  fpsr, xzr
    adr  x22, src_vec_d
    ldr  q13, [x22]
    fsqrt v12.2d, v13.2d
    str  q12, [x21, #56]
    movz x23, #'7'
    add  x0, x21, #56
    adr  x1, exp_vec_d_rne
    bl   check16
    mrs  x2, fpsr
    cmp  x2, #0x10
    b.ne fail

    mov  x24, #(2 << 22)
    msr  fpcr, x24
    isb
    msr  fpsr, xzr
    adr  x22, src_vec_d
    ldr  q15, [x22]
    fsqrt v14.2d, v15.2d
    str  q14, [x21, #72]
    movz x23, #'8'
    add  x0, x21, #72
    adr  x1, exp_vec_d_rm
    bl   check16
    mrs  x2, fpsr
    cmp  x2, #0x10
    b.ne fail

    movz x0, #'R'
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
    mov  x0, x23
    str  x0, [x20]
    brk  #0

    .data
    .balign 8
src_scalar_s:
    .word 0x4d305e6a
exp_scalar_s_rne:
    .word 0x46547c7c
exp_scalar_s_ru:
    .word 0x46547c7d

src_scalar_d:
    .quad 0x4000000000000000
exp_scalar_d_rne:
    .quad 0x3ff6a09e667f3bcd
exp_scalar_d_rm:
    .quad 0x3ff6a09e667f3bcc

    .balign 16
src_vec_s:
    .word 0x4d305e6a, 0x40800000, 0x3e800000, 0x3f800000
exp_vec_s_rne:
    .word 0x46547c7c, 0x40000000, 0x3f000000, 0x3f800000
exp_vec_s_ru:
    .word 0x46547c7d, 0x40000000, 0x3f000000, 0x3f800000

src_vec_d:
    .quad 0x4000000000000000, 0x4010000000000000
exp_vec_d_rne:
    .quad 0x3ff6a09e667f3bcd, 0x4000000000000000
exp_vec_d_rm:
    .quad 0x3ff6a09e667f3bcc, 0x4000000000000000

    .balign 16
dst:
    .space 88
