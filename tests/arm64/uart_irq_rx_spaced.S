    .text
    .global _start
_start:
    movz x20, #0x0000
    movk x20, #0x0900, lsl #16      // UART
    movz x21, #0x0000
    movk x21, #0x0800, lsl #16      // GICD base

    adr  x22, vectors
    msr  vbar_el1, x22

    adr  x23, irq_count
    movz x24, #0
    str  x24, [x23]

    // Minimal GICv3 CPU interface enable.
    movz x0, #0x80
    msr  icc_pmr_el1, x0
    movz x0, #1
    msr  icc_sre_el1, x0
    msr  icc_igrpen1_el1, x0

    // Enable distributor and SPI 33, set priority to 0x40.
    movz w0, #1
    str  w0, [x21]
    movz w0, #2
    str  w0, [x21, #0x104]
    movz w0, #0x40
    strb w0, [x21, #0x421]

    // Enable UART RX and receive-timeout interrupts.
    movz w0, #0x50
    str  w0, [x20, #0x38]

wait_irq:
    ldr  x0, [x23]
    cmp  x0, #3
    b.eq done
    wfi
    b wait_irq

done:
    movz x0, #'P'
    str  x0, [x20]
    movz x0, #'\n'
    str  x0, [x20]
    brk  #0

fail:
    movz x0, #'F'
    str  x0, [x20]
    brk  #0

    .balign 2048
vectors:
    .space 0x280
irq_spx:
    b irq_handler

irq_handler:
    mrs  x0, icc_iar1_el1
    movz x1, #33
    cmp  x0, x1
    b.ne fail_from_handler

    ldrb w2, [x20]
    str  w2, [x20]

    adr  x3, irq_count
    ldr  x4, [x3]
    add  x4, x4, #1
    str  x4, [x3]

    msr  icc_eoir1_el1, x0
    msr  icc_dir_el1, x0
    eret

fail_from_handler:
    adr  x3, irq_count
    movz x4, #0
    str  x4, [x3]
    adr  x5, fail
    msr  elr_el1, x5
    eret

    .data
    .balign 8
irq_count:
    .quad 0
