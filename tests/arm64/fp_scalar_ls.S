    .text
    .global _start
_start:
    movz x20, #0
    movk x20, #0x0900, lsl #16
    movz x25, #0x0030, lsl #16
    msr  cpacr_el1, x25
    isb

    adr  x0, buf

    movz x1, #0x7788
    movk x1, #0x5566, lsl #16
    movk x1, #0x3344, lsl #32
    movk x1, #0x1122, lsl #48
    fmov d8, x1
    movz x2, #0xff00
    movk x2, #0xddee, lsl #16
    movk x2, #0xbbcc, lsl #32
    movk x2, #0x99aa, lsl #48
    fmov d9, x2
    stp d8, d9, [x0, #16]
    ldp d10, d11, [x0, #16]
    fmov x3, d10
    fmov x4, d11
    cmp  x3, x1
    b.ne fail1
    cmp  x4, x2
    b.ne fail1

    str  d8, [x0, #40]
    ldr  d12, [x0, #40]
    fmov x5, d12
    cmp  x5, x1
    b.ne fail2

    movz w6, #0xbeef
    movk w6, #0x1234, lsl #16
    fmov s13, w6
    str  s13, [x0, #56]
    ldr  s14, [x0, #56]
    fmov w7, s14
    cmp  w7, w6
    b.ne fail3

    add  x15, x0, #64
    str  d9, [x15], #8
    sub  x16, x15, #8
    cmp  x16, x0
    b.eq fail4
    ldr  d15, [x0, #64]
    fmov x17, d15
    cmp  x17, x2
    b.ne fail4

    movz x0, #'W'
    str  x0, [x20]
    movz x0, #'
'
    str  x0, [x20]
    brk  #0
fail1: movz x0, #'1'; b out
fail2: movz x0, #'2'; b out
fail3: movz x0, #'3'; b out
fail4: movz x0, #'4'
out:
    str  x0, [x20]
    brk  #0
    .data
    .balign 16
buf:
    .space 96
