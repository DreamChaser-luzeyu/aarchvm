    .text
    .global _start
_start:
    movz x1, #0
    movk x1, #0x0900, lsl #16      // UART

    movz x9, #3
    movz x15, #0x0c03              // L3 attrs: valid + page + AF + nG
    movz x16, #0x0401              // L1 block attrs for low identity map

    // TTBR0: low 1GiB identity block for boot code/data.
    movz x20, #0x1000              // L0 low
    movz x21, #0x2000              // L1 low
    orr  x2, x21, x9
    str  x2, [x20]
    str  x16, [x21]

    // TTBR1 hierarchy for a tagged upper executable alias.
    movz x22, #0x5000              // L0 high
    movz x23, #0x6000              // L1 high
    movz x24, #0x7000              // L2 high code
    movz x25, #0x8000              // L3 high code

    orr  x2, x23, x9
    str  x2, [x22]                 // L0[0] -> L1 high
    orr  x2, x24, x9
    str  x2, [x23]                 // L1[0] -> code subtree
    orr  x2, x25, x9
    str  x2, [x24]                 // L2[0] -> L3 code

    // High alias initially points at PA 0x500000, then remaps to 0x600000.
    movz x3, #0x0050, lsl #16
    orr  x2, x3, x15
    str  x2, [x25]
    movz x3, #0x0060, lsl #16
    orr  x28, x3, x15

    // Code page A at PA 0x500000: movz x0, #'A'; ret
    movz x10, #0x0050, lsl #16
    movz x11, #0x0820
    movk x11, #0xD280, lsl #16
    str  w11, [x10]
    movz x12, #0x03c0
    movk x12, #0xD65F, lsl #16
    str  w12, [x10, #4]

    // Code page B at PA 0x600000: movz x0, #'B'; ret
    movz x10, #0x0060, lsl #16
    movz x11, #0x0840
    movk x11, #0xD280, lsl #16
    str  w11, [x10]
    str  w12, [x10, #4]

    msr  ttbr0_el1, x20
    msr  ttbr1_el1, x22
    movz x2, #16
    movk x2, #0x8010, lsl #16      // TG1=4KB (10b), T1SZ=16
    movk x2, #0x40, lsl #32        // TBI1=1
    msr  tcr_el1, x2
    movz x2, #0
    msr  mair_el1, x2
    movz x2, #1
    msr  sctlr_el1, x2
    isb

    // Tagged upper executable VA: top byte ignored, TTBR1 selected by bit[55].
    movz x19, #0
    movk x19, #0xabff, lsl #48

    // Fill fetch-side TLB with code page A.
    blr  x19
    str  x0, [x1]

    // Change the PTE but keep the stale cached fetch translation.
    str  x28, [x25]

    // Before TLBI, fetch should still observe the stale mapping and return A.
    blr  x19
    str  x0, [x1]

    // TLBI operand consumes VA[55:12], so the tag byte must be ignored.
    ubfx x14, x19, #12, #44
    tlbi vae1, x14
    dsb  sy
    isb

    // After TLBI, fetch should observe the remapped code page B.
    blr  x19
    str  x0, [x1]

    movz x2, #0
    msr  sctlr_el1, x2
    isb
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
