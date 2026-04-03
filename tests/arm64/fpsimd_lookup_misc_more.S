    .text
    .global _start
_start:
    movz x20, #0
    movk x20, #0x0900, lsl #16
    movz x25, #0x0030, lsl #16
    msr  cpacr_el1, x25
    isb

    adr  x21, scratch

    adr  x22, ext8_src
    ldr  q0, [x22]
    ldr  q1, [x22, #16]
    ext  v2.8b, v0.8b, v1.8b, #3
    str  d2, [x21]
    mov  x0, x21
    adr  x1, exp_ext8
    bl   check8

    adr  x22, tbl8_table
    ldr  q3, [x22]
    adr  x22, tbl8_idx
    ldr  d4, [x22]
    tbl  v5.8b, {v3.16b}, v4.8b
    str  d5, [x21, #8]
    add  x0, x21, #8
    adr  x1, exp_tbl8
    bl   check8

    adr  x22, tbx8_table
    ldr  q6, [x22]
    adr  x22, tbx8_idx
    ldr  d7, [x22]
    adr  x22, tbx8_dst
    ldr  d8, [x22]
    tbx  v8.8b, {v6.16b}, v7.8b
    str  d8, [x21, #16]
    add  x0, x21, #16
    adr  x1, exp_tbx8
    bl   check8

    adr  x22, tbl16_table
    ldr  q10, [x22]
    ldr  q11, [x22, #16]
    adr  x22, tbl16_idx
    ldr  q12, [x22]
    tbl  v13.16b, {v10.16b-v11.16b}, v12.16b
    str  q13, [x21, #32]
    add  x0, x21, #32
    adr  x1, exp_tbl16
    bl   check16

    adr  x22, cls_src
    ldr  q14, [x22]
    cls  v15.16b, v14.16b
    str  q15, [x21, #48]
    add  x0, x21, #48
    adr  x1, exp_cls
    bl   check16

    adr  x22, clz_src
    ldr  q16, [x22]
    clz  v17.8h, v16.8h
    str  q17, [x21, #64]
    add  x0, x21, #64
    adr  x1, exp_clz
    bl   check16

    adr  x22, saddlp_src
    ldr  q18, [x22]
    saddlp v19.4h, v18.8b
    str  d19, [x21, #80]
    add  x0, x21, #80
    adr  x1, exp_saddlp
    bl   check8

    adr  x22, uaddlp_src
    ldr  q20, [x22]
    uaddlp v21.8h, v20.16b
    str  q21, [x21, #88]
    add  x0, x21, #88
    adr  x1, exp_uaddlp
    bl   check16

    movz x0, #'L'
    str  x0, [x20]
    movz x0, #'\n'
    str  x0, [x20]
    brk  #0

check8:
    ldr  x2, [x0]
    ldr  x3, [x1]
    cmp  x2, x3
    b.ne fail
    ret

check16:
    ldr  x2, [x0]
    ldr  x3, [x1]
    cmp  x2, x3
    b.ne fail
    ldr  x2, [x0, #8]
    ldr  x3, [x1, #8]
    cmp  x2, x3
    b.ne fail
    ret

fail:
    movz x0, #'F'
    str  x0, [x20]
    brk  #0

    .data
    .balign 16
ext8_src:
    .byte 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
    .byte 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31
exp_ext8:
    .byte 3, 4, 5, 6, 7, 16, 17, 18

tbl8_table:
    .byte 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17
    .byte 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f
tbl8_idx:
    .byte 0, 5, 15, 16, 2, 14, 7, 8
exp_tbl8:
    .byte 0x10, 0x15, 0x1f, 0x00, 0x12, 0x1e, 0x17, 0x18

tbx8_table:
    .byte 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28
    .byte 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30
tbx8_idx:
    .byte 0, 16, 3, 20, 15, 1, 17, 2
tbx8_dst:
    .byte 0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7
exp_tbx8:
    .byte 0x21, 0xa1, 0x24, 0xa3, 0x30, 0x22, 0xa6, 0x23

tbl16_table:
    .byte 0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47
    .byte 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f
    .byte 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57
    .byte 0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f
tbl16_idx:
    .byte 0, 15, 16, 31, 32, 5, 20, 7, 8, 24, 1, 30, 12, 17, 29, 33
exp_tbl16:
    .byte 0x40, 0x4f, 0x50, 0x5f, 0x00, 0x45, 0x54, 0x47
    .byte 0x48, 0x58, 0x41, 0x5e, 0x4c, 0x51, 0x5d, 0x00

cls_src:
    .byte 0x00, 0xff, 0x01, 0xfe, 0x7f, 0x80, 0x3f, 0xc0
    .byte 0x10, 0xf0, 0x55, 0xaa, 0x08, 0xf8, 0x02, 0xfd
exp_cls:
    .byte 7, 7, 6, 6, 0, 0, 1, 1
    .byte 2, 3, 0, 0, 3, 4, 5, 5

clz_src:
    .hword 0x0000, 0x0001, 0x00f0, 0x0f00, 0x8000, 0x7fff, 0x00ff, 0xf000
exp_clz:
    .hword 16, 15, 8, 4, 0, 1, 8, 0

saddlp_src:
    .byte 1, 2, 0xff, 0xfe, 0x7f, 0x80, 0x10, 0xf0
    .byte 0, 0, 0, 0, 0, 0, 0, 0
exp_saddlp:
    .hword 3, -3, -1, 0

uaddlp_src:
    .byte 1, 2, 3, 4, 5, 6, 7, 8
    .byte 0xff, 1, 2, 0xfe, 0x10, 0x20, 0x30, 0x40
exp_uaddlp:
    .hword 3, 7, 11, 15, 256, 256, 48, 112

scratch:
    .space 112
