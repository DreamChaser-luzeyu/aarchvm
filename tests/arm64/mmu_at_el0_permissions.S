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

    // VA 0x40000000 -> PA 0x500000, EL0 RW.
    movz x3, #0x0050, lsl #16
    movz x2, #0x0443
    orr  x26, x3, x2
    str  x26, [x25, #0]

    // VA 0x40001000 -> PA 0x600000, EL0 no access.
    movz x3, #0x0060, lsl #16
    movz x2, #0x0403
    orr  x26, x3, x2
    str  x26, [x25, #8]

    // VA 0x40002000 -> PA 0x700000, EL0 read-only.
    movz x3, #0x0070, lsl #16
    movz x2, #0x04c3
    orr  x26, x3, x2
    str  x26, [x25, #16]

    msr  ttbr0_el1, x20
    movz x2, #16
    msr  tcr_el1, x2
    movz x2, #0
    msr  mair_el1, x2
    movz x2, #1
    msr  sctlr_el1, x2
    isb

    movz x30, #0x4000, lsl #16
    add  x29, x30, #0x1000
    add  x28, x30, #0x2000

    // EL0 read on EL0-RW page succeeds.
    at   s1e0r, x30
    mrs  x4, par_el1
    tbnz x4, #0, fail
    tbz  x4, #11, fail
    ubfx x5, x4, #12, #44
    movz x6, #0x0500
    subs xzr, x5, x6
    b.ne fail
    lsr  x5, x4, #56
    cbnz x5, fail
    ubfx x5, x4, #7, #2
    movz x6, #2
    subs xzr, x5, x6
    b.ne fail

    // EL0 read on privileged-only page reports a permission fault in PAR_EL1.
    at   s1e0r, x29
    mrs  x4, par_el1
    and  x4, x4, #0x7f
    movz x5, #0x1f
    subs xzr, x4, x5
    b.ne fail

    // EL0 write on EL0-RW page succeeds.
    at   s1e0w, x30
    mrs  x4, par_el1
    tbnz x4, #0, fail
    tbz  x4, #11, fail
    ubfx x5, x4, #12, #44
    movz x6, #0x0500
    subs xzr, x5, x6
    b.ne fail
    lsr  x5, x4, #56
    cbnz x5, fail
    ubfx x5, x4, #7, #2
    movz x6, #2
    subs xzr, x5, x6
    b.ne fail

    // EL0 write on EL0-RO page reports the same permission-fault FST in PAR_EL1.
    at   s1e0w, x28
    mrs  x4, par_el1
    and  x4, x4, #0x7f
    movz x5, #0x1f
    subs xzr, x4, x5
    b.ne fail

    movz x2, #0
    msr  sctlr_el1, x2
    isb
    movz x0, #'A'
    str  x0, [x1]
    movz x0, #'\n'
    str  x0, [x1]
    brk  #0

fail:
    movz x2, #0
    msr  sctlr_el1, x2
    isb
    movz x0, #'F'
    str  x0, [x1]
    brk  #0
