    .text
    .global _start
_start:
    movz x1, #0
    movk x1, #0x0900, lsl #16      // UART

    // PRFUM is a hint. Even with unmapped addresses it must not take a
    // synchronous exception or perform a guest-visible access.
    movz x0, #0
    prfum pldl1keep, [x0]
    prfum pldl1strm, [x0, #1]

    movn x2, #0
    prfum pstl1keep, [x2, #-1]
    prfum plil1keep, [x2, #8]

    movz x0, #'U'
    str  x0, [x1]
    movz x0, #'\n'
    str  x0, [x1]
    brk  #0
