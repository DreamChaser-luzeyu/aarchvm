    .text
    .global _start
_start:
    movz x20, #0
    movk x20, #0x0900, lsl #16

    movz x0, #'A'
    str  x0, [x20]

    movz x1, #0x6a80
loop:
    subs x1, x1, #1
    b.ne loop

    movz x0, #'B'
    str  x0, [x20]
    movz x0, #'\n'
    str  x0, [x20]
    brk  #0
