    .text
    .global _start
_start:
    movz x1, #0
    movk x1, #0x0900, lsl #16      // UART

    movz x9, #3                    // table descriptor bits
    movz x15, #0x0403              // page descriptor bits, AF=1
    movz x20, #0x1000              // L0
    movz x21, #0x2000              // L1
    movz x22, #0x3000              // L2 low
    movz x23, #0x4000              // L3 low
    movz x24, #0x7000              // L2 high
    movz x25, #0x8000              // L3 high

    // L0 -> L1
    orr  x2, x21, x9
    str  x2, [x20]

    // L1[0] -> low VA tables, L1[1] -> VA 0x40000000 region.
    orr  x2, x22, x9
    str  x2, [x21]
    orr  x2, x24, x9
    str  x2, [x21, #8]

    // Low VA identity map for code/data/table pages.
    orr  x2, x23, x9
    str  x2, [x22]
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

    // VA 0x40000000-0x40001fff -> discontiguous physical pages.
    orr  x2, x25, x9
    str  x2, [x24]
    movz x3, #0x0050, lsl #16      // PA 0x00500000
    orr  x2, x3, x15
    str  x2, [x25, #0]
    movz x3, #0x0070, lsl #16      // PA 0x00700000
    orr  x2, x3, x15
    str  x2, [x25, #8]

    // Enable MMU with 4KB granule, TTBR0 only. Attr0 is explicit Normal
    // memory so this test exercises cross-page RAM behaviour rather than
    // Device-memory alignment rules.
    msr  ttbr0_el1, x20
    movz x2, #16
    msr  tcr_el1, x2
    movz x2, #0x00ff
    msr  mair_el1, x2
    movz x2, #1
    msr  sctlr_el1, x2
    isb

    // STTR/LDTR 64-bit unaligned cross-page.
    movz x10, #0x4000, lsl #16
    add  x10, x10, #0x0ffc
    movz x11, #0x7788
    movk x11, #0x5566, lsl #16
    movk x11, #0x3344, lsl #32
    movk x11, #0x1122, lsl #48
    sttr x11, [x10]
    ldtr x12, [x10]
    cmp  x12, x11
    b.ne fail1

    // STP/LDP X pair across page boundary.
    movz x13, #0x4000, lsl #16
    add  x13, x13, #0x0ff8
    movz x14, #0xbbbb
    movk x14, #0xaaaa, lsl #16
    movk x14, #0x9999, lsl #32
    movk x14, #0x8888, lsl #48
    movz x15, #0xffff
    movk x15, #0xeeee, lsl #16
    movk x15, #0xdddd, lsl #32
    movk x15, #0xcccc, lsl #48
    stp  x14, x15, [x13]
    ldp  x16, x17, [x13]
    cmp  x16, x14
    b.ne fail2
    cmp  x17, x15
    b.ne fail2

    // STP/LDP W pair across page boundary.
    movz x18, #0x4000, lsl #16
    add  x18, x18, #0x0ffc
    movz w19, #0x1357
    movk w19, #0x2468, lsl #16
    movz w20, #0xabcd
    movk w20, #0xef01, lsl #16
    stp  w19, w20, [x18]
    ldp  w21, w22, [x18]
    cmp  w21, w19
    b.ne fail3
    cmp  w22, w20
    b.ne fail3

    // LDPSW cross-page signed extension. Re-seed values just before reading.
    movz x23, #0x4000, lsl #16
    add  x23, x23, #0x0ffc
    movz w24, #0x0001
    movk w24, #0x8000, lsl #16
    movz w25, #0xfffe
    movk w25, #0x7fff, lsl #16
    str  w24, [x23]
    str  w25, [x23, #4]
    ldpsw x24, x25, [x23]
    mov  x26, #2147483647
    neg  x26, x26                  // -2147483647
    cmp  x24, x26
    b.ne fail4
    mov  x26, #2147483646
    cmp  x25, x26
    b.ne fail4

    // LDXP/STXP across page boundary. Re-seed pair before exclusive sequence.
    movz x27, #0x4000, lsl #16
    add  x27, x27, #0x0ff8
    stp  x14, x15, [x27]
    ldxp x2, x3, [x27]
    cmp  x2, x14
    b.ne fail5
    cmp  x3, x15
    b.ne fail5
    movz x4, #0x2222
    movk x4, #0x1111, lsl #16
    movk x4, #0x4444, lsl #32
    movk x4, #0x3333, lsl #48
    movz x5, #0x6666
    movk x5, #0x5555, lsl #16
    movk x5, #0x8888, lsl #32
    movk x5, #0x7777, lsl #48
    stxp w6, x4, x5, [x27]
    cbnz w6, fail5
    ldp  x7, x8, [x27]
    cmp  x7, x4
    b.ne fail5
    cmp  x8, x5
    b.ne fail5

    movz x2, #0
    msr  sctlr_el1, x2
    isb

    movz x0, #'V'
    str  x0, [x1]
    movz x0, #'\n'
    str  x0, [x1]
    brk  #0

fail1:
    movz x0, #'1'
    b fail_out
fail2:
    movz x0, #'2'
    b fail_out
fail3:
    movz x0, #'3'
    b fail_out
fail4:
    movz x0, #'4'
    b fail_out
fail5:
    movz x0, #'5'
fail_out:
    movz x2, #0
    msr  sctlr_el1, x2
    isb
    str  x0, [x1]
    brk  #0
