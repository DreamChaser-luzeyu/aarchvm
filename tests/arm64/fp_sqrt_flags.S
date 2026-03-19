    .text
    .global _start
_start:
    movz x20, #0
    movk x20, #0x0900, lsl #16
    movz x25, #0x0030, lsl #16
    msr  cpacr_el1, x25
    isb

    adr  x21, dst

    msr  fpsr, xzr
    adr  x22, src_neg_s
    ldr  s1, [x22]
    fsqrt s0, s1
    str  s0, [x21]
    mov  x0, x21
    adr  x1, exp_neg_s
    bl   check4
    mrs  x2, fpsr
    cmp  x2, #1
    b.ne fail

    msr  fpsr, xzr
    adr  x22, src_snan_s
    ldr  s3, [x22]
    fsqrt s2, s3
    str  s2, [x21, #4]
    add  x0, x21, #4
    adr  x1, exp_snan_s
    bl   check4
    mrs  x2, fpsr
    cmp  x2, #1
    b.ne fail

    msr  fpsr, xzr
    adr  x22, src_nzero_s
    ldr  s5, [x22]
    fsqrt s4, s5
    str  s4, [x21, #8]
    add  x0, x21, #8
    adr  x1, exp_nzero_s
    bl   check4
    mrs  x2, fpsr
    cbnz x2, fail

    msr  fpsr, xzr
    adr  x22, src_two_s
    ldr  s7, [x22]
    fsqrt s6, s7
    str  s6, [x21, #12]
    add  x0, x21, #12
    adr  x1, exp_two_s
    bl   check4
    mrs  x2, fpsr
    cmp  x2, #0x10
    b.ne fail

    msr  fpsr, xzr
    adr  x22, src_neg_d
    ldr  d9, [x22]
    fsqrt d8, d9
    str  d8, [x21, #16]
    add  x0, x21, #16
    adr  x1, exp_neg_d
    bl   check8
    mrs  x2, fpsr
    cmp  x2, #1
    b.ne fail

    msr  fpsr, xzr
    adr  x22, src_two_d
    ldr  d11, [x22]
    fsqrt d10, d11
    str  d10, [x21, #24]
    add  x0, x21, #24
    adr  x1, exp_two_d
    bl   check8
    mrs  x2, fpsr
    cmp  x2, #0x10
    b.ne fail

    msr  fpsr, xzr
    adr  x22, src_vec_s
    ldr  q13, [x22]
    fsqrt v12.4s, v13.4s
    str  q12, [x21, #32]
    add  x0, x21, #32
    adr  x1, exp_vec_s
    bl   check16
    mrs  x2, fpsr
    cmp  x2, #0x11
    b.ne fail

    msr  fpsr, xzr
    adr  x22, src_vec_d
    ldr  q15, [x22]
    fsqrt v14.2d, v15.2d
    str  q14, [x21, #48]
    add  x0, x21, #48
    adr  x1, exp_vec_d
    bl   check16
    mrs  x2, fpsr
    cmp  x2, #1
    b.ne fail

    movz x0, #'Q'
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
    movz x0, #'F'
    str  x0, [x20]
    brk  #0

    .data
    .balign 16
src_neg_s:
    .word 0xbf800000
exp_neg_s:
    .word 0x7fc00000

src_snan_s:
    .word 0x7f812345
exp_snan_s:
    .word 0x7fc12345

src_nzero_s:
    .word 0x80000000
exp_nzero_s:
    .word 0x80000000

src_two_s:
    .word 0x40000000
exp_two_s:
    .word 0x3fb504f3

    .balign 8
src_neg_d:
    .quad 0xbff0000000000000
exp_neg_d:
    .quad 0x7ff8000000000000

src_two_d:
    .quad 0x4000000000000000
exp_two_d:
    .quad 0x3ff6a09e667f3bcd

    .balign 16
src_vec_s:
    .word 0xbf800000, 0x40000000, 0x7fc12345, 0x80000000
exp_vec_s:
    .word 0x7fc00000, 0x3fb504f3, 0x7fc12345, 0x80000000

src_vec_d:
    .quad 0x7ff0123456789abc, 0x7ff0000000000000
exp_vec_d:
    .quad 0x7ff8123456789abc, 0x7ff0000000000000

    .balign 16
dst:
    .space 64
