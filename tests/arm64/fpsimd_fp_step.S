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

    adr  x22, src_frecps_s
    ldr  s1, [x22]
    ldr  s2, [x22, #4]
    frecps s0, s1, s2
    str  s0, [x21]
    movz x23, #'1'
    add  x0, x21, #0
    adr  x1, exp_frecps_s
    bl   check4

    adr  x22, src_frecps_d
    ldr  d3, [x22]
    ldr  d4, [x22, #8]
    frecps d2, d3, d4
    str  d2, [x21, #8]
    movz x23, #'2'
    add  x0, x21, #8
    adr  x1, exp_frecps_d
    bl   check8

    msr  fpsr, xzr
    adr  x22, src_frecps_snan
    ldr  s5, [x22]
    ldr  s6, [x22, #4]
    frecps s4, s5, s6
    str  s4, [x21, #16]
    movz x23, #'3'
    add  x0, x21, #16
    adr  x1, exp_frecps_snan
    bl   check4
    mrs  x2, fpsr
    cmp  x2, #1
    b.ne fail3

    mov  x24, #(2 << 22)
    msr  fpcr, x24
    adr  x22, src_frecps_zero
    ldr  s8, [x22]
    ldr  s9, [x22, #4]
    frecps s7, s8, s9
    str  s7, [x21, #20]
    movz x23, #'4'
    add  x0, x21, #20
    adr  x1, exp_frecps_zero_rm
    bl   check4
    msr  fpcr, xzr

    adr  x22, src_frsqrts_s
    ldr  s11, [x22]
    ldr  s12, [x22, #4]
    frsqrts s10, s11, s12
    str  s10, [x21, #24]
    movz x23, #'5'
    add  x0, x21, #24
    adr  x1, exp_frsqrts_s
    bl   check4

    adr  x22, src_frsqrts_d
    ldr  d13, [x22]
    ldr  d14, [x22, #8]
    frsqrts d12, d13, d14
    str  d12, [x21, #32]
    movz x23, #'6'
    add  x0, x21, #32
    adr  x1, exp_frsqrts_d
    bl   check8

    msr  fpsr, xzr
    adr  x22, src_frsqrts_snan
    ldr  s16, [x22]
    ldr  s17, [x22, #4]
    frsqrts s15, s16, s17
    str  s15, [x21, #40]
    movz x23, #'7'
    add  x0, x21, #40
    adr  x1, exp_frsqrts_snan
    bl   check4
    mrs  x2, fpsr
    cmp  x2, #1
    b.ne fail7

    mov  x24, #(2 << 22)
    msr  fpcr, x24
    adr  x22, src_frsqrts_zero
    ldr  s19, [x22]
    ldr  s20, [x22, #4]
    frsqrts s18, s19, s20
    str  s18, [x21, #44]
    movz x23, #'8'
    add  x0, x21, #44
    adr  x1, exp_frsqrts_zero_rm
    bl   check4
    msr  fpcr, xzr

    adr  x22, src_vec_frecps_s
    ldr  q22, [x22]
    adr  x22, src_vec_frecps_s_rhs
    ldr  q23, [x22]
    frecps v21.4s, v22.4s, v23.4s
    str  q21, [x21, #48]
    movz x23, #'9'
    add  x0, x21, #48
    adr  x1, exp_vec_frecps_s
    bl   check16

    msr  fpsr, xzr
    adr  x22, src_vec_frecps_d
    ldr  q25, [x22]
    adr  x22, src_vec_frecps_d_rhs
    ldr  q26, [x22]
    frecps v24.2d, v25.2d, v26.2d
    str  q24, [x21, #64]
    movz x23, #'A'
    add  x0, x21, #64
    adr  x1, exp_vec_frecps_d
    bl   check16
    mrs  x2, fpsr
    cmp  x2, #1
    b.ne failA

    adr  x22, src_vec_frsqrts_s
    ldr  q28, [x22]
    adr  x22, src_vec_frsqrts_s_rhs
    ldr  q29, [x22]
    frsqrts v27.4s, v28.4s, v29.4s
    str  q27, [x21, #80]
    movz x23, #'B'
    add  x0, x21, #80
    adr  x1, exp_vec_frsqrts_s
    bl   check16

    msr  fpsr, xzr
    adr  x22, src_vec_frsqrts_d
    ldr  q31, [x22]
    adr  x22, src_vec_frsqrts_d_rhs
    ldr  q0, [x22]
    frsqrts v30.2d, v31.2d, v0.2d
    str  q30, [x21, #96]
    movz x23, #'C'
    add  x0, x21, #96
    adr  x1, exp_vec_frsqrts_d
    bl   check16
    mrs  x2, fpsr
    cmp  x2, #1
    b.ne failC

    movz x0, #'S'
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
fail7:
    movz x0, #'7'
    b done
failA:
    movz x0, #'A'
    b done
failC:
    movz x0, #'C'
done:
    str  x0, [x20]
    brk  #0

    .data
    .balign 16
src_frecps_s:
    .word 0x3f800000, 0x3f000000
exp_frecps_s:
    .word 0x3fc00000

src_frecps_d:
    .quad 0x3ff0000000000000, 0x3fe0000000000000
exp_frecps_d:
    .quad 0x3ff8000000000000

src_frecps_snan:
    .word 0x7f812345, 0x3f800000
exp_frecps_snan:
    .word 0xffc12345

src_frecps_zero:
    .word 0x40000000, 0x3f800000
exp_frecps_zero_rm:
    .word 0x80000000

src_frsqrts_s:
    .word 0x3f800000, 0x3f000000
exp_frsqrts_s:
    .word 0x3fa00000

src_frsqrts_d:
    .quad 0x3ff0000000000000, 0x3fe0000000000000
exp_frsqrts_d:
    .quad 0x3ff4000000000000

src_frsqrts_snan:
    .word 0x7f812345, 0x3f800000
exp_frsqrts_snan:
    .word 0xffc12345

src_frsqrts_zero:
    .word 0x40400000, 0x3f800000
exp_frsqrts_zero_rm:
    .word 0x80000000

    .balign 16
src_vec_frecps_s:
    .word 0x3f800000, 0x40000000, 0x7f800000, 0xbf800000
src_vec_frecps_s_rhs:
    .word 0x3f000000, 0x3f800000, 0x00000000, 0x40000000
exp_vec_frecps_s:
    .word 0x3fc00000, 0x00000000, 0x40000000, 0x40800000

src_vec_frecps_d:
    .quad 0x3ff0000000000000, 0x7ff0123456789abc
src_vec_frecps_d_rhs:
    .quad 0x3fe0000000000000, 0x3ff0000000000000
exp_vec_frecps_d:
    .quad 0x3ff8000000000000, 0xfff8123456789abc

src_vec_frsqrts_s:
    .word 0x3f800000, 0x40400000, 0x7f800000, 0xbf800000
src_vec_frsqrts_s_rhs:
    .word 0x3f000000, 0x3f800000, 0x00000000, 0x40000000
exp_vec_frsqrts_s:
    .word 0x3fa00000, 0x00000000, 0x3fc00000, 0x40200000

src_vec_frsqrts_d:
    .quad 0x3ff0000000000000, 0x7ff0123456789abc
src_vec_frsqrts_d_rhs:
    .quad 0x3fe0000000000000, 0x3ff0000000000000
exp_vec_frsqrts_d:
    .quad 0x3ff4000000000000, 0xfff8123456789abc

    .balign 16
dst:
    .space 112
