    .text
    .global _start
_start:
    movz x20, #0x0000
    movk x20, #0x0900, lsl #16

    mov  w0, #123
    ucvtf s0, w0
    fcvtzu w1, s0
    cmp  w1, #123
    b.ne fail

    movz x2, #0x3bfb
    movk x2, #0x71c7, lsl #16
    movk x2, #0x001f, lsl #32
    ucvtf d1, x2
    fcvtzu x3, d1
    cmp  x3, x2
    b.ne fail

    mov  w4, #5
    ucvtf s2, w4
    fneg s2, s2
    fcvtzu w5, s2
    cbnz w5, fail

    movz x0, #'U'
    str  x0, [x20]
    movz x0, #'\n'
    str  x0, [x20]
    brk  #0

fail:
    movz x0, #'F'
    str  x0, [x20]
    brk  #0
