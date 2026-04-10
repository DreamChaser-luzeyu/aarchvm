    .text
    .global _start
_start:
    movz x1, #0
    movk x1, #0x0900, lsl #16      // UART

    adr  x10, vectors
    msr  vbar_el1, x10

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
    orr  x2, x25, x9
    str  x2, [x24]

    movz x15, #0x0443              // Low identity map: EL0 RW, AF=1.

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

    // VA 0x40000000 aliases the low code page but is UXN at EL0.
    movz x26, #0x0443
    movk x26, #0x0040, lsl #48     // UXN=1, PXN=0.
    str  x26, [x25]

    msr  ttbr0_el1, x20
    movz x2, #16
    msr  tcr_el1, x2
    movz x2, #0
    msr  mair_el1, x2
    movz x2, #1
    msr  sctlr_el1, x2
    isb

    movz x20, #0x4000, lsl #16
    adr  x12, aliased_target
    add  x20, x20, x12

    msr  elr_el1, x20
    msr  spsr_el1, xzr             // EL0t
    eret

after_fault:
    movz x2, #0
    msr  sctlr_el1, x2
    isb
    movz x0, #'u'
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

aliased_target:
    movz x0, #'X'
    str  x0, [x1]
    b fail

    .balign 2048
vectors:
    .space 0x200
sync_spx:
    b fail
    .space 0x1fc
sync_lower:
    mrs  x2, esr_el1
    ubfx x3, x2, #26, #6
    cmp  x3, #0x20                // instruction abort from lower EL
    b.ne fail_from_handler
    tbz  x2, #25, fail_from_handler // IL=1
    tbnz x2, #6, fail_from_handler  // WnR=0
    and  x4, x2, #0x3f
    cmp  x4, #0x0f                // permission fault, level 3
    b.ne fail_from_handler
    mrs  x5, far_el1
    subs xzr, x5, x20
    b.ne fail_from_handler
    mrs  x6, elr_el1
    subs xzr, x6, x20
    b.ne fail_from_handler
    mrs  x7, spsr_el1
    and  x8, x7, #0xf
    cbnz x8, fail_from_handler
    tbnz x7, #20, fail_from_handler
    ubfx x8, x7, #28, #4
    cbnz x8, fail_from_handler
    ubfx x8, x7, #6, #4
    cbnz x8, fail_from_handler
    tbnz x7, #22, fail_from_handler

    adr  x8, after_fault
    movz x9, #0x5                 // EL1h
    msr  spsr_el1, x9
    msr  elr_el1, x8
    eret

fail_from_handler:
    adr  x8, fail
    msr  elr_el1, x8
    eret
