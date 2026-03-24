    .arch armv8.1-a
    .text
    .global _start
_start:
    movz x1, #0x0000
    movk x1, #0x0900, lsl #16      // UART

    adr  x10, vectors
    msr  vbar_el1, x10

    adr  x11, handled
    str  xzr, [x11]
    adr  x12, phase
    movz x13, #'1'
    str  x13, [x12]

    movz x9, #3
    movz x20, #0x1000              // TTBR0 root (RW context)
    movz x21, #0x2000              // L1 (RW context)
    movz x22, #0x3000              // Common L2 low
    movz x23, #0x4000              // Common L3 low
    movz x24, #0x7000              // L2 high (RW context)
    movz x25, #0x8000              // L3 high (RW context)
    movz x16, #0x5000              // TTBR0 root (RO context)
    movz x17, #0x6000              // L1 (RO context)
    movz x18, #0x9000              // L2 high (RO context)
    movz x19, #0xa000              // L3 high (RO context)

    // Root/L1 for RW context.
    orr  x2, x21, x9
    str  x2, [x20]
    orr  x2, x22, x9
    str  x2, [x21]
    orr  x2, x24, x9
    str  x2, [x21, #8]

    // Root/L1 for RO context.
    orr  x2, x17, x9
    str  x2, [x16]
    orr  x2, x22, x9
    str  x2, [x17]
    orr  x2, x18, x9
    str  x2, [x17, #8]

    // Shared low VA mappings for code/data/vectors.
    orr  x2, x23, x9
    str  x2, [x22]

    movz x14, #0x0403              // AF=1, EL1 RW
    movz x15, #0x0483              // AF=1, EL1 RO

    movz x3, #0x0000
    orr  x2, x3, x14
    str  x2, [x23, #0]
    movz x3, #0x1000
    orr  x2, x3, x14
    str  x2, [x23, #8]
    movz x3, #0x2000
    orr  x2, x3, x14
    str  x2, [x23, #16]
    movz x3, #0x3000
    orr  x2, x3, x14
    str  x2, [x23, #24]
    movz x3, #0x4000
    orr  x2, x3, x14
    str  x2, [x23, #32]

    // High VA mapping differs only in write permission.
    orr  x2, x25, x9
    str  x2, [x24]
    orr  x2, x19, x9
    str  x2, [x18]

    movz x3, #0x0050, lsl #16      // PA 0x00500000
    orr  x2, x3, x14
    str  x2, [x25, #0]
    orr  x2, x3, x15
    str  x2, [x19, #0]

    // Seed physical memory before enabling the MMU.
    movz x26, #0x0050, lsl #16
    add  x26, x26, #0x0ff0
    movz x27, #0x1111
    movk x27, #0x2222, lsl #16
    movk x27, #0x3333, lsl #32
    movk x27, #0x4444, lsl #48
    movz x28, #0x5555
    movk x28, #0x6666, lsl #16
    movk x28, #0x7777, lsl #32
    movk x28, #0x8888, lsl #48
    str  x27, [x26]
    add  x26, x26, #8
    str  x28, [x26]

    msr  ttbr0_el1, x20
    movz x2, #16
    msr  tcr_el1, x2
    movz x2, #0
    msr  mair_el1, x2
    movz x2, #1
    msr  sctlr_el1, x2
    isb

    movz x30, #0x4000, lsl #16
    add  x30, x30, #0x0ff0         // Aligned 16-byte pair fully inside one page.

    ldxp x2, x3, [x30]
    cmp  x2, x27
    b.ne fail
    cmp  x3, x28
    b.ne fail

    // Switch to an alternate TTBR0 context that remaps the same VA page as RO.
    // This preserves the existing exclusive monitor while making the subsequent
    // store paths fault on write permission.
    msr  ttbr0_el1, x16
    tlbi vmalle1
    dsb  ish
    isb

    movz x4, #0x9999
    movk x4, #0xaaaa, lsl #16
    movk x4, #0xbbbb, lsl #32
    movk x4, #0xcccc, lsl #48
    movz x5, #0xdddd
    movk x5, #0xeeee, lsl #16
    movk x5, #0xffff, lsl #32
    movk x5, #0x1111, lsl #48
    movz w6, #0x0077
fault_stxp:
    stxp w6, x4, x5, [x30]
    b    fail
after_stxp:
    ldr  x7, [x11]
    cmp  x7, #1
    b.ne fail
    cmp  w6, #0x0077
    b.ne fail
    ldr  x8, [x30]
    cmp  x8, x27
    b.ne fail
    ldr  x9, [x30, #8]
    cmp  x9, x28
    b.ne fail
    adr  x12, phase
    movz x13, #'2'
    str  x13, [x12]

    mov  x12, x27
    mov  x13, x28
    movz x14, #0x1212
    movk x14, #0x3434, lsl #16
    movk x14, #0x5656, lsl #32
    movk x14, #0x7878, lsl #48
    movz x15, #0x9a9a
    movk x15, #0xbcbc, lsl #16
    movk x15, #0xdede, lsl #32
    movk x15, #0xf0f0, lsl #48
fault_casp:
    casp x12, x13, x14, x15, [x30]
    b    fail
after_casp:
    ldr  x7, [x11]
    cmp  x7, #2
    b.ne fail
    cmp  x12, x27
    b.ne fail
    cmp  x13, x28
    b.ne fail
    ldr  x8, [x30]
    cmp  x8, x27
    b.ne fail
    ldr  x9, [x30, #8]
    cmp  x9, x28
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
    adr  x12, phase
    ldr  x0, [x12]
    str  x0, [x1]
    brk  #0

    .balign 2048
vectors:
    .space 0x200
sync_spx:
    b sync_handler

sync_handler:
    adr  x20, saved_regs
    str  x6, [x20]
    mrs  x2, esr_el1
    ubfx x3, x2, #26, #6
    cmp  x3, #0x25
    b.ne fail_from_handler
    tbz  x2, #6, fail_from_handler
    and  x3, x2, #0x3f
    cmp  x3, #0x0f                // permission fault, level 3
    b.ne fail_from_handler
    mrs  x4, far_el1
    cmp  x4, x30
    b.ne fail_from_handler
    mrs  x6, elr_el1

    adr  x7, fault_stxp
    cmp  x6, x7
    b.eq handle_stxp
    adr  x7, fault_casp
    cmp  x6, x7
    b.eq handle_casp
    b    fail_from_handler

handle_stxp:
    movz x8, #1
    str  x8, [x11]
    adr  x20, saved_regs
    ldr  x6, [x20]
    adr  x9, after_stxp
    msr  elr_el1, x9
    eret

handle_casp:
    movz x8, #2
    str  x8, [x11]
    adr  x20, saved_regs
    ldr  x6, [x20]
    adr  x9, after_casp
    msr  elr_el1, x9
    eret

fail_from_handler:
    adr  x8, fail
    msr  elr_el1, x8
    eret

    .data
    .balign 8
handled:
    .quad 0
phase:
    .quad '0'
saved_regs:
    .quad 0
