    .text
    .global _start
_start:
    movz x1, #0
    movk x1, #0x0900, lsl #16      // UART

    adr  x10, vectors
    msr  vbar_el1, x10

    adr  x11, handled
    str  xzr, [x11]

    adr  x30, buf                  // RAM-backed VA, but M=0 direct output is Device
fault_zva:
    dc   zva, x30
    b    fail

after_fault:
    adr  x11, handled
    ldr  x12, [x11]
    cbz  x12, fail

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

sync_handler:
    mrs  x2, esr_el1
    ubfx x3, x2, #26, #6
    cmp  x3, #0x25
    b.ne fail_from_handler
    tbnz x2, #10, fail_from_handler // FnV=0: FAR_EL1 is valid in this model.
    tbnz x2, #8, fail_from_handler // DC ZVA faults are store-like, CM must be 0.
    tbnz x2, #7, fail_from_handler // S1PTW=0 for this alignment fault.
    tbz  x2, #6, fail_from_handler // WnR must be 1.
    and  x4, x2, #0x3f
    cmp  x4, #0x21                // alignment fault
    b.ne fail_from_handler

    mrs  x5, far_el1
    subs xzr, x5, x30
    b.ne fail_from_handler
    mrs  x6, elr_el1
    adr  x7, fault_zva
    subs xzr, x6, x7
    b.ne fail_from_handler

    adr  x11, handled
    movz x12, #1
    str  x12, [x11]
    adr  x8, after_fault
    msr  elr_el1, x8
    eret

fail_from_handler:
    adr  x8, fail
    msr  elr_el1, x8
    eret

    .data
    .balign 64
buf:
    .space 64

    .balign 8
handled:
    .quad 0
