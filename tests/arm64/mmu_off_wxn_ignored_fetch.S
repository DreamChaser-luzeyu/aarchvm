    .text
    .global _start
_start:
    movz x1, #0
    movk x1, #0x0900, lsl #16      // UART

    adr  x10, vectors
    msr  vbar_el1, x10

    mrs  x2, sctlr_el1
    movz x3, #1
    lsl  x3, x3, #19               // WXN
    orr  x2, x2, x3
    msr  sctlr_el1, x2
    isb

after_wxn:
    movz x0, #'W'
    str  x0, [x1]
    movz x0, #'\n'
    str  x0, [x1]
    brk  #0

fail:
    movz x0, #'F'
    str  x0, [x1]
    brk  #0

    .balign 2048
vectors:
    .space 0x200
sync_spx:
    b fail
