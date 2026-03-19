    .text
    .global _start
_start:
    movz x20, #0
    movk x20, #0x0900, lsl #16
    movz x25, #0x0030, lsl #16
    msr  cpacr_el1, x25
    isb

    adr  x21, data32
    ldr  w0, [x21, #0]      // qNaN
    ldr  w1, [x21, #4]      // sNaN
    ldr  w2, [x21, #8]      // quiet(sNaN)
    ldr  w3, [x21, #12]     // +1.0
    ldr  w4, [x21, #16]     // -0.0
    ldr  w5, [x21, #20]     // +0.0

    msr  fpsr, xzr
    fmov s1, w0
    fmov s2, w3
    fmax s0, s1, s2
    fmov w6, s0
    cmp  w6, w0
    b.ne fail1
    mrs  x7, fpsr
    cbnz x7, fail1

    msr  fpsr, xzr
    fmov s1, w0
    fmov s2, w3
    fmaxnm s0, s1, s2
    fmov w6, s0
    cmp  w6, w3
    b.ne fail2
    mrs  x7, fpsr
    cbnz x7, fail2

    msr  fpsr, xzr
    fmov s1, w1
    fmov s2, w3
    fminnm s0, s1, s2
    fmov w6, s0
    cmp  w6, w2
    b.ne fail3
    mrs  x7, fpsr
    cmp  x7, #1
    b.ne fail3

    msr  fpsr, xzr
    fmov s1, w4
    fmov s2, w5
    fmax s0, s1, s2
    fmov w6, s0
    cmp  w6, w5
    b.ne fail4
    mrs  x7, fpsr
    cbnz x7, fail4

    msr  fpsr, xzr
    fmov s1, w5
    fmov s2, w4
    fmin s0, s1, s2
    fmov w6, s0
    cmp  w6, w4
    b.ne fail5
    mrs  x7, fpsr
    cbnz x7, fail5

    adr  x22, data64
    ldr  x10, [x22, #0]     // qNaN
    ldr  x11, [x22, #8]     // sNaN
    ldr  x12, [x22, #16]    // quiet(sNaN)
    ldr  x13, [x22, #24]    // +1.0

    msr  fpsr, xzr
    fmov d1, x10
    fmov d2, x13
    fmin d0, d1, d2
    fmov x14, d0
    cmp  x14, x10
    b.ne fail6
    mrs  x7, fpsr
    cbnz x7, fail6

    msr  fpsr, xzr
    fmov d1, x10
    fmov d2, x13
    fminnm d0, d1, d2
    fmov x14, d0
    cmp  x14, x13
    b.ne fail7
    mrs  x7, fpsr
    cbnz x7, fail7

    msr  fpsr, xzr
    fmov d1, x11
    fmov d2, x13
    fmax d0, d1, d2
    fmov x14, d0
    cmp  x14, x12
    b.ne fail8
    mrs  x7, fpsr
    cmp  x7, #1
    b.ne fail8

    msr  fpsr, xzr
    dup  v1.4s, w0
    dup  v2.4s, w3
    fmax v0.4s, v1.4s, v2.4s
    umov w6, v0.s[0]
    cmp  w6, w0
    b.ne fail9
    mrs  x7, fpsr
    cbnz x7, fail9

    msr  fpsr, xzr
    dup  v1.4s, w0
    dup  v2.4s, w3
    fminnm v0.4s, v1.4s, v2.4s
    umov w6, v0.s[0]
    cmp  w6, w3
    b.ne faila
    mrs  x7, fpsr
    cbnz x7, faila

    msr  fpsr, xzr
    dup  v1.4s, w1
    dup  v2.4s, w3
    fmaxnm v0.4s, v1.4s, v2.4s
    umov w6, v0.s[0]
    cmp  w6, w2
    b.ne failb
    mrs  x7, fpsr
    cmp  x7, #1
    b.ne failb

    msr  fpsr, xzr
    dup  v1.4s, w5
    dup  v2.4s, w4
    fmin v0.4s, v1.4s, v2.4s
    umov w6, v0.s[0]
    cmp  w6, w4
    b.ne failc
    mrs  x7, fpsr
    cbnz x7, failc

    movz x0, #'M'
    str  x0, [x20]
    movz x0, #'\n'
    str  x0, [x20]
    brk  #0

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
fail9:
    movz x0, #'9'
    b out
faila:
    movz x0, #'A'
    b out
failb:
    movz x0, #'B'
    b out
failc:
    movz x0, #'C'
out:
    str  x0, [x20]
    brk  #0

    .data
    .balign 8
data32:
    .word 0x7fc12345
    .word 0x7f812345
    .word 0x7fc12345
    .word 0x3f800000
    .word 0x80000000
    .word 0x00000000

    .balign 8
data64:
    .quad 0x7ff8123456789abc
    .quad 0x7ff0123456789abc
    .quad 0x7ff8123456789abc
    .quad 0x3ff0000000000000
