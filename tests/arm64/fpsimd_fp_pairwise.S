    .text
    .global _start
_start:
    movz x20, #0
    movk x20, #0x0900, lsl #16
    adr  x21, dst
    movz x25, #0x0030, lsl #16
    msr  cpacr_el1, x25
    isb

    adr  x22, src_addp_2s_a
    ldr  q1, [x22]
    adr  x22, src_addp_2s_b
    ldr  q2, [x22]
    faddp v0.2s, v1.2s, v2.2s
    str  q0, [x21]
    mov  x0, x21
    adr  x1, exp_addp_2s
    bl   check16_1

    adr  x22, src_addp_2d_a
    ldr  q3, [x22]
    adr  x22, src_addp_2d_b
    ldr  q4, [x22]
    faddp v2.2d, v3.2d, v4.2d
    str  q2, [x21, #16]
    add  x0, x21, #16
    adr  x1, exp_addp_2d
    bl   check16_2

    msr  fpsr, xzr
    adr  x22, src_maxp_4s_a
    ldr  q5, [x22]
    adr  x22, src_maxp_4s_b
    ldr  q6, [x22]
    fmaxp v4.4s, v5.4s, v6.4s
    str  q4, [x21, #32]
    add  x0, x21, #32
    adr  x1, exp_maxp_4s
    bl   check16_3
    mrs  x2, fpsr
    cbnz x2, fail3

    msr  fpsr, xzr
    adr  x22, src_minp_2s_a
    ldr  q7, [x22]
    adr  x22, src_minp_2s_b
    ldr  q8, [x22]
    fminp v6.2s, v7.2s, v8.2s
    str  q6, [x21, #48]
    add  x0, x21, #48
    adr  x1, exp_minp_2s
    bl   check16_4
    mrs  x2, fpsr
    cbnz x2, fail4

    msr  fpsr, xzr
    adr  x22, src_maxnmp_2d_a
    ldr  q9, [x22]
    adr  x22, src_maxnmp_2d_b
    ldr  q10, [x22]
    fmaxnmp v8.2d, v9.2d, v10.2d
    str  q8, [x21, #64]
    add  x0, x21, #64
    adr  x1, exp_maxnmp_2d
    bl   check16_5
    mrs  x2, fpsr
    cbnz x2, fail5

    msr  fpsr, xzr
    adr  x22, src_minnmp_4s_a
    ldr  q11, [x22]
    adr  x22, src_minnmp_4s_b
    ldr  q12, [x22]
    fminnmp v10.4s, v11.4s, v12.4s
    str  q10, [x21, #80]
    add  x0, x21, #80
    adr  x1, exp_minnmp_4s
    bl   check16_6
    mrs  x2, fpsr
    cmp  x2, #1
    b.ne fail6

    msr  fpcr, xzr
    msr  fpsr, xzr
    adr  x22, src_nan_order_4s
    ldr  q13, [x22]
    ldr  q14, [x22]
    fmaxp v12.4s, v13.4s, v14.4s
    str  q12, [x21, #96]
    add  x0, x21, #96
    adr  x1, exp_nan_order_4s
    bl   check16_7
    mrs  x2, fpsr
    cmp  x2, #1
    b.ne fail7

    mov  x24, #(1 << 25)
    msr  fpcr, x24
    isb
    msr  fpsr, xzr
    adr  x22, src_dn_nan_2d_a
    ldr  q15, [x22]
    adr  x22, src_dn_nan_2d_b
    ldr  q16, [x22]
    fminnmp v14.2d, v15.2d, v16.2d
    str  q14, [x21, #112]
    add  x0, x21, #112
    adr  x1, exp_dn_nan_2d
    bl   check16_8
    mrs  x2, fpsr
    cmp  x2, #1
    b.ne fail8

    movz x0, #'Z'
    str  x0, [x20]
    brk  #0

check16_1:
    ldr  x2, [x0]
    ldr  x3, [x1]
    cmp  x2, x3
    b.ne fail1
    ldr  x2, [x0, #8]
    ldr  x3, [x1, #8]
    cmp  x2, x3
    b.ne fail1
    ret

check16_2:
    ldr  x2, [x0]
    ldr  x3, [x1]
    cmp  x2, x3
    b.ne fail2
    ldr  x2, [x0, #8]
    ldr  x3, [x1, #8]
    cmp  x2, x3
    b.ne fail2
    ret

check16_3:
    ldr  x2, [x0]
    ldr  x3, [x1]
    cmp  x2, x3
    b.ne fail3
    ldr  x2, [x0, #8]
    ldr  x3, [x1, #8]
    cmp  x2, x3
    b.ne fail3
    ret

check16_4:
    ldr  x2, [x0]
    ldr  x3, [x1]
    cmp  x2, x3
    b.ne fail4
    ldr  x2, [x0, #8]
    ldr  x3, [x1, #8]
    cmp  x2, x3
    b.ne fail4
    ret

check16_5:
    ldr  x2, [x0]
    ldr  x3, [x1]
    cmp  x2, x3
    b.ne fail5
    ldr  x2, [x0, #8]
    ldr  x3, [x1, #8]
    cmp  x2, x3
    b.ne fail5
    ret

check16_6:
    ldr  x2, [x0]
    ldr  x3, [x1]
    cmp  x2, x3
    b.ne fail6
    ldr  x2, [x0, #8]
    ldr  x3, [x1, #8]
    cmp  x2, x3
    b.ne fail6
    ret

check16_7:
    ldr  x2, [x0]
    ldr  x3, [x1]
    cmp  x2, x3
    b.ne fail7
    ldr  x2, [x0, #8]
    ldr  x3, [x1, #8]
    cmp  x2, x3
    b.ne fail7
    ret

check16_8:
    ldr  x2, [x0]
    ldr  x3, [x1]
    cmp  x2, x3
    b.ne fail8
    ldr  x2, [x0, #8]
    ldr  x3, [x1, #8]
    cmp  x2, x3
    b.ne fail8
    ret

fail1:
    movz x0, #'1'
    b done
fail2:
    movz x0, #'2'
    b done
fail3:
    movz x0, #'3'
    b done
fail4:
    movz x0, #'4'
    b done
fail5:
    movz x0, #'5'
    b done
fail6:
    movz x0, #'6'
    b done
fail7:
    movz x0, #'7'
    b done
fail8:
    movz x0, #'8'
done:
    str  x0, [x20]
    brk  #0

    .data
    .balign 16
src_addp_2s_a:
    .word 0x3fc00000, 0x40100000, 0x00000000, 0x00000000
src_addp_2s_b:
    .word 0x40800000, 0xbf000000, 0x00000000, 0x00000000
exp_addp_2s:
    .word 0x40700000, 0x40600000, 0x00000000, 0x00000000

src_addp_2d_a:
    .quad 0x3ff8000000000000, 0x4004000000000000
src_addp_2d_b:
    .quad 0xbff0000000000000, 0x3fd0000000000000
exp_addp_2d:
    .quad 0x4010000000000000, 0xbfe8000000000000

src_maxp_4s_a:
    .word 0x7fc12345, 0x3f800000, 0x40000000, 0x40400000
src_maxp_4s_b:
    .word 0x80000000, 0x00000000, 0x40800000, 0xc0a00000
exp_maxp_4s:
    .word 0x7fc12345, 0x40400000, 0x00000000, 0x40800000

src_minp_2s_a:
    .word 0x40a00000, 0xc0000000, 0x00000000, 0x00000000
src_minp_2s_b:
    .word 0x3fc00000, 0x40200000, 0x00000000, 0x00000000
exp_minp_2s:
    .word 0xc0000000, 0x3fc00000, 0x00000000, 0x00000000

src_maxnmp_2d_a:
    .quad 0x7ff8123456789abc, 0x3ff8000000000000
src_maxnmp_2d_b:
    .quad 0xc010000000000000, 0x401c000000000000
exp_maxnmp_2d:
    .quad 0x3ff8000000000000, 0x401c000000000000

src_minnmp_4s_a:
    .word 0x7f812345, 0x3f800000, 0x40a00000, 0xc0400000
src_minnmp_4s_b:
    .word 0x7fc12345, 0x40000000, 0x3fc00000, 0x3f000000
exp_minnmp_4s:
    .word 0x7fc12345, 0xc0400000, 0x40000000, 0x3f000000

src_nan_order_4s:
    .word 0x7fc11111, 0x7f822222, 0x7f833333, 0x7fc44444
exp_nan_order_4s:
    .word 0x7fc22222, 0x7fc33333, 0x7fc22222, 0x7fc33333

src_dn_nan_2d_a:
    .quad 0x7ff8111111111111, 0x7ff0222222222222
src_dn_nan_2d_b:
    .quad 0x7ff8333333333333, 0x7ff0444444444444
exp_dn_nan_2d:
    .quad 0x7ff8000000000000, 0x7ff8000000000000

dst:
    .space 128
