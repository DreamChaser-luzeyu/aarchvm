    .text
    .global _start
_start:
    movz x1, #0
    movk x1, #0x0900, lsl #16      // UART
    adr  x20, data_buf
    add  x21, x20, #2
    add  x22, x20, #6

    // LDRSB W/X negative.
    ldrsb w2, [x20, #0]
    movn  w3, #0x007f
    subs  wzr, w2, w3
    b.ne fail1

    ldrsb x4, [x20, #0]
    movn  x5, #0x007f
    subs  xzr, x4, x5
    b.ne fail2

    // LDRSH W/X negative.
    ldrsh w6, [x20, #2]
    movn  w7, #0x007f
    subs  wzr, w6, w7
    b.ne fail3

    ldrsh x8, [x20, #2]
    movn  x9, #0x007f
    subs  xzr, x8, x9
    b.ne fail4

    // LDURSB W/X with negative offset, positive value.
    ldursb w10, [x21, #-1]
    movz   w11, #0x7f
    subs   wzr, w10, w11
    b.ne fail5

    ldursb x12, [x21, #-1]
    movz   x13, #0x7f
    subs   xzr, x12, x13
    b.ne fail6

    // LDURSH W/X with negative offset.
    ldursh w14, [x22, #-2]
    movz   w15, #0x1234
    subs   wzr, w14, w15
    b.ne fail7

    ldursh x16, [x22, #-2]
    movz   x17, #0x1234
    subs   xzr, x16, x17
    b.ne fail8

    movz x0, #'N'
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
    b out
fail5:
    movz x0, #'5'
    b out
fail6:
    movz x0, #'6'
    b out
fail7:
    movz x0, #'7'
    b out
fail8:
    movz x0, #'8'
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
