    .text
    .global _start
_start:
    movz x1, #0
    movk x1, #0x0900, lsl #16      // UART
    adr  x20, buf

    movz x2, #0x1111
    movz x3, #0x2222
    stnp x2, x3, [x20]
    ldnp x4, x5, [x20]
    subs xzr, x4, x2
    b.ne fail1
    subs xzr, x5, x3
    b.ne fail2

    movz w6, #0x3333
    movz w7, #0x4444
    stnp w6, w7, [x20, #8]
    ldnp w8, w9, [x20, #8]
    subs wzr, w8, w6
    b.ne fail3
    subs wzr, w9, w7
    b.ne fail4

    movz x0, #'F'
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
    .balign 16
buf:
    .quad 0
    .quad 0
    .quad 0
