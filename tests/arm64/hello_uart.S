    .text
    .global _start
_start:
    movz x1, #0x0000
    movk x1, #0x0900, lsl #16   // x1 = 0x09000000 (PL011 DR)

    movz x0, #'A'
    str  x0, [x1]
    movz x0, #'S'
    str  x0, [x1]
    movz x0, #'M'
    str  x0, [x1]
    movz x0, #'\n'
    str  x0, [x1]

    brk #0
