    .text
    .global _start
_start:
    movz x20, #0
    movk x20, #0x0900, lsl #16
    adr  x21, dst

    adr  x0, src_st1_16
    ldr  q0, [x0]
    add  x1, x21, #0
    st1  {v0.16b}, [x1]
    mov  x0, x1
    adr  x1, src_st1_16
    bl   check16

    adr  x0, src_st1_8
    ldr  d1, [x0]
    add  x1, x21, #16
    st1  {v1.8b}, [x1], #8
    add  x2, x21, #24
    cmp  x1, x2
    b.ne fail
    add  x0, x21, #16
    adr  x1, src_st1_8
    bl   check8

    adr  x2, inter_src16
    ld2  {v2.16b, v3.16b}, [x2]
    str  q2, [x21, #32]
    str  q3, [x21, #48]
    add  x0, x21, #32
    adr  x1, inter_even16
    bl   check16
    add  x0, x21, #48
    adr  x1, inter_odd16
    bl   check16

    add  x3, x21, #64
    st2  {v2.16b, v3.16b}, [x3]
    mov  x0, x3
    adr  x1, inter_src16
    bl   check32

    adr  x4, inter_src8
    ld2  {v4.8b, v5.8b}, [x4], #16
    adr  x6, inter_src8
    add  x6, x6, #16
    cmp  x4, x6
    b.ne fail
    str  d4, [x21, #96]
    str  d5, [x21, #104]
    add  x0, x21, #96
    adr  x1, inter_even8
    bl   check8
    add  x0, x21, #104
    adr  x1, inter_odd8
    bl   check8

    add  x7, x21, #112
    st2  {v4.8b, v5.8b}, [x7], #16
    add  x6, x21, #128
    cmp  x7, x6
    b.ne fail
    add  x0, x21, #112
    adr  x1, inter_src8
    bl   check16

    movz x0, #'T'
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

check32:
    ldr  x2, [x0]
    ldr  x3, [x1]
    cmp  x2, x3
    b.ne fail
    ldr  x2, [x0, #8]
    ldr  x3, [x1, #8]
    cmp  x2, x3
    b.ne fail
    ldr  x2, [x0, #16]
    ldr  x3, [x1, #16]
    cmp  x2, x3
    b.ne fail
    ldr  x2, [x0, #24]
    ldr  x3, [x1, #24]
    cmp  x2, x3
    b.ne fail
    ret

fail:
    movz x0, #'F'
    str  x0, [x20]
    brk  #0

    .data
    .balign 16
src_st1_16:
    .byte 0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x1e,0x1f
src_st1_8:
    .byte 0xa0,0xa1,0xa2,0xa3,0xa4,0xa5,0xa6,0xa7
inter_src16:
    .byte 0x00,0x80,0x01,0x81,0x02,0x82,0x03,0x83,0x04,0x84,0x05,0x85,0x06,0x86,0x07,0x87
    .byte 0x08,0x88,0x09,0x89,0x0a,0x8a,0x0b,0x8b,0x0c,0x8c,0x0d,0x8d,0x0e,0x8e,0x0f,0x8f
inter_even16:
    .byte 0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f
inter_odd16:
    .byte 0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x8a,0x8b,0x8c,0x8d,0x8e,0x8f
inter_src8:
    .byte 0x20,0x90,0x21,0x91,0x22,0x92,0x23,0x93,0x24,0x94,0x25,0x95,0x26,0x96,0x27,0x97
inter_even8:
    .byte 0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27
inter_odd8:
    .byte 0x90,0x91,0x92,0x93,0x94,0x95,0x96,0x97

dst:
    .space 128
