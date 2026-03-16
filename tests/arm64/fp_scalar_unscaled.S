    .text
    .global _start
_start:
    movz x20, #0
    movk x20, #0x0900, lsl #16
    movz x25, #0x0030, lsl #16
    msr  cpacr_el1, x25
    isb

    adr  x0, buf
    add  x1, x0, #32
    add  x2, x0, #64

    movz w3, #0x005a
    strb w3, [x0, #24]
    ldr  b0, [x0, #24]
    stur b0, [x1, #-7]
    ldur b1, [x1, #-7]
    add  x4, x0, #80
    str  b1, [x4], #1
    ldrb w5, [x0, #80]
    cmp  w5, w3
    b.ne fail1
    ldrb w6, [x0, #25]
    cmp  w6, w3
    b.ne fail1

    movz w6, #0x1234
    strh w6, [x0, #26]
    ldr  h2, [x0, #26]
    stur h2, [x1, #-4]
    ldur h3, [x1, #-4]
    add  x7, x0, #82
    str  h3, [x7], #2
    ldrh w8, [x0, #82]
    cmp  w8, w6
    b.ne fail2
    ldrh w9, [x0, #28]
    cmp  w9, w6
    b.ne fail2

    movz w10, #0xbeef
    movk w10, #0xcafe, lsl #16
    fmov s4, w10
    stur s4, [x2, #-12]
    ldur s5, [x2, #-12]
    fmov w11, s5
    cmp  w11, w10
    b.ne fail3
    ldr  w12, [x0, #52]
    cmp  w12, w10
    b.ne fail3

    movz x13, #0x7788
    movk x13, #0x5566, lsl #16
    movk x13, #0x3344, lsl #32
    movk x13, #0x1122, lsl #48
    fmov d6, x13
    stur d6, [x2, #-8]
    ldur d7, [x2, #-8]
    fmov x14, d7
    cmp  x14, x13
    b.ne fail4
    ldr  x15, [x0, #56]
    cmp  x15, x13
    b.ne fail4

    ldr  b8, [x0, #80]
    add  x16, x0, #80
    str  b8, [x16], #1
    sub  x23, x16, #1
    add  x24, x0, #80
    cmp  x23, x24
    b.ne fail5
    ldrb w17, [x0, #80]
    movz w18, #0x5a
    cmp  w17, w18
    b.ne fail5

    ldr  h9, [x0, #82]
    add  x19, x0, #82
    str  h9, [x19], #2
    sub  x23, x19, #2
    add  x24, x0, #82
    cmp  x23, x24
    b.ne fail6
    ldrh w21, [x0, #82]
    movz w22, #0x1234
    cmp  w21, w22
    b.ne fail6

    movz x0, #'U'
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
out:
    str  x0, [x20]
    brk  #0

    .data
    .balign 16
buf:
    .space 80
    .byte 0x5a
    .hword 0x1234
    .space 32
