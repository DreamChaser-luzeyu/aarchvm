    .text
    .global _start
_start:
    movz x1, #0
    movk x1, #0x0900, lsl #16      // UART

    mrs  x2, id_aa64mmfr0_el1
    ubfx x3, x2, #8, #4
    cbnz x3, fail1                 // BigEnd absent
    ubfx x3, x2, #16, #4
    cbnz x3, fail1                 // BigEndEL0 absent

    mrs  x4, sctlr_el1
    movz x5, #0x0300, lsl #16      // EE|E0E
    orr  x6, x4, x5
    msr  sctlr_el1, x6
    isb
    mrs  x7, sctlr_el1
    tst  x7, x5
    b.ne fail2

    movz x8, #1
    lsl  x8, x8, #26               // UCI
    orr  x6, x7, x8
    msr  sctlr_el1, x6
    isb
    mrs  x9, sctlr_el1
    tst  x9, x8
    b.eq fail3

    msr  sctlr_el1, x4
    isb

    movz x0, #'E'
    str  x0, [x1]
    movz x0, #'\n'
    str  x0, [x1]
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
    str  x0, [x1]
    movz x0, #'F'
    str  x0, [x1]
    brk  #0
