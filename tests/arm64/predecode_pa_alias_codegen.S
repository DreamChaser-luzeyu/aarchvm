    .text
    .global _start
_start:
    movz x1, #0
    movk x1, #0x0900, lsl #16      // UART

    movz x9, #3
    movz x15, #0x0403              // EL1 RW page, AF=1.
    movz x16, #0x0483              // EL1 RO page, AF=1.
    movz x17, #0x0401              // L1 block attrs, AF=1.

    movz x20, #0x1000              // L0
    movz x21, #0x2000              // L1
    movz x22, #0x3000              // L2 low
    movz x23, #0x4000              // L3 low
    movz x24, #0x7000              // L2 high
    movz x25, #0x8000              // L3 high

    orr  x2, x21, x9
    str  x2, [x20]
    str  x17, [x21]
    orr  x2, x24, x9
    str  x2, [x21, #8]
    orr  x2, x25, x9
    str  x2, [x24]

    // Code page at PA 0x00500000: movz x0, #'A' / ret.
    movz x10, #0x0050, lsl #16
    movz x11, #0x0820
    movk x11, #0xD280, lsl #16
    str  w11, [x10]
    movz x12, #0x03C0
    movk x12, #0xD65F, lsl #16
    str  w12, [x10, #4]

    // High aliases:
    //   VA 0x40001000 -> PA 0x00500000, executable RO
    //   VA 0x40002000 -> PA 0x00500000, writable alias used for patching
    movz x3, #0x0050, lsl #16
    orr  x2, x3, x16
    str  x2, [x25, #8]
    movz x3, #0x0050, lsl #16
    orr  x2, x3, x15
    str  x2, [x25, #16]

    msr  ttbr0_el1, x20
    movz x2, #16
    msr  tcr_el1, x2
    movz x2, #0
    msr  mair_el1, x2
    movz x2, #1
    msr  sctlr_el1, x2
    isb

    movz x20, #0x4000, lsl #16
    add  x20, x20, #0x1000        // executable alias
    movz x21, #0x4000, lsl #16
    add  x21, x21, #0x2000        // writable alias

    blr  x20
    str  x0, [x1]

    // Patch via the writable alias and perform the architected cache
    // maintenance sequence on that alias VA.
    movz x11, #0x0840
    movk x11, #0xD280, lsl #16    // movz x0, #'B'
    str  w11, [x21]
    dc   cvau, x21
    dsb  sy
    ic   ivau, x21
    dsb  sy
    isb

    blr  x20
    str  x0, [x1]

    movz x2, #0
    msr  sctlr_el1, x2
    isb
    movz x0, #'\n'
    str  x0, [x1]
    brk  #0
