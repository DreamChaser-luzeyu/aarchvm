    .text
    .global _start
_start:
    movz x1, #0
    movk x1, #0x0900, lsl #16
    adr  x20, data_buf

    mov  x2, x20
    ldrsb w3, [x2], #1
    movn  w4, #0x007f
    subs  wzr, w3, w4
    b.ne fail1
    add   x5, x20, #1
    subs  xzr, x2, x5
    b.ne fail1

    mov  x2, x20
    ldrsb x3, [x2], #1
    movn  x4, #0x007f
    subs  xzr, x3, x4
    b.ne fail2
    add   x5, x20, #1
    subs  xzr, x2, x5
    b.ne fail2

    add  x2, x20, #1
    ldrsh w3, [x2, #1]!
    movn  w4, #0x007f
    subs  wzr, w3, w4
    b.ne fail3
    add   x5, x20, #2
    subs  xzr, x2, x5
    b.ne fail3

    add  x2, x20, #4
    ldrsh x3, [x2, #-2]!
    movn  x4, #0x007f
    subs  xzr, x3, x4
    b.ne fail4
    add   x5, x20, #2
    subs  xzr, x2, x5
    b.ne fail4

    add  x2, x20, #2
    ldrsh w3, [x2], #2
    movn  w4, #0x007f
    subs  wzr, w3, w4
    b.ne fail5
    add   x5, x20, #4
    subs  xzr, x2, x5
    b.ne fail5

    movz x0, #'W'
    str  x0, [x1]
    movz x0, #'
'
    str  x0, [x1]
    brk  #0

fail1: movz x0, #'1'; b out
fail2: movz x0, #'2'; b out
fail3: movz x0, #'3'; b out
fail4: movz x0, #'4'; b out
fail5: movz x0, #'5'
out:
    str  x0, [x1]
    brk  #0

    .data
    .balign 8
data_buf:
    .byte 0x80
    .byte 0x7f
    .hword 0xff80
    .hword 0x1234
