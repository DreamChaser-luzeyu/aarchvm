    .text
    .global _start
_start:
    movz x20, #0
    movk x20, #0x0900, lsl #16
    movz x25, #0x0030, lsl #16
    msr  cpacr_el1, x25
    isb

    adr  x21, dst
    adr  x22, data32
    adr  x23, data64

    msr  fpsr, xzr
    movz x24, #'1'
    ldr  w0, [x22, #0]      // -sNaN32
    fmov s1, w0
    fabs s0, s1
    str  s0, [x21]
    mov  x0, x21
    add  x1, x22, #12
    bl   check4
    mrs  x2, fpsr
    cbnz x2, fail1

    msr  fpsr, xzr
    movz x24, #'2'
    ldr  w1, [x22, #4]      // +sNaN32
    fmov s1, w1
    fneg s0, s1
    str  s0, [x21, #4]
    add  x0, x21, #4
    add  x1, x22, #16
    bl   check4
    mrs  x2, fpsr
    cbnz x2, fail2

    msr  fpsr, xzr
    movz x24, #'3'
    ldr  w2, [x22, #8]      // +qNaN32
    fmov s1, w2
    fneg s0, s1
    str  s0, [x21, #8]
    add  x0, x21, #8
    add  x1, x22, #20
    bl   check4
    mrs  x2, fpsr
    cbnz x2, fail3

    msr  fpsr, xzr
    movz x24, #'4'
    ldr  x10, [x23, #0]     // -sNaN64
    fmov d1, x10
    fabs d0, d1
    str  d0, [x21, #16]
    add  x0, x21, #16
    add  x1, x23, #24
    bl   check8
    mrs  x2, fpsr
    cbnz x2, fail4

    msr  fpsr, xzr
    movz x24, #'5'
    ldr  x11, [x23, #8]     // +sNaN64
    fmov d1, x11
    fneg d0, d1
    str  d0, [x21, #24]
    add  x0, x21, #24
    add  x1, x23, #32
    bl   check8
    mrs  x2, fpsr
    cbnz x2, fail5

    msr  fpsr, xzr
    movz x24, #'6'
    ldr  x12, [x23, #16]    // +qNaN64
    fmov d1, x12
    fabs d0, d1
    str  d0, [x21, #32]
    add  x0, x21, #32
    add  x1, x23, #40
    bl   check8
    mrs  x2, fpsr
    cbnz x2, fail6

    msr  fpsr, xzr
    movz x24, #'7'
    ldr  w0, [x22, #0]      // -sNaN32
    dup  v1.4s, w0
    fabs v0.4s, v1.4s
    str  q0, [x21, #48]
    add  x0, x21, #48
    adr  x1, exp_vec_abs_s
    bl   check16
    mrs  x2, fpsr
    cbnz x2, fail7

    msr  fpsr, xzr
    movz x24, #'8'
    ldr  x11, [x23, #8]     // +sNaN64
    dup  v1.2d, x11
    fneg v0.2d, v1.2d
    str  q0, [x21, #64]
    add  x0, x21, #64
    adr  x1, exp_vec_neg_d
    bl   check16
    mrs  x2, fpsr
    cbnz x2, fail8

    movz x0, #'N'
    str  x0, [x20]
    movz x0, #'\n'
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
fail6:
    movz x0, #'6'
    b out
fail7:
    movz x0, #'7'
    b out
fail8:
    movz x0, #'8'
    b out
fail:
    mov  x0, x24
out:
    str  x0, [x20]
    brk  #0

    .data
    .balign 8
data32:
    .word 0xff812345
    .word 0x7f812345
    .word 0x7fc12345
    .word 0x7f812345
    .word 0xff812345
    .word 0xffc12345

    .balign 8
data64:
    .quad 0xfff0123456789abc
    .quad 0x7ff0123456789abc
    .quad 0x7ff8123456789abc
    .quad 0x7ff0123456789abc
    .quad 0xfff0123456789abc
    .quad 0x7ff8123456789abc

    .balign 16
exp_vec_abs_s:
    .word 0x7f812345, 0x7f812345, 0x7f812345, 0x7f812345

exp_vec_neg_d:
    .quad 0xfff0123456789abc, 0xfff0123456789abc

    .balign 16
dst:
    .space 80
