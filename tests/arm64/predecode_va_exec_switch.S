    .text
    .global _start
_start:
    movz x1, #0
    movk x1, #0x0900, lsl #16      // UART

    movz x9, #3
    movz x15, #0x0403              // L3 page attrs
    movz x16, #0x0401              // L1 block attrs

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

    // Code page at PA 0x500000: movz x0, #'A'; ret
    movz x10, #0x0050, lsl #16
    movz x11, #0x0820
    movk x11, #0xD280, lsl #16
    str  w11, [x10]
    movz x12, #0x03C0
    movk x12, #0xD65F, lsl #16
    str  w12, [x10, #4]

    // Code page at PA 0x600000: movz x0, #'B'; ret
    movz x10, #0x0060, lsl #16
    movz x11, #0x0840
    movk x11, #0xD280, lsl #16
    str  w11, [x10]
    str  w12, [x10, #4]

    msr  ttbr0_el1, x20
    movz x2, #16
    msr  tcr_el1, x2
    movz x2, #0
    msr  mair_el1, x2
    movz x2, #1
    msr  sctlr_el1, x2
    isb

    movz x19, #0x4000, lsl #16
    blr  x19
    str  x0, [x1]

    msr  ttbr0_el1, x24
    tlbi vmalle1
    dsb  sy
    isb
    blr  x19
    str  x0, [x1]

    msr  ttbr0_el1, x20
    tlbi vmalle1
    dsb  sy
    isb
    blr  x19
    str  x0, [x1]

    movz x2, #0
    msr  sctlr_el1, x2
    isb
    movz x0, #'\n'
    str  x0, [x1]
    brk  #0
