    .text
    .global _start
_start:
    movz x20, #0x0000
    movk x20, #0x0900, lsl #16      // UART
    adr  x21, shared

    mrs  x0, mpidr_el1
    and  x19, x0, #0xff

    movz x22, #0x0000
    movk x22, #0x080a, lsl #16      // GICR cpu0 base
    lsl  x23, x19, #17
    add  x22, x22, x23

    adr  x24, vectors
    msr  vbar_el1, x24
    movz x0, #1
    msr  ICC_SRE_EL1, x0
    movz x0, #0xff
    msr  ICC_PMR_EL1, x0
    movz x0, #1
    msr  ICC_IGRPEN1_EL1, x0

    movz w0, #0x0800, lsl #16       // enable PPI 27
    add  x25, x22, #0x10, lsl #12   // GICR_SGI base
    str  w0, [x25, #0x100]

    movz x0, #40
    msr  CNTV_TVAL_EL0, x0
    movz x0, #1
    msr  CNTV_CTL_EL0, x0

wait_irq:
    cbz  x19, wait_primary
wait_secondary:
    add  x3, x21, #8
    ldar x1, [x3]
    cbnz x1, secondary_done
    wfi
    b    wait_secondary
secondary_done:
    brk  #0

wait_primary:
    ldar x1, [x21]
    cbz  x1, wait_irq_primary
    add  x3, x21, #8
    ldar x2, [x3]
    cbz  x2, wait_irq_primary
    movz x3, #'T'
    str  x3, [x20]
    movz x3, #'\n'
    str  x3, [x20]
    brk  #0
wait_irq_primary:
    wfi
    b    wait_primary

    .balign 2048
vectors:
    .space 0x280
irq_vector:
    b irq_handler

irq_handler:
    mrs  x0, mpidr_el1
    and  x0, x0, #0xff
    cbz  x0, irq_primary
    movz x1, #1
    add  x3, x21, #8
    stlr x1, [x3]
    movz x2, #0
    msr  CNTV_CTL_EL0, x2
    movz x2, #27
    msr  ICC_EOIR1_EL1, x2
    sev
    eret
irq_primary:
    movz x1, #1
    stlr x1, [x21]
    movz x2, #0
    msr  CNTV_CTL_EL0, x2
    movz x2, #27
    msr  ICC_EOIR1_EL1, x2
    sev
    eret

    .data
    .balign 8
shared:
    .quad 0
    .quad 0
