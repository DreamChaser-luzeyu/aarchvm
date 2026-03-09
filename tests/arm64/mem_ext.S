    .text
    .global _start
_start:
    movz x1, #0x0000
    movk x1, #0x0900, lsl #16      // UART

    adr  x20, data_buf

    movz w0, #0x12
    strb w0, [x20, #0]

    movz w12, #0x34
    strh w12, [x20, #2]

    movz x2, #0x5566
    stur x2, [x20, #8]

    ldrb w3, [x20, #0]
    subs wzr, w3, #0x12
    b.ne fail1

    ldrh w4, [x20, #2]
    subs wzr, w4, #0x34
    b.ne fail2

    ldur x5, [x20, #8]
    movz x6, #0x5566
    subs xzr, x5, x6
    b.ne fail3

    movz x0, #'M'
    str  x0, [x1]
    movz x0, #'\n'
    str  x0, [x1]
    brk  #0

fail1:
    movz x0, #'1'
    str  x0, [x1]
    brk  #0
fail2:
    movz x0, #'2'
    str  x0, [x1]
    brk  #0
fail3:
    movz x0, #'3'
    str  x0, [x1]
    brk  #0

    .data
    .balign 16
data_buf:
    .quad 0
    .quad 0
