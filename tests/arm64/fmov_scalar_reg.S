    .text
    .global _start
_start:
    movz x20, #0x0000
    movk x20, #0x0900, lsl #16

    ldr  x0, =0x400921fb54442d18
    fmov d0, x0
    fmov d31, d0
    fmov x1, d31
    cmp  x1, x0
    b.ne fail

    ldr  w2, =0x3f800000
    fmov s1, w2
    fmov s3, s1
    fmov w3, s3
    cmp  w3, w2
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
