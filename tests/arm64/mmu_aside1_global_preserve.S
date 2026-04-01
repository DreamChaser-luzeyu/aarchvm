    .text
    .global _start
_start:
    movz x1, #0
    movk x1, #0x0900, lsl #16      // UART

    movz x9, #3
    movz x15, #0x0403              // valid + AF, global entry (nG=0)
    movz x16, #0x0401

    movz x20, #0x1000              // L0
    movz x21, #0x2000              // L1
    movz x22, #0x3000              // L2
    movz x23, #0x4000              // L3

    orr  x2, x21, x9
    str  x2, [x20]
    str  x16, [x21]
    orr  x2, x22, x9
    str  x2, [x21, #8]
    orr  x2, x23, x9
    str  x2, [x22]

    // Global leaf entry for VA 0x40000000.
    movz x3, #0x0050, lsl #16
    orr  x2, x3, x15
    str  x2, [x23]
    movz x3, #0x0052, lsl #16
    orr  x24, x3, x15

    movz x10, #0x0050, lsl #16
    movz x11, #0x11
    str  x11, [x10]
    movz x10, #0x0052, lsl #16
    movz x11, #0x33
    str  x11, [x10]

    movz x4, #1, lsl #48
    orr  x4, x4, x20
    movz x6, #1, lsl #48

    msr  ttbr0_el1, x4
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

    // Update the global leaf but invalidate by ASID only: the stale TLB entry
    // must survive because ASIDE1 does not target final-level global entries.
    str  x24, [x23]
    tlbi aside1, x6
    dsb  sy
    isb

    ldr  x12, [x30]
    subs xzr, x12, #0x11
    b.ne fail

    tlbi vmalle1
    dsb  sy
    isb

    ldr  x12, [x30]
    subs xzr, x12, #0x33
    b.ne fail

    movz x2, #0
    msr  sctlr_el1, x2
    isb
    movz x0, #'G'
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
