    .text
    .global _start
_start:
    movz x20, #0
    movk x20, #0x0900, lsl #16
    movz x25, #0x0030, lsl #16
    msr  cpacr_el1, x25
    isb

    adr  x21, scratch

    // UMINP (vector, 4H)
    adr  x22, uminp_h_src
    ldr  d1, [x22]
    ldr  d2, [x22, #8]
    uminp v0.4h, v1.4h, v2.4h
    str  d0, [x21]
    mov  x0, x21
    adr  x1, exp_uminp_h
    bl   check8

    // UMAXP (vector, 2S)
    adr  x22, umaxp_s_src
    ldr  d4, [x22]
    ldr  d5, [x22, #8]
    umaxp v3.2s, v4.2s, v5.2s
    str  d3, [x21, #8]
    add  x0, x21, #8
    adr  x1, exp_umaxp_s
    bl   check8

    // ADDP (vector, 8H)
    adr  x22, addp_h_src
    ldr  q7, [x22]
    ldr  q8, [x22, #16]
    addp v6.8h, v7.8h, v8.8h
    str  q6, [x21, #16]
    add  x0, x21, #16
    adr  x1, exp_addp_h
    bl   check16

    // SMAXP (vector, 4S)
    adr  x22, smaxp_s_src
    ldr  q10, [x22]
    ldr  q11, [x22, #16]
    smaxp v9.4s, v10.4s, v11.4s
    str  q9, [x21, #32]
    add  x0, x21, #32
    adr  x1, exp_smaxp_s
    bl   check16

    // SMINP (vector, 8H)
    adr  x22, sminp_h_src
    ldr  q13, [x22]
    ldr  q14, [x22, #16]
    sminp v12.8h, v13.8h, v14.8h
    str  q12, [x21, #48]
    add  x0, x21, #48
    adr  x1, exp_sminp_h
    bl   check16

    movz x0, #'P'
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
uminp_h_src:
    .hword 10, 20, 30, 40
    .hword 7, 25, 35, 15
exp_uminp_h:
    .hword 10, 30, 7, 15

umaxp_s_src:
    .word 100, 50
    .word 77, 88
exp_umaxp_s:
    .word 100, 88

addp_h_src:
    .hword 1, 2, 3, 4, 5, 6, 7, 8
    .hword 9, 10, 11, 12, 13, 14, 15, 16
exp_addp_h:
    .hword 3, 7, 11, 15, 19, 23, 27, 31

smaxp_s_src:
    .word -1, 20, -30, 40
    .word -5, -6, 7, 8
exp_smaxp_s:
    .word 20, 40, -5, 8

sminp_h_src:
    .hword -1, 2, -3, 4, 300, -400, 500, -600
    .hword 7, -8, 9, -10, 11, 12, -13, -14
exp_sminp_h:
    .hword -1, -3, -400, -600, -8, -10, 11, -14

scratch:
    .space 64
