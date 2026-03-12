    .text
    .align 2

    .global fpsimd_logic_case
    .type fpsimd_logic_case, %function
fpsimd_logic_case:
    adrp x1, logic_a
    add  x1, x1, :lo12:logic_a
    adrp x2, logic_b
    add  x2, x2, :lo12:logic_b
    adrp x3, logic_mask
    add  x3, x3, :lo12:logic_mask
    ldr  q0, [x1]
    ldr  q1, [x2]
    ldr  q5, [x3]

    and  v2.16b, v0.16b, v1.16b
    str  q2, [x0], #16
    eor  v3.16b, v0.16b, v1.16b
    str  q3, [x0], #16

    mov  v4.16b, v0.16b
    bit  v4.16b, v1.16b, v5.16b
    str  q4, [x0], #16

    mov  v6.16b, v0.16b
    bif  v6.16b, v1.16b, v5.16b
    str  q6, [x0], #16

    mov  v7.16b, v5.16b
    bsl  v7.16b, v0.16b, v1.16b
    str  q7, [x0], #16

    cmeq v8.16b, v0.16b, #0
    str  q8, [x0], #16
    cmhs v9.16b, v0.16b, v1.16b
    str  q9, [x0]
    ret

    .global fpsimd_perm_case
    .type fpsimd_perm_case, %function
fpsimd_perm_case:
    adrp x1, perm_a
    add  x1, x1, :lo12:perm_a
    adrp x2, perm_b
    add  x2, x2, :lo12:perm_b
    adrp x3, perm_h_a
    add  x3, x3, :lo12:perm_h_a
    adrp x4, perm_h_b
    add  x4, x4, :lo12:perm_h_b
    adrp x5, perm_s_a
    add  x5, x5, :lo12:perm_s_a
    adrp x6, perm_s_b
    add  x6, x6, :lo12:perm_s_b

    ldr  q0, [x1]
    ldr  q1, [x2]
    ext  v2.16b, v0.16b, v1.16b, #5
    str  q2, [x0], #16

    zip1 v3.16b, v0.16b, v1.16b
    str  q3, [x0], #16

    ldr  q4, [x3]
    ldr  q5, [x4]
    uzp1 v6.8h, v4.8h, v5.8h
    str  q6, [x0], #16

    ldr  q7, [x5]
    ldr  q8, [x6]
    trn1 v9.4s, v7.4s, v8.4s
    str  q9, [x0]
    ret

    .global fpsimd_arith_case
    .type fpsimd_arith_case, %function
fpsimd_arith_case:
    adrp x1, arith_b_a
    add  x1, x1, :lo12:arith_b_a
    adrp x2, arith_b_b
    add  x2, x2, :lo12:arith_b_b
    adrp x3, arith_h_a
    add  x3, x3, :lo12:arith_h_a
    adrp x4, arith_h_b
    add  x4, x4, :lo12:arith_h_b
    adrp x5, arith_s_a
    add  x5, x5, :lo12:arith_s_a
    adrp x6, arith_s_b
    add  x6, x6, :lo12:arith_s_b

    ldr  q0, [x1]
    ldr  q1, [x2]
    add  v2.16b, v0.16b, v1.16b
    str  q2, [x0], #16

    ldr  q3, [x3]
    ldr  q4, [x4]
    sub  v5.8h, v3.8h, v4.8h
    str  q5, [x0], #16

    ldr  q6, [x5]
    ldr  q7, [x6]
    mul  v8.4s, v6.4s, v7.4s
    str  q8, [x0]
    ret

    .global fpsimd_shift_case
    .type fpsimd_shift_case, %function
fpsimd_shift_case:
    adrp x1, shift_h_src
    add  x1, x1, :lo12:shift_h_src
    adrp x2, shift_s_src
    add  x2, x2, :lo12:shift_s_src
    adrp x3, shrn_h_src
    add  x3, x3, :lo12:shrn_h_src
    adrp x4, uxtl_b_src
    add  x4, x4, :lo12:uxtl_b_src
    adrp x5, sxtl_h_src
    add  x5, x5, :lo12:sxtl_h_src

    ldr  q0, [x1]
    ushr v1.8h, v0.8h, #3
    str  q1, [x0], #16

    ldr  q2, [x2]
    sshr v3.4s, v2.4s, #5
    str  q3, [x0], #16

    ldr  q4, [x3]
    shrn v5.8b, v4.8h, #8
    str  d5, [x0], #8

    ldr  d6, [x4]
    uxtl v7.8h, v6.8b
    str  q7, [x0], #16

    ldr  d8, [x5]
    sxtl v9.4s, v8.4h
    str  q9, [x0]
    ret

    .global fpsimd_fp_scalar_case
    .type fpsimd_fp_scalar_case, %function
