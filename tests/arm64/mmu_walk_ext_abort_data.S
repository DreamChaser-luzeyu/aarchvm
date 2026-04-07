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
    movz x15, #0x0403
    movz x20, #0x1000              // L0
    movz x21, #0x2000              // L1
    movz x22, #0x3000              // L2 low
    movz x23, #0x4000              // L3 low

    orr  x2, x21, x9
    str  x2, [x20]
    orr  x2, x22, x9
    str  x2, [x21]
    orr  x2, x23, x9
    str  x2, [x22]

    // VA 0x40000000 enters a valid walk, but the level-2 descriptor lives in
    // an unmapped physical page so the walk itself takes a synchronous
    // External abort.
    movz x3, #0x2000, lsl #16
    orr  x2, x3, x9
    str  x2, [x21, #8]

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

    msr  ttbr0_el1, x20
    movz x2, #16
    msr  tcr_el1, x2
    movz x2, #0
    msr  mair_el1, x2
    movz x2, #1
    msr  sctlr_el1, x2
    isb

    movz x30, #0x4000, lsl #16

fault_load:
    ldr  x2, [x30]
    b    fail
after_load:
fault_store:
    str  xzr, [x30]
    b    fail
after_store:
    adr  x11, handled
    ldr  x12, [x11]
    cmp  x12, #3
    b.ne fail

    movz x2, #0
    msr  sctlr_el1, x2
    isb
    movz x0, #'W'
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
    cmp  x3, #0x25
    b.ne fail_from_handler
    and  x4, x2, #0x3f
    movz x5, #0x16                // walk external abort, level 2
    cmp  x4, x5
    b.ne fail_from_handler
    tbnz x2, #8, fail_from_handler
    mrs  x6, far_el1
    subs xzr, x6, x30
    b.ne fail_from_handler
    mrs  x7, elr_el1

    adr  x8, fault_load
    cmp  x7, x8
    b.eq handle_load
    adr  x8, fault_store
    cmp  x7, x8
    b.eq handle_store
    b    fail_from_handler

handle_load:
    tbnz x2, #6, fail_from_handler
    adr  x11, handled
    movz x12, #1
    str  x12, [x11]
    adr  x8, after_load
    msr  elr_el1, x8
    eret

handle_store:
    tbz  x2, #6, fail_from_handler
    adr  x11, handled
    ldr  x12, [x11]
    orr  x12, x12, #2
    str  x12, [x11]
    adr  x8, after_store
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
