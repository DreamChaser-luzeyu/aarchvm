    .text
    .global _start
_start:
    movz x20, #0
    movk x20, #0x0900, lsl #16
    movz x25, #0x0030, lsl #16
    msr  cpacr_el1, x25
    isb

    adr  x21, data_s
    adr  x24, data_d

    movz x23, #'1'
    msr  fpcr, xzr
    isb
    msr  fpsr, xzr
    ldr  w0, [x21, #(0 * 4)]
    fmov s0, w0
    fcvtnu w1, s0
    cbnz w1, fail
    mrs  x2, fpsr
    cmp  x2, #1
    b.ne fail

    movz x23, #'2'
    msr  fpsr, xzr
    ldr  w0, [x21, #(1 * 4)]
    fmov s0, w0
    fcvtns w1, s0
    cbnz w1, fail
    mrs  x2, fpsr
    cmp  x2, #1
    b.ne fail

    movz x23, #'3'
    msr  fpsr, xzr
    ldr  w0, [x21, #(2 * 4)]
    fmov s0, w0
    fcvtpu w1, s0
    cbnz w1, fail
    mrs  x2, fpsr
    cmp  x2, #0x10
    b.ne fail

    movz x23, #'4'
    msr  fpsr, xzr
    ldr  w0, [x21, #(2 * 4)]
    fmov s0, w0
    fcvtnu w1, s0
    cbnz w1, fail
    mrs  x2, fpsr
    cmp  x2, #1
    b.ne fail

    movz x23, #'5'
    msr  fpsr, xzr
    ldr  w0, [x21, #(3 * 4)]
    fmov s0, w0
    fcvtnu w1, s0
    cbnz w1, fail
    mrs  x2, fpsr
    cmp  x2, #0x10
    b.ne fail

    movz x23, #'6'
    msr  fpsr, xzr
    ldr  w0, [x21, #(3 * 4)]
    fmov s0, w0
    fcvtau w1, s0
    cbnz w1, fail
    mrs  x2, fpsr
    cmp  x2, #1
    b.ne fail

    movz x23, #'7'
    msr  fpsr, xzr
    ldr  w0, [x21, #(4 * 4)]
    fmov s0, w0
    fcvtns w1, s0
    ldr  w2, =0x7fffffff
    cmp  w1, w2
    b.ne fail
    mrs  x2, fpsr
    cmp  x2, #1
    b.ne fail

    movz x23, #'8'
    msr  fpsr, xzr
    ldr  w0, [x21, #(5 * 4)]
    fmov s0, w0
    fcvtns w1, s0
    ldr  w2, =0x80000000
    cmp  w1, w2
    b.ne fail
    mrs  x2, fpsr
    cmp  x2, #1
    b.ne fail

    movz x23, #'9'
    msr  fpsr, xzr
    ldr  x0, [x24, #(0 * 8)]
    fmov d0, x0
    fcvtpu x1, d0
    ldr  x2, =0xffffffffffffffff
    cmp  x1, x2
    b.ne fail
    mrs  x2, fpsr
    cmp  x2, #1
    b.ne fail

    movz x23, #'A'
    msr  fpsr, xzr
    ldr  x0, [x24, #(1 * 8)]
    fmov d0, x0
    fcvtns x1, d0
    ldr  x2, =0x8000000000000000
    cmp  x1, x2
    b.ne fail
    mrs  x2, fpsr
    cmp  x2, #1
    b.ne fail

    movz x23, #'B'
    mov  x3, #(1 << 24)
    msr  fpcr, x3
    isb
    msr  fpsr, xzr
    ldr  w0, [x21, #(6 * 4)]
    fmov s0, w0
    fcvtzs w1, s0
    cbnz w1, fail
    mrs  x2, fpsr
    cmp  x2, #0x80
    b.ne fail

    movz x23, #'C'
    msr  fpsr, xzr
    ldr  x0, [x24, #(2 * 8)]
    fmov d0, x0
    fcvtnu x1, d0
    cbnz x1, fail
    mrs  x2, fpsr
    cmp  x2, #0x80
    b.ne fail

    movz x0, #'S'
    str  x0, [x20]
    brk  #0

fail:
    str  x23, [x20]
    brk  #0

    .data
    .balign 8
data_s:
    .word 0x7fc12345              // qNaN
    .word 0x7f812345              // sNaN
    .word 0xbf19999a              // -0.6f
    .word 0xbf000000              // -0.5f
    .word 0x7f800000              // +inf
    .word 0xff800000              // -inf
    .word 0x00000001              // smallest positive subnormal
    .word 0x00000000
    .balign 8
data_d:
    .quad 0x7ff0000000000000      // +inf
    .quad 0xfff0000000000000      // -inf
    .quad 0x0000000000000001      // smallest positive double subnormal
