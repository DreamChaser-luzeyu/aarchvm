    .text
    .global _start
_start:
    movz x20, #0
    movk x20, #0x0900, lsl #16
    movz x25, #0x0030, lsl #16
    msr  cpacr_el1, x25
    isb

    // Current model:
    // - FEAT_AFP is absent, so FPCR.{NEP,AH,FIZ} are RES0.
    // - FEAT_FP16/FEAT_EBF16 are absent, so FPCR.{FZ16,EBF} are RES0.
    // - Trapped FP exceptions are not implemented, so FPCR.{IDE,IXE,UFE,OFE,DZE,IOE} are RAZ/WI.
    // - We preserve AHP/DN/FZ/RMode/Stride/Len.
    movz x21, #0x07f7, lsl #16

    movz x0, #0xffff
    movk x0, #0xffff, lsl #16
    msr  fpcr, x0
    mrs  x1, fpcr
    cmp  x1, x21
    b.ne fail

    movz x0, #0x0003
    movk x0, #0x0181, lsl #16
    msr  fpcr, x0
    mrs  x1, fpcr
    movz x2, #0x0181, lsl #16
    cmp  x1, x2
    b.ne fail

    msr  fpcr, x21
    mrs  x1, fpcr
    cmp  x1, x21
    b.ne fail

    movz x0, #'P'
    str  x0, [x20]
    movz x0, #'\n'
    str  x0, [x20]
    brk  #0

fail:
    movz x0, #'F'
    str  x0, [x20]
    brk  #0
