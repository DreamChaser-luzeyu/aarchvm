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
    lsl  x23, x19, #17              // 0x20000 per cpu
    add  x22, x22, x23

    adr  x24, vectors
    msr  vbar_el1, x24
    movz x0, #1
    msr  ICC_SRE_EL1, x0
    movz x0, #0xff
    msr  ICC_PMR_EL1, x0
    movz x0, #1
    msr  ICC_IGRPEN1_EL1, x0

    movz w0, #0x2                   // enable SGI1
    add  x25, x22, #0x10, lsl #12   // GICR_SGI base
    str  w0, [x25, #0x100]

    cbz  x19, primary

    movz x1, #1
    add  x3, x21, #8
    stlr x1, [x3]                   // secondary_ready
    sev
    wfi
wait_done:
    add  x3, x21, #16
    ldar x2, [x3]
    cbnz x2, secondary_exit
    wfi
    b    wait_done

primary:
wait_secondary:
    add  x3, x21, #8
    ldar x1, [x3]
    cbnz x1, send_sgi
    wfe
    b    wait_secondary

send_sgi:
    movz x0, #0x0002                // target list bit1
    movk x0, #0x0100, lsl #16       // SGI id 1 in bits[27:24]
    msr  ICC_SGI1R_EL1, x0

wait_irq:
    ldar x3, [x21]
    cbnz x3, success
    wfe
    b    wait_irq

success:
    movz x4, #1
    add  x3, x21, #16
    stlr x4, [x3]
    sev
    movz x0, #'G'
    str  x0, [x20]
    movz x0, #'\n'
    str  x0, [x20]
    brk  #0

secondary_exit:
    brk  #0

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
    stlr x1, [x21]
    movz x2, #1
    msr  ICC_EOIR1_EL1, x2
    sev
    eret
irq_primary:
    movz x2, #1
    msr  ICC_EOIR1_EL1, x2
    eret

    .data
    .balign 8
shared:
    .quad 0                          // irq_seen
    .quad 0                          // secondary_ready
    .quad 0                          // done
