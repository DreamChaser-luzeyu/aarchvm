    .text
    .global _start
_start:
    movz x20, #0x0000
    movk x20, #0x0900, lsl #16
    movz x25, #0x0030, lsl #16
    msr  cpacr_el1, x25
    isb

    movi d0, #0
    fmov x0, d0
    cbnz x0, fail

    movi d1, #0xff
    fmov x1, d1
    ldr  x2, =0x00000000000000ff
    cmp  x1, x2
    b.ne fail

    movz x0, #'W'
    str  x0, [x20]
    movz x0, #'\n'
    str  x0, [x20]
    brk  #0

fail:
    movz x0, #'F'
    str  x0, [x20]
    brk  #0
