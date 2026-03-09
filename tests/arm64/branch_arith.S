    .text
    .global _start
_start:
    movz x1, #0x0000
    movk x1, #0x0900, lsl #16

    movz x2, #1
    subs x2, x2, #1
    b.ne fail
    cbz x2, pass

fail:
    movz x0, #'F'
    str  x0, [x1]
    movz x0, #'\n'
    str  x0, [x1]
    brk #0

pass:
    mrs x3, CurrentEL
    movz x0, #'B'
    str  x0, [x1]
    movz x0, #'\n'
    str  x0, [x1]
    brk #0
