    .text
    .global _start
_start:
    // UART base: 0x09000000
    movz x1, #0x0000
    movk x1, #0x0900, lsl #16

    // Plugin MMIO base: 0x09060000
    movz x2, #0x0000
    movk x2, #0x0906, lsl #16

    // The reset_observable test plugin initializes scratch0 to a non-zero
    // sentinel in create() and only clears it in reset(). A zero here means
    // the startup path actually issued RESET before guest code ran.
    ldr  x3, [x2, #0x0]
    cbnz x3, fail

    movz x0, #'Z'
    str  x0, [x1]
    brk  #0

fail:
    movz x0, #'F'
    str  x0, [x1]
    brk  #0
