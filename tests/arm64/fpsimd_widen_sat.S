    .text
    .global _start
_start:
    movz x20, #0
    movk x20, #0x0900, lsl #16
    adr  x21, dst

    adr  x22, sshll2_src
    ldr  q1, [x22]
    sshll2 v0.8h, v1.16b, #0
    str  q0, [x21]
    mov  x0, x21
    adr  x1, exp_sshll2
    bl   check16

    adr  x22, ushll2_src
    ldr  q3, [x22]
    ushll2 v2.4s, v3.8h, #0
    str  q2, [x21, #16]
    add  x0, x21, #16
    adr  x1, exp_ushll2
    bl   check16

    adr  x22, sqxtn2_init
    ldr  q4, [x22]
    adr  x22, sqxtn2_src
    ldr  q5, [x22]
    sqxtn2 v4.16b, v5.8h
    str  q4, [x21, #32]
    add  x0, x21, #32
    adr  x1, exp_sqxtn2
    bl   check16

    adr  x22, uqxtn2_init
    ldr  q6, [x22]
    adr  x22, uqxtn2_src
    ldr  q7, [x22]
    uqxtn2 v6.8h, v7.4s
    str  q6, [x21, #48]
    add  x0, x21, #48
    adr  x1, exp_uqxtn2
    bl   check16

    adr  x22, saddl2_a
    ldr  q8, [x22]
    adr  x22, saddl2_b
    ldr  q9, [x22]
    saddl2 v10.8h, v8.16b, v9.16b
    str  q10, [x21, #64]
    add  x0, x21, #64
    adr  x1, exp_saddl2
    bl   check16

    adr  x22, uaddl2_a
    ldr  q11, [x22]
    adr  x22, uaddl2_b
    ldr  q12, [x22]
    uaddl2 v13.4s, v11.8h, v12.8h
    str  q13, [x21, #80]
    add  x0, x21, #80
    adr  x1, exp_uaddl2
    bl   check16

    adr  x22, saddw2_wide
    ldr  q14, [x22]
    adr  x22, saddw2_narrow
    ldr  q15, [x22]
    saddw2 v16.8h, v14.8h, v15.16b
    str  q16, [x21, #96]
    add  x0, x21, #96
    adr  x1, exp_saddw2
    bl   check16

    adr  x22, uaddw2_wide
    ldr  q17, [x22]
    adr  x22, uaddw2_narrow
    ldr  q18, [x22]
    uaddw2 v19.4s, v17.4s, v18.8h
    str  q19, [x21, #112]
    add  x0, x21, #112
    adr  x1, exp_uaddw2
    bl   check16

    adr  x22, sqadd_a
    ldr  q20, [x22]
    adr  x22, sqadd_b
    ldr  q21, [x22]
    sqadd v22.16b, v20.16b, v21.16b
    str  q22, [x21, #128]
    add  x0, x21, #128
    adr  x1, exp_sqadd
    bl   check16

    adr  x22, uqadd_a
    ldr  q23, [x22]
    adr  x22, uqadd_b
    ldr  q24, [x22]
    uqadd v25.8h, v23.8h, v24.8h
    str  q25, [x21, #144]
    add  x0, x21, #144
    adr  x1, exp_uqadd
    bl   check16

    movz x0, #'Y'
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
sshll2_src:
    .byte 0,1,2,3,4,5,6,7,0xf8,0xf9,0xfa,0xfb,0xfc,0xfd,0xfe,0xff
exp_sshll2:
    .hword 0xfff8,0xfff9,0xfffa,0xfffb,0xfffc,0xfffd,0xfffe,0xffff

ushll2_src:
    .hword 0,0,0,0,1,0x00ff,0x8000,0xffff
exp_ushll2:
    .word 1,0x00ff,0x8000,0xffff

sqxtn2_init:
    .byte 0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0,0,0,0,0,0,0,0
sqxtn2_src:
    .hword 127,128,-129,300,-1,42,-500,126
exp_sqxtn2:
    .byte 0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x7f,0x7f,0x80,0x7f,0xff,0x2a,0x80,0x7e

uqxtn2_init:
    .hword 0x1111,0x2222,0x3333,0x4444,0,0,0,0
uqxtn2_src:
    .word 0x00007fff,0x00010000,0xffffffff,0x00001234
exp_uqxtn2:
    .hword 0x1111,0x2222,0x3333,0x4444,0x7fff,0xffff,0xffff,0x1234

saddl2_a:
    .byte 0,1,2,3,4,5,6,7,1,2,3,4,-1,-2,-3,-4
saddl2_b:
    .byte 10,11,12,13,14,15,16,17,10,20,30,40,1,2,3,4
exp_saddl2:
    .hword 11,22,33,44,0,0,0,0

uaddl2_a:
    .hword 0,0,0,0,1,0x00ff,0x8000,0xffff
uaddl2_b:
    .hword 0,0,0,0,2,1,0x7fff,1
exp_uaddl2:
    .word 3,256,65535,65536

saddw2_wide:
    .hword 100,200,300,400,500,600,700,800
saddw2_narrow:
    .byte 0,1,2,3,4,5,6,7,-1,2,-3,4,-5,6,-7,8
exp_saddw2:
    .hword 99,202,297,404,495,606,693,808

uaddw2_wide:
    .word 100000,200000,300000,400000
uaddw2_narrow:
    .hword 0,0,0,0,1,2,0xffff,0x8000
exp_uaddw2:
    .word 100001,200002,365535,432768

sqadd_a:
    .byte 120,-120,100,-100,127,-128,60,-60,10,-10,30,-30,1,-1,50,-50
sqadd_b:
    .byte 10,-10,40,-40,1,-1,80,-80,120,-120,-100,100,127,-128,100,-100
exp_sqadd:
    .byte 127,-128,127,-128,127,-128,127,-128,127,-128,-70,70,127,-128,127,-128

uqadd_a:
    .hword 0xfff0,1,100,0x8000,0xffff,0,0x1234,0xff00
uqadd_b:
    .hword 0x20,2,0xffff,0x9000,1,0xffff,0x1111,0x200
exp_uqadd:
    .hword 0xffff,3,0xffff,0xffff,0xffff,0xffff,0x2345,0xffff

dst:
    .space 160
