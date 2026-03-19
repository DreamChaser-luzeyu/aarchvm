    .text
    .global _start
_start:
    movz x1, #0x0000
    movk x1, #0x0900, lsl #16      // UART

    adr  x10, vectors
    msr  vbar_el1, x10

    adr  x11, handled
    movz x12, #0
    str  x12, [x11]

    // Trigger instruction abort by jumping to an unmapped address outside the
    // 1 GiB SDRAM window [0x40000000, 0x80000000).
    movz x20, #0x8000, lsl #16      // 0x80000000
    br   x20

after_fault:
    ldr  x13, [x11]
    cbz  x13, fail

    movz x0, #'X'
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
    // ESR_EL1.EC must be 0x21 (instruction abort, same EL).
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
    tbz  x2, #28, ec_b28_ok
    b fail_from_handler
ec_b28_ok:
    tbz  x2, #27, ec_b27_ok
    b fail_from_handler
ec_b27_ok:
    tbnz x2, #26, ec_ok
    b fail_from_handler
ec_ok:

    // FAR_EL1 and ELR_EL1 should point to the faulting PC.
    mrs  x5, far_el1
    subs xzr, x5, x20
    b.ne fail_from_handler

    mrs  x6, elr_el1
    subs xzr, x6, x20
    b.ne fail_from_handler

    // SPSR_EL1 stores pre-exception PSTATE. Here I should still be 0.
    mrs  x7, spsr_el1
    tbnz x7, #7, fail_from_handler

    // Mark handled, then redirect return PC to resume path.
    adr  x11, handled
    movz x12, #1
    str  x12, [x11]

    adr  x8, after_fault
    msr  elr_el1, x8
    eret

fail_from_handler:
    movz x12, #0
    str  x12, [x11]
    adr  x8, fail
    msr  elr_el1, x8
    eret

    .data
    .balign 8
handled:
    .quad 0
