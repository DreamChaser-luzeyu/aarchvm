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

    // Identity-map the low VA range so execution continues after enabling the MMU.
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

    // VA 0x40000000 -> PA 0x500000, Device attr0, descriptor SH=Non-shareable.
    movz x3, #0x0050, lsl #16
    movz x2, #0x0403
    orr  x26, x3, x2
    str  x26, [x25, #0]

    // VA 0x40001000 -> PA 0x600000, Normal NC attr1, descriptor SH=Non-shareable.
    movz x3, #0x0060, lsl #16
    movz x2, #0x0407
    orr  x26, x3, x2
    str  x26, [x25, #8]

    // VA 0x40002000 -> PA 0x700000, Normal WB attr2, descriptor SH=Inner-shareable.
    movz x3, #0x0070, lsl #16
    movz x2, #0x070b
    orr  x26, x3, x2
    str  x26, [x25, #16]

    // VA 0x40003000 -> PA 0x800000, attr2, EL0 privileged-only for PAR fault-format checks.
    movz x3, #0x0080, lsl #16
    movz x2, #0x040b
    orr  x26, x3, x2
    str  x26, [x25, #24]

    // MAIR: attr0 = 0x00 (Device), attr1 = 0x44 (Normal NC), attr2 = 0xff (Normal WB WA).
    movz x2, #0x4400
    movk x2, #0x00ff, lsl #16
    msr  mair_el1, x2

    movz x2, #16
    msr  ttbr0_el1, x20
    msr  tcr_el1, x2
    movz x2, #1
    msr  sctlr_el1, x2
    isb

    movz x30, #0x4000, lsl #16
    add  x29, x30, #0x1000
    add  x28, x30, #0x2000
    add  x27, x30, #0x3000

    // Device memory reports Outer Shareable in PAR_EL1.SH regardless of leaf SH.
    at   s1e1r, x30
    mrs  x4, par_el1
    tbnz x4, #0, fail1
    tbz  x4, #11, fail1
    ubfx x5, x4, #12, #44
    movz x6, #0x0500
    cmp  x5, x6
    b.ne fail1
    lsr  x5, x4, #56
    cbnz x5, fail1
    ubfx x5, x4, #7, #2
    movz x6, #2
    cmp  x5, x6
    b.ne fail1

    // Normal Non-cacheable memory also reports Outer Shareable in PAR_EL1.SH.
    at   s1e1r, x29
    mrs  x4, par_el1
    tbnz x4, #0, fail2
    tbz  x4, #11, fail2
    ubfx x5, x4, #12, #44
    movz x6, #0x0600
    cmp  x5, x6
    b.ne fail2
    lsr  x5, x4, #56
    movz x6, #0x44
    cmp  x5, x6
    b.ne fail2
    ubfx x5, x4, #7, #2
    movz x6, #2
    cmp  x5, x6
    b.ne fail2

    // Cacheable Normal memory preserves the translated shareability.
    at   s1e1r, x28
    mrs  x4, par_el1
    tbnz x4, #0, fail3
    tbz  x4, #11, fail3
    ubfx x5, x4, #12, #44
    movz x6, #0x0700
    cmp  x5, x6
    b.ne fail3
    lsr  x5, x4, #56
    movz x6, #0x00ff
    cmp  x5, x6
    b.ne fail3
    ubfx x5, x4, #7, #2
    movz x6, #3
    cmp  x5, x6
    b.ne fail3

    // Stage-1 permission fault in PAR_EL1 keeps RES1 bit 11 set and S/PTW clear.
    at   s1e0r, x27
    mrs  x4, par_el1
    tbz  x4, #0, fail4
    tbz  x4, #11, fail4
    and  x5, x4, #0x7f
    movz x6, #0x1f
    cmp  x5, x6
    b.ne fail4
    movz x5, #0x0300
    tst  x4, x5
    b.ne fail4
    movz x5, #0xf000
    tst  x4, x5
    b.ne fail4
    ubfx x5, x4, #16, #32
    cbnz x5, fail4

    movz x2, #0
    msr  sctlr_el1, x2
    isb
    movz x0, #'P'
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
    b out
fail4:
    movz x0, #'4'

out:
    movz x2, #0
    msr  sctlr_el1, x2
    isb
    str  x0, [x1]
    brk  #0
