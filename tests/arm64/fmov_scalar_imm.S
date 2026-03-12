    .text
    .global _start
_start:
    movz x20, #0x0000
    movk x20, #0x0900, lsl #16

    fmov d0, #5.0
    fmov x0, d0
    ldr  x1, =0x4014000000000000
    cmp  x0, x1
    b.ne fail

    fmov s1, #1.0
    fmov w2, s1
    ldr  w3, =0x3f800000
    cmp  w2, w3
    b.ne fail

    fmov d4, #-0.125
    fmov x4, d4
    ldr  x5, =0xbfc0000000000000
    cmp  x4, x5
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
