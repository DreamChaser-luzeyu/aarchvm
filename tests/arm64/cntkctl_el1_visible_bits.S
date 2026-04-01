    .text
    .global _start
_start:
    movz x20, #0
    movk x20, #0x0900, lsl #16

    mrs  x0, cntkctl_el1
    cbnz x0, fail0

    movn x1, #0
    msr  cntkctl_el1, x1
    isb
    mrs  x2, cntkctl_el1
    movz x3, #0x03ff
    cmp  x2, x3
    b.ne fail1

    movz x0, #'Q'
    str  x0, [x20]
    movz x0, #'\n'
    str  x0, [x20]
    brk  #0

fail0:
    movz x0, #'0'
    b out
fail1:
    movz x0, #'1'
    b out

out:
    str  x0, [x20]
    movz x0, #'F'
    str  x0, [x20]
    brk  #0
