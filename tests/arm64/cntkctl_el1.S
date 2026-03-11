    .text
    .global _start
_start:
    movz x1, #0x0000
    movk x1, #0x0900, lsl #16

    mrs  x2, cntkctl_el1
    cbnz x2, fail1

    movz x3, #0x2
    msr  cntkctl_el1, x3
    mrs  x4, cntkctl_el1
    cmp  x4, x3
    b.ne fail2

    movz x0, #'W'
    str  x0, [x1]
    movz x0, #'\n'
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
