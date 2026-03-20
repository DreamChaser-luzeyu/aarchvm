    .text
    .global _start
_start:
    movz x20, #0
    movk x20, #0x0900, lsl #16
    movz x25, #0x0030, lsl #16
    msr  cpacr_el1, x25
    isb

    adr  x21, dst
    mov  x24, #(1 << 25)          // FPCR.DN
    msr  fpcr, x24
    isb

    movz x24, #'1'
    msr  fpsr, xzr
    adr  x22, qnan_s
    ldr  s1, [x22]
    frintn s0, s1
    str  s0, [x21, #0]
    add  x0, x21, #0
    adr  x1, exp_defnan_s
    bl   check4
    mrs  x2, fpsr
    cbnz x2, fail1

    movz x24, #'2'
    msr  fpsr, xzr
    adr  x22, snan_s
    ldr  s3, [x22]
    frintn s2, s3
    str  s2, [x21, #4]
    add  x0, x21, #4
    adr  x1, exp_defnan_s
    bl   check4
    mrs  x2, fpsr
    cmp  x2, #1
    b.ne fail2

    movz x24, #'3'
    msr  fpsr, xzr
    adr  x22, qnan_d
    ldr  d5, [x22]
    fsqrt d4, d5
    str  d4, [x21, #8]
    add  x0, x21, #8
    adr  x1, exp_defnan_d
    bl   check8
    mrs  x2, fpsr
    cbnz x2, fail3

    movz x24, #'4'
    msr  fpsr, xzr
    adr  x22, qnan_s
    ldr  s7, [x22]
    frecpe s6, s7
    str  s6, [x21, #16]
    add  x0, x21, #16
    adr  x1, exp_defnan_s
    bl   check4
    mrs  x2, fpsr
    cbnz x2, fail4

    movz x24, #'5'
    msr  fpsr, xzr
    adr  x22, snan_d
    ldr  d9, [x22]
    frsqrte d8, d9
    str  d8, [x21, #24]
    add  x0, x21, #24
    adr  x1, exp_defnan_d
    movz x24, #'a'
    bl   check8
    movz x24, #'5'
    mrs  x2, fpsr
    cbz  x2, fail5_zero
    cmp  x2, #1
    b.ne fail5_other

    movz x24, #'6'
    msr  fpsr, xzr
    adr  x22, qnan_s
    ldr  s10, [x22]
    adr  x22, one_s
    ldr  s11, [x22]
    frecps s12, s10, s11
    str  s12, [x21, #32]
    add  x0, x21, #32
    adr  x1, exp_defnan_s
    movz x24, #'b'
    bl   check4
    movz x24, #'6'
    mrs  x2, fpsr
    cbnz x2, fail6

    movz x24, #'7'
    msr  fpsr, xzr
    adr  x22, qnan_d
    ldr  d13, [x22]
    fcvtxn s14, d13
    str  s14, [x21, #36]
    add  x0, x21, #36
    adr  x1, exp_defnan_s
    bl   check4
    mrs  x2, fpsr
    cbnz x2, fail7

    movz x24, #'8'
    msr  fpsr, xzr
    adr  x22, src_fcvtl
    ldr  d1, [x22]
    fcvtl v0.2d, v1.2s
    str  q0, [x21, #48]
    add  x0, x21, #48
    adr  x1, exp_fcvtl
    bl   check16
    mrs  x2, fpsr
    cbnz x2, fail8

    movz x24, #'9'
    msr  fpsr, xzr
    adr  x22, src_fcvtn
    ldr  q2, [x22]
    fcvtn v3.2s, v2.2d
    str  q3, [x21, #64]
    add  x0, x21, #64
    adr  x1, exp_fcvtn
    bl   check16
    mrs  x2, fpsr
    cbnz x2, fail9

    movz x0, #'D'
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
fail5:
    movz x0, #'5'
    b out
fail5_zero:
    movz x0, #'z'
    b out
fail5_other:
    movz x0, #'y'
    b out
fail6:
    movz x0, #'6'
    b out
fail7:
    movz x0, #'7'
    b out
fail8:
    movz x0, #'8'
    b out
fail9:
    movz x0, #'9'
    b out
fail:
    mov  x0, x24
out:
    str  x0, [x20]
    brk  #0

    .data
    .balign 16
qnan_s:
    .word 0x7fc12345
snan_s:
    .word 0x7f812345
one_s:
    .word 0x3f800000
exp_defnan_s:
    .word 0x7fc00000

    .balign 8
qnan_d:
    .quad 0x7ff8123456789abc
snan_d:
    .quad 0x7ff0123456789abc
exp_defnan_d:
    .quad 0x7ff8000000000000

    .balign 16
src_fcvtl:
    .word 0x7fc12345, 0x3f800000
exp_fcvtl:
    .quad 0x7ff8000000000000, 0x3ff0000000000000

src_fcvtn:
    .quad 0x7ff8123456789abc, 0x3ff0000000000000
exp_fcvtn:
    .word 0x7fc00000, 0x3f800000, 0x00000000, 0x00000000

dst:
    .space 80
