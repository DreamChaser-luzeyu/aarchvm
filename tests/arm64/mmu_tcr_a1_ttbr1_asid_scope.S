    .text
    .global _start
_start:
    movz x1, #0
    movk x1, #0x0900, lsl #16      // UART

    movz x9, #3
    movz x15, #0x0c03              // valid + AF + nG
    movz x16, #0x0401              // L1 block attrs for low identity map

    // Table set A.
    movz x20, #0x1000              // L0A
    movz x21, #0x2000              // L1A
    movz x22, #0x3000              // L2A
    movz x23, #0x4000              // L3A
    // Table set B.
    movz x24, #0x5000              // L0B
    movz x25, #0x6000              // L1B
    movz x26, #0x7000              // L2B
    movz x27, #0x8000              // L3B
    // Dummy TTBR1 root: only the ASID matters in this test.
    movz x19, #0x9000

    orr  x2, x21, x9
    str  x2, [x20]
    str  x16, [x21]
    orr  x2, x22, x9
    str  x2, [x21, #8]
    orr  x2, x23, x9
    str  x2, [x22]

    orr  x2, x25, x9
    str  x2, [x24]
    str  x16, [x25]
    orr  x2, x26, x9
    str  x2, [x25, #8]
    orr  x2, x27, x9
    str  x2, [x26]

    // Same low VA in two TTBR0 table sets.
    movz x3, #0x0050, lsl #16
    orr  x2, x3, x15
    str  x2, [x23]                 // Table set A: VA 0x40000000 -> PA 0x500000
    movz x3, #0x0060, lsl #16
    orr  x2, x3, x15
    str  x2, [x27]                 // Table set B: VA 0x40000000 -> PA 0x600000

    movz x10, #0x0050, lsl #16
    movz x11, #0x11
    str  x11, [x10]
    movz x10, #0x0060, lsl #16
    movz x11, #0x22
    str  x11, [x10]

    // TTBR0 ASIDs differ, but with TCR.A1=1 they must not affect TLB matching.
    movz x4, #0x1111, lsl #48
    orr  x4, x4, x20               // TTBR0 A
    movz x5, #0x2222, lsl #48
    orr  x5, x5, x24               // TTBR0 B

    // TTBR1 ASID must be the translation ASID source when A1=1.
    movz x6, #1, lsl #48
    orr  x6, x6, x19               // TTBR1 dummy root, ASID 1
    movz x7, #2, lsl #48
    orr  x7, x7, x19               // TTBR1 dummy root, ASID 2

    msr  ttbr0_el1, x4
    msr  ttbr1_el1, x6
    movz x2, #16
    movk x2, #0x8050, lsl #16      // TG1=4KB, A1=1, T1SZ=16
    msr  tcr_el1, x2
    movz x2, #0
    msr  mair_el1, x2
    movz x2, #1
    msr  sctlr_el1, x2
    isb

    movz x30, #0x4000, lsl #16

    // Fill the TLB using TTBR0 A, but ASID=TTBR1.ASID=1.
    ldr  x12, [x30]
    subs xzr, x12, #0x11
    b.ne fail

    // TTBR0 changes alone must not change the cached translation while A1=1.
    msr  ttbr0_el1, x5
    isb
    ldr  x12, [x30]
    subs xzr, x12, #0x11
    b.ne fail

    // Switching TTBR1 ASID must retag the lookup and force a new walk.
    msr  ttbr1_el1, x7
    isb
    ldr  x12, [x30]
    subs xzr, x12, #0x22
    b.ne fail

    // With the new ASID cached, TTBR0 changes alone still do not matter.
    msr  ttbr0_el1, x4
    isb
    ldr  x12, [x30]
    subs xzr, x12, #0x22
    b.ne fail

    // Restoring TTBR1 ASID 1 must pick up the old ASID-1 cached translation.
    msr  ttbr1_el1, x6
    isb
    ldr  x12, [x30]
    subs xzr, x12, #0x11
    b.ne fail

    movz x2, #0
    msr  sctlr_el1, x2
    isb
    movz x0, #'I'
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
