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

    // Enable local PPI 27 in the redistributor.
    movz x15, #0x0000
    movk x15, #0x080b, lsl #16     // GICR_SGI base + ISENABLER0
    movz x16, #0
    movk x16, #0x0800, lsl #16
    str  w16, [x15, #0x100]

    movz x2, #0xff
    msr  icc_pmr_el1, x2
    movz x2, #1
    msr  icc_sre_el1, x2
    msr  icc_igrpen1_el1, x2

    // Program a near timer expiry.
    mrs  x4, cntvct_el0
    add  x4, x4, #40
    msr  cntv_cval_el0, x4
    movz x5, #1
    msr  cntv_ctl_el0, x5

wait_irq:
    ldr  x6, [x11]
    cbz  x6, wait_irq

    // Spin for a while. If EOIR observes a stale high timer line, the IRQ will
    // spuriously retrigger and irq_count will become > 1 before the re-armed
    // compare value is actually due.
    movz x7, #0x800
post_irq_spin:
    ldr  x6, [x11]
    movz x8, #1
    subs xzr, x6, x8
    b.ne fail
    subs x7, x7, #1
    b.ne post_irq_spin

    movz x0, #'S'
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
    movz x9, #27
    subs xzr, x8, x9
    b.ne fail_from_handler

    adr  x11, irq_count
    ldr  x12, [x11]
    add  x12, x12, #1
    str  x12, [x11]
    movz x13, #1
    subs xzr, x12, x13
    b.ne fail_from_handler

    // Rearm the timer well into the future before ending the interrupt.
    mrs  x14, cntvct_el0
    movz x15, #0x4, lsl #16
    add  x14, x14, x15
    msr  cntv_cval_el0, x14
    movz x15, #1
    msr  cntv_ctl_el0, x15

    msr  icc_eoir1_el1, x8
    msr  icc_dir_el1, x8

    // Temporarily unmask IRQs before returning. If the timer PPI level is not
    // updated immediately, a stale level will cause an incorrect nested entry.
    msr  daifclr, #2
    movz x16, #0x40
handler_spin:
    subs x16, x16, #1
    b.ne handler_spin
    eret

fail_from_handler:
    adr  x8, fail
    msr  elr_el1, x8
    eret

    .data
    .balign 8
irq_count:
    .quad 0
