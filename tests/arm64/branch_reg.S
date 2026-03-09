    .text
    .global _start
_start:
    movz x1, #0x0000
    movk x1, #0x0900, lsl #16      // UART

    adr  x2, target
    blr  x2
    str  x0, [x1]

    adr  x3, done
    br   x3

    movz x0, #'F'
    str  x0, [x1]

 done:
    movz x0, #'\n'
    str  x0, [x1]
    brk  #0

 target:
    movz x0, #'R'
    ret
