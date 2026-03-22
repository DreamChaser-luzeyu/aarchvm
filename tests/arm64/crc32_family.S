    .arch armv8.1-a+crc
    .text
    .global _start

    .macro LOAD64 reg, imm0, imm1, imm2, imm3
    movz \reg, #\imm0
    movk \reg, #\imm1, lsl #16
    movk \reg, #\imm2, lsl #32
    movk \reg, #\imm3, lsl #48
    .endm

_start:
    movz x20, #0x0000
    movk x20, #0x0900, lsl #16      // UART

    // CRC32B
    LOAD64 x0, 0xffff, 0xffff, 0xffff, 0xffff
    LOAD64 x1, 0x5678, 0x1234, 0x0000, 0x0000
    LOAD64 x2, 0x00ab, 0x0000, 0x0000, 0x0000
    crc32b w0, w1, w2
    LOAD64 x25, 0xb738, 0x1fc8, 0x0000, 0x0000
    cmp  x0, x25
    b.ne fail1

    // CRC32H
    LOAD64 x3, 0xffff, 0xffff, 0xffff, 0xffff
    LOAD64 x4, 0x5678, 0x1234, 0x0000, 0x0000
    LOAD64 x5, 0xcdef, 0x0000, 0x0000, 0x0000
    crc32h w3, w4, w5
    LOAD64 x25, 0x4425, 0x59dd, 0x0000, 0x0000
    cmp  x3, x25
    b.ne fail2

    // CRC32W
    LOAD64 x6, 0xffff, 0xffff, 0xffff, 0xffff
    LOAD64 x7, 0x5678, 0x1234, 0x0000, 0x0000
    LOAD64 x8, 0xcdef, 0x89ab, 0x0000, 0x0000
    crc32w w6, w7, w8
    LOAD64 x25, 0x5215, 0x40d5, 0x0000, 0x0000
    cmp  x6, x25
    b.ne fail3

    // CRC32X
    LOAD64 x9, 0xffff, 0xffff, 0xffff, 0xffff
    LOAD64 x10, 0x5678, 0x1234, 0x0000, 0x0000
    LOAD64 x11, 0xcdef, 0x89ab, 0x4567, 0x0123
    crc32x w9, w10, x11
    LOAD64 x25, 0xeadf, 0x9b62, 0x0000, 0x0000
    cmp  x9, x25
    b.ne fail4

    // CRC32CB
    LOAD64 x12, 0xffff, 0xffff, 0xffff, 0xffff
    LOAD64 x13, 0x5678, 0x1234, 0x0000, 0x0000
    LOAD64 x14, 0x00ab, 0x0000, 0x0000, 0x0000
    crc32cb w12, w13, w14
    LOAD64 x25, 0x2609, 0xc091, 0x0000, 0x0000
    cmp  x12, x25
    b.ne fail5

    // CRC32CH
    LOAD64 x15, 0xffff, 0xffff, 0xffff, 0xffff
    LOAD64 x16, 0x5678, 0x1234, 0x0000, 0x0000
    LOAD64 x17, 0xcdef, 0x0000, 0x0000, 0x0000
    crc32ch w15, w16, w17
    LOAD64 x25, 0x8725, 0xb54a, 0x0000, 0x0000
    cmp  x15, x25
    b.ne fail6

    // CRC32CW
    LOAD64 x18, 0xffff, 0xffff, 0xffff, 0xffff
    LOAD64 x19, 0x5678, 0x1234, 0x0000, 0x0000
    LOAD64 x21, 0xcdef, 0x89ab, 0x0000, 0x0000
    crc32cw w18, w19, w21
    LOAD64 x25, 0x621e, 0xa360, 0x0000, 0x0000
    cmp  x18, x25
    b.ne fail7

    // CRC32CX
    LOAD64 x22, 0xffff, 0xffff, 0xffff, 0xffff
    LOAD64 x23, 0x5678, 0x1234, 0x0000, 0x0000
    LOAD64 x24, 0xcdef, 0x89ab, 0x4567, 0x0123
    crc32cx w22, w23, x24
    LOAD64 x25, 0x07be, 0xa3d2, 0x0000, 0x0000
    cmp  x22, x25
    b.ne fail8

pass:
    movz x0, #'R'
    str  x0, [x20]
    movz x0, #'\n'
    str  x0, [x20]
    brk  #0

fail1:
    movz x0, #'1'
    b out
fail2:
    movz x0, #'2'
    b out
fail3:
    movz x0, #'3'
    b out
fail4:
    movz x0, #'4'
    b out
fail5:
    movz x0, #'5'
    b out
fail6:
    movz x0, #'6'
    b out
fail7:
    movz x0, #'7'
    b out
fail8:
    movz x0, #'8'

out:
    str  x0, [x20]
    brk  #0
