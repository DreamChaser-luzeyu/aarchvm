    .text
    .global _start
_start:
    movz x1, #0
    movk x1, #0x0900, lsl #16      // UART

    movz x9, #3                    // valid + table/page bits
    movz x15, #0x0403

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

    // Identity map low pages for code + page tables.
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

    // VA 0x40000000 -> PA 0x500000 initially, then remap to PA 0x520000.
    movz x3, #0x0050, lsl #16
    orr  x2, x3, x15
    str  x2, [x25]
    movz x3, #0x0052, lsl #16
    orr  x26, x3, x15

    movz x10, #0x0050, lsl #16
    movz x11, #0x11
    str  x11, [x10]
    movz x10, #0x0052, lsl #16
    movz x11, #0x33
    str  x11, [x10]

    msr  ttbr0_el1, x20
    movz x2, #16
    msr  tcr_el1, x2
    movz x2, #0
    msr  mair_el1, x2
    movz x2, #1
    msr  sctlr_el1, x2
    isb

    movz x30, #0x4000, lsl #16
    ldr  x12, [x30]
    subs xzr, x12, #0x11
    b.ne fail

    // Keep a stale TLB entry, then try a non-architected raw-VA TLBI operand.
    str  x26, [x25]
    tlbi vae1, x30
    dsb  sy
    isb

    ldr  x12, [x30]
    subs xzr, x12, #0x11
    b.ne fail

    // The architected operand is VA[55:12].
    lsr  x14, x30, #12
    tlbi vae1, x14
    dsb  sy
    isb

    ldr  x12, [x30]
    subs xzr, x12, #0x33
    b.ne fail

    movz x2, #0
    msr  sctlr_el1, x2
    isb
    movz x0, #'E'
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
