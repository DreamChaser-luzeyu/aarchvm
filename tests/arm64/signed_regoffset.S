    .text
    .global _start
_start:
    movz x1, #0
    movk x1, #0x0900, lsl #16      // UART
    adr  x20, data_buf

    movz x2, #0
    ldrsb w3, [x20, x2]
    movn  w4, #0x007f
    subs  wzr, w3, w4
    b.ne fail1

    movz x5, #1
    ldrsb x6, [x20, x5]
    movz  x7, #0x7f
    subs  xzr, x6, x7
    b.ne fail2

    movz x8, #2
    ldrsh w9, [x20, x8]
    movn  w10, #0x007f
    subs  wzr, w9, w10
    b.ne fail3

    movz w11, #2
    ldrsh x12, [x20, w11, uxtw #1]
    movz  x13, #0x1234
    subs  xzr, x12, x13
    b.ne fail4

    movz x0, #'R'
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
fail4:
    movz x0, #'4'
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
