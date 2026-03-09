    .text
    .global _start
_start:
    movz x1, #0
    movk x1, #0x0900, lsl #16      // UART

    movz x9, #3                     // valid + table/page descriptor bits

    // Tables
    movz x20, #0x1000               // L0
    movz x21, #0x2000               // L1
    movz x22, #0x3000               // L2
    movz x23, #0x4000               // L3

    // L0[0] -> L1, L1[1] -> L2 (VA 0x40000000), L2[0] -> L3
    orr  x2, x21, x9
    str  x2, [x20]
    orr  x2, x22, x9
    str  x2, [x21, #8]
    orr  x2, x23, x9
    str  x2, [x22]

    // L3[0] => VA 0x40000000 -> PA 0x500000 (value 0x11)
    movz x3, #0x0050, lsl #16
    orr  x2, x3, x9
    str  x2, [x23]

    // L3[1] => VA 0x40001000 -> PA 0x510000 (value 0x22)
    movz x3, #0x0051, lsl #16
    orr  x2, x3, x9
    str  x2, [x23, #8]

    // Alternate page for VA0 after remap: PA 0x520000 (value 0x33)
    movz x3, #0x0052, lsl #16
    orr  x24, x3, x9

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

    // MMU on (4KB granule, TTBR0 only)
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
    subs xzr, x12, #0x11            // old TLB mapping must still hit
    b.ne fail
    ldr  x12, [x28]
    subs xzr, x12, #0x22            // untouched page stays unchanged
    b.ne fail

    // Invalidate only VA0.
    tlbi vae1, x30
    dsb  sy
    isb

    ldr  x12, [x30]
    subs xzr, x12, #0x33
    b.ne fail
    ldr  x12, [x28]
    subs xzr, x12, #0x22
    b.ne fail

    movz x0, #'1'
    str  x0, [x1]
    movz x0, #'\n'
    str  x0, [x1]
    brk  #0

fail:
    movz x0, #'F'
    str  x0, [x1]
    brk  #0
