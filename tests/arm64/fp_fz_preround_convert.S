    .text
    .global _start
_start:
    movz x20, #0
    movk x20, #0x0900, lsl #16
    movz x25, #0x0030, lsl #16
    msr  cpacr_el1, x25
    isb

    mov  x24, #(1 << 24)         // FPCR.FZ

    movz x23, #'1'
    msr  fpcr, x24
    isb
    msr  fpsr, xzr
    ldr  x0, =0x380fffffe0000000 // midpoint that rounds to min-normal single
    fmov d0, x0
    fcvt s1, d0
    fmov w2, s1
    cmp  w2, wzr
    b.ne fail
    mrs  x3, fpsr
    cmp  x3, #0x8                // UFC only when flushed to zero with AH=0
    b.ne fail

    movz x23, #'2'
    msr  fpcr, xzr
    isb
    msr  fpsr, xzr
    ldr  x0, =0x380fffffe0000000
    fmov d0, x0
    fcvt s1, d0
    fmov w2, s1
    ldr  w4, =0x00800000         // min-normal single
    cmp  w2, w4
    b.ne fail
    mrs  x3, fpsr
    mov  x4, #0x18               // UFC | IXC
    cmp  x3, x4
    b.ne fail

    movz x23, #'3'
    msr  fpcr, x24
    isb
    msr  fpsr, xzr
    ldr  x0, =0xb80fffffe0000000
    fmov d0, x0
    fcvt s1, d0
    fmov w2, s1
    ldr  w4, =0x80000000         // -0.0f
    cmp  w2, w4
    b.ne fail
    mrs  x3, fpsr
    cmp  x3, #0x8
    b.ne fail

    movz x23, #'4'
    msr  fpcr, x24
    isb
    msr  fpsr, xzr
    ldr  x0, =0x380fffffe0000000
    fmov d0, x0
    fcvtxn s1, d0
    fmov w2, s1
    cmp  w2, wzr
    b.ne fail
    mrs  x3, fpsr
    cmp  x3, #0x8
    b.ne fail

    movz x0, #'B'
    str  x0, [x20]
    brk  #0

fail:
    str  x23, [x20]
    brk  #0
