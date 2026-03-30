    .text
    .global _start
_start:
    movz x20, #0
    movk x20, #0x0900, lsl #16      // UART
    movz x25, #0x0030, lsl #16
    msr  cpacr_el1, x25
    isb

    mov  x24, #(1 << 1)             // FPCR.AH

    // FEAT_AFP is absent in the current model, so FPCR.AH must be ignored.
    movz x23, #'1'
    msr  fpcr, xzr
    isb
    msr  fpsr, xzr
    adr  x22, snan_s
    ldr  s0, [x22]
    frecpe s1, s0
    fmov w2, s1
    mrs  x3, fpsr

    msr  fpcr, x24
    isb
    msr  fpsr, xzr
    ldr  s0, [x22]
    frecpe s1, s0
    fmov w4, s1
    mrs  x5, fpsr
    cmp  w2, w4
    b.ne fail
    cmp  x3, x5
    b.ne fail

    movz x23, #'2'
    msr  fpcr, xzr
    isb
    msr  fpsr, xzr
    adr  x22, snan_s
    ldr  s0, [x22]
    adr  x22, one_s
    ldr  s1, [x22]
    frecps s2, s0, s1
    fmov w2, s2
    mrs  x3, fpsr

    msr  fpcr, x24
    isb
    msr  fpsr, xzr
    adr  x22, snan_s
    ldr  s0, [x22]
    adr  x22, one_s
    ldr  s1, [x22]
    frecps s2, s0, s1
    fmov w4, s2
    mrs  x5, fpsr
    cmp  w2, w4
    b.ne fail
    cmp  x3, x5
    b.ne fail

    movz x23, #'3'
    msr  fpcr, xzr
    isb
    msr  fpsr, xzr
    adr  x22, neg_one_s
    ldr  s0, [x22]
    frsqrte s1, s0
    fmov w2, s1
    mrs  x3, fpsr

    msr  fpcr, x24
    isb
    msr  fpsr, xzr
    adr  x22, neg_one_s
    ldr  s0, [x22]
    frsqrte s1, s0
    fmov w4, s1
    mrs  x5, fpsr
    cmp  w2, w4
    b.ne fail
    cmp  x3, x5
    b.ne fail

    movz x23, #'4'
    msr  fpcr, xzr
    isb
    msr  fpsr, xzr
    adr  x22, snan_s
    ldr  s0, [x22]
    adr  x22, one_s
    ldr  s1, [x22]
    frsqrts s2, s0, s1
    fmov w2, s2
    mrs  x3, fpsr

    msr  fpcr, x24
    isb
    msr  fpsr, xzr
    adr  x22, snan_s
    ldr  s0, [x22]
    adr  x22, one_s
    ldr  s1, [x22]
    frsqrts s2, s0, s1
    fmov w4, s2
    mrs  x5, fpsr
    cmp  w2, w4
    b.ne fail
    cmp  x3, x5
    b.ne fail

    movz x23, #'5'
    msr  fpcr, xzr
    isb
    msr  fpsr, xzr
    adr  x22, snan_s
    ldr  s0, [x22]
    adr  x22, one_s
    ldr  s1, [x22]
    fmax s2, s0, s1
    fmov w2, s2
    mrs  x3, fpsr

    msr  fpcr, x24
    isb
    msr  fpsr, xzr
    adr  x22, snan_s
    ldr  s0, [x22]
    adr  x22, one_s
    ldr  s1, [x22]
    fmax s2, s0, s1
    fmov w4, s2
    mrs  x5, fpsr
    cmp  w2, w4
    b.ne fail
    cmp  x3, x5
    b.ne fail

    movz x0, #'H'
    str  x0, [x20]
    movz x0, #'\n'
    str  x0, [x20]
    brk  #0

fail:
    mov  x0, x23
    str  x0, [x20]
    brk  #0

    .data
    .balign 4
snan_s:
    .word 0x7f812345
one_s:
    .word 0x3f800000
neg_one_s:
    .word 0xbf800000
