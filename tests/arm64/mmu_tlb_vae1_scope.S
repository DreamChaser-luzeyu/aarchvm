    .text
    .global _start
_start:
    movz x1, #0
    movk x1, #0x0900, lsl #16      // UART

    movz x9, #3                     // valid + table descriptor bits
    movz x15, #0x0403               // valid + page + AF
    movz x16, #0x0401               // valid block + AF

    // Tables
    movz x20, #0x1000               // L0
    movz x21, #0x2000               // L1
    movz x22, #0x3000               // L2
    movz x23, #0x4000               // L3

    // L0[0] -> L1. L1[0] keeps low VA identity; L1[1] covers 0x40000000.
    orr  x2, x21, x9
    str  x2, [x20]
    str  x16, [x21]
    orr  x2, x22, x9
    str  x2, [x21, #8]
    orr  x2, x23, x9
    str  x2, [x22]

    // L3[0] => VA 0x40000000 -> PA 0x500000 (value 0x11)
    movz x3, #0x0050, lsl #16
    orr  x2, x3, x15
    str  x2, [x23]

    // L3[1] => VA 0x40001000 -> PA 0x510000 (value 0x22)
    movz x3, #0x0051, lsl #16
    orr  x2, x3, x15
    str  x2, [x23, #8]

    // Alternate page for VA0 after remap: PA 0x520000 (value 0x33)
    movz x3, #0x0052, lsl #16
    orr  x24, x3, x15

    // Payload data
    movz x10, #0x0050, lsl #16
    movz x11, #0x11
    str  x11, [x10]
    movz x10, #0x0051, lsl #16
    movz x11, #0x22
    str  x11, [x10]
    movz x10, #0x0052, lsl #16
    movz x11, #0x33
    str  x11, [x10]

    msr  ttbr0_el1, x20
    movz x2, #16
    msr  tcr_el1, x2
    movz x2, #0
    msr  mair_el1, x2
    movz x2, #1
    msr  sctlr_el1, x2
    isb

    movz x30, #0x4000, lsl #16      // VA0
    ldr  x12, [x30]
    subs xzr, x12, #0x11
    b.ne fail

    add  x28, x30, #0x1000          // VA1
    ldr  x12, [x28]
    subs xzr, x12, #0x22
    b.ne fail

    // Remap VA0 to PA 0x520000 but no TLBI yet.
    str  x24, [x23]
    ldr  x12, [x30]
    subs xzr, x12, #0x11
    b.ne fail
    ldr  x12, [x28]
    subs xzr, x12, #0x22
    b.ne fail

    // Invalidate only VA0.
    lsr  x14, x30, #12
    tlbi vae1, x14
    dsb  sy
    isb

    ldr  x12, [x30]
    subs xzr, x12, #0x33
    b.ne fail
    ldr  x12, [x28]
    subs xzr, x12, #0x22
    b.ne fail

    movz x2, #0
    msr  sctlr_el1, x2
    isb
    movz x0, #'1'
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
