    .text
    .global _start
_start:
    movz x20, #0x0000
    movk x20, #0x0900, lsl #16      // UART base

    adr  x0, prompt
1:
    ldrb w1, [x0], #1
    cbz  w1, 2f
    str  w1, [x20]
    b    1b

2:
    ldr  w2, [x20, #0x18]
    tbnz w2, #4, 2b                 // RXFE
    ldrb w3, [x20]
    movz w4, #'A'
    cmp  w3, w4
    b.ne fail

3:
    ldr  w2, [x20, #0x18]
    tbnz w2, #4, 3b
    ldrb w3, [x20]
    movz w4, #'\n'
    cmp  w3, w4
    b.ne fail

    movz w0, #'P'
    str  w0, [x20]
    movz w0, #'\n'
    str  w0, [x20]
    brk  #0

fail:
    movz w0, #'F'
    str  w0, [x20]
    movz w0, #'\n'
    str  w0, [x20]
    brk  #0

    .data
prompt:
    .ascii "=> "
    .byte 0
