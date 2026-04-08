    .text
    .global _start

    .macro PUTCH imm
    movz x0, #\imm
    str  x0, [x1]
    .endm

_start:
    movz x1, #0x0000
    movk x1, #0x0900, lsl #16

    // OS Lock starts locked after reset, so TDA is only being saved/restored
    // here and must not affect the direct DBGBCR access below.
    movz x0, #0x20, lsl #16         // MDSCR_EL1.TDA
    msr  mdscr_el1, x0

    // Even before unlocking OS Lock, direct reads of DBGBCR<n>_EL1 must still
    // reflect AArch64-only BAS[3:0]=RES1 semantics.
    mrs  x2, dbgbcr0_el1
    movz x3, #0x01e0
    cmp  x2, x3
    b.ne hang

    PUTCH 'T'
    PUTCH 'D'
    PUTCH 'A'
    PUTCH '-'
    PUTCH 'R'
    PUTCH 'E'
    PUTCH 'A'
    PUTCH 'D'
    PUTCH '\n'

    msr  oslar_el1, xzr
    mrs  x2, dbgbcr0_el1

hang:
    b hang
