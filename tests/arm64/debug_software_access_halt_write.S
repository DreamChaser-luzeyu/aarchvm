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
    // here and must not affect the direct DBGWCR access below.
    movz x0, #0x20, lsl #16         // MDSCR_EL1.TDA
    msr  mdscr_el1, x0

    movz x2, #0x0001
    msr  dbgwcr0_el1, x2

    PUTCH 'T'
    PUTCH 'D'
    PUTCH 'A'
    PUTCH '-'
    PUTCH 'W'
    PUTCH 'R'
    PUTCH 'I'
    PUTCH 'T'
    PUTCH 'E'
    PUTCH '\n'

    msr  oslar_el1, xzr
    movz x2, #0x0003
    msr  dbgwcr0_el1, x2

hang:
    b hang
