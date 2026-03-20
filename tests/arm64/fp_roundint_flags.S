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
    adr  x22, src_snan_s
    ldr  s1, [x22]
    frintn s0, s1
    str  s0, [x21]
    mov  x0, x21
    adr  x1, exp_qnan_s
    bl   check4
    mrs  x2, fpsr
    cmp  x2, #1
    b.ne fail

    msr  fpsr, xzr
    adr  x22, src_qnan_s
    ldr  s3, [x22]
    frintn s2, s3
    str  s2, [x21, #4]
    add  x0, x21, #4
    adr  x1, exp_qnan_s
    bl   check4
    mrs  x2, fpsr
    cbnz x2, fail

    mov  x3, #(1 << 22)
    msr  fpcr, x3
    isb
    msr  fpsr, xzr
    adr  x22, src_1p2_s
    ldr  s5, [x22]
    frintx s4, s5
    str  s4, [x21, #8]
    add  x0, x21, #8
    adr  x1, exp_two_s
    bl   check4
    mrs  x2, fpsr
    cmp  x2, #0x10
    b.ne fail

    mov  x3, #(2 << 22)
    msr  fpcr, x3
    isb
    msr  fpsr, xzr
    adr  x22, src_neg_1p2_d
    ldr  d7, [x22]
    frinti d6, d7
    str  d6, [x21, #16]
    add  x0, x21, #16
    adr  x1, exp_neg_two_d
    bl   check8
    mrs  x2, fpsr
    cbnz x2, fail

    mov  x3, #(1 << 22)
    msr  fpcr, x3
    isb
    msr  fpsr, xzr
    adr  x22, src_vec_x
    ldr  q9, [x22]
    frintx v8.4s, v9.4s
    str  q8, [x21, #32]
    add  x0, x21, #32
    adr  x1, exp_vec_x
    bl   check16
    mrs  x2, fpsr
    cmp  x2, #0x11
    b.ne fail

    msr  fpsr, xzr
    adr  x22, src_vec_i
    ldr  q11, [x22]
    frinti v10.2d, v11.2d
    str  q10, [x21, #48]
    add  x0, x21, #48
    adr  x1, exp_vec_i
    bl   check16
    mrs  x2, fpsr
    cmp  x2, #1
    b.ne fail

    mov  x3, #(1 << 25)
    msr  fpcr, x3
    isb
    msr  fpsr, xzr
    adr  x22, src_vec_dn
    ldr  q13, [x22]
    frintn v12.4s, v13.4s
    str  q12, [x21, #64]
    add  x0, x21, #64
    adr  x1, exp_vec_dn
    bl   check16
    mrs  x2, fpsr
    cmp  x2, #1
    b.ne fail

    mov  x3, #(1 << 24)
    msr  fpcr, x3
    isb
    msr  fpsr, xzr
    adr  x22, src_neg_sub_s
    ldr  s15, [x22]
    frintx s14, s15
    str  s14, [x21, #80]
    add  x0, x21, #80
    adr  x1, exp_neg_zero_s
    bl   check4
    mrs  x2, fpsr
    cmp  x2, #0x80
    b.ne fail

    msr  fpsr, xzr
    adr  x22, src_vec_fz
    ldr  q17, [x22]
    frinti v16.4s, v17.4s
    str  q16, [x21, #96]
    add  x0, x21, #96
    adr  x1, exp_vec_fz
    bl   check16
    mrs  x2, fpsr
    cmp  x2, #0x80
    b.ne fail

    movz x0, #'I'
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
src_snan_s:
    .word 0x7f812345
src_qnan_s:
    .word 0x7fc12345
exp_qnan_s:
    .word 0x7fc12345
src_1p2_s:
    .word 0x3f99999a
exp_two_s:
    .word 0x40000000

    .balign 8
src_neg_1p2_d:
    .quad 0xbff3333333333333
exp_neg_two_d:
    .quad 0xc000000000000000

    .balign 16
src_vec_x:
    .word 0x3f99999a, 0x7fc12345, 0x7f812345, 0xbe99999a
exp_vec_x:
    .word 0x40000000, 0x7fc12345, 0x7fc12345, 0x80000000

src_vec_i:
    .quad 0x3ff3333333333333, 0x7ff0123456789abc
exp_vec_i:
    .quad 0x4000000000000000, 0x7ff8123456789abc

src_vec_dn:
    .word 0x7fc12345, 0x7f812345, 0x3f99999a, 0x80000001
exp_vec_dn:
    .word 0x7fc00000, 0x7fc00000, 0x3f800000, 0x80000000

src_neg_sub_s:
    .word 0x80000001
exp_neg_zero_s:
    .word 0x80000000

src_vec_fz:
    .word 0x80000001, 0x00000001, 0x00000000, 0x80000000
exp_vec_fz:
    .word 0x80000000, 0x00000000, 0x00000000, 0x80000000

dst:
    .space 112
