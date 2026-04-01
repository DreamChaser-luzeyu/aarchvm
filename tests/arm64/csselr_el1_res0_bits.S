    .text
    .global _start
_start:
    movz x20, #0
    movk x20, #0x0900, lsl #16

    movn x0, #0
    msr  csselr_el1, x0
    isb
    mrs  x1, csselr_el1
    lsr  x2, x1, #5
    cbnz x2, fail1
    tbnz x1, #4, fail2

    movz x0, #0x10
    msr  csselr_el1, x0
    isb
    mrs  x1, csselr_el1
    lsr  x2, x1, #5
    cbnz x2, fail3
    tbnz x1, #4, fail3

    movz x0, #'S'
    str  x0, [x20]
    movz x0, #'\n'
    str  x0, [x20]
    brk  #0

fail1:
    movz x0, #'1'
    b out
fail2:
    movz x0, #'2'
    b out
fail3:
    movz x0, #'3'
    b out

out:
    str  x0, [x20]
    movz x0, #'F'
    str  x0, [x20]
    brk  #0
