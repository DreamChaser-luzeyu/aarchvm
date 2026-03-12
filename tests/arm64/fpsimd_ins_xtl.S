    .text
    .global _start
_start:
    movz x20, #0x0000
    movk x20, #0x0900, lsl #16
    adr  x21, dst

    movi v0.16b, #0
    ldr  w4, =0x12345678
    mov  v0.s[1], w4
    str  q0, [x21]
    ldr  x0, [x21]
    ldr  x1, [x21, #8]
    ldr  x2, =0x1234567800000000
    cmp  x0, x2
    b.ne fail
    cbnz x1, fail
    b.ne fail

    movi v1.2d, #0
    mov  w5, #-2
    mov  v1.s[1], w5
    sxtl v2.2d, v1.2s
    str  q2, [x21, #16]
    ldr  x0, [x21, #16]
    ldr  x1, [x21, #24]
    cbnz x0, fail
    ldr  x2, =0xfffffffffffffffe
    cmp  x1, x2
    b.ne fail

    movi v3.2d, #0
    ldr  w6, =0xff00ff00
    mov  v3.s[1], w6
    uxtl v4.2d, v3.2s
    str  q4, [x21, #32]
    ldr  x0, [x21, #32]
    ldr  x1, [x21, #40]
    cbnz x0, fail
    ldr  x2, =0x00000000ff00ff00
    cmp  x1, x2
    b.ne fail

    movi v5.4s, #1
    sshll v6.2d, v5.2s, #2
    str  q6, [x21, #48]
    ldr  x0, [x21, #48]
    ldr  x1, [x21, #56]
    ldr  x2, =0x0000000000000004
    cmp  x0, x2
    b.ne fail
    cmp  x1, x2
    b.ne fail

    movz x0, #'W'
    str  x0, [x20]
    movz x0, #'\n'
    str  x0, [x20]
    brk  #0

fail:
    movz x0, #'F'
    str  x0, [x20]
    brk  #0

    .data
    .balign 16
dst:
    .space 64
