    .text
    .global _start
_start:
    // UART base: 0x09000000
    movz x1, #0x0000
    movk x1, #0x0900, lsl #16

    // Plugin MMIO base: 0x09050000
    movz x2, #0x0000
    movk x2, #0x0905, lsl #16

    // scratch0 and scratch1 should both start at zero.
    ldr  x3, [x2, #0x8]
    cbnz x3, fail
    ldr  x3, [x2, #0x10]
    cbnz x3, fail

    // Build scratch0 via subword writes:
    // bytes => 11 22 33 44 55 66 77 88
    movz w3, #0x2211
    movk w3, #0x4433, lsl #16
    str  w3, [x2, #0x8]

    movz w3, #0x6655
    strh w3, [x2, #0xC]

    movz w3, #0x77
    strb w3, [x2, #0xE]

    movz w3, #0x88
    strb w3, [x2, #0xF]

    ldr  x4, [x2, #0x8]
    movz x5, #0x2211
    movk x5, #0x4433, lsl #16
    movk x5, #0x6655, lsl #32
    movk x5, #0x8877, lsl #48
    cmp  x4, x5
    b.ne fail

    // Aligned halfword read starting at byte 2 should observe 0x4433.
    ldrh w6, [x2, #0xA]
    movz w7, #0x4433
    cmp  w6, w7
    b.ne fail

    // scratch1 is independent and should accept a 32-bit write.
    movz w3, #0xBEEF
    movk w3, #0xA1A2, lsl #16
    str  w3, [x2, #0x10]

    ldr  x4, [x2, #0x10]
    movz x5, #0xBEEF
    movk x5, #0xA1A2, lsl #16
    cmp  x4, x5
    b.ne fail

    // scratch0 must remain unchanged.
    ldr  x4, [x2, #0x8]
    movz x5, #0x2211
    movk x5, #0x4433, lsl #16
    movk x5, #0x6655, lsl #32
    movk x5, #0x8877, lsl #48
    cmp  x4, x5
    b.ne fail

    // Undefined hole reads inside the declared MMIO space currently return zero.
    ldr  x4, [x2, #0x18]
    cbnz x4, fail

    movz x0, #'S'
    str  x0, [x1]
    brk  #0

fail:
    movz x0, #'F'
    str  x0, [x1]
    brk  #0
