    .text
    .global _start
_start:
    movz x1, #0
    movk x1, #0x0900, lsl #16      // UART

    // Constants
    movz x9, #3                     // descriptor bits

    // Table base addresses (physical)
    movz x20, #0x1000               // L0
    movz x21, #0x2000               // L1
    movz x22, #0x3000               // L2 low
    movz x23, #0x4000               // L3 low
    movz x24, #0x7000               // L2 high
    movz x25, #0x8000               // L3 high

    // L0[0] -> L1
    orr  x2, x21, x9
    str  x2, [x20]

    // L1[0] -> L2 low, L1[1] -> L2 high
    orr  x2, x22, x9
    str  x2, [x21]
    orr  x2, x24, x9
    str  x2, [x21, #8]

    // L2 low[0] -> L3 low ; L2 high[0] -> L3 high
    orr  x2, x23, x9
    str  x2, [x22]
    orr  x2, x25, x9
    str  x2, [x24]

    // L3 low identity map required pages (code + page tables): 0x0..0x8fff
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

    // L3 high[0]: VA 0x40000000 -> PA 0x500000 (initial)
    movz x3, #0x0050, lsl #16       // 0x500000
    orr  x2, x3, x9
    str  x2, [x25]

    // Prepare physical pages data at 0x500000/0x600000/0x700000
    movz x10, #0x0050, lsl #16
    movz x11, #0x11
    str  x11, [x10]

    movz x10, #0x0060, lsl #16
    movz x11, #0x22
    str  x11, [x10]

    movz x10, #0x0070, lsl #16
    movz x11, #0x33
    str  x11, [x10]

    // Program MMU-related registers
    msr  ttbr0_el1, x20
    movz x2, #16
    movk x2, #0x8010, lsl #16       // T0SZ=16, T1SZ=16, TG1=4KB
    msr  tcr_el1, x2
    movz x2, #0
    msr  mair_el1, x2

    // Read-back check of TTBR0
    mrs  x2, ttbr0_el1
    subs xzr, x2, x20
    b.ne fail

    // Enable MMU: SCTLR_EL1.M = 1
    movz x2, #1
    msr  sctlr_el1, x2
    isb

    // Test VA access at 0x40000000
    movz x30, #0x4000, lsl #16

    ldr  x12, [x30]
    subs xzr, x12, #0x11
    b.ne fail

    // Remap VA->PA 0x600000 but do not TLBI yet
    movz x3, #0x0060, lsl #16
    orr  x2, x3, x9
    str  x2, [x25]

    // Should still hit old TLB mapping (0x11)
    ldr  x12, [x30]
    subs xzr, x12, #0x11
    b.ne fail

    // Per-VA invalidate then should see 0x22
    tlbi vae1, x30
    dsb sy
    isb
    ldr  x12, [x30]
    subs xzr, x12, #0x22
    b.ne fail

    // Remap to 0x700000 and global invalidate
    movz x3, #0x0070, lsl #16
    orr  x2, x3, x9
    str  x2, [x25]

    tlbi vmalle1
    dsb sy
    isb
    ldr  x12, [x30]
    subs xzr, x12, #0x33
    b.ne fail

    // AT + PAR_EL1
    at   s1e1r, x30
    mrs  x13, par_el1
    movz x14, #0x0070, lsl #16
    subs xzr, x13, x14
    b.ne fail

    // Cache maintenance instructions (decode/execute)
    ic   iallu
    ic   ivau, x30
    dc   ivac, x30
    dc   cvac, x30
    dc   civac, x30

    movz x2, #0
    msr  sctlr_el1, x2
    isb

    movz x0, #'Q'
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
