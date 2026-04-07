    .text
    .global _start
_start:
    movz x1, #0
    movk x1, #0x0900, lsl #16      // UART

    movz x9, #3
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

    movz x15, #0x0403

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

    // VA 0x40000000 -> PA 0x1_0000_0000, AF=1, AttrIdx=1.
    movz x3, #0x0407
    movk x3, #0x0001, lsl #32
    str  x3, [x25, #0]

    // VA 0x40001000 unmapped => translation fault.
    str  xzr, [x25, #8]

    // VA 0x40002000 -> PA 0x00500000, AF=0 => access flag fault.
    movz x3, #0x0050, lsl #16
    movz x2, #0x0003
    orr  x3, x3, x2
    str  x3, [x25, #16]

    // VA 0x40003000 -> PA 0x00600000, EL0 no access => permission fault.
    movz x3, #0x0060, lsl #16
    movz x2, #0x0403
    orr  x3, x3, x2
    str  x3, [x25, #24]

    // Attr1 = 0xff, attr0 = 0x00.
    movz x2, #0xff00
    msr  mair_el1, x2

    // TCR: T0SZ=16, IPS=32-bit.
    movz x2, #16
    msr  ttbr0_el1, x20
    msr  tcr_el1, x2
    movz x2, #1
    msr  sctlr_el1, x2
    isb

    movz x30, #0x4000, lsl #16
    add  x29, x30, #0x1000
    add  x28, x30, #0x2000
    add  x27, x30, #0x3000

    at   s1e1r, x30
    mrs  x4, par_el1
    tbz  x4, #0, fail
    tbz  x4, #11, fail
    and  x5, x4, #0x7f
    movz x6, #0x07
    cmp  x5, x6
    b.ne fail
    movz x5, #0x0700
    tst  x4, x5
    b.ne fail
    ubfx x5, x4, #16, #48
    cbnz x5, fail

    at   s1e1r, x29
    mrs  x4, par_el1
    tbz  x4, #0, fail
    tbz  x4, #11, fail
    and  x5, x4, #0x7f
    movz x6, #0x0f
    cmp  x5, x6
    b.ne fail
    movz x5, #0x0700
    tst  x4, x5
    b.ne fail
    ubfx x5, x4, #16, #48
    cbnz x5, fail

    at   s1e1r, x28
    mrs  x4, par_el1
    tbz  x4, #0, fail
    tbz  x4, #11, fail
    and  x5, x4, #0x7f
    movz x6, #0x17
    cmp  x5, x6
    b.ne fail
    movz x5, #0x0700
    tst  x4, x5
    b.ne fail
    ubfx x5, x4, #16, #48
    cbnz x5, fail

    at   s1e0r, x27
    mrs  x4, par_el1
    tbz  x4, #0, fail
    tbz  x4, #11, fail
    and  x5, x4, #0x7f
    movz x6, #0x1f
    cmp  x5, x6
    b.ne fail
    movz x5, #0x0700
    tst  x4, x5
    b.ne fail
    ubfx x5, x4, #16, #48
    cbnz x5, fail

    movz x2, #0
    msr  sctlr_el1, x2
    isb
    movz x0, #'G'
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
