    .text
    .global _start
_start:
    movz x1, #0x0000
    movk x1, #0x0900, lsl #16      // UART

    mrs  x2, id_aa64pfr0_el1
    lsr  x2, x2, #24
    and  x2, x2, #0xf
    movz x3, #1
    subs xzr, x2, x3
    b.ne fail_g

    mrs  x4, icc_pmr_el1
    movz x5, #0xff
    subs xzr, x4, x5
    b.ne fail_p

    movz x6, #0x80
    msr  icc_pmr_el1, x6
    mrs  x7, icc_pmr_el1
    subs xzr, x7, x6
    b.ne fail_p

    movz x8, #1
    msr  icc_sre_el1, x8
    mrs  x9, icc_sre_el1
    subs xzr, x9, x8
    b.ne fail_s

    movz x0, #'J'
    str  x0, [x1]
    movz x0, #'\n'
    str  x0, [x1]
    brk  #0

fail_g:
    movz x0, #'G'
    b out
fail_p:
    movz x0, #'P'
    b out
fail_s:
    movz x0, #'S'
out:
    str  x0, [x1]
    brk  #0
