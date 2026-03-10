    .text
    .global _start
_start:
    movz x1, #0
    movk x1, #0x0900, lsl #16      // UART

    adr  x20, slot

    // offset form
    ldpsw x2, x3, [x20]
    movn x4, #0                     // -1
    movz x5, #0x1234
    cmp  x2, x4
    b.ne fail
    cmp  x3, x5
    b.ne fail

    // post-index form
    sub  x6, x20, #8
    ldpsw x7, x8, [x6], #8
    movn x9, #1                     // -2
    movz x10, #0x5678
    cmp  x7, x9
    b.ne fail
    cmp  x8, x10
    b.ne fail
    cmp  x6, x20
    b.ne fail

    // pre-index form
    sub  x12, x20, #8
    ldpsw x13, x14, [x12, #8]!
    cmp  x13, x4
    b.ne fail
    cmp  x14, x5
    b.ne fail
    cmp  x12, x20
    b.ne fail

    movz x0, #'L'
    str  x0, [x1]
    movz x0, #'\n'
    str  x0, [x1]
    brk  #0

fail:
    movz x0, #'F'
    str  x0, [x1]
    brk  #0

    .data
    .balign 8
    .word 0xfffffffe
    .word 0x00005678
slot:
    .word 0xffffffff
    .word 0x00001234
