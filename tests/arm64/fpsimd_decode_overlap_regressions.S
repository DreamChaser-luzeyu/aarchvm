    .text
    .global _start
_start:
    movz x20, #0
    movk x20, #0x0900, lsl #16
    movz x25, #0x0030, lsl #16
    msr  cpacr_el1, x25
    isb

    adr  x21, scratch

    adr  x22, shift_h_src
    ldr  q0, [x22]
    ushr v1.8h, v0.8h, #3
    str  q1, [x21]
    movz x24, #'1'
    mov  x0, x21
    adr  x1, exp_ushr
    bl   check16

    adr  x22, shift_s_src
    ldr  q2, [x22]
    sshr v3.4s, v2.4s, #5
    str  q3, [x21, #16]
    movz x24, #'2'
    add  x0, x21, #16
    adr  x1, exp_sshr
    bl   check16

    movi v4.4s, #0x34, lsl #8
    str  q4, [x21, #32]
    movz x24, #'3'
    add  x0, x21, #32
    adr  x1, exp_movi32_8
    bl   check16

    movi v5.16b, #0
    orr  v5.8h, #0xf, lsl #8
    str  q5, [x21, #48]
    movz x24, #'4'
    add  x0, x21, #48
    adr  x1, exp_orr16_8
    bl   check16

    mvni v6.4s, #0x34, lsl #8
    str  q6, [x21, #64]
    movz x24, #'5'
    add  x0, x21, #64
    adr  x1, exp_mvni32_8
    bl   check16

    movi v7.16b, #0x33
    bic  v7.8h, #0xf0
    str  q7, [x21, #80]
    movz x24, #'6'
    add  x0, x21, #80
    adr  x1, exp_bic_lo
    bl   check16

    adr  x22, trn_a
    ldr  q16, [x22]
    adr  x22, trn_b
    ldr  q17, [x22]
    trn1 v18.4s, v16.4s, v17.4s
    str  q18, [x21, #96]
    movz x24, #'7'
    add  x0, x21, #96
    adr  x1, exp_trn1
    bl   check16

    adr  x22, xtn8_src
    ldr  q19, [x22]
    xtn  v20.8b, v19.8h
    str  d20, [x21, #112]
    movz x24, #'8'
    add  x0, x21, #112
    adr  x1, exp_xtn8
    bl   check8

    adr  x22, xtn2_8_init
    ldr  q21, [x22]
    adr  x22, xtn2_8_src
    ldr  q22, [x22]
    xtn2 v21.16b, v22.8h
    str  q21, [x21, #128]
    movz x24, #'9'
    add  x0, x21, #128
    adr  x1, exp_xtn2_8
    bl   check16

    adr  x22, div_s_a
    ldr  q23, [x22]
    adr  x22, div_s_b
    ldr  q24, [x22]
    fdiv v25.4s, v23.4s, v24.4s
    str  q25, [x21, #144]
    movz x24, #'a'
    add  x0, x21, #144
    adr  x1, exp_div_s
    bl   check16

    msr  fpcr, xzr
    msr  fpsr, xzr
    adr  x22, src_vec_frecps_s
    ldr  q26, [x22]
    adr  x22, src_vec_frecps_s_rhs
    ldr  q27, [x22]
    frecps v28.4s, v26.4s, v27.4s
    str  q28, [x21, #160]
    movz x24, #'b'
    add  x0, x21, #160
    adr  x1, exp_vec_frecps_s
    bl   check16

    adr  x22, overlap_fcvt_s_src
    ldr  q0, [x22]
    fcvtns v1.4s, v0.4s
    str  q1, [x21, #176]
    movz x24, #'c'
    add  x0, x21, #176
    adr  x1, exp_overlap_fcvtns_s
    bl   check16

    adr  x22, overlap_fcvt_2s_src
    ldr  q0, [x22]
    fcvtps v2.2s, v0.2s
    str  q2, [x21, #192]
    movz x24, #'d'
    add  x0, x21, #192
    adr  x1, exp_overlap_fcvtps_2s
    bl   check16

    adr  x22, overlap_fcvt_d_src
    ldr  q0, [x22]
    fcvtns v3.2d, v0.2d
    str  q3, [x21, #208]
    movz x24, #'e'
    add  x0, x21, #208
    adr  x1, exp_overlap_fcvtns_d
    bl   check16

    movz x0, #'O'
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
shift_h_src:
    .hword 0x1234, 0x5678, 0x9abc, 0xdef0, 0x00ff, 0xff00, 0x0f0f, 0xf0f0
exp_ushr:
    .hword 0x0246, 0x0acf, 0x1357, 0x1bde, 0x001f, 0x1fe0, 0x01e1, 0x1e1e

shift_s_src:
    .word 0x00000100, 0xfffffe00, 0x7fffffff, 0x80000000
exp_sshr:
    .word 0x00000008, 0xfffffff0, 0x03ffffff, 0xfc000000

exp_movi32_8:
    .quad 0x0000340000003400
    .quad 0x0000340000003400
exp_orr16_8:
    .quad 0x0f000f000f000f00
    .quad 0x0f000f000f000f00
exp_mvni32_8:
    .quad 0xffffcbffffffcbff
    .quad 0xffffcbffffffcbff
exp_bic_lo:
    .quad 0x3303330333033303
    .quad 0x3303330333033303

trn_a:
    .word 0x11111111, 0x22222222, 0x33333333, 0x44444444
trn_b:
    .word 0xaaaaaaaa, 0xbbbbbbbb, 0xcccccccc, 0xdddddddd
exp_trn1:
    .word 0x11111111, 0xaaaaaaaa, 0x33333333, 0xcccccccc

xtn8_src:
    .hword 0x0102, 0x0304, 0x0506, 0x0708, 0xff09, 0x1234, 0xabcd, 0x00ff
exp_xtn8:
    .byte 0x02, 0x04, 0x06, 0x08, 0x09, 0x34, 0xcd, 0xff

xtn2_8_init:
    .byte 0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7
    .byte 0, 0, 0, 0, 0, 0, 0, 0
xtn2_8_src:
    .hword 0x1111, 0x2222, 0x3333, 0x4444, 0x5555, 0x6666, 0x7777, 0x8888
exp_xtn2_8:
    .byte 0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7
    .byte 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88

div_s_a:
    .word 0x41100000, 0xc1000000, 0x40f00000, 0xc0c00000
div_s_b:
    .word 0x40400000, 0x40000000, 0x40200000, 0xc0000000
exp_div_s:
    .word 0x40400000, 0xc0800000, 0x40400000, 0x40400000

src_vec_frecps_s:
    .word 0x3f800000, 0x40000000, 0x7f800000, 0xbf800000
src_vec_frecps_s_rhs:
    .word 0x3f000000, 0x3f800000, 0x00000000, 0x40000000
exp_vec_frecps_s:
    .word 0x3fc00000, 0x00000000, 0x40000000, 0x40800000

overlap_fcvt_s_src:
    .word 0x3fc00000, 0xc0200000, 0x40600000, 0xbf400000
exp_overlap_fcvtns_s:
    .word 0x00000002, 0xfffffffe, 0x00000004, 0xffffffff

overlap_fcvt_2s_src:
    .word 0x40066666, 0xc0066666, 0x00000000, 0x00000000
exp_overlap_fcvtps_2s:
    .word 0x00000003, 0xfffffffe, 0x00000000, 0x00000000

overlap_fcvt_d_src:
    .quad 0x3ff8000000000000, 0xc004000000000000
exp_overlap_fcvtns_d:
    .quad 0x0000000000000002, 0xfffffffffffffffe

scratch:
    .space 224
