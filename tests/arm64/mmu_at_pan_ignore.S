    .arch armv8.1-a
    .text
    .global _start
_start:
    movz x1, #0
    movk x1, #0x0900, lsl #16      // UART

    movz x9, #3
    movz x20, #0x1000              // L0
    movz x21, #0x2000              // L1
    movz x22, #0x3000              // L2 low
    movz x23, #0x4000              // L3 low
    movz x24, #0x7000              // L2 high
    movz x25, #0x8000              // L3 high

    orr  x2, x21, x9
    str  x2, [x20]
    orr  x2, x22, x9
    str  x2, [x21]
    orr  x2, x24, x9
    str  x2, [x21, #8]
    orr  x2, x23, x9
    str  x2, [x22]
    orr  x2, x25, x9
    str  x2, [x24]

    movz x15, #0x0403

    // Keep the low VA range identity-mapped so the test continues executing
    // after enabling the MMU.
    movz x3, #0x0000
    orr  x2, x3, x15
    str  x2, [x23, #0]
    movz x3, #0x1000
    orr  x2, x3, x15
    str  x2, [x23, #8]
    movz x3, #0x2000
    orr  x2, x3, x15
    str  x2, [x23, #16]
    movz x3, #0x3000
    orr  x2, x3, x15
    str  x2, [x23, #24]
    movz x3, #0x4000
    orr  x2, x3, x15
    str  x2, [x23, #32]
    movz x3, #0x7000
    orr  x2, x3, x15
    str  x2, [x23, #56]
    movz x3, #0x8000
    orr  x2, x3, x15
    str  x2, [x23, #64]

    // VA 0x40000000 -> PA 0x500000, EL0 RW, AF=1.
    movz x3, #0x0050, lsl #16
    movz x2, #0x0443
    orr  x26, x3, x2
    str  x26, [x25]

    msr  ttbr0_el1, x20
    movz x2, #16
    msr  tcr_el1, x2
    movz x2, #0
    msr  mair_el1, x2
    movz x2, #1
    msr  sctlr_el1, x2
    isb

    movz x30, #0x4000, lsl #16

    msr  pan, #1
    mrs  x4, pan
    cmp  x4, #1
    b.ne fail1

    // FEAT_PAN2 is absent, so plain AT S1E1R/W must ignore PSTATE.PAN.
    at   s1e1r, x30
    mrs  x4, par_el1
    tbnz x4, #0, fail2
    tbz  x4, #11, fail2
    ubfx x5, x4, #12, #44
    movz x6, #0x0500
    cmp  x5, x6
    b.ne fail2

    at   s1e1w, x30
    mrs  x4, par_el1
    tbnz x4, #0, fail3
    tbz  x4, #11, fail3
    ubfx x5, x4, #12, #44
    movz x6, #0x0500
    cmp  x5, x6
    b.ne fail3

    msr  pan, #0
    movz x2, #0
    msr  sctlr_el1, x2
    isb
    movz x0, #'N'
    str  x0, [x1]
    movz x0, #'\n'
    str  x0, [x1]
    brk  #0

fail1:
    movz x0, #'1'
    b out
fail2:
    movz x0, #'2'
    b out
fail3:
    movz x0, #'3'

out:
    msr  pan, #0
    movz x2, #0
    msr  sctlr_el1, x2
    isb
    str  x0, [x1]
    brk  #0
