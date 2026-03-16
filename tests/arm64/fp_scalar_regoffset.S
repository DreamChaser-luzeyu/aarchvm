    .text
    .global _start
_start:
    movz x20, #0
    movk x20, #0x0900, lsl #16
    movz x25, #0x0030, lsl #16
    msr  cpacr_el1, x25
    isb

    adr  x0, buf

    movz w1, #0x5a
    strb w1, [x0, #24]
    movz x2, #24
    ldr  b0, [x0, x2]
    movz x3, #80
    str  b0, [x0, x3]
    ldrb w4, [x0, #80]
    cmp  w4, w1
    b.ne fail1

    movz w5, #0x1234
    strh w5, [x0, #26]
    movz x6, #26
    ldr  h1, [x0, x6]
    movz x7, #82
    str  h1, [x0, x7]
    ldrh w8, [x0, #82]
    cmp  w8, w5
    b.ne fail2

    movz x9, #0x7788
    movk x9, #0x5566, lsl #16
    movk x9, #0x3344, lsl #32
    movk x9, #0x1122, lsl #48
    str  x9, [x0, #56]
    movz x10, #56
    ldr  d2, [x0, x10]
    movz x11, #64
    str  d2, [x0, x11]
    ldr  x12, [x0, #64]
    cmp  x12, x9
    b.ne fail3

    movz x0, #'R'
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
out:
    str  x0, [x20]
    brk  #0

    .data
    .balign 16
buf:
    .space 96
