    .text
    .global _start
_start:
    movz x20, #0
    movk x20, #0x0900, lsl #16
    movz x25, #0x0030, lsl #16
    msr  cpacr_el1, x25
    isb

    adr  x21, scratch

    adr  x22, cmgt_src
    ldr  q0, [x22]
    cmgt v1.16b, v0.16b, #0
    str  q1, [x21]
    mov  x0, x21
    adr  x1, exp_cmgt
    bl   check16

    adr  x22, cmge_src
    ldr  q2, [x22]
    cmge v3.8h, v2.8h, #0
    str  q3, [x21, #16]
    add  x0, x21, #16
    adr  x1, exp_cmge
    bl   check16

    adr  x22, cmeq_src
    ldr  q4, [x22]
    cmeq v5.4s, v4.4s, #0
    str  q5, [x21, #32]
    add  x0, x21, #32
    adr  x1, exp_cmeq
    bl   check16

    adr  x22, cmle_src
    ldr  q6, [x22]
    cmle v7.2d, v6.2d, #0
    str  q7, [x21, #48]
    add  x0, x21, #48
    adr  x1, exp_cmle
    bl   check16

    adr  x22, cmlt_src
    ldr  q8, [x22]
    cmlt v9.4s, v8.4s, #0
    str  q9, [x21, #64]
    add  x0, x21, #64
    adr  x1, exp_cmlt
    bl   check16

    movz x0, #'Z'
    str  x0, [x20]
    movz x0, #'\n'
    str  x0, [x20]
    brk  #0

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
cmgt_src:
    .byte -1, 0, 1, 2, -2, -3, 127, -128, 3, -4, 5, 0, -1, 64, -64, 0
exp_cmgt:
    .byte 0x00, 0x00, 0xff, 0xff, 0x00, 0x00, 0xff, 0x00
    .byte 0xff, 0x00, 0xff, 0x00, 0x00, 0xff, 0x00, 0x00

cmge_src:
    .hword 0, -1, 1, -2, 32767, -32768, 5, 0
exp_cmge:
    .hword 0xffff, 0x0000, 0xffff, 0x0000, 0xffff, 0x0000, 0xffff, 0xffff

cmeq_src:
    .word 0, 1, -1, 0
exp_cmeq:
    .word 0xffffffff, 0x00000000, 0x00000000, 0xffffffff

cmle_src:
    .xword 0, -1
exp_cmle:
    .xword 0xffffffffffffffff, 0xffffffffffffffff

cmlt_src:
    .word -1, 0, 1, -5
exp_cmlt:
    .word 0xffffffff, 0x00000000, 0x00000000, 0xffffffff

scratch:
    .space 80
