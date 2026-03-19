    .text
    .global _start
_start:
    movz x20, #0
    movk x20, #0x0900, lsl #16
    adr  x21, dst
    movz x25, #0x0030, lsl #16
    msr  cpacr_el1, x25
    isb

    msr  fpcr, xzr

    adr  x22, src_frecpe_s
    ldr  s1, [x22]
    frecpe s0, s1
    str  s0, [x21]
    movz x23, #'1'
    add  x0, x21, #0
    adr  x1, exp_frecpe_s
    bl   check4

    adr  x22, src_frecpe_d
    ldr  d3, [x22]
    frecpe d2, d3
    str  d2, [x21, #8]
    movz x23, #'2'
    add  x0, x21, #8
    adr  x1, exp_frecpe_d
    bl   check8

    msr  fpsr, xzr
    adr  x22, src_frecpe_snan
    ldr  s5, [x22]
    frecpe s4, s5
    str  s4, [x21, #16]
    movz x23, #'3'
    add  x0, x21, #16
    adr  x1, exp_frecpe_snan
    bl   check4
    mrs  x2, fpsr
    cmp  x2, #1
    b.ne fail3

    msr  fpsr, xzr
    adr  x22, src_frecpe_tiny
    ldr  s7, [x22]
    frecpe s6, s7
    str  s6, [x21, #20]
    movz x23, #'4'
    add  x0, x21, #20
    adr  x1, exp_frecpe_tiny
    bl   check4
    mrs  x2, fpsr
    mov  x3, #(1 << 4)
    cmp  x2, x3
    b.ne fail4

    adr  x22, src_frsqrte_s
    ldr  s9, [x22]
    frsqrte s8, s9
    str  s8, [x21, #24]
    movz x23, #'5'
    add  x0, x21, #24
    adr  x1, exp_frsqrte_s
    bl   check4

    msr  fpsr, xzr
    adr  x22, src_frsqrte_neg_d
    ldr  d11, [x22]
    frsqrte d10, d11
    str  d10, [x21, #32]
    movz x23, #'6'
    add  x0, x21, #32
    adr  x1, exp_frsqrte_neg_d
    bl   check8
    mrs  x2, fpsr
    cmp  x2, #1
    b.ne fail6

    adr  x22, src_vec_frecpe_s
    ldr  q13, [x22]
    frecpe v12.4s, v13.4s
    str  q12, [x21, #48]
    movz x23, #'7'
    add  x0, x21, #48
    adr  x1, exp_vec_frecpe_s
    bl   check16

    msr  fpsr, xzr
    adr  x22, src_vec_frecpe_d
    ldr  q15, [x22]
    frecpe v14.2d, v15.2d
    str  q14, [x21, #64]
    movz x23, #'8'
    add  x0, x21, #64
    adr  x1, exp_vec_frecpe_d
    bl   check16
    mrs  x2, fpsr
    cmp  x2, #1
    b.ne fail8

    adr  x22, src_vec_frsqrte_s
    ldr  q17, [x22]
    frsqrte v16.4s, v17.4s
    str  q16, [x21, #80]
    movz x23, #'9'
    add  x0, x21, #80
    adr  x1, exp_vec_frsqrte_s
    bl   check16

    msr  fpsr, xzr
    adr  x22, src_vec_frsqrte_d
    ldr  q19, [x22]
    frsqrte v18.2d, v19.2d
    str  q18, [x21, #96]
    movz x23, #'A'
    add  x0, x21, #96
    adr  x1, exp_vec_frsqrte_d
    bl   check16
    mrs  x2, fpsr
    cmp  x2, #1
    b.ne failA

    movz x0, #'E'
    str  x0, [x20]
    brk  #0

check4:
    ldr  w2, [x0]
    ldr  w3, [x1]
    cmp  w2, w3
    b.ne fail_generic
    ret

check8:
    ldr  x2, [x0]
    ldr  x3, [x1]
    cmp  x2, x3
    b.ne fail_generic
    ret

check16:
    ldr  x2, [x0]
    ldr  x3, [x1]
    cmp  x2, x3
    b.ne fail_generic
    ldr  x2, [x0, #8]
    ldr  x3, [x1, #8]
    cmp  x2, x3
    b.ne fail_generic
    ret

fail_generic:
    mov  x0, x23
    b done
fail3:
    movz x0, #'3'
    b done
fail4:
    movz x0, #'4'
    b done
fail6:
    movz x0, #'6'
    b done
fail8:
    movz x0, #'8'
    b done
failA:
    movz x0, #'A'
done:
    str  x0, [x20]
    brk  #0

    .data
    .balign 16
src_frecpe_s:
    .word 0x3f800000
exp_frecpe_s:
    .word 0x3f7f8000

src_frecpe_d:
    .quad 0x4000000000000000
exp_frecpe_d:
    .quad 0x3fdff00000000000

src_frecpe_snan:
    .word 0x7f812345
exp_frecpe_snan:
    .word 0x7fc12345

src_frecpe_tiny:
    .word 0x00010000
exp_frecpe_tiny:
    .word 0x7f800000

src_frsqrte_s:
    .word 0x40800000
exp_frsqrte_s:
    .word 0x3f348000

src_frsqrte_neg_d:
    .quad 0xbff0000000000000
exp_frsqrte_neg_d:
    .quad 0x7ff8000000000000

    .balign 16
src_vec_frecpe_s:
    .word 0x3f800000, 0x40000000, 0x7f800000, 0xbf800000
exp_vec_frecpe_s:
    .word 0x3f7f8000, 0x3eff8000, 0x00000000, 0xbf7f8000

src_vec_frecpe_d:
    .quad 0x3ff0000000000000, 0x7ff0123456789abc
exp_vec_frecpe_d:
    .quad 0x3feff00000000000, 0x7ff8123456789abc

src_vec_frsqrte_s:
    .word 0x3f800000, 0x40800000, 0x7f800000, 0x00000000
exp_vec_frsqrte_s:
    .word 0x3f7f8000, 0x3f348000, 0x00000000, 0x7f800000

src_vec_frsqrte_d:
    .quad 0x4000000000000000, 0xbff0000000000000
exp_vec_frsqrte_d:
    .quad 0x3fe6900000000000, 0x7ff8000000000000

dst:
    .space 112
