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

    msr  fpsr, xzr
    adr  x22, src_norm
    ldr  q1, [x22]
    fmaxv s0, v1.4s
    str  s0, [x21]
    ldr  w2, [x21]
    ldr  w3, =0x40500000
    cmp  w2, w3
    b.ne fail1
    mrs  x2, fpsr
    cbnz x2, fail1

    msr  fpsr, xzr
    fminv s2, v1.4s
    str  s2, [x21, #4]
    ldr  w2, [x21, #4]
    ldr  w3, =0xc0800000
    cmp  w2, w3
    b.ne fail2
    mrs  x2, fpsr
    cbnz x2, fail2

    msr  fpsr, xzr
    adr  x22, src_zeros
    ldr  q3, [x22]
    fmaxv s4, v3.4s
    str  s4, [x21, #8]
    ldr  w2, [x21, #8]
    cbnz w2, fail3
    fminv s5, v3.4s
    str  s5, [x21, #12]
    ldr  w2, [x21, #12]
    ldr  w3, =0x80000000
    cmp  w2, w3
    b.ne fail3
    mrs  x2, fpsr
    cbnz x2, fail3

    msr  fpsr, xzr
    adr  x22, src_qnan_numeric
    ldr  q6, [x22]
    fmaxnmv s7, v6.4s
    str  s7, [x21, #16]
    ldr  w2, [x21, #16]
    ldr  w3, =0x40e00000
    cmp  w2, w3
    b.ne fail4
    mrs  x2, fpsr
    cbnz x2, fail4

    msr  fpsr, xzr
    fminnmv s8, v6.4s
    str  s8, [x21, #20]
    ldr  w2, [x21, #20]
    ldr  w3, =0xc0000000
    cmp  w2, w3
    b.ne fail5
    mrs  x2, fpsr
    cbnz x2, fail5

    msr  fpsr, xzr
    adr  x22, src_qnan_prop
    ldr  q9, [x22]
    fmaxv s10, v9.4s
    str  s10, [x21, #24]
    ldr  w2, [x21, #24]
    ldr  w3, =0x7fc12345
    cmp  w2, w3
    b.ne fail6
    mrs  x2, fpsr
    cbnz x2, fail6

    msr  fpsr, xzr
    adr  x22, src_snan
    ldr  q11, [x22]
    fminnmv s12, v11.4s
    str  s12, [x21, #28]
    ldr  w2, [x21, #28]
    ldr  w3, =0x40200000
    cmp  w2, w3
    b.ne fail7
    mrs  x2, fpsr
    cmp  x2, #1
    b.ne fail7

    mov  x24, #(1 << 25)
    msr  fpcr, x24
    isb

    msr  fpsr, xzr
    adr  x22, src_dn_nan_mixed
    ldr  q13, [x22]
    fmaxv s14, v13.4s
    str  s14, [x21, #32]
    ldr  w2, [x21, #32]
    ldr  w3, =0x7fc00000
    cmp  w2, w3
    b.ne fail8
    mrs  x2, fpsr
    cmp  x2, #1
    b.ne fail8

    msr  fpsr, xzr
    fmaxnmv s15, v13.4s
    str  s15, [x21, #36]
    ldr  w2, [x21, #36]
    ldr  w3, =0x00000001
    cmp  w2, w3
    b.ne fail9
    mrs  x2, fpsr
    cmp  x2, #1
    b.ne fail9

    mov  x24, #(1 << 24)
    msr  fpcr, x24
    isb

    msr  fpsr, xzr
    adr  x22, src_fz_clean
    ldr  q16, [x22]
    fmaxnmv s17, v16.4s
    str  s17, [x21, #40]
    ldr  w2, [x21, #40]
    ldr  w3, =0x00000000
    cmp  w2, w3
    b.ne failA
    mrs  x2, fpsr
    cmp  x2, #0x80
    b.ne failA

    msr  fpsr, xzr
    fminnmv s18, v16.4s
    str  s18, [x21, #44]
    ldr  w2, [x21, #44]
    ldr  w3, =0x80000000
    cmp  w2, w3
    b.ne failB
    mrs  x2, fpsr
    cmp  x2, #0x80
    b.ne failB

    movz x0, #'R'
    str  x0, [x20]
    brk  #0

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
    b done
fail9:
    movz x0, #'9'
    b done
failA:
    movz x0, #'A'
    b done
failB:
    movz x0, #'B'
done:
    str  x0, [x20]
    brk  #0

    .data
    .balign 16
src_norm:
    .word 0x3fc00000, 0xc0800000, 0x40500000, 0x40000000
src_zeros:
    .word 0x00000000, 0x80000000, 0x00000000, 0x80000000
src_qnan_numeric:
    .word 0x7fc12345, 0xc0000000, 0x40e00000, 0x40200000
src_qnan_prop:
    .word 0x3f800000, 0x7fc12345, 0x40e00000, 0x40200000
src_snan:
    .word 0x3f800000, 0x7f812345, 0x40200000, 0x40800000
src_dn_nan_mixed:
    .word 0x7fc12345, 0x7f812345, 0x00000001, 0x80000001
src_fz_clean:
    .word 0x80000001, 0x00000001, 0x00000000, 0x80000000

dst:
    .space 48
