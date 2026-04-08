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
    movz x22, #0x3000              // L2 low
    movz x23, #0x4000              // L3 low
    movz x24, #0x7000              // L2 high
    movz x25, #0x8000              // L3 high

    orr  x2, x21, x9
    str  x2, [x20]
    orr  x2, x22, x9
    str  x2, [x21]
    orr  x2, x24, x9
    str  x2, [x21, #8]
    orr  x2, x23, x9
    str  x2, [x22]
    movz x15, #0x0403
    orr  x2, x25, x9
    str  x2, [x24]

    movz x3, #0x0000
    orr  x2, x3, x15
    str  x2, [x23, #0]
    movz x3, #0x1000
    orr  x2, x3, x15
    str  x2, [x23, #8]
    movz x3, #0x2000
    orr  x2, x3, x15
    str  x2, [x23, #16]
    movz x3, #0x3000
    orr  x2, x3, x15
    str  x2, [x23, #24]
    movz x3, #0x4000
    orr  x2, x3, x15
    str  x2, [x23, #32]
    movz x3, #0x7000
    orr  x2, x3, x15
    str  x2, [x23, #56]
    movz x3, #0x8000
    orr  x2, x3, x15
    str  x2, [x23, #64]

    // VA 0x40000000 -> PA 0x500000, AF=0 initially.
    movz x3, #0x0050, lsl #16
    movz x26, #0x0003
    orr  x26, x26, x3
    movz x27, #0x0403
    orr  x27, x27, x3
    str  x26, [x25]

    movz x10, #0x0050, lsl #16
    movz x11, #0x77
    str  x11, [x10]

    msr  ttbr0_el1, x20
    movz x2, #16
    msr  tcr_el1, x2
    movz x2, #0
    msr  mair_el1, x2
    movz x2, #1
    msr  sctlr_el1, x2
    isb

    movz x30, #0x4000, lsl #16

    // AT should fail while AF=0.
    at   s1e1r, x30
    mrs  x13, par_el1
    tbnz x13, #0, par_fault_ok
    b    fail
par_fault_ok:

faulting_ldr:
    ldr  x14, [x30]
    b fail

after_abort:
    adr  x11, handled
    ldr  x13, [x11]
    cbz  x13, fail

    at   s1e1r, x30
    mrs  x13, par_el1
    tbnz x13, #0, fail
    tbz  x13, #11, fail
    ubfx x14, x13, #12, #44
    movz x15, #0x0500
    subs xzr, x14, x15
    b.ne fail
    lsr  x14, x13, #56
    cbnz x14, fail
    ubfx x14, x13, #7, #2
    movz x15, #2
    subs xzr, x14, x15
    b.ne fail

    ldr  x14, [x30]
    subs xzr, x14, #0x77
    b.ne fail

    movz x2, #0
    msr  sctlr_el1, x2
    isb
    movz x0, #'0'
    str  x0, [x1]
    movz x0, #'\n'
    str  x0, [x1]
    brk  #0

fail:
    movz x2, #0
    msr  sctlr_el1, x2
    isb
    movz x0, #'F'
    str  x0, [x1]
    brk  #0

    .balign 2048
vectors:
    .space 0x200
sync_spx:
    b sync_handler

sync_handler:
    // ESR_EL1.EC must be 0x25 (data abort, same EL).
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

    // Access-flag fault level 3, read access, with the remaining data-abort ISS bits clear.
    tbnz x2, #10, fail_from_handler // FnV=0: FAR_EL1 is valid in this model.
    tbnz x2, #8, fail_from_handler  // CM=0: normal load, not cache maintenance/AT.
    tbnz x2, #7, fail_from_handler  // S1PTW=0: final access, not stage-2 on stage-1 walk.
    tbz  x2, #6, not_write_ok
    b fail_from_handler
not_write_ok:
    tbnz x2, #0, fsc_b0_ok
    b fail_from_handler
fsc_b0_ok:
    tbnz x2, #1, fsc_b1_ok
    b fail_from_handler
fsc_b1_ok:
    tbz  x2, #2, fsc_b2_ok
    b fail_from_handler
fsc_b2_ok:
    tbnz x2, #3, fsc_b3_ok
    b fail_from_handler
fsc_b3_ok:
    tbz  x2, #4, fsc_b4_ok
    b fail_from_handler
fsc_b4_ok:
    tbz  x2, #5, fsc_ok
    b fail_from_handler
fsc_ok:

    mrs  x5, far_el1
    subs xzr, x5, x30
    b.ne fail_from_handler
    mrs  x6, elr_el1
    adr  x7, faulting_ldr
    subs xzr, x6, x7
    b.ne fail_from_handler

    str  x27, [x25]
    lsr  x16, x30, #12
    tlbi vale1is, x16
    dsb  sy
    isb

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
