    .text
    .global _start
_start:
    // UART base: 0x09000000
    movz x1, #0x0000
    movk x1, #0x0900, lsl #16

    // Plugin MMIO base: 0x09050000
    movz x2, #0x0000
    movk x2, #0x0905, lsl #16

    // Read ID register, expect "PLUG" (0x504c5547)
    ldr  w3, [x2, #0x0]
    movz w4, #0x5547
    movk w4, #0x504c, lsl #16
    cmp  w3, w4
    b.ne fail

    // Read version register, expect 0x00010000
    ldr  w3, [x2, #0x4]
    movz w4, #0x0000
    movk w4, #0x0001, lsl #16
    cmp  w3, w4
    b.ne fail

    // Write and read back scratch0.
    movz x5, #0x7788
    movk x5, #0x5566, lsl #16
    movk x5, #0x3344, lsl #32
    movk x5, #0x1122, lsl #48
    str  x5, [x2, #0x8]
    ldr  x6, [x2, #0x8]
    cmp  x6, x5
    b.ne fail

    movz x0, #'P'
    str  x0, [x1]
    brk  #0

fail:
    movz x0, #'F'
    str  x0, [x1]
    brk  #0
