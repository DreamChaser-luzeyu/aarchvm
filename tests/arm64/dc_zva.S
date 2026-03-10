    .text
    .global _start
_start:
    movz x1, #0
    movk x1, #0x0900, lsl #16      // UART
    adr  x20, buf

    mrs  x2, dczid_el0
    tbnz x2, #4, fail1
    and  x2, x2, #0xf
    subs xzr, x2, #4
    b.ne fail2

    movz x3, #0x1111
    movk x3, #0x2222, lsl #16
    movk x3, #0x3333, lsl #32
    movk x3, #0x4444, lsl #48
    movz x4, #8
fill:
    str  x3, [x20, x4, lsl #3]
    subs xzr, x4, #0
    b.eq filled
    sub  x4, x4, #1
    b fill
filled:

    add  x5, x20, #24              // unaligned address within the 64B block
    dc   zva, x5

    movz x6, #0
check:
    ldr  x7, [x20, x6, lsl #3]
    cbnz x7, fail3
    add  x6, x6, #1
    subs xzr, x6, #8
    b.ne check

    movz x0, #'Z'
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
    .balign 64
buf:
    .space 72
