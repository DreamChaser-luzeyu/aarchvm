    .text
    .global _start
_start:
    movz x20, #0
    movk x20, #0x0900, lsl #16
    movz x25, #0x0030, lsl #16
    msr  cpacr_el1, x25
    isb

    adr  x21, scratch

    adr  x22, src_urecpe_2s
    ldr  d0, [x22]
    urecpe v1.2s, v0.2s
    str  d1, [x21]
    movz x24, #'1'
    mov  x0, x21
    adr  x1, exp_urecpe_2s
    bl   check8

    adr  x22, src_urecpe_4s
    ldr  q2, [x22]
    urecpe v3.4s, v2.4s
    str  q3, [x21, #16]
    movz x24, #'2'
    add  x0, x21, #16
    adr  x1, exp_urecpe_4s
    bl   check16

    adr  x22, src_ursqrte_2s
    ldr  d4, [x22]
    ursqrte v5.2s, v4.2s
    str  d5, [x21, #32]
    movz x24, #'3'
    add  x0, x21, #32
    adr  x1, exp_ursqrte_2s
    bl   check8

    adr  x22, src_ursqrte_4s
    ldr  q6, [x22]
    ursqrte v7.4s, v6.4s
    str  q7, [x21, #48]
    movz x24, #'4'
    add  x0, x21, #48
    adr  x1, exp_ursqrte_4s
    bl   check16

    movz x0, #'U'
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
    str  x24, [x20]
    brk  #0

    .data
    .balign 16
src_urecpe_2s:
    .word 0x7fffffff, 0x80000000
exp_urecpe_2s:
    .word 0xffffffff, 0xff800000

src_urecpe_4s:
    .word 0x90000000, 0xc0000000, 0xffffffff, 0x40000000
exp_urecpe_4s:
    .word 0xe3000000, 0xaa800000, 0x80000000, 0xffffffff

src_ursqrte_2s:
    .word 0x3fffffff, 0x40000000
exp_ursqrte_2s:
    .word 0xffffffff, 0xff800000

src_ursqrte_4s:
    .word 0x7fffffff, 0x80000000, 0xc0000000, 0xffffffff
exp_ursqrte_4s:
    .word 0xb5000000, 0xb4800000, 0x93800000, 0x80000000

scratch:
    .space 64
