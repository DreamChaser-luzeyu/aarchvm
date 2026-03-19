    .text
    .global _start
_start:
    movz x1, #0x0000
    movk x1, #0x0900, lsl #16      // UART

    adr  x10, vectors
    msr  vbar_el1, x10

    adr  x3, phase
    movz x4, #0
    str  x4, [x3]

    // Enable virtual timer PPI and CPU interface explicitly.
    movz x15, #0x0000
    movk x15, #0x080b, lsl #16     // GICR SGI base
    movz x16, #0
    movk x16, #0x0800, lsl #16     // bit 27
    str  w16, [x15, #0x100]

    movz x16, #0x00ff
    msr  icc_pmr_el1, x16
    movz x16, #1
    msr  icc_sre_el1, x16
    msr  icc_igrpen1_el1, x16

    // Test SP0 vector path: IRQ should enter VBAR+0x80.
    msr  spsel, #0
    mrs  x5, cntvct_el0
    add  x5, x5, #30
    msr  cntv_cval_el0, x5
    movz x6, #1
    msr  cntv_ctl_el0, x6

    movz x7, #140
wait0:
    subs x7, x7, #1
    b.ne wait0

    ldr  x8, [x3]
    subs x9, x8, #1
    b.ne fail

    // Test SPx vector path: IRQ should enter VBAR+0x280.
    msr  spsel, #1
    mrs  x5, cntvct_el0
    add  x5, x5, #30
    msr  cntv_cval_el0, x5
    movz x6, #1
    msr  cntv_ctl_el0, x6

    movz x7, #140
wait1:
    subs x7, x7, #1
    b.ne wait1

    ldr  x8, [x3]
    subs x9, x8, #2
    b.ne fail

    movz x0, #'P'
    str  x0, [x1]
    movz x0, #'\n'
    str  x0, [x1]
    brk  #0

fail:
    movz x0, #'F'
    str  x0, [x1]
    brk  #0

    .balign 2048
vectors:
    .space 0x80
irq_sp0_vector:
    b irq_sp0
    .space 0x1FC
irq_spx_vector:
    b irq_spx

irq_sp0:
    mrs  x13, icc_iar1_el1
    movz x14, #27
    cmp  x13, x14
    b.ne fail
    msr  cntv_ctl_el0, xzr
    msr  icc_eoir1_el1, x13
    msr  icc_dir_el1, x13
    adr  x11, phase
    movz x12, #1
    str  x12, [x11]
    eret

irq_spx:
    mrs  x13, icc_iar1_el1
    movz x14, #27
    cmp  x13, x14
    b.ne fail
    msr  cntv_ctl_el0, xzr
    msr  icc_eoir1_el1, x13
    msr  icc_dir_el1, x13
    adr  x11, phase
    movz x12, #2
    str  x12, [x11]
    eret

    .data
    .balign 8
phase:
    .quad 0
