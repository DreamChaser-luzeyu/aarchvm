    .text
    .global _start
_start:
    movz x1, #0
    movk x1, #0x0900, lsl #16      // UART

    movz x9, #3
    movz x15, #0x0403              // Valid + AF + AttrIdx0

    movz x20, #0x1000              // L0
    movz x21, #0x2000              // L1
    movz x22, #0x3000              // L2 low
    movz x23, #0x4000              // L3 low identity
    movz x24, #0x7000              // L3 high remap

    orr  x2, x21, x9
    str  x2, [x20]
    orr  x2, x22, x9
    str  x2, [x21]
    orr  x2, x23, x9
    str  x2, [x22]
    orr  x2, x24, x9
    str  x2, [x22, #64]            // L2 idx 8 => VA 0x01000000

    // Identity-map the low VA range used by the test harness itself.
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

    // VA 0x01000000 -> PA 0x00500000 while the MMU is enabled.
    movz x3, #0x0050, lsl #16
    orr  x2, x3, x15
    str  x2, [x24]

    // Backing data for the translated mapping.
    movz x10, #0x0050, lsl #16
    movz x11, #0x11
    str  x11, [x10]

    // Backing data for the direct-mapped MMU-off path.
    movz x12, #0x0100, lsl #16     // 0x01000000
    movz x13, #0x44
    str  x13, [x12]

    msr  ttbr0_el1, x20
    movz x2, #16
    msr  tcr_el1, x2
    movz x2, #0
    msr  mair_el1, x2
    movz x2, #1
    msr  sctlr_el1, x2
    isb

    movz x30, #0x0100, lsl #16     // Test VA 0x01000000

    // MMU on: use the translated mapping.
    ldr  x4, [x30]
    subs xzr, x4, #0x11
    b.ne fail

    // MMU off: the stale TLB entry must not survive. The same VA now uses the
    // direct-mapped stage-1-disabled output address.
    msr  sctlr_el1, xzr
    isb
    ldr  x4, [x30]
    subs xzr, x4, #0x44
    b.ne fail

    // MMU on again: translation must be re-established, not left in MMU-off
    // direct-map mode.
    movz x2, #1
    msr  sctlr_el1, x2
    isb
    ldr  x4, [x30]
    subs xzr, x4, #0x11
    b.ne fail

    msr  sctlr_el1, xzr
    isb
    movz x0, #'T'
    str  x0, [x1]
    movz x0, #'\n'
    str  x0, [x1]
1:
    b    1b

fail:
    msr  sctlr_el1, xzr
    isb
    movz x0, #'F'
    str  x0, [x1]
1:
    b    1b
