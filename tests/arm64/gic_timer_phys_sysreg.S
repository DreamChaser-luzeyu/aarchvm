    .text
    .global _start
_start:
    movz x1, #0x0000
    movk x1, #0x0900, lsl #16      // UART

    adr  x10, vectors
    msr  vbar_el1, x10

    adr  x11, irq_count
    movz x12, #0
    str  x12, [x11]

    // Enable the relevant PPI in the redistributor.
    movz x15, #0x0000
    movk x15, #0x080b, lsl #16     // GICR_SGI base + ISENABLER0
    movz x16, #0
    movk x16, #0x4000, lsl #16
    str  w16, [x15, #0x100]
    movz w16, #0x40
    strb w16, [x15, #0x41e]

    movz x2, #0x80
    msr  icc_pmr_el1, x2
    movz x2, #1
    msr  icc_sre_el1, x2
    movz x2, #1
    msr  icc_igrpen1_el1, x2

    // Program physical timer via sysregs: fire after ~40 ticks.
    mrs  x4, cntpct_el0
    add  x4, x4, #40
    msr  cntp_cval_el0, x4
    movz x5, #1
    msr  cntp_ctl_el0, x5          // ENABLE=1, IMASK=0

wait_irq:
    ldr  x6, [x11]
    cbz  x6, wait_irq

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
    .space 0x280
irq_spx:
    b irq_handler

irq_handler:
    mrs  x8, icc_iar1_el1
    movz x9, #30
    subs xzr, x8, x9
    b.ne fail_from_handler

    msr  icc_eoir1_el1, x8
    msr  icc_dir_el1, x8
    movz x5, #0
    msr  cntp_ctl_el0, x5

    adr  x11, irq_count
    movz x12, #1
    str  x12, [x11]
    eret

fail_from_handler:
    adr  x11, irq_count
    movz x12, #0
    str  x12, [x11]
    adr  x8, fail
    msr  elr_el1, x8
    eret

    .data
    .balign 8
irq_count:
    .quad 0
