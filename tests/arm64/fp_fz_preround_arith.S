    .text
    .global _start
_start:
    movz x20, #0
    movk x20, #0x0900, lsl #16
    movz x25, #0x0030, lsl #16
    msr  cpacr_el1, x25
    isb

    mov  x24, #(1 << 24)         // FPCR.FZ

    // FMUL S: 2^-126 * (1 - 2^-24) is the midpoint between max-subnormal
    // and min-normal single. With FZ and AH=0 it must flush before rounding.
    movz x23, #'1'
    msr  fpcr, x24
    isb
    msr  fpsr, xzr
    ldr  w0, =0x00800000
    fmov s0, w0
    ldr  w1, =0x3f7fffff
    fmov s1, w1
    fmul s2, s0, s1
    fmov w3, s2
    cmp  w3, wzr
    b.ne fail
    mrs  x4, fpsr
    cmp  x4, #0x8
    b.ne fail

    movz x23, #'2'
    msr  fpcr, xzr
    isb
    msr  fpsr, xzr
    fmul s2, s0, s1
    fmov w3, s2
    ldr  w5, =0x00800000
    cmp  w3, w5
    b.ne fail
    mrs  x4, fpsr
    mov  x5, #0x18
    cmp  x4, x5
    b.ne fail

    // FMADD S shares the same pre-round tiny rule via the FMA helper.
    movz x23, #'3'
    msr  fpcr, x24
    isb
    msr  fpsr, xzr
    fmov s3, wzr
    fmadd s4, s0, s1, s3
    fmov w3, s4
    cmp  w3, wzr
    b.ne fail
    mrs  x4, fpsr
    cmp  x4, #0x8
    b.ne fail

    // FMUL D: 2^-1022 * (1 - 2^-53) is the double midpoint boundary.
    movz x23, #'4'
    msr  fpcr, x24
    isb
    msr  fpsr, xzr
    ldr  x0, =0x0010000000000000
    fmov d0, x0
    ldr  x1, =0x3fefffffffffffff
    fmov d1, x1
    fmul d2, d0, d1
    fmov x3, d2
    cbnz x3, fail
    mrs  x4, fpsr
    cmp  x4, #0x8
    b.ne fail

    movz x0, #'A'
    str  x0, [x20]
    brk  #0

fail:
    str  x23, [x20]
    brk  #0
