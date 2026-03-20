    .text
    .global _start
_start:
    movz x1, #0
    movk x1, #0x0900, lsl #16      // UART

    movz x9, #3
    movz x15, #0x0403               // valid page + AF
    movz x20, #0x1000               // L0
    movz x21, #0x2000               // L1
    movz x22, #0x3000               // L2 low
    movz x23, #0x4000               // L3 low
    movz x24, #0x7000               // L2 high
    movz x25, #0x8000               // L3 high

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

    // VA 0x40000000 -> PA 0x1_0000_0000, AttrIdx=1, SH=InnerShareable.
    movz x3, #0x0707                // AttrIdx=1 + AF + SH=0b00? fixed below by high bits on PA
    movk x3, #0x0001, lsl #32
    movz x4, #0x0300                // SH = InnerShareable
    orr  x3, x3, x4
    str  x3, [x25]

    // MAIR attr1 = 0xff (normal WB WA cacheable), attr0 = 0x00.
    movz x2, #0xff00
    msr  mair_el1, x2

    // TCR: T0SZ=16, IRGN0=WBWA, ORGN0=WBWA, SH0=InnerShareable, IPS=32-bit initially.
    movz x2, #0x3510
    msr  ttbr0_el1, x20
    msr  tcr_el1, x2
    movz x2, #1
    msr  sctlr_el1, x2
    isb

    movz x30, #0x4000, lsl #16

    // AT must fail because TCR.IPS=32 but PA uses bit 32.
    at   s1e1r, x30
    mrs  x13, par_el1
    tbnz x13, #0, par_fault_ok
    b    fail
par_fault_ok:

    // Widen IPS to 48-bit and retry.
    movz x2, #0x3510
    movk x2, #0x0005, lsl #32
    msr  tcr_el1, x2
    isb

    at   s1e1r, x30
    mrs  x13, par_el1
    tbnz x13, #0, fail
    tbz  x13, #11, fail
    ubfx x14, x13, #12, #44
    movz x15, #0x0010, lsl #16
    subs xzr, x14, x15
    b.ne fail
    lsr  x14, x13, #56
    movz x15, #0x00ff
    subs xzr, x14, x15
    b.ne fail
    ubfx x14, x13, #7, #2
    movz x15, #3
    subs xzr, x14, x15
    b.ne fail

    movz x2, #0
    msr  sctlr_el1, x2
    isb
    movz x0, #'Z'
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
