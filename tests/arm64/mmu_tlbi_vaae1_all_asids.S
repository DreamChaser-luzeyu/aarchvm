    .text
    .global _start
_start:
    movz x1, #0
    movk x1, #0x0900, lsl #16      // UART

    movz x9, #3
    movz x15, #0x0c03              // valid + AF + nG
    movz x16, #0x0401

    // Table set A
    movz x20, #0x1000              // L0A
    movz x21, #0x2000              // L1A
    movz x22, #0x3000              // L2A
    movz x23, #0x4000              // L3A
    // Table set B
    movz x24, #0x5000              // L0B
    movz x25, #0x6000              // L1B
    movz x26, #0x7000              // L2B
    movz x27, #0x8000              // L3B

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

    // ASID 1 target: VA 0x40000000 -> PA 0x500000, later remap to 0x700000.
    movz x3, #0x0050, lsl #16
    orr  x2, x3, x15
    str  x2, [x23]
    movz x3, #0x0070, lsl #16
    orr  x28, x3, x15

    // ASID 2 target: VA 0x40000000 -> PA 0x600000, later remap to 0x800000.
    movz x3, #0x0060, lsl #16
    orr  x2, x3, x15
    str  x2, [x27]
    movz x3, #0x0080, lsl #16
    orr  x29, x3, x15

    movz x10, #0x0050, lsl #16
    movz x11, #0x11
    str  x11, [x10]
    movz x10, #0x0060, lsl #16
    movz x11, #0x22
    str  x11, [x10]
    movz x10, #0x0070, lsl #16
    movz x11, #0x33
    str  x11, [x10]
    movz x10, #0x0080, lsl #16
    movz x11, #0x44
    str  x11, [x10]

    movz x4, #1, lsl #48
    orr  x4, x4, x20               // TTBR0 A, ASID 1
    movz x5, #2, lsl #48
    orr  x5, x5, x24               // TTBR0 B, ASID 2
    movz x6, #1, lsl #48           // Operand carries ASID 1, but VAAE1 must ignore it

    msr  ttbr0_el1, x4
    movz x2, #16
    msr  tcr_el1, x2
    movz x2, #0
    msr  mair_el1, x2
    movz x2, #1
    msr  sctlr_el1, x2
    isb

    movz x30, #0x4000, lsl #16
    lsr  x7, x30, #12
    orr  x6, x6, x7

    ldr  x12, [x30]
    subs xzr, x12, #0x11
    b.ne fail

    msr  ttbr0_el1, x5
    isb
    ldr  x12, [x30]
    subs xzr, x12, #0x22
    b.ne fail

    // Update both page tables without invalidating cached translations yet.
    str  x28, [x23]
    str  x29, [x27]

    .inst 0xD5088766              // TLBI VAAE1, X6
    dsb  sy
    isb

    msr  ttbr0_el1, x4
    isb
    ldr  x12, [x30]
    subs xzr, x12, #0x33
    b.ne fail

    msr  ttbr0_el1, x5
    isb
    ldr  x12, [x30]
    subs xzr, x12, #0x44
    b.ne fail

    movz x2, #0
    msr  sctlr_el1, x2
    isb
    movz x0, #'Y'
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
