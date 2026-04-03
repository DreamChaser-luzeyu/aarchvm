    .text
    .global _start
_start:
    movz x20, #0
    movk x20, #0x0900, lsl #16
    movz x25, #0x0030, lsl #16
    msr  cpacr_el1, x25
    isb

    adr  x21, scratch

    adr  x22, addv_b_src
    ldr  d1, [x22]
    addv b0, v1.8b
    str  q0, [x21]
    movz x4, #'1'
    mov  x0, x21
    adr  x1, exp_addv_b
    bl   check16

    adr  x22, addv_h_src
    ldr  q3, [x22]
    addv h2, v3.8h
    str  q2, [x21]
    movz x4, #'2'
    mov  x0, x21
    adr  x1, exp_addv_h
    bl   check16

    adr  x22, addv_s_src
    ldr  q5, [x22]
    addv s4, v5.4s
    str  q4, [x21]
    movz x4, #'3'
    mov  x0, x21
    adr  x1, exp_addv_s
    bl   check16

    adr  x22, saddlv_b_src
    ldr  d7, [x22]
    saddlv h6, v7.8b
    str  q6, [x21]
    movz x4, #'4'
    mov  x0, x21
    adr  x1, exp_saddlv_h
    bl   check16

    adr  x22, saddlv_h_src
    ldr  q9, [x22]
    saddlv s8, v9.8h
    str  q8, [x21]
    movz x4, #'5'
    mov  x0, x21
    adr  x1, exp_saddlv_s
    bl   check16

    adr  x22, saddlv_s_src
    ldr  q11, [x22]
    saddlv d10, v11.4s
    str  q10, [x21]
    movz x4, #'6'
    mov  x0, x21
    adr  x1, exp_saddlv_d
    bl   check16

    adr  x22, uaddlv_b_src
    ldr  d13, [x22]
    uaddlv h12, v13.8b
    str  q12, [x21]
    movz x4, #'7'
    mov  x0, x21
    adr  x1, exp_uaddlv_h
    bl   check16

    adr  x22, uaddlv_h_src
    ldr  q15, [x22]
    uaddlv s14, v15.8h
    str  q14, [x21]
    movz x4, #'8'
    mov  x0, x21
    adr  x1, exp_uaddlv_s
    bl   check16

    adr  x22, uaddlv_s_src
    ldr  q17, [x22]
    uaddlv d16, v17.4s
    str  q16, [x21]
    movz x4, #'9'
    mov  x0, x21
    adr  x1, exp_uaddlv_d
    bl   check16

    adr  x22, smaxv_b_src
    ldr  q19, [x22]
    smaxv b18, v19.16b
    str  q18, [x21]
    movz x4, #'a'
    mov  x0, x21
    adr  x1, exp_smaxv_b
    bl   check16

    adr  x22, sminv_h_src
    ldr  q21, [x22]
    sminv h20, v21.8h
    str  q20, [x21]
    movz x4, #'b'
    mov  x0, x21
    adr  x1, exp_sminv_h
    bl   check16

    adr  x22, umaxv_s_src
    ldr  q23, [x22]
    umaxv s22, v23.4s
    str  q22, [x21]
    movz x4, #'c'
    mov  x0, x21
    adr  x1, exp_umaxv_s
    bl   check16

    adr  x22, uminv_b_src
    ldr  q25, [x22]
    uminv b24, v25.16b
    str  q24, [x21]
    movz x4, #'d'
    mov  x0, x21
    adr  x1, exp_uminv_b
    bl   check16

    adr  x22, sadalp_h_dst
    ldr  d26, [x22]
    adr  x22, sadalp_h_src
    ldr  d27, [x22]
    sadalp v26.4h, v27.8b
    str  q26, [x21]
    movz x4, #'e'
    mov  x0, x21
    adr  x1, exp_sadalp_h
    bl   check16

    adr  x22, sadalp_s_dst
    ldr  d28, [x22]
    adr  x22, sadalp_s_src
    ldr  d29, [x22]
    sadalp v28.2s, v29.4h
    str  q28, [x21]
    movz x4, #'f'
    mov  x0, x21
    adr  x1, exp_sadalp_s
    bl   check16

    adr  x22, uadalp_h_dst
    ldr  q30, [x22]
    adr  x22, uadalp_h_src
    ldr  q31, [x22]
    uadalp v30.8h, v31.16b
    str  q30, [x21]
    movz x4, #'g'
    mov  x0, x21
    adr  x1, exp_uadalp_h
    bl   check16

    adr  x22, sadalp_d_dst
    ldr  q0, [x22]
    adr  x22, sadalp_d_src
    ldr  q1, [x22]
    sadalp v0.2d, v1.4s
    str  q0, [x21]
    movz x4, #'h'
    mov  x0, x21
    adr  x1, exp_sadalp_d
    bl   check16

    adr  x22, uadalp_d_dst
    ldr  q2, [x22]
    adr  x22, uadalp_d_src
    ldr  q3, [x22]
    uadalp v2.2d, v3.4s
    str  q2, [x21]
    movz x4, #'i'
    mov  x0, x21
    adr  x1, exp_uadalp_d
    bl   check16

    movz x0, #'A'
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
    str  x4, [x20]
    brk  #0

    .data
    .balign 16
