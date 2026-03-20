    .text
    .global _start
_start:
    movz x20, #0
    movk x20, #0x0900, lsl #16
    adr  x21, dst
    movz x25, #0x0030, lsl #16
    msr  cpacr_el1, x25
    isb

    mov  x24, #(1 << 25)          // FPCR.DN
    msr  fpcr, x24
    isb

    adr  x26, data32

    movz x24, #'1'
    msr  fpsr, xzr
    ldr  w0, [x26, #0]            // qNaN
    ldr  w2, [x26, #8]            // +1.0
    fmov s1, w0
    fmov s2, w2
    fmax s0, s1, s2
    str  s0, [x21, #0]
    add  x0, x21, #0
    adr  x1, exp_defnan_s
    bl   check4
    mrs  x2, fpsr
    cbnz x2, fail1

    movz x24, #'2'
    msr  fpsr, xzr
    ldr  w0, [x26, #0]            // qNaN
    ldr  w2, [x26, #8]            // +1.0
    fmov s1, w0
    fmov s2, w2
    fminnm s0, s1, s2
    str  s0, [x21, #4]
    add  x0, x21, #4
    adr  x1, one_s
    bl   check4
    mrs  x2, fpsr
    cbnz x2, fail2

    movz x24, #'3'
    msr  fpsr, xzr
    ldr  w0, [x26, #0]            // qNaN
    fmov s1, w0
    fmov s2, w0
    fmaxnm s0, s1, s2
    str  s0, [x21, #8]
    add  x0, x21, #8
    adr  x1, exp_defnan_s
    bl   check4
    mrs  x2, fpsr
    cbnz x2, fail3

    movz x24, #'4'
    msr  fpsr, xzr
    ldr  w1, [x26, #4]            // sNaN
    ldr  w2, [x26, #8]            // +1.0
    fmov s1, w1
    fmov s2, w2
    fmin s0, s1, s2
    str  s0, [x21, #12]
    add  x0, x21, #12
    adr  x1, exp_defnan_s
    bl   check4
    mrs  x2, fpsr
    cmp  x2, #1
    b.ne fail4

    movz x24, #'5'
    msr  fpsr, xzr
    adr  x22, src_maxp_s
    ldr  d5, [x22]
    fmaxp s6, v5.2s
    str  s6, [x21, #16]
    add  x0, x21, #16
    adr  x1, exp_defnan_s
    bl   check4
    mrs  x2, fpsr
    cbnz x2, fail5

    movz x24, #'6'
    msr  fpsr, xzr
    adr  x22, src_maxnmp_s
    ldr  d7, [x22]
    fmaxnmp s8, v7.2s
    str  s8, [x21, #20]
    add  x0, x21, #20
    adr  x1, exp_defnan_s
    bl   check4
    mrs  x2, fpsr
    cbnz x2, fail6

    movz x24, #'7'
    msr  fpsr, xzr
    adr  x22, src_vec_maxnm_a
    ldr  q9, [x22]
    adr  x22, src_vec_maxnm_b
    ldr  q10, [x22]
    fmaxnm v11.4s, v9.4s, v10.4s
    str  q11, [x21, #32]
    add  x0, x21, #32
    adr  x1, exp_vec_maxnm
    bl   check16
    mrs  x2, fpsr
    cmp  x2, #1
    b.ne fail7

    movz x24, #'8'
    msr  fpsr, xzr
    adr  x22, src_reduce_max
    ldr  q12, [x22]
    fmaxv s13, v12.4s
    str  s13, [x21, #48]
    add  x0, x21, #48
    adr  x1, exp_defnan_s
    bl   check4
    mrs  x2, fpsr
    cbnz x2, fail8

    movz x24, #'9'
    msr  fpsr, xzr
    adr  x22, src_reduce_maxnm
    ldr  q14, [x22]
    fmaxnmv s15, v14.4s
    str  s15, [x21, #52]
    add  x0, x21, #52
    adr  x1, exp_defnan_s
    bl   check4
    mrs  x2, fpsr
    cbnz x2, fail9

    movz x0, #'X'
    str  x0, [x20]
    brk  #0

check4:
    ldr  w2, [x0]
    ldr  w3, [x1]
    cmp  w2, w3
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

fail1:
    movz x0, #'1'
    b out
fail2:
    movz x0, #'2'
    b out
fail3:
    movz x0, #'3'
    b out
fail4:
    movz x0, #'4'
    b out
fail5:
    movz x0, #'5'
    b out
fail6:
    movz x0, #'6'
    b out
fail7:
    movz x0, #'7'
    b out
fail8:
    movz x0, #'8'
    b out
fail9:
    movz x0, #'9'
    b out
fail:
    mov  x0, x24
out:
    str  x0, [x20]
    brk  #0

    .data
    .balign 16
data32:
    .word 0x7fc12345
    .word 0x7f812345
    .word 0x3f800000
    .word 0x40000000
    .word 0x40400000

one_s:
    .word 0x3f800000
exp_defnan_s:
    .word 0x7fc00000

src_maxp_s:
    .word 0x7fc12345, 0x3f800000

src_maxnmp_s:
    .word 0x7fc12345, 0x7fc54321

src_vec_maxnm_a:
    .word 0x7fc12345, 0x7fc11111, 0x7f812345, 0x3f800000
src_vec_maxnm_b:
    .word 0x3f800000, 0x7fc22222, 0x40000000, 0x40000000
exp_vec_maxnm:
    .word 0x3f800000, 0x7fc00000, 0x7fc00000, 0x40000000

src_reduce_max:
    .word 0x3f800000, 0x7fc12345, 0x40000000, 0x40400000

src_reduce_maxnm:
    .word 0x7fc12345, 0x7fc54321, 0x7fc11111, 0x7fc22222

dst:
    .space 64
