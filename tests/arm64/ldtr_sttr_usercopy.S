    .text
    .global _start
_start:
    movz x20, #0x0000
    movk x20, #0x0900, lsl #16

    adr  x1, buf
    add  x1, x1, #8

    movz x2, #0x5678
    movk x2, #0x1234, lsl #16
    movk x2, #0xcdef, lsl #32
    movk x2, #0x89ab, lsl #48
    sttr x2, [x1, #-8]

    movz x3, #0xbeef
    movk x3, #0xdead, lsl #16
    sttr w3, [x1, #8]

    movz x4, #0x1234
    sttrh w4, [x1, #12]

    movz x5, #0x56
    sttrb w5, [x1, #14]

    ldr  x6, [x1, #-8]
    cmp  x6, x2
    b.ne fail1

    ldtr x7, [x1, #-8]
    cmp  x7, x2
    b.ne fail2

    ldtr w8, [x1, #8]
    cmp  w8, w3
    b.ne fail2

    ldtrh w9, [x1, #12]
    cmp  w9, w4
    b.ne fail2

    ldtrb w10, [x1, #14]
    cmp  w10, w5
    b.ne fail2

    mov  w11, #0x80
    sttrb w11, [x1, #15]
    ldtrsb w12, [x1, #15]
    mov  w14, #128
    neg  w14, w14
    cmp  w12, w14
    b.ne fail3
    ldtrsb x13, [x1, #15]
    mov  x14, #128
    neg  x14, x14
    cmp  x13, x14
    b.ne fail3

    mov  w11, #0x8001
    sttrh w11, [x1, #16]
    ldtrsh w12, [x1, #16]
    mov  w14, #32767
    neg  w14, w14
    cmp  w12, w14
    b.ne fail3
    ldtrsh x13, [x1, #16]
    mov  x14, #32767
    neg  x14, x14
    cmp  x13, x14
    b.ne fail3

    movz x11, #0x0001
    movk x11, #0x8000, lsl #16
    sttr w11, [x1, #20]
    ldtrsw x12, [x1, #20]
    mov  x14, #2147483647
    neg  x14, x14
    cmp  x12, x14
    b.ne fail3

    movz x0, #'W'
    str  x0, [x20]
    movz x0, #'\n'
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
    .balign 8
buf:
    .space 32
