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
    movz x24, #0x7000              // L2 good-high
    movz x25, #0x8000              // L3 good-high

    orr  x2, x21, x9
    str  x2, [x20]
    orr  x2, x22, x9
    str  x2, [x21]
    orr  x2, x23, x9
    str  x2, [x22]
    orr  x2, x24, x9
    str  x2, [x21, #8]             // VA 0x40000000

    // VA 0x80000000: walk reaches an unmapped level-2 table page.
    movz x3, #0x2000, lsl #16
    orr  x2, x3, x9
    str  x2, [x21, #16]

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

    movz x3, #0x0050, lsl #16      // PA 0x500000
    orr  x2, x3, x15
    str  x2, [x25]

    msr  ttbr0_el1, x20
    movz x2, #16
    msr  tcr_el1, x2
    movz x2, #0
    msr  mair_el1, x2
    movz x2, #1
    msr  sctlr_el1, x2
    isb

    movz x30, #0x4000, lsl #16
    movz x29, #0x8000, lsl #16

    at   s1e1r, x30
    mrs  x4, par_el1
    tbnz x4, #0, fail
    adr  x11, par_before
    str  x4, [x11]

fault_at:
    at   s1e1r, x29
    b    fail
after_fault_at:
    adr  x11, handled
    ldr  x12, [x11]
    cmp  x12, #1
    b.ne fail

    movz x2, #0
    msr  sctlr_el1, x2
    isb
    movz x0, #'A'
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
    tbz  x2, #8, fail_from_handler // CM=1 for AT
    tbz  x2, #6, fail_from_handler // WnR=1 for AT
    mrs  x6, far_el1
    subs xzr, x6, x29
    b.ne fail_from_handler
    mrs  x7, elr_el1
    adr  x8, fault_at
    cmp  x7, x8
    b.ne fail_from_handler
    mrs  x9, par_el1
    adr  x10, par_before
    ldr  x11, [x10]
    cmp  x9, x11
    b.ne fail_from_handler

    adr  x12, handled
    movz x13, #1
    str  x13, [x12]
    adr  x8, after_fault_at
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
par_before:
    .quad 0
