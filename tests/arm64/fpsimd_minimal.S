    .text
    .global _start
_start:
    movz x20, #0x0000
    movk x20, #0x0900, lsl #16
    movz x25, #0x0030, lsl #16
    msr  cpacr_el1, x25
    isb

    adr  x0, src
    adr  x1, dst

    movz x2, #0x1234
    movk x2, #0xabcd, lsl #16
    msr  fpcr, x2
    mrs  x3, fpcr
    movz x4, #0x03c5, lsl #16
    cmp  x3, x4
    b.ne fail1

    movz x2, #0x00ff
    movk x2, #0x0055, lsl #16
    msr  fpsr, x2
    mrs  x3, fpsr
    movz x4, #0x009f
    cmp  x3, x4
    b.ne fail1

    ldp  q0, q1, [x0]
    stp  q0, q1, [x1]

    ldr  x4, [x0]
    ldr  x5, [x1]
    cmp  x4, x5
    b.ne fail2
    ldr  x4, [x0, #8]
    ldr  x5, [x1, #8]
    cmp  x4, x5
    b.ne fail2
    ldr  x4, [x0, #16]
    ldr  x5, [x1, #16]
    cmp  x4, x5
    b.ne fail2
    ldr  x4, [x0, #24]
    ldr  x5, [x1, #24]
    cmp  x4, x5
    b.ne fail2

    ldp  q2, q3, [x0, #32]!
    add  x1, x1, #32
    stp  q2, q3, [x1], #32
    sub  x0, x0, #32
    sub  x1, x1, #64

    ldr  x4, [x0, #32]
    ldr  x5, [x1, #32]
    cmp  x4, x5
    b.ne fail3
    ldr  x4, [x0, #40]
    ldr  x5, [x1, #40]
    cmp  x4, x5
    b.ne fail3
    ldr  x4, [x0, #48]
    ldr  x5, [x1, #48]
    cmp  x4, x5
    b.ne fail3
    ldr  x4, [x0, #56]
    ldr  x5, [x1, #56]
    cmp  x4, x5
    b.ne fail3

    movz x0, #'W'
    str  x0, [x20]
    movz x0, #'
'
    str  x0, [x20]
    brk  #0

fail1:
    movz x0, #'1'
    str  x0, [x20]
    brk  #0
fail2:
    movz x0, #'2'
    str  x0, [x20]
    brk  #0
fail3:
    movz x0, #'3'
    str  x0, [x20]
    brk  #0

    .data
    .balign 16
src:
    .quad 0x1111111122222222
    .quad 0x3333333344444444
    .quad 0x5555555566666666
    .quad 0x7777777788888888
    .quad 0x99999999aaaaaaaa
    .quad 0xbbbbbbbbcccccccc
    .quad 0xddddddddeeeeeeee
    .quad 0xffff000011112222
dst:
    .space 64
