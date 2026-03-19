    .arch armv8.1-a
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

    // L0 -> L1
    orr  x2, x21, x9
    str  x2, [x20]

    // L1[0] -> identity-map tables, L1[1] -> 0x4000_0000 region.
    orr  x2, x22, x9
    str  x2, [x21]
    orr  x2, x24, x9
    str  x2, [x21, #8]

    // Identity-map low pages for code/data/table pages.
    orr  x2, x23, x9
    str  x2, [x22]
    movz x15, #0x0403              // AF=1, EL1 RW
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

    // High VA region.
    orr  x2, x25, x9
    str  x2, [x24]

    // VA 0x4000_0000 -> PA 0x0050_0000, EL0 RW.
    movz x3, #0x0050, lsl #16
    movz x15, #0x0443              // AF=1, EL0 RW
    orr  x2, x3, x15
    str  x2, [x25, #0]

    // VA 0x4000_1000 -> PA 0x0050_1000, EL1 RW only.
    movz x3, #0x0050, lsl #16
    add  x3, x3, #0x1000
    movz x15, #0x0403              // AF=1, EL1 RW
    orr  x2, x3, x15
    str  x2, [x25, #8]

    // Seed backing pages while MMU is off.
    movz x11, #0x0050, lsl #16
    movz x12, #0x7788
    movk x12, #0x5566, lsl #16
    movk x12, #0x3344, lsl #32
    movk x12, #0x1122, lsl #48
    str  x12, [x11]
    add  x11, x11, #0x1000
    movz x12, #0x6655
    str  x12, [x11]

    msr  ttbr0_el1, x20
    movz x2, #16
    msr  tcr_el1, x2
    movz x2, #0
    msr  mair_el1, x2
    movz x2, #1
    msr  sctlr_el1, x2
    isb

    movz x30, #0x4000, lsl #16
    add  x29, x30, #0x1000

    // With PAN=1, unprivileged accesses must still use EL0 permissions.
    msr  pan, #1

    movz x16, #0x7788
    movk x16, #0x5566, lsl #16
    movk x16, #0x3344, lsl #32
    movk x16, #0x1122, lsl #48
    ldtr x13, [x30]
    cmp  x13, x16
    b.ne fail

    movz x27, #1
faulting_ldr:
    ldr  x13, [x30]
    b fail

after_ldr_fault:
    movz x14, #0xbbaa
    movk x14, #0xddcc, lsl #16
    movk x14, #0xffee, lsl #32
    movk x14, #0x1100, lsl #48
    sttr x14, [x30, #8]
    ldtr x15, [x30, #8]
    cmp  x15, x14
    b.ne fail

    movz x27, #2
faulting_sttr_priv:
    sttr x14, [x29]
    b fail

after_sttr_priv:
    movz x27, #3
faulting_ldtr_priv:
    ldtr x15, [x29]
    b fail

after_ldtr_priv:
    msr  pan, #0
    movz x2, #0
    msr  sctlr_el1, x2
    isb
    movz x0, #'U'
    str  x0, [x1]
    movz x0, #'\n'
    str  x0, [x1]
    brk  #0

fail:
    msr  pan, #0
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
    mrs  x2, esr_el1
    ubfx x4, x2, #26, #6          // EC
    cmp  x4, #0x25
    b.ne fail_from_handler
    and  x4, x2, #0x3f            // FSC
    cmp  x4, #0x0f                // permission fault, level 3
    b.ne fail_from_handler
    mrs  x5, far_el1
    mrs  x6, elr_el1

    cmp  x27, #1
    b.eq handle_pan_ldr
    cmp  x27, #2
    b.eq handle_priv_sttr
    cmp  x27, #3
    b.eq handle_priv_ldtr
    b fail_from_handler

handle_pan_ldr:
    tbnz x2, #6, fail_from_handler
    subs xzr, x5, x30
    b.ne fail_from_handler
    adr  x7, faulting_ldr
    subs xzr, x6, x7
    b.ne fail_from_handler
    adr  x8, after_ldr_fault
    msr  elr_el1, x8
    eret

handle_priv_sttr:
    tbz  x2, #6, fail_from_handler
    subs xzr, x5, x29
    b.ne fail_from_handler
    adr  x7, faulting_sttr_priv
    subs xzr, x6, x7
    b.ne fail_from_handler
    adr  x8, after_sttr_priv
    msr  elr_el1, x8
    eret

handle_priv_ldtr:
    tbnz x2, #6, fail_from_handler
    subs xzr, x5, x29
    b.ne fail_from_handler
    adr  x7, faulting_ldtr_priv
    subs xzr, x6, x7
    b.ne fail_from_handler
    adr  x8, after_ldtr_priv
    msr  elr_el1, x8
    eret

fail_from_handler:
    adr  x8, fail
    msr  elr_el1, x8
    eret
