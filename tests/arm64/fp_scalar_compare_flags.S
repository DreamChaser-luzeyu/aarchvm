    .text
    .global _start
_start:
    movz x20, #0
    movk x20, #0x0900, lsl #16
    movz x25, #0x0030, lsl #16
    msr  cpacr_el1, x25
    isb

    msr  fpsr, xzr
    ldr  w0, =0x7fc00000 // qNaN
    ldr  w1, =0x3f800000 // 1.0f
    fmov s0, w0
    fmov s1, w1
    fcmeq s2, s0, s1
    fmov w2, s2
    cbnz w2, fail1
    mrs  x2, fpsr
    cbnz x2, fail1

    msr  fpsr, xzr
    ldr  w0, =0x7f800001 // sNaN
    ldr  w1, =0x3f800000 // 1.0f
    fmov s0, w0
    fmov s1, w1
    fcmge s3, s0, s1
    fmov w2, s3
    cbnz w2, fail2
    mrs  x2, fpsr
    cmp  x2, #1
    b.ne fail2

    msr  fpsr, xzr
    ldr  x0, =0x7ff0000000000001 // sNaN
    ldr  x1, =0x3ff0000000000000 // 1.0
    fmov d0, x0
    fmov d1, x1
    facgt d4, d0, d1
    fmov x2, d4
    cbnz x2, fail3
    mrs  x2, fpsr
    cmp  x2, #1
    b.ne fail3

    msr  fpsr, xzr
    ldr  w0, =0x7f800001 // sNaN
    fmov s0, w0
    fcmgt s5, s0, #0.0
    fmov w2, s5
    cbnz w2, fail4
    mrs  x2, fpsr
    cmp  x2, #1
    b.ne fail4

    movz x0, #'K'
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
out:
    str  x0, [x20]
    brk  #0
