    .text
    .global _start
_start:
    movz x1, #0
    movk x1, #0x0900, lsl #16      // UART

    adr  x10, vectors
    msr  vbar_el1, x10

    adr  x11, handled
    str  xzr, [x11]

    movz x9, #3
    movz x20, #0x1000              // L0
    movz x21, #0x2000              // L1
    movz x16, #0x0401              // L1 block attrs: valid + block + AF

    // L0[0] -> L1, L1[0] -> low 1GB identity block only.
    orr  x2, x21, x9
    str  x2, [x20]
    str  x16, [x21]

    msr  ttbr0_el1, x20
    movz x2, #16
    movk x2, #0x20, lsl #32        // TBI0=1
    msr  tcr_el1, x2
    movz x2, #0
    msr  mair_el1, x2
    movz x2, #1
    msr  sctlr_el1, x2
    isb

    // Tagged lower-range VA 0xAB00000040000000 is outside the single mapped
    // block, so the access must fault and FAR_EL1 must retain the tag.
    movz x20, #0x4000, lsl #16
    movk x20, #0xAB00, lsl #48

fault_load:
    ldr  x2, [x20]
    b    fail
after_fault:
    adr  x11, handled
    ldr  x12, [x11]
    cmp  x12, #1
    b.ne fail

    movz x2, #0
    msr  sctlr_el1, x2
    isb
    movz x0, #'K'
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
    mrs  x2, esr_el1
    ubfx x3, x2, #26, #6
    cmp  x3, #0x25                // Data abort, same EL
    b.ne fail_from_handler
    and  x4, x2, #0x3f
    cmp  x4, #0x5                 // Translation fault, level 1
    b.ne fail_from_handler
    tbnz x2, #6, fail_from_handler

    mrs  x5, far_el1
    cmp  x5, x20
    b.ne fail_from_handler

    mrs  x6, elr_el1
    adr  x7, fault_load
    cmp  x6, x7
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
    .balign 8
handled:
    .quad 0
