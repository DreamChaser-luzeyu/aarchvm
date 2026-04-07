    .text
    .global _start
_start:
    movz x1, #0
    movk x1, #0x0900, lsl #16      // UART

    adr  x10, vectors
    msr  vbar_el1, x10

    adr  x11, handled
    str  xzr, [x11]

    // This model advertises ID_AA64MMFR1_EL1.HAFDBS == 0b0000.
    mrs  x27, id_aa64mmfr1_el1
    and  x27, x27, #0xf
    cbnz x27, fail

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
    movz x15, #0x0403

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

    // VA 0x40000000 -> PA 0x500000, AF=1, AP[2]=1 (read-only), DBM=1.
    // Because HAFDBS is absent, DBM must not make the mapping writable.
    movz x3, #0x0050, lsl #16
    movz x2, #0x0483
    orr  x26, x3, x2
    movk x26, #0x0008, lsl #48     // DBM bit[51]
    str  x26, [x25]

    movz x10, #0x0050, lsl #16
    movz x11, #0x11
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
    ldr  x13, [x30]
    subs xzr, x13, #0x11
    b.ne fail

    at   s1e1w, x30
    mrs  x4, par_el1
    tbz  x4, #0, fail
    tbz  x4, #11, fail
    and  x5, x4, #0x7f
    movz x6, #0x1f                // F=1, permission fault level 3
    cmp  x5, x6
    b.ne fail
    movz x5, #0x0700
    tst  x4, x5
    b.ne fail
    ubfx x5, x4, #16, #48
    cbnz x5, fail

    movz x14, #0x22
faulting_str:
    str  x14, [x30]
    b fail

after_abort:
    adr  x11, handled
    ldr  x13, [x11]
    cbz  x13, fail
    ldr  x13, [x30]
    subs xzr, x13, #0x11
    b.ne fail

    movz x2, #0
    msr  sctlr_el1, x2
    isb
    movz x0, #'D'
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
    and  x4, x2, #0x7f
    movz x5, #0x4f                // FSC 0x0f + WnR=1
    cmp  x4, x5
    b.ne fail_from_handler
    tbnz x2, #8, fail_from_handler
    mrs  x6, far_el1
    subs xzr, x6, x30
    b.ne fail_from_handler
    mrs  x7, elr_el1
    adr  x8, faulting_str
    cmp  x7, x8
    b.ne fail_from_handler

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
