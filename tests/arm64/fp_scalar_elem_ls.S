    .text
    .global _start
_start:
    movz x20, #0
    movk x20, #0x0900, lsl #16
    adr  x0, buf
    movz w1, #0x34
    fmov s0, w1
    str b0, [x0, #1]
    ldr b1, [x0, #1]
    umov w2, v1.b[0]
    and  w2, w2, #0xff
    cmp  w2, w1
    b.ne fail1

    movz w3, #0x5678
    fmov s2, w3
    str h2, [x0, #2]
    ldr h3, [x0, #2]
    umov w4, v3.h[0]
    and  w4, w4, #0xffff
    cmp  w4, w3
    b.ne fail2

    movz x0, #'W'
    str  x0, [x20]
    movz x0, #'
'
    str  x0, [x20]
    brk  #0
fail1: movz x0, #'1'; b out
fail2: movz x0, #'2'
out:
    str  x0, [x20]
    brk  #0
    .data
    .balign 8
buf:
    .space 8
