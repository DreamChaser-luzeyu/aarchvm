    .text
    .global _start
_start:
    movz x1, #0x0000
    movk x1, #0x0900, lsl #16

    adr  x10, vectors
    msr  vbar_el1, x10

    adr  x11, handled
    movz x12, #0
    str  x12, [x11]

    // Start from fully unmasked state so the exception handler can verify
    // that exception entry forced DAIF to 0b1111 and SPSR_EL1 preserved 0.
    movz x0, #0
    msr  daif, x0
    svc  #0

after_svc:
    ldr  x13, [x11]
    cbz  x13, fail

    mrs  x14, daif
    and  x14, x14, #0x3c0
    cbnz x14, fail

    movz x0, #'D'
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
    .space 0x5fc

sync_handler:
    mrs  x2, esr_el1
    ubfx x2, x2, #26, #6
    cmp  x2, #0x15
    b.ne fail_from_handler

    mrs  x3, daif
    and  x3, x3, #0x3c0
    movz x4, #0x3c0
    cmp  x3, x4
    b.ne fail_from_handler

    mrs  x5, spsr_el1
    and  x5, x5, #0x3c0
    cbnz x5, fail_from_handler

    adr  x11, handled
    movz x12, #1
    str  x12, [x11]
    adr  x8, after_svc
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
