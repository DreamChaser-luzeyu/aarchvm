    .text
    .global _start
_start:
    movz x1, #0
    movk x1, #0x0900, lsl #16      // UART

    adr  x10, vectors
    msr  vbar_el1, x10

    movz x9, #3                    // table descriptor bits
    movz x15, #0x0403              // page descriptor bits, AF=1
    movz x20, #0x1000              // L0
    movz x21, #0x2000              // L1
    movz x22, #0x3000              // L2 low
    movz x23, #0x4000              // L3 low
    movz x24, #0x7000              // L2 high
    movz x25, #0x8000              // L3 high

    // L0 -> L1.
    orr  x2, x21, x9
    str  x2, [x20]

    // L1[0] -> low VA tables, L1[1] -> VA 0x40000000 region.
    orr  x2, x22, x9
    str  x2, [x21]
    orr  x2, x24, x9
    str  x2, [x21, #8]

    // Low VA identity map for code/data/table pages.
    orr  x2, x23, x9
    str  x2, [x22]
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

    // Map only the first page at VA 0x40000000. Leave the second page unmapped.
    orr  x2, x25, x9
    str  x2, [x24]
    movz x3, #0x0050, lsl #16      // PA 0x00500000
    orr  x2, x3, x15
    str  x2, [x25, #0]

    msr  ttbr0_el1, x20
    movz x2, #16
    msr  tcr_el1, x2
    movz x2, #0
    msr  mair_el1, x2
    movz x2, #1
    msr  sctlr_el1, x2
    isb

    movz x30, #0x4000, lsl #16
    add  x30, x30, #0x0ffa         // 8-byte store crosses into 0x40001000
    movz x13, #0x7788
    movk x13, #0x5566, lsl #16
    movk x13, #0x3344, lsl #32
    movk x13, #0x1122, lsl #48
faulting_str:
    str  x13, [x30]
    b fail

after_abort:
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
    ubfx x4, x2, #26, #6          // EC
    cmp  x4, #0x25
    b.ne fail_from_handler
    tbz  x2, #6, fail_from_handler // WnR must be 1 for store
    and  x4, x2, #0x3f            // FSC
    cmp  x4, #0x07                // translation fault, level 3
    b.ne fail_from_handler
    mrs  x5, far_el1
    add  x6, x30, #6
    subs xzr, x5, x6
    b.ne fail_from_handler
    mrs  x6, elr_el1
    adr  x7, faulting_str
    subs xzr, x6, x7
    b.ne fail_from_handler
    adr  x8, after_abort
    msr  elr_el1, x8
    eret

fail_from_handler:
    adr  x8, fail
    msr  elr_el1, x8
    eret