fpsimd_fp_scalar_case:
    adrp x1, fp_two_bits
    add  x1, x1, :lo12:fp_two_bits
    ldr  d0, [x1]
    adrp x1, fp_neg_three_bits
    add  x1, x1, :lo12:fp_neg_three_bits
    ldr  d1, [x1]
    adrp x1, fp_seven_bits
    add  x1, x1, :lo12:fp_seven_bits
    ldr  d2, [x1]
    adrp x1, fp_five_bits
    add  x1, x1, :lo12:fp_five_bits
    ldr  d3, [x1]
    adrp x1, fp_one_half_bits
    add  x1, x1, :lo12:fp_one_half_bits
    ldr  d4, [x1]
    adrp x1, fp_three_point_seven_five_bits
    add  x1, x1, :lo12:fp_three_point_seven_five_bits
    ldr  d5, [x1]

    fmul d6, d0, d1
    fmov x2, d6
    str  x2, [x0], #8

    fadd d6, d3, d4
    fmov x2, d6
    str  x2, [x0], #8

    fsub d6, d3, d4
    fmov x2, d6
    str  x2, [x0], #8

    fdiv d6, d2, d0
    fmov x2, d6
    str  x2, [x0], #8

    fabs d6, d1
    fmov x2, d6
    str  x2, [x0], #8

    fneg d6, d0
    fmov x2, d6
    str  x2, [x0], #8

    mov  x2, #-42
    scvtf d6, x2
    fmov x3, d6
    str  x3, [x0], #8

    mov  x2, #42
    ucvtf d6, x2
    fmov x3, d6
    str  x3, [x0], #8

    fcvtzs x2, d5
    str  x2, [x0], #8

    fcvtzu x2, d5
    str  x2, [x0], #8

    fcmp d3, d4
    fcsel d6, d3, d4, gt
    fmov x2, d6
    str  x2, [x0]
    ret

    .section .rodata
    .align 4
logic_a:
    .byte 0x00,0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88,0x99,0xaa,0xbb,0xcc,0xdd,0xee,0xff
logic_b:
    .byte 0xff,0xee,0xdd,0xcc,0xbb,0xaa,0x99,0x88,0x77,0x66,0x55,0x44,0x33,0x22,0x11,0x00
logic_mask:
    .byte 0xf0,0x0f,0xf0,0x0f,0xf0,0x0f,0xf0,0x0f,0xf0,0x0f,0xf0,0x0f,0xf0,0x0f,0xf0,0x0f

perm_a:
    .byte 0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f
perm_b:
    .byte 0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x8a,0x8b,0x8c,0x8d,0x8e,0x8f
perm_h_a:
    .hword 0x1000,0x1001,0x1002,0x1003,0x1004,0x1005,0x1006,0x1007
perm_h_b:
    .hword 0x2000,0x2001,0x2002,0x2003,0x2004,0x2005,0x2006,0x2007
perm_s_a:
    .word 0x11111111,0x22222222,0x33333333,0x44444444
perm_s_b:
    .word 0xaaaaaaaa,0xbbbbbbbb,0xcccccccc,0xdddddddd

arith_b_a:
    .byte 1,4,7,10,13,16,19,22,25,28,31,34,37,40,43,46
arith_b_b:
    .byte 3,8,13,18,23,28,33,38,43,48,53,58,63,68,73,78
arith_h_a:
    .hword 1000,2000,3000,4000,5000,6000,7000,8000
arith_h_b:
    .hword 7,11,13,17,19,23,29,31
arith_s_a:
    .word 7,11,13,17
arith_s_b:
    .word 19,23,29,31

shift_h_src:
    .hword 0x1234,0x5678,0x9abc,0xdef0,0x00ff,0xff00,0x0f0f,0xf0f0
shift_s_src:
    .word 0x00000100,0xfffffe00,0x7fffffff,0x80000000
shrn_h_src:
    .hword 0x1234,0x5678,0x9abc,0xdef0,0x00ff,0xff00,0x0f0f,0xf0f0
uxtl_b_src:
    .byte 0x00,0x7f,0x80,0xff,0x12,0x34,0x56,0x78
sxtl_h_src:
    .hword 0x0001,0x7fff,0x8000,0xffff

fp_two_bits:
    .quad 0x4000000000000000
fp_neg_three_bits:
    .quad 0xc008000000000000
fp_seven_bits:
    .quad 0x401c000000000000
fp_five_bits:
    .quad 0x4014000000000000
fp_one_half_bits:
    .quad 0x3ff8000000000000
fp_three_point_seven_five_bits:
    .quad 0x400e000000000000
