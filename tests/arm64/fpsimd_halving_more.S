    .text
    .global _start
_start:
    movz x20, #0
    movk x20, #0x0900, lsl #16
    movz x25, #0x0030, lsl #16
    msr  cpacr_el1, x25
    isb

    adr  x21, dst

    adr  x22, shadd_a
    ldr  d1, [x22]
    adr  x22, shadd_b
    ldr  d2, [x22]
    shadd v0.8b, v1.8b, v2.8b
    str  d0, [x21]
    mov  x0, x21
    adr  x1, exp_shadd
    bl   check8

    adr  x22, uhadd_a
    ldr  q4, [x22]
    adr  x22, uhadd_b
    ldr  q5, [x22]
    uhadd v3.8h, v4.8h, v5.8h
    str  q3, [x21, #16]
    add  x0, x21, #16
    adr  x1, exp_uhadd
    bl   check16

    adr  x22, shsub_a
    ldr  q7, [x22]
    adr  x22, shsub_b
    ldr  q8, [x22]
    shsub v6.4s, v7.4s, v8.4s
    str  q6, [x21, #32]
    add  x0, x21, #32
    adr  x1, exp_shsub
    bl   check16

    adr  x22, uhsub_a
    ldr  d10, [x22]
    adr  x22, uhsub_b
    ldr  d11, [x22]
    uhsub v9.2s, v10.2s, v11.2s
    str  q9, [x21, #48]
    add  x0, x21, #48
    adr  x1, exp_uhsub
    bl   check16

    adr  x22, srhadd_a
    ldr  q13, [x22]
    adr  x22, srhadd_b
    ldr  q14, [x22]
    srhadd v12.8h, v13.8h, v14.8h
    str  q12, [x21, #64]
    add  x0, x21, #64
    adr  x1, exp_srhadd
    bl   check16

    adr  x22, urhadd_a
    ldr  q16, [x22]
    adr  x22, urhadd_b
    ldr  q17, [x22]
    urhadd v15.4s, v16.4s, v17.4s
    str  q15, [x21, #80]
    add  x0, x21, #80
    adr  x1, exp_urhadd
    bl   check16

    movz x0, #'H'
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
    .balign 8
shadd_a:
    .byte 0x80, 0x7f, 0xff, 0x01, 0x05, 0xfb, 0x0a, 0xf6
shadd_b:
    .byte 0x01, 0xff, 0x02, 0x03, 0xfa, 0x07, 0x0b, 0xf5
exp_shadd:
    .byte 0xc0, 0x3f, 0x00, 0x02, 0xff, 0x01, 0x0a, 0xf5

    .balign 16
uhadd_a:
    .hword 0, 1, 2, 3, 0xffff, 0xfffe, 100, 101
uhadd_b:
    .hword 1, 2, 3, 4, 1, 2, 101, 102
exp_uhadd:
    .hword 0, 1, 2, 3, 0x8000, 0x8000, 100, 101

    .balign 16
shsub_a:
    .word 0x80000000, 0x7fffffff, 5, -5
shsub_b:
    .word 1, -1, 6, -8
exp_shsub:
    .word 0xbfffffff, 0x40000000, 0xffffffff, 1

    .balign 8
uhsub_a:
    .word 0, 10
uhsub_b:
    .word 1, 20
exp_uhsub:
    .word 0xffffffff, 0xfffffffb, 0, 0

    .balign 16
srhadd_a:
    .hword 0, 1, 0xffff, 2, 0xfffe, 0x7fff, 0x8000, 0xfffd
srhadd_b:
    .hword 1, 2, 2, 3, 3, 0xffff, 0xffff, 0xfffc
exp_srhadd:
    .hword 1, 2, 1, 3, 1, 0x3fff, 0xc000, 0xfffd

    .balign 16
urhadd_a:
    .word 0, 1, 0xffffffff, 0xfffffffe
urhadd_b:
    .word 1, 2, 1, 3
exp_urhadd:
    .word 1, 2, 0x80000000, 0x80000001

dst:
    .space 96
