    .text
    .global _start
_start:
    movz x1, #0
    movk x1, #0x0900, lsl #16      // UART

    adr  x10, vectors
    msr  vbar_el1, x10

    movz x9, #3
    movz x15, #0x0403
    movz x20, #0x1000              // L0
    movz x21, #0x2000              // L1
    movz x22, #0x3000              // L2 low
    movz x23, #0x4000              // L3 low
    movz x24, #0x7000              // L2 high
    movz x25, #0x8000              // L3 high

    orr  x2, x21, x9
    str  x2, [x20]
    orr  x2, x22, x9
    str  x2, [x21]
    orr  x2, x24, x9
    str  x2, [x21, #8]
    orr  x2, x23, x9
    str  x2, [x22]
    orr  x2, x25, x9
    str  x2, [x24]

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

    // VA 0x40000000 -> PA 0x500000, AttrIdx=0, AF=1.
    movz x3, #0x0050, lsl #16
    movz x2, #0x0403
    orr  x2, x2, x3
    str  x2, [x25]

    // Seed the backing RAM with non-zero data before enabling the MMU.
    movz x26, #0x0050, lsl #16
    movz x27, #0x7788
    movk x27, #0x5566, lsl #16
    movk x27, #0x3344, lsl #32
    movk x27, #0x1122, lsl #48
    str  x27, [x26]
    movz x28, #0xff00
    movk x28, #0xddee, lsl #16
    movk x28, #0xbbcc, lsl #32
    movk x28, #0x99aa, lsl #48
    str  x28, [x26, #8]

    msr  ttbr0_el1, x20
    movz x2, #16
    msr  tcr_el1, x2

    // AttrIdx0 initially describes Device memory. The first load fills the TLB
    // with that attribute.
    movz x2, #0
    msr  mair_el1, x2
    movz x2, #1
    msr  sctlr_el1, x2
    isb

    movz x30, #0x4000, lsl #16
    ldr  x4, [x30]

    // After the MAIR_EL1 write + ISB, subsequent instructions must observe the
    // new Normal-memory attribute rather than a stale TLB-cached Device type.
    movz x2, #0x00ff
    msr  mair_el1, x2
    isb

    dc   zva, x30

    ldr  x5, [x30]
    cbnz x5, fail
    ldr  x6, [x30, #8]
    cbnz x6, fail

    movz x2, #0
    msr  sctlr_el1, x2
    isb
    movz x0, #'R'
    str  x0, [x1]
    movz x0, #'\n'
    str  x0, [x1]
1:
    b    1b

fail:
    movz x2, #0
    msr  sctlr_el1, x2
    isb
    movz x0, #'F'
    str  x0, [x1]
1:
    b    1b

    .balign 2048
vectors:
    .space 0x200
sync_spx:
    b fail
