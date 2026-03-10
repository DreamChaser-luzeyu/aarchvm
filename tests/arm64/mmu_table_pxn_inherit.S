    .text
    .global _start
_start:
    movz x1, #0
    movk x1, #0x0900, lsl #16      // UART
    adr  x29, print_target

    adr  x10, vectors
    msr  vbar_el1, x10
    adr  x11, handled
    movz x12, #0
    str  x12, [x11]

    movz x9, #3
    movz x15, #0x0403
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

    // PXNTable on L1[1].
    orr  x26, x24, x9
    movz x27, #0x0800, lsl #48
    orr  x26, x26, x27
    str  x26, [x21, #8]
    orr  x28, x24, x9

    orr  x2, x23, x9
    str  x2, [x22]
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

    // High VA aliases low physical page 0.
    movz x3, #0x0000
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

    movz x20, #0x4000, lsl #16
    adr  x12, aliased_target
    add  x20, x20, x12
    br   x20
    b    fail

aliased_target:
    br   x29

print_target:
    adr  x11, handled
    ldr  x12, [x11]
    cbz  x12, fail
    movz x2, #0
    msr  sctlr_el1, x2
    isb
    movz x0, #'Y'
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
    adr  x11, handled
    ldr  x12, [x11]
    cbnz x12, fail_from_handler
    str  x28, [x21, #8]
    tlbi vmalle1is
    dsb  sy
    isb
    movz x12, #1
    str  x12, [x11]
    msr  elr_el1, x20
    eret

fail_from_handler:
    adr  x8, fail
    msr  elr_el1, x8
    eret

    .data
    .balign 8
handled:
    .quad 0
