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

    // VA 0x40000000 -> PA 0x500000, Device attr0, AF=1.
    movz x3, #0x0050, lsl #16
    movz x2, #0x0403
    orr  x2, x2, x3
    str  x2, [x25]

    // Seed the physical backing storage. The Device memory type must still
    // force an alignment fault before this data is read or modified.
    movz x26, #0x0050, lsl #16
    movz x27, #0x2211
    movk x27, #0x4433, lsl #16
    movk x27, #0x6655, lsl #32
    movk x27, #0x8877, lsl #48
    str  x27, [x26]

    msr  ttbr0_el1, x20
    movz x2, #16
    msr  tcr_el1, x2
    movz x2, #0
    msr  mair_el1, x2

    // A=0: Normal misaligned accesses may succeed, but Device memory must still
    // fault due to memory type.
    movz x2, #1
    msr  sctlr_el1, x2
    isb

    movz x30, #0x4000, lsl #16
    add  x30, x30, #1

    adr  x11, expected_wnr
    str  xzr, [x11]
    adr  x11, expected_elr
    adr  x12, fault_ldr
    str  x12, [x11]
    adr  x11, resume_elr
    adr  x12, after_ldr_fault
    str  x12, [x11]
    adr  x11, expected_far
    str  x30, [x11]
fault_ldr:
    ldr  x4, [x30]
    b    fail

after_ldr_fault:
    adr  x11, expected_wnr
    movz x12, #1
    str  x12, [x11]
    adr  x11, expected_elr
    adr  x12, fault_str
    str  x12, [x11]
    adr  x11, resume_elr
    adr  x12, after_str_fault
    str  x12, [x11]
    adr  x11, expected_far
    str  x30, [x11]

    movz x5, #0xdead
    movk x5, #0xbeef, lsl #16
    movk x5, #0xcafe, lsl #32
    movk x5, #0x1234, lsl #48
fault_str:
    str  x5, [x30]
    b    fail

after_str_fault:
    movz x2, #0
    msr  sctlr_el1, x2
    isb

    ldr  x6, [x26]
    subs xzr, x6, x27
    b.ne fail

    movz x0, #'U'
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
    tbz  x2, #25, fail_from_handler
    tbnz x2, #8, fail_from_handler

    ubfx x4, x2, #6, #1
    adr  x5, expected_wnr
    ldr  x5, [x5]
    cmp  x4, x5
    b.ne fail_from_handler

    and  x4, x2, #0x3f
    cmp  x4, #0x21
    b.ne fail_from_handler

    mrs  x6, far_el1
    adr  x7, expected_far
    ldr  x7, [x7]
    subs xzr, x6, x7
    b.ne fail_from_handler

    mrs  x8, elr_el1
    adr  x9, expected_elr
    ldr  x9, [x9]
    subs xzr, x8, x9
    b.ne fail_from_handler

    adr  x10, resume_elr
    ldr  x10, [x10]
    msr  elr_el1, x10
    eret

fail_from_handler:
    adr  x10, fail
    msr  elr_el1, x10
    eret

    .data
    .balign 8
expected_wnr:
    .quad 0
expected_elr:
    .quad 0
resume_elr:
    .quad 0
expected_far:
    .quad 0
