    .text
    .global _start
_start:
    movz x20, #0
    movk x20, #0x0900, lsl #16
    movz x25, #0x0030, lsl #16
    msr  cpacr_el1, x25
    isb

    adr  x21, src
    ldr  q0, [x21]
    ldr  q1, [x21, #16]

    umov w0, v0.b[3]
    cmp  w0, #0x44
    b.ne fail1

    umov w1, v0.h[2]
    movz w2, #0x6655
    cmp  w1, w2
    b.ne fail2

    mov  w3, v0.s[2]
    movz w4, #0xaa99
    movk w4, #0xccbb, lsl #16
    cmp  w3, w4
    b.ne fail3

    mov  x5, v1.d[1]
    movz x6, #0x2211
    movk x6, #0x4433, lsl #16
    movk x6, #0x6655, lsl #32
    movk x6, #0x8877, lsl #48
    cmp  x5, x6
    b.ne fail4

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
fail4:
    movz x0, #'4'
    str  x0, [x20]
    brk  #0

    .data
    .balign 16
src:
    .byte 0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88
    .byte 0x99,0xaa,0xbb,0xcc,0xdd,0xee,0xff,0x00
    .byte 0x10,0x32,0x54,0x76,0x98,0xba,0xdc,0xfe
    .byte 0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88
