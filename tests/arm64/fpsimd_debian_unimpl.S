    .text
    .global _start
_start:
    movz x20, #0
    movk x20, #0x0900, lsl #16
    movz x25, #0x0030, lsl #16
    msr  cpacr_el1, x25
    isb

    adr  x21, dst

    // FCVT Dd, Sn
    ldr  w0, =0x40600000          // 3.5f
    fmov s2, w0
    fcvt d3, s2
    fmov x1, d3
    ldr  x2, =0x400c000000000000  // 3.5
    cmp  x1, x2
    b.ne fail

    // FCVT Sd, Dn
    ldr  x0, =0xc004000000000000  // -2.5
    fmov d4, x0
    fcvt s5, d4
    fmov w1, s5
    ldr  w2, =0xc0200000          // -2.5f
    cmp  w1, w2
    b.ne fail

    // CNT (8B)
    adr  x22, cnt_src8
    ldr  d0, [x22]
    cnt  v1.8b, v0.8b
    str  d1, [x21]
    mov  x0, x21
    adr  x1, cnt_exp8
    bl   check8

    // CNT (16B)
    adr  x22, cnt_src16
    ldr  q2, [x22]
    cnt  v3.16b, v2.16b
    str  q3, [x21, #16]
    add  x0, x21, #16
    adr  x1, cnt_exp16
    bl   check16

    // BIC (16B)
    adr  x22, bic16_a
    ldr  q4, [x22]
    adr  x22, bic16_b
    ldr  q5, [x22]
    bic  v6.16b, v4.16b, v5.16b
    str  q6, [x21, #32]
    add  x0, x21, #32
    adr  x1, bic16_exp
    bl   check16

    // BIC (8B)
    adr  x22, bic8_a
    ldr  d7, [x22]
    adr  x22, bic8_b
    ldr  d8, [x22]
    bic  v9.8b, v7.8b, v8.8b
    str  d9, [x21, #48]
    add  x0, x21, #48
    adr  x1, bic8_exp
    bl   check8

    // MOVI (8H immediate, high byte) regression against SSHLL mask overlap
    movi v22.8h, #0x4, lsl #8
    str  q22, [x21, #64]
    add  x0, x21, #64
    adr  x1, movi8h_hi_exp
    bl   check16

    // SADDW (2D)
    adr  x22, saddw_wide
    ldr  q10, [x22]
    adr  x22, saddw_narrow
    ldr  d11, [x22]
    saddw v12.2d, v10.2d, v11.2s
    str  q12, [x21, #80]
    add  x0, x21, #80
    adr  x1, saddw_exp
    bl   check16

    // SADDW2 (2D)
    adr  x22, saddw2_wide
    ldr  q13, [x22]
    adr  x22, saddw2_narrow
    ldr  q14, [x22]
    saddw2 v15.2d, v13.2d, v14.4s
    str  q15, [x21, #96]
    add  x0, x21, #96
    adr  x1, saddw2_exp
    bl   check16

    // UADDW (2D)
    adr  x22, uaddw_wide
    ldr  q16, [x22]
    adr  x22, uaddw_narrow
    ldr  d17, [x22]
    uaddw v18.2d, v16.2d, v17.2s
    str  q18, [x21, #112]
    add  x0, x21, #112
    adr  x1, uaddw_exp
    bl   check16

    // UADDW2 (2D)
    adr  x22, uaddw2_wide
    ldr  q19, [x22]
    adr  x22, uaddw2_narrow
    ldr  q20, [x22]
    uaddw2 v21.2d, v19.2d, v20.4s
    str  q21, [x21, #128]
    add  x0, x21, #128
    adr  x1, uaddw2_exp
    bl   check16

    // REV32 (16B), exact Debian blocker
    adr  x22, rev32_16b_src
    ldr  q23, [x22]
    rev32 v24.16b, v23.16b
    str  q24, [x21, #144]
    add  x0, x21, #144
    adr  x1, rev32_16b_exp
    bl   check16

    // REV64 (8H), same decode family with a different valid arrangement
    adr  x22, rev64_8h_src
    ldr  q25, [x22]
    rev64 v26.8h, v25.8h
    str  q26, [x21, #160]
    add  x0, x21, #160
    adr  x1, rev64_8h_exp
    bl   check16

    // CMEQ (2D), must not alias CMTST
    adr  x22, cmp_eq_lhs
    ldr  q27, [x22]
    adr  x22, cmp_eq_rhs
    ldr  q28, [x22]
    cmeq v29.2d, v27.2d, v28.2d
    str  q29, [x21, #176]
    add  x0, x21, #176
    adr  x1, cmp_eq_exp
    bl   check16

    // CMTST (2D), same opcode family as CMEQ with a different discriminator
    adr  x22, cmtst_lhs
    ldr  q27, [x22]
    adr  x22, cmtst_rhs
    ldr  q28, [x22]
    cmtst v29.2d, v27.2d, v28.2d
    str  q29, [x21, #192]
    add  x0, x21, #192
    adr  x1, cmtst_exp
    bl   check16

    // CMHI/CMGT/CMHS/CMGE (2D), exact family hit by Debian systemd
    adr  x22, cmp_ord_lhs
    ldr  q27, [x22]
    adr  x22, cmp_ord_rhs
    ldr  q28, [x22]

    cmhi v29.2d, v27.2d, v28.2d
    str  q29, [x21, #208]
    add  x0, x21, #208
    adr  x1, cmhi_exp
    bl   check16

    cmgt v29.2d, v27.2d, v28.2d
    str  q29, [x21, #224]
    add  x0, x21, #224
    adr  x1, cmgt_exp
    bl   check16

    cmhs v29.2d, v27.2d, v28.2d
    str  q29, [x21, #240]
    add  x0, x21, #240
    adr  x1, cmhs_exp
    bl   check16

    cmge v29.2d, v27.2d, v28.2d
    str  q29, [x21, #256]
    add  x0, x21, #256
    adr  x1, cmge_exp
    bl   check16

    movz x0, #'J'
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
cnt_src8:
    .byte 0x00,0x01,0x03,0x07,0x0f,0x55,0x80,0xff
cnt_exp8:
    .byte 0,1,2,3,4,4,1,8

cnt_src16:
    .byte 0x00,0x11,0x22,0x33,0x44,0x55,0x66,0x77
    .byte 0x88,0x99,0xaa,0xbb,0xcc,0xdd,0xee,0xff
cnt_exp16:
    .byte 0,2,2,4,2,4,4,6,2,4,4,6,4,6,6,8

bic16_a:
    .byte 0xff,0x0f,0xaa,0x55,0x00,0x3c,0xf0,0x87
    .byte 0x12,0x34,0x56,0x78,0x9a,0xbc,0xde,0xf0
bic16_b:
    .byte 0x0f,0xf0,0x0f,0xaa,0xff,0x33,0x0f,0x78
    .byte 0x10,0x30,0x50,0x70,0x90,0xb0,0xd0,0xf0
bic16_exp:
    .byte 0xf0,0x0f,0xa0,0x55,0x00,0x0c,0xf0,0x87
    .byte 0x02,0x04,0x06,0x08,0x0a,0x0c,0x0e,0x00

bic8_a:
    .byte 0xff,0x0f,0x55,0xaa,0x12,0x34,0x56,0x78
bic8_b:
    .byte 0xf0,0xf0,0x0f,0x0f,0x10,0x30,0x50,0x70
bic8_exp:
    .byte 0x0f,0x0f,0x50,0xa0,0x02,0x04,0x06,0x08

    .balign 16
movi8h_hi_exp:
    .hword 0x0400,0x0400,0x0400,0x0400,0x0400,0x0400,0x0400,0x0400

    .balign 16
saddw_wide:
    .quad 1000,-1000
saddw_narrow:
    .word -10,25
saddw_exp:
    .quad 990,-975

saddw2_wide:
    .quad 100,200
saddw2_narrow:
    .word 1,2,-3,4
saddw2_exp:
    .quad 97,204

uaddw_wide:
    .quad 10,20
uaddw_narrow:
    .word 0xffffffff,0x80000000
uaddw_exp:
    .quad 4294967305,2147483668

uaddw2_wide:
    .quad 30,40
uaddw2_narrow:
    .word 1,2,0xffffffff,0x80000000
uaddw2_exp:
    .quad 4294967325,2147483688

    .balign 16
rev32_16b_src:
    .byte 0x00,0x01,0x02,0x03,0x10,0x11,0x12,0x13
    .byte 0x20,0x21,0x22,0x23,0x30,0x31,0x32,0x33
rev32_16b_exp:
    .byte 0x03,0x02,0x01,0x00,0x13,0x12,0x11,0x10
    .byte 0x23,0x22,0x21,0x20,0x33,0x32,0x31,0x30

    .balign 16
rev64_8h_src:
    .hword 0x0001,0x0002,0x0003,0x0004,0x1001,0x1002,0x1003,0x1004
rev64_8h_exp:
    .hword 0x0004,0x0003,0x0002,0x0001,0x1004,0x1003,0x1002,0x1001

    .balign 16
cmp_eq_lhs:
    .quad 0x1122334455667788,0x00000000deadbeef
cmp_eq_rhs:
    .quad 0x1122334455667788,0x00000000deadbeee
cmp_eq_exp:
    .quad 0xffffffffffffffff,0x0000000000000000

    .balign 16
cmtst_lhs:
    .quad 0x00000000000000f0,0x0000000000000000
cmtst_rhs:
    .quad 0x0000000000000030,0x0000000000000001
cmtst_exp:
    .quad 0xffffffffffffffff,0x0000000000000000

    .balign 16
cmp_ord_lhs:
    .quad 0x8000000000000000,0x0000000000000005
cmp_ord_rhs:
    .quad 0x7fffffffffffffff,0x0000000000000005
cmhi_exp:
    .quad 0xffffffffffffffff,0x0000000000000000
cmgt_exp:
    .quad 0x0000000000000000,0x0000000000000000
cmhs_exp:
    .quad 0xffffffffffffffff,0xffffffffffffffff
cmge_exp:
    .quad 0x0000000000000000,0xffffffffffffffff

dst:
    .space 272
