    .text
    .global _start
_start:
    movz x1, #0x0000
    movk x1, #0x0900, lsl #16      // UART

    adr  x10, vectors
    msr  vbar_el1, x10

    // Enable virtual timer PPI 27 in the redistributor.
    movz x15, #0x0000
    movk x15, #0x080b, lsl #16
    movz x16, #0
    movk x16, #0x0800, lsl #16
    str  w16, [x15, #0x100]

    movz x2, #0xff
    msr  icc_pmr_el1, x2
    movz x2, #1
    msr  icc_sre_el1, x2
    msr  icc_igrpen1_el1, x2

    // Keep IRQ delivery masked so the test exercises the CPU-interface sysregs
    // outside the normal IRQ exception entry path.
    msr  daifset, #0x2

    mrs  x4, cntvct_el0
    msr  cntv_cval_el0, x4
    movz x5, #1
    msr  cntv_ctl_el0, x5

    mrs  x6, icc_hppir1_el1
    movz x7, #27
    cmp  x6, x7
    b.ne fail_h

    mrs  x8, icc_iar1_el1
    cmp  x8, x7
    b.ne fail_i

    mrs  x9, icc_hppir1_el1
    movz x10, #1023
    cmp  x9, x10
    b.ne fail_p

    mrs  x11, icc_rpr_el1
    movz x12, #0xc0
    cmp  x11, x12
    b.ne fail_r

    // Remove the level source before ending the interrupt.
    msr  cntv_ctl_el0, xzr
    msr  icc_eoir1_el1, x8

    mrs  x11, icc_rpr_el1
    movz x12, #0xff
    cmp  x11, x12
    b.ne fail_e

    mrs  x9, icc_hppir1_el1
    movz x10, #1023
    cmp  x9, x10
    b.ne fail_p

    msr  daifclr, #0x2
    movz x13, #0x200
spin:
    subs x13, x13, #1
    b.ne spin

    movz x0, #'M'
    str  x0, [x1]
    movz x0, #'\n'
    str  x0, [x1]
    brk  #0

fail_h:
    movz x0, #'H'
    b out
fail_i:
    movz x0, #'I'
    b out
fail_p:
    movz x0, #'P'
    b out
fail_r:
    movz x0, #'R'
    b out
fail_e:
    movz x0, #'E'
    b out
fail_q:
    movz x0, #'Q'

out:
    str  x0, [x1]
    brk  #0

    .balign 2048
vectors:
    .space 0x280
irq_spx:
    b fail_q
