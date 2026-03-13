    .text
    .global _start
_start:
    movz x1, #0
    movk x1, #0x0900, lsl #16      // UART

    movz x20, #0x2000              // generated code buffer

    // movz x0, #'A'
    movz x22, #0x0820
    movk x22, #0xD280, lsl #16
    // movz x0, #'B'
    movz x23, #0x0840
    movk x23, #0xD280, lsl #16
    // ret
    movz x24, #0x03C0
    movk x24, #0xD65F, lsl #16

    str  w22, [x20]
    str  w24, [x20, #4]

    blr  x20
    str  x0, [x1]

    str  w23, [x20]
    ic   ivau, x20
    dsb  sy
    isb

    blr  x20
    str  x0, [x1]

    movz x0, #'\n'
    str  x0, [x1]
    brk  #0
