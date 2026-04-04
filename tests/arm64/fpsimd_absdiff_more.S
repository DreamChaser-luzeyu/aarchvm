    .text
    .global _start
_start:
    movz x20, #0
    movk x20, #0x0900, lsl #16
    movz x25, #0x0030, lsl #16
    msr  cpacr_el1, x25
    isb

    adr  x21, dst

    adr  x22, saba_acc
    ldr  d0, [x22]
    adr  x22, saba_a
    ldr  d1, [x22]
    adr  x22, saba_b
    ldr  d2, [x22]
    saba v0.8b, v1.8b, v2.8b
    str  d0, [x21]
    mov  x0, x21
    adr  x1, exp_saba
    bl   check8

    adr  x22, uaba_acc
    ldr  q3, [x22]
    adr  x22, uaba_a
    ldr  q4, [x22]
    adr  x22, uaba_b
    ldr  q5, [x22]
    uaba v3.8h, v4.8h, v5.8h
    str  q3, [x21, #16]
    add  x0, x21, #16
    adr  x1, exp_uaba
    bl   check16

    adr  x22, sabd_a
    ldr  q6, [x22]
    adr  x22, sabd_b
    ldr  q7, [x22]
    sabd v8.4s, v6.4s, v7.4s
    str  q8, [x21, #32]
    add  x0, x21, #32
    adr  x1, exp_sabd
    bl   check16

    adr  x22, uabd_a
    ldr  d9, [x22]
    adr  x22, uabd_b
    ldr  d10, [x22]
    uabd v11.8b, v9.8b, v10.8b
    str  d11, [x21, #48]
    add  x0, x21, #48
    adr  x1, exp_uabd
    bl   check8

    adr  x22, sabal_acc
    ldr  q12, [x22]
    adr  x22, sabal_a
    ldr  q13, [x22]
    adr  x22, sabal_b
    ldr  q14, [x22]
    sabal v12.8h, v13.8b, v14.8b
    str  q12, [x21, #64]
    add  x0, x21, #64
    adr  x1, exp_sabal
    bl   check16

    adr  x22, uabal2_acc
    ldr  q15, [x22]
    adr  x22, uabal2_a
    ldr  q16, [x22]
    adr  x22, uabal2_b
    ldr  q17, [x22]
    uabal2 v15.4s, v16.8h, v17.8h
    str  q15, [x21, #80]
    add  x0, x21, #80
    adr  x1, exp_uabal2
    bl   check16

    adr  x22, sabdl_a
    ldr  q18, [x22]
    adr  x22, sabdl_b
    ldr  q19, [x22]
    sabdl v20.8h, v18.8b, v19.8b
    str  q20, [x21, #96]
    add  x0, x21, #96
    adr  x1, exp_sabdl
    bl   check16

    adr  x22, uabdl2_a
    ldr  q21, [x22]
    adr  x22, uabdl2_b
    ldr  q22, [x22]
    uabdl2 v23.4s, v21.8h, v22.8h
    str  q23, [x21, #112]
    add  x0, x21, #112
    adr  x1, exp_uabdl2
    bl   check16

    movz x0, #'A'
    str  x0, [x20]
    movz x0, #'D'
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
saba_acc:
    .byte 1, 2, 3, 4, 5, 6, 7, 8
saba_a:
    .byte 0x80, 0x7f, 0xff, 0x01, 0x05, 0xfb, 0x0a, 0xf6
saba_b:
    .byte 0x01, 0xff, 0x02, 0x03, 0xfa, 0x07, 0x0b, 0xf5
exp_saba:
    .byte 0x82, 0x82, 0x06, 0x06, 0x10, 0x12, 0x08, 0x09

    .balign 16
uaba_acc:
    .hword 10, 20, 30, 40, 0xff00, 1, 100, 200
uaba_a:
    .hword 0, 1, 2, 3, 0, 0xffff, 100, 0
uaba_b:
    .hword 1, 2, 10, 4, 0xffff, 2, 50, 0xffff
exp_uaba:
    .hword 11, 21, 38, 41, 0xfeff, 0xfffe, 150, 199

    .balign 16
sabd_a:
    .word 0x80000000, 0x7fffffff, 5, -5
sabd_b:
    .word 1, -1, 6, -8
exp_sabd:
    .word 0x80000001, 0x80000000, 1, 3

    .balign 8
uabd_a:
    .byte 0, 10, 255, 1, 50, 100, 200, 5
uabd_b:
    .byte 1, 20, 0, 250, 45, 110, 100, 5
exp_uabd:
    .byte 1, 10, 255, 249, 5, 10, 100, 0

    .balign 16
sabal_acc:
    .hword 1, 2, 3, 4, 5, 6, 7, 8
sabal_a:
    .byte 0x80, 0x7f, 0xff, 0x01, 0x05, 0xfb, 0x0a, 0xf6
    .byte 0, 0, 0, 0, 0, 0, 0, 0
sabal_b:
    .byte 0x01, 0xff, 0x02, 0x03, 0xfa, 0x07, 0x0b, 0xf5
    .byte 0, 0, 0, 0, 0, 0, 0, 0
exp_sabal:
    .hword 130, 130, 6, 6, 16, 18, 8, 9

    .balign 16
uabal2_acc:
    .word 1000, 2000, 3000, 4000
uabal2_a:
    .hword 0, 0, 0, 0, 100, 200, 300, 400
uabal2_b:
    .hword 0, 0, 0, 0, 90, 250, 100, 1000
exp_uabal2:
    .word 1010, 2050, 3200, 4600

    .balign 16
sabdl_a:
    .byte 0x80, 0x7f, 0xff, 0x01, 0x05, 0xfb, 0x0a, 0xf6
    .byte 0, 0, 0, 0, 0, 0, 0, 0
sabdl_b:
    .byte 0x01, 0xff, 0x02, 0x03, 0xfa, 0x07, 0x0b, 0xf5
    .byte 0, 0, 0, 0, 0, 0, 0, 0
exp_sabdl:
    .hword 129, 128, 3, 2, 11, 12, 1, 1

    .balign 16
uabdl2_a:
    .hword 0, 0, 0, 0, 1, 2, 0xffff, 10
uabdl2_b:
    .hword 0, 0, 0, 0, 10, 1, 0, 30
exp_uabdl2:
    .word 9, 1, 0xffff, 20

dst:
    .space 128
