    .text
    .global _start
_start:
    movz x1, #0
    movk x1, #0x0900, lsl #16      // UART

    movz x9, #3                    // table descriptor bits
    movz x15, #0x0403              // page descriptor bits, AF=1
    movz x16, #0x0401              // block descriptor bits, AF=1
    movz x20, #0x1000              // L0
    movz x21, #0x2000              // L1
    movz x22, #0x3000              // L2 low
    movz x23, #0x4000              // L3 low
    movz x24, #0x7000              // L2 high
    movz x25, #0x8000              // L3 high

    // L0 -> L1
    orr  x2, x21, x9
    str  x2, [x20]

    // L1[0] -> low VA tables, L1[1] -> VA 0x40000000 region.
    orr  x2, x22, x9
    str  x2, [x21]
    orr  x2, x24, x9
    str  x2, [x21, #8]

    // Low VA identity map for code/data/table pages.
    orr  x2, x23, x9
    str  x2, [x22]
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

    // VA 0x40000000-0x40001fff -> discontiguous physical pages.
    orr  x2, x25, x9
    str  x2, [x24]
    movz x3, #0x0050, lsl #16      // PA 0x00500000
    orr  x2, x3, x15
    str  x2, [x25, #0]
    movz x3, #0x0070, lsl #16      // PA 0x00700000
    orr  x2, x3, x15
    str  x2, [x25, #8]

    // Enable MMU with 4KB granule, TTBR0 only. Attr0 is explicit Normal
    // memory so this test exercises cross-page RAM behaviour rather than
    // Device-memory alignment rules.
    msr  ttbr0_el1, x20
    movz x2, #16
    msr  tcr_el1, x2
    movz x2, #0x00ff
    msr  mair_el1, x2
    movz x2, #1
    msr  sctlr_el1, x2
    isb

    // Store eight bytes across the 0x40000fff/0x40001000 page boundary.
    movz x10, #0x4000, lsl #16
    add  x10, x10, #0x0ffc
    movz x11, #0x7788
    movk x11, #0x5566, lsl #16
    movk x11, #0x3344, lsl #32
    movk x11, #0x1122, lsl #48
    str  x11, [x10]
    ldr  x12, [x10]
    subs xzr, x12, x11
    b.ne fail

    // Disable MMU and inspect the physical backing pages directly.
    movz x2, #0
    msr  sctlr_el1, x2
    isb

    movz x13, #0x0050, lsl #16
    add  x13, x13, #0x0ffc
    ldr  w14, [x13]
    movz x15, #0x7788
    movk x15, #0x5566, lsl #16
    subs xzr, x14, x15
    b.ne fail

    movz x13, #0x0070, lsl #16
    ldr  w14, [x13]
    movz x15, #0x3344
    movk x15, #0x1122, lsl #16
    subs xzr, x14, x15
    b.ne fail

    movz x13, #0x0051, lsl #16
    ldr  w14, [x13]
    cbnz x14, fail

    movz x0, #'C'
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