addv_b_src:
    .byte 1, 2, 3, 4, 5, 6, 7, 8
exp_addv_b:
    .byte 36
    .zero 15

    .balign 16
addv_h_src:
    .hword 1, 2, 3, 4, 5, 6, 7, 8
exp_addv_h:
    .hword 36
    .zero 14

    .balign 16
addv_s_src:
    .word 1, 2, 3, 4
exp_addv_s:
    .word 10
    .zero 12

    .balign 16
saddlv_b_src:
    .byte 1, -2, 127, -128, 16, -16, 10, -5
exp_saddlv_h:
    .hword 3
    .zero 14

    .balign 16
saddlv_h_src:
    .hword 1000, -2000, 3000, -4000, 500, -600, 700, -800
exp_saddlv_s:
    .word -2200
    .zero 12

    .balign 16
saddlv_s_src:
    .word 100000, -200000, 300000, -50000
exp_saddlv_d:
    .quad 150000
    .zero 8

    .balign 16
uaddlv_b_src:
    .byte 250, 1, 2, 3, 4, 5, 6, 7
exp_uaddlv_h:
    .hword 278
    .zero 14

    .balign 16
uaddlv_h_src:
    .hword 1000, 2000, 3000, 4000, 5000, 6000, 7000, 8000
exp_uaddlv_s:
    .word 36000
    .zero 12

    .balign 16
uaddlv_s_src:
    .word 0xffffffff, 1, 2, 3
exp_uaddlv_d:
    .quad 0x0000000100000005
    .zero 8

    .balign 16
smaxv_b_src:
    .byte -5, 100, -1, 50, 20, -30, 40, -60, 10, 11, 12, 13, 14, 15, 16, 17
exp_smaxv_b:
    .byte 100
    .zero 15

    .balign 16
sminv_h_src:
    .hword 100, -200, 300, -400, 500, 600, -700, 800
exp_sminv_h:
    .hword -700
    .zero 14

    .balign 16
umaxv_s_src:
    .word 1, 0xffffffff, 123, 456
exp_umaxv_s:
    .word 0xffffffff
    .zero 12

    .balign 16
uminv_b_src:
    .byte 9, 8, 7, 6, 5, 4, 3, 2, 1, 0, 10, 11, 12, 13, 14, 15
exp_uminv_b:
    .byte 0
    .zero 15

    .balign 16
sadalp_h_dst:
    .hword 100, 200, 300, 400
sadalp_h_src:
    .byte 1, 2, -1, -2, 127, -128, 16, -16
exp_sadalp_h:
    .hword 103, 197, 299, 400
    .zero 8

    .balign 16
sadalp_s_dst:
    .word 100000, 200000
sadalp_s_src:
    .hword 1000, -2000, 3000, -4000
exp_sadalp_s:
    .word 99000, 199000
    .zero 8

    .balign 16
uadalp_h_dst:
    .hword 100, 200, 300, 400, 500, 600, 700, 800
uadalp_h_src:
    .byte 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16
exp_uadalp_h:
    .hword 103, 207, 311, 415, 519, 623, 727, 831

    .balign 16
sadalp_d_dst:
    .quad 10000000000, 20000000000
sadalp_d_src:
    .word 100000, -200000, 300000, -400000
exp_sadalp_d:
    .quad 9999900000, 19999900000

    .balign 16
uadalp_d_dst:
    .quad 0x0000000100000000, 0x0000000200000000
uadalp_d_src:
    .word 0xffffffff, 1, 2, 3
exp_uadalp_d:
    .quad 0x0000000200000000, 0x0000000200000005

    .balign 16
scratch:
    .space 16
