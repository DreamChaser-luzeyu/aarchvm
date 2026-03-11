    .text
    .global _start
_start:
    movz x20, #0x0000
    movk x20, #0x0900, lsl #16

    adr  x21, dst

    mvni v0.4s, #0
    str  q0, [x21]
    adr  x22, exp_ff32
    ldr  x0, [x21]
    ldr  x1, [x22]
    cmp  x0, x1
    b.ne fail
    ldr  x0, [x21, #8]
    ldr  x1, [x22, #8]
    cmp  x0, x1
    b.ne fail

    mvni v1.8h, #0
    str  q1, [x21, #16]
    adr  x22, exp_ff16
    add  x23, x21, #16
    ldr  x0, [x23]
    ldr  x1, [x22]
    cmp  x0, x1
    b.ne fail
    ldr  x0, [x23, #8]
    ldr  x1, [x22, #8]
    cmp  x0, x1
    b.ne fail

    movz x0, #'W'
    str  x0, [x20]
    movz x0, #'
'
    str  x0, [x20]
    brk  #0

fail:
    movz x0, #'F'
    str  x0, [x20]
    brk  #0

    .data
    .balign 16
dst:
    .space 32
exp_ff32:
    .quad 0xffffffffffffffff
    .quad 0xffffffffffffffff
exp_ff16:
    .quad 0xffffffffffffffff
    .quad 0xffffffffffffffff
