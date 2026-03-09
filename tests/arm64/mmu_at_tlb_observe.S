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

    // Keep low VA identity for code + page tables.
    movz x3, #0x0000
    orr  x2, x3, x9
    str  x2, [x23, #0]
    movz x3, #0x1000
    orr  x2, x3, x9
    str  x2, [x23, #8]
    movz x3, #0x2000
    orr  x2, x3, x9
    str  x2, [x23, #16]
    movz x3, #0x3000
    orr  x2, x3, x9
    str  x2, [x23, #24]
    movz x3, #0x4000
    orr  x2, x3, x9
    str  x2, [x23, #32]
    movz x3, #0x7000
    orr  x2, x3, x9
    str  x2, [x23, #56]
    movz x3, #0x8000
    orr  x2, x3, x9
    str  x2, [x23, #64]

    // Initial: VA -> PA 0x500000 (0x19)
    movz x3, #0x0050, lsl #16
    orr  x2, x3, x9
    str  x2, [x25]

    // Alternate: VA -> PA 0x520000 (0x2A)
    movz x3, #0x0052, lsl #16
    orr  x26, x3, x9

    movz x10, #0x0050, lsl #16
    movz x11, #0x19
    str  x11, [x10]
    movz x10, #0x0052, lsl #16
    movz x11, #0x2A
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

    // Fill TLB.
    ldr  x12, [x30]
    subs xzr, x12, #0x19
    b.ne fail

    // Update PTE but keep stale TLB entry.
    str  x26, [x25]

    // AT currently consults software TLB first; expect stale mapping pre-TLBI.
    at   s1e1r, x30
    mrs  x13, par_el1
    movz x14, #0x0050, lsl #16
    subs xzr, x13, x14
    b.ne fail

    // Data access should still observe old mapping before TLBI.
    ldr  x12, [x30]
    subs xzr, x12, #0x19
    b.ne fail

    tlbi vae1, x30
    dsb  sy
    isb

    at   s1e1r, x30
    mrs  x13, par_el1
    movz x14, #0x0052, lsl #16
    subs xzr, x13, x14
    b.ne fail

    ldr  x12, [x30]
    subs xzr, x12, #0x2A
    b.ne fail

    movz x2, #0
    msr  sctlr_el1, x2
    isb
    movz x0, #'6'
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
