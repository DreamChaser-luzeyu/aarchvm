    .text
    .global _start
_start:
    movz x20, #0x0000
    movk x20, #0x0900, lsl #16      // UART base
    adr  x21, shared

    mrs  x0, mpidr_el1
    and  x19, x0, #0xff
    cbnz x19, secondary_core

primary_core:
    movz x22, #0x0000
    movk x22, #0x080a, lsl #16      // GICR cpu0 base
    adr  x24, vectors
    msr  vbar_el1, x24
    movz x0, #1
    msr  ICC_SRE_EL1, x0
    movz x0, #0xff
    msr  ICC_PMR_EL1, x0
    movz x0, #1
    msr  ICC_IGRPEN1_EL1, x0

    movz w0, #0x0800, lsl #16       // Enable PPI 27.
    add  x25, x22, #0x10, lsl #12   // GICR_SGI base.
    str  w0, [x25, #0x100]

    mrs  x26, CNTVCT_EL0
    movz x0, #50
    msr  CNTV_TVAL_EL0, x0
    movz x0, #1
    msr  CNTV_CTL_EL0, x0

wait_irq:
    ldar x0, [x21]
    cbnz x0, done
    wfi
    b    wait_irq

done:
    brk  #0

secondary_core:
spin:
    ldar x0, [x21]
    cbnz x0, secondary_done
    add  x5, x5, #1
    b    spin
secondary_done:
    brk  #0

    .balign 2048
vectors:
    .space 0x280
irq_vector:
    b irq_handler

irq_handler:
    mrs  x3, CNTVCT_EL0
    sub  x3, x3, x26
    movz x1, #'F'
    cmp  x3, #45
    b.lo 1f
    cmp  x3, #80
    b.hi 1f
    movz x1, #'R'
1:
    str  x1, [x20]
    movz x1, #'\n'
    str  x1, [x20]
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
