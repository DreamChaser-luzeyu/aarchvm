    .text
    .global _start
_start:
    movz x1, #0
    movk x1, #0x0900, lsl #16      // UART

    movz x9, #3
    movz x15, #0x0403
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

    // A hierarchy: low VA identity + VA 0x40000000 -> PA 0x500000
    orr  x2, x21, x9
    str  x2, [x20]
    str  x16, [x21]
    orr  x2, x22, x9
    str  x2, [x21, #8]
    orr  x2, x23, x9
    str  x2, [x22]
    movz x3, #0x0050, lsl #16
    orr  x2, x3, x15
    str  x2, [x23]

    // B hierarchy: low VA identity + VA 0x40000000 -> PA 0x600000
    orr  x2, x25, x9
    str  x2, [x24]
    str  x16, [x25]
    orr  x2, x26, x9
    str  x2, [x25, #8]
    orr  x2, x27, x9
    str  x2, [x26]
    movz x3, #0x0060, lsl #16
    orr  x2, x3, x15
    str  x2, [x27]

    // Payload pages
    movz x10, #0x0050, lsl #16
    movz x11, #0x11
    str  x11, [x10]
    movz x10, #0x0060, lsl #16
    movz x11, #0x44
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

    msr  ttbr0_el1, x24
    isb
    ldr  x12, [x30]
    subs xzr, x12, #0x44
    b.ne fail

    msr  ttbr0_el1, x20
    isb
    ldr  x12, [x30]
    subs xzr, x12, #0x11
    b.ne fail

    movz x2, #0
    msr  sctlr_el1, x2
    isb
    movz x0, #'2'
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
