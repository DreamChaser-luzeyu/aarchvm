    .text
    .global _start
_start:
    movz x20, #0
    movk x20, #0x0900, lsl #16
    adr  x21, dst
    movz x25, #0x0030, lsl #16
    msr  cpacr_el1, x25
    isb

    msr  fpsr, xzr
    adr  x22, src_one_s
    ldr  s1, [x22]
    frecpx s0, s1
    str  s0, [x21]
    movz x23, #'1'
    add  x0, x21, #0
    adr  x1, exp_one_s
    bl   check4

    adr  x22, src_two_s
    ldr  s3, [x22]
    frecpx s2, s3
    str  s2, [x21, #4]
    movz x23, #'2'
    add  x0, x21, #4
    adr  x1, exp_two_s
    bl   check4

    adr  x22, src_zero_s
    ldr  s5, [x22]
    frecpx s4, s5
    str  s4, [x21, #8]
    movz x23, #'3'
    add  x0, x21, #8
    adr  x1, exp_zero_s
    bl   check4

    adr  x22, src_sub_s
    ldr  s7, [x22]
    frecpx s6, s7
    str  s6, [x21, #12]
    movz x23, #'4'
    add  x0, x21, #12
    adr  x1, exp_sub_s
    bl   check4

    adr  x22, src_inf_s
    ldr  s9, [x22]
    frecpx s8, s9
    str  s8, [x21, #16]
    movz x23, #'5'
    add  x0, x21, #16
    adr  x1, exp_inf_s
    bl   check4

    msr  fpsr, xzr
    adr  x22, src_snan_s
    ldr  s11, [x22]
    frecpx s10, s11
    str  s10, [x21, #20]
    movz x23, #'6'
    add  x0, x21, #20
    adr  x1, exp_snan_s
    bl   check4
    mrs  x2, fpsr
    cmp  x2, #1
    b.ne fail6

    adr  x22, src_neg_s
    ldr  s13, [x22]
    frecpx s12, s13
    str  s12, [x21, #24]
    movz x23, #'7'
    add  x0, x21, #24
    adr  x1, exp_neg_s
    bl   check4

    adr  x22, src_one_d
    ldr  d15, [x22]
    frecpx d14, d15
    str  d14, [x21, #32]
    movz x23, #'8'
    add  x0, x21, #32
    adr  x1, exp_one_d
    bl   check8

    adr  x22, src_two_d
    ldr  d17, [x22]
    frecpx d16, d17
    str  d16, [x21, #40]
    movz x23, #'9'
    add  x0, x21, #40
    adr  x1, exp_two_d
    bl   check8

    adr  x22, src_zero_d
    ldr  d19, [x22]
    frecpx d18, d19
    str  d18, [x21, #48]
    movz x23, #'A'
    add  x0, x21, #48
    adr  x1, exp_zero_d
    bl   check8

    adr  x22, src_sub_d
    ldr  d21, [x22]
    frecpx d20, d21
    str  d20, [x21, #56]
    movz x23, #'B'
    add  x0, x21, #56
    adr  x1, exp_sub_d
    bl   check8

    adr  x22, src_inf_d
    ldr  d23, [x22]
    frecpx d22, d23
    str  d22, [x21, #64]
    movz x23, #'C'
    add  x0, x21, #64
    adr  x1, exp_inf_d
    bl   check8

    msr  fpsr, xzr
    adr  x22, src_snan_d
    ldr  d25, [x22]
    frecpx d24, d25
    str  d24, [x21, #72]
    movz x23, #'D'
    add  x0, x21, #72
    adr  x1, exp_snan_d
    bl   check8
    mrs  x2, fpsr
    cmp  x2, #1
    b.ne failD

    adr  x22, src_neg_d
    ldr  d27, [x22]
    frecpx d26, d27
    str  d26, [x21, #80]
    movz x23, #'E'
    add  x0, x21, #80
    adr  x1, exp_neg_d
    bl   check8

    movz x0, #'X'
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

fail_generic:
    mov  x0, x23
    b done
fail6:
    movz x0, #'6'
    b done
failD:
    movz x0, #'D'
done:
    str  x0, [x20]
    brk  #0

    .data
src_one_s:   .word 0x3f800000
exp_one_s:   .word 0x40000000
src_two_s:   .word 0x40000000
exp_two_s:   .word 0x3f800000
src_zero_s:  .word 0x00000000
exp_zero_s:  .word 0x7f000000
src_sub_s:   .word 0x00010000
exp_sub_s:   .word 0x7f000000
src_inf_s:   .word 0x7f800000
exp_inf_s:   .word 0x00000000
src_snan_s:  .word 0x7f812345
exp_snan_s:  .word 0x7fc12345
src_neg_s:   .word 0xbf800000
exp_neg_s:   .word 0xc0000000

src_one_d:   .quad 0x3ff0000000000000
exp_one_d:   .quad 0x4000000000000000
src_two_d:   .quad 0x4000000000000000
exp_two_d:   .quad 0x3ff0000000000000
src_zero_d:  .quad 0x0000000000000000
exp_zero_d:  .quad 0x7fe0000000000000
src_sub_d:   .quad 0x0004000000000000
exp_sub_d:   .quad 0x7fe0000000000000
src_inf_d:   .quad 0x7ff0000000000000
exp_inf_d:   .quad 0x0000000000000000
src_snan_d:  .quad 0x7ff0123456789abc
exp_snan_d:  .quad 0x7ff8123456789abc
src_neg_d:   .quad 0xbff0000000000000
exp_neg_d:   .quad 0xc000000000000000

    .balign 16
dst:
    .space 88
