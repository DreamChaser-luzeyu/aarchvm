    .arch armv8.1-a
    .text
    .global _start
_start:
    movz x1, #0x0000
    movk x1, #0x0900, lsl #16

    mrs  x2, pan
    cbnz x2, fail1

    msr  pan, #1
    mrs  x3, pan
    cmp  x3, #1
    b.ne fail2

    msr  pan, #0
    mrs  x3, pan
    cbnz x3, fail3

    movz x0, #'W'
    str  x0, [x1]
    movz x0, #'
'
    str  x0, [x1]
    brk  #0
fail1:
    movz x0, #'1'
    str  x0, [x1]
    brk  #0
fail2:
    movz x0, #'2'
    str  x0, [x1]
    brk  #0
fail3:
    movz x0, #'3'
    str  x0, [x1]
    brk  #0
