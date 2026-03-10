    .text
    .global _start
_start:
    movz x1, #0
    movk x1, #0x0900, lsl #16      // UART
    adr  x20, data_buf

    // [Xn, Xm]
    movz x2, #0
    ldrsw x3, [x20, x2]
    movn x4, #0x007f
    subs xzr, x3, x4
    b.ne fail1

    // [Xn, Wm, UXTW #2]
    movz w5, #1
    ldrsw x6, [x20, w5, uxtw #2]
    movz x7, #0x1234
    subs xzr, x6, x7
    b.ne fail2

    // [Xn, Xm, LSL #2]
    movz x8, #2
    ldrsw x9, [x20, x8, lsl #2]
    movn x10, #0x0000
    subs xzr, x9, x10
    b.ne fail3

    movz x0, #'K'
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
out:
    str  x0, [x1]
    brk  #0

    .data
    .balign 16
data_buf:
    .word 0xffffff80
    .word 0x00001234
    .word 0xffffffff
