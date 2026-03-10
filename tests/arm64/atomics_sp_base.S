    .text
    .global _start
_start:
    movz x1, #0
    movk x1, #0x0900, lsl #16      // UART
    adr  x20, stack_top
    mov  sp, x20
    sub  sp, sp, #16

    movz x28, #'a'
    movz x2, #0x1111
    stlr x2, [sp]
    ldar x3, [sp]
    subs xzr, x3, x2
    b.ne fail

    movz x28, #'b'
    ldxr x4, [sp]
    subs xzr, x4, x2
    b.ne fail
    movz x5, #0x2222
    stxr w6, x5, [sp]
    cbnz w6, fail
    ldar x7, [sp]
    subs xzr, x7, x5
    b.ne fail

    movz x28, #'c'
    ldaxr x8, [sp]
    subs xzr, x8, x5
    b.ne fail
    movz x9, #0x3333
    stlxr w10, x9, [sp]
    cbnz w10, fail
    ldar x11, [sp]
    subs xzr, x11, x9
    b.ne fail

    movz x0, #'S'
    str  x0, [x1]
    movz x0, #'\n'
    str  x0, [x1]
    brk  #0

fail:
    orr  x0, x28, xzr
    str  x0, [x1]
    brk  #0

    .bss
    .balign 16
    .space 64
stack_top:
