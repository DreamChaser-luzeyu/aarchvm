    .text
    .global _start
_start:
    movz x1, #0
    movk x1, #0x0900, lsl #16      // UART

    adr  x10, vectors
    msr  vbar_el1, x10

    adr  x11, handled
    movz x12, #0
    str  x12, [x11]

    movz x9, #3
    movz x20, #0x1000              // L0
    movz x21, #0x2000              // L1
    movz x22, #0x3000              // L2
    movz x23, #0x4000              // L3

    // VA 0x40000000 -> PA 0x500000
    orr  x2, x21, x9
    str  x2, [x20]
    orr  x2, x22, x9
    str  x2, [x21, #8]
    orr  x2, x23, x9
    str  x2, [x22]
    movz x3, #0x0050, lsl #16
    orr  x2, x3, x9
    str  x2, [x23]

    movz x4, #0x0050, lsl #16
    movz x5, #0x77
    str  x5, [x4]

    msr  ttbr0_el1, x20
    movz x2, #16
    msr  tcr_el1, x2
    movz x2, #0
    msr  mair_el1, x2
    movz x2, #1
    msr  sctlr_el1, x2
    isb

    movz x30, #0x4000, lsl #16
    ldr  x13, [x30]
    subs xzr, x13, #0x77
    b.ne fail

    // Unmap and invalidate VA.
    movz x2, #0
    str  x2, [x23]
    tlbi vae1, x30
    dsb  sy
    isb

    // Must trigger data abort (EC=0x25).
faulting_ldr:
    ldr  x13, [x30]
    b fail

after_abort:
    ldr  x13, [x11]
    cbz  x13, fail
    movz x0, #'3'
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
    .space 0x200
sync_spx:
    b sync_handler

sync_handler:
    // Expect data abort same EL: ESR_EL1.EC = 0x25 (100101b).
    mrs  x2, esr_el1
    tbnz x2, #31, ec_b31_ok
    b fail_from_handler
ec_b31_ok:
    tbz  x2, #30, ec_b30_ok
    b fail_from_handler
ec_b30_ok:
    tbz  x2, #29, ec_b29_ok
    b fail_from_handler
ec_b29_ok:
    tbnz x2, #28, ec_b28_ok
    b fail_from_handler
ec_b28_ok:
    tbz  x2, #27, ec_b27_ok
    b fail_from_handler
ec_b27_ok:
    tbnz x2, #26, ec_ok
    b fail_from_handler
ec_ok:
    // FAR/ELR should match faulting VA.
    mrs  x5, far_el1
    subs xzr, x5, x30
    b.ne fail_from_handler
    mrs  x6, elr_el1
    adr  x7, faulting_ldr
    subs xzr, x6, x7
    b.ne fail_from_handler

    adr  x11, handled
    movz x12, #1
    str  x12, [x11]
    adr  x8, after_abort
    msr  elr_el1, x8
    eret

fail_from_handler:
    adr  x8, fail
    msr  elr_el1, x8
    eret

    .data
    .balign 8
handled:
    .quad 0
