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

    orr  x2, x21, x9
    str  x2, [x20]
    orr  x2, x22, x9
    str  x2, [x21]
    orr  x2, x24, x9
    str  x2, [x21, #8]
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
    movz x3, #0x5000
    orr  x2, x3, x15
    str  x2, [x23, #40]
    movz x3, #0x6000
    orr  x2, x3, x15
    str  x2, [x23, #48]
    movz x3, #0x7000
    orr  x2, x3, x15
    str  x2, [x23, #56]
    movz x3, #0x8000
    orr  x2, x3, x15
    str  x2, [x23, #64]
    movz x3, #0x9000
    orr  x2, x3, x15
    str  x2, [x23, #72]

    // Map only the first target page at VA 0x40000000. Leave 0x40001000 unmapped.
    orr  x2, x25, x9
    str  x2, [x24]
    movz x3, #0x0050, lsl #16      // PA 0x00500000
    orr  x2, x3, x15
    str  x2, [x25, #0]

    msr  ttbr0_el1, x20
    movz x2, #16
    msr  tcr_el1, x2
    movz x2, #0x00ff
    msr  mair_el1, x2
    movz x2, #1
    msr  sctlr_el1, x2
    isb

    movz x2, #0x0030, lsl #16
    msr  cpacr_el1, x2
    movi v0.16b, #0x11
    movi v1.16b, #0x22

    adr  x5, phase

    movz x6, #0
    str  x6, [x5]
    movz x19, #0x4000, lsl #16
    add  x19, x19, #0x0ffd
fault_ldr_d:
    ldr  d0, [x19]
    b fail

after_ldr_d:
    adr  x5, phase
    movz x6, #1
    str  x6, [x5]
    movz x19, #0x4000, lsl #16
    add  x19, x19, #0x0ffd
fault_str_d:
    str  d0, [x19]
    b fail

after_str_d:
    adr  x5, phase
    movz x6, #2
    str  x6, [x5]
    movz x19, #0x4000, lsl #16
    add  x19, x19, #0x0ffa
fault_ldr_q:
    ldr  q1, [x19]
    b fail

after_ldr_q:
    adr  x5, phase
    movz x6, #3
    str  x6, [x5]
    movz x19, #0x4000, lsl #16
    add  x19, x19, #0x0ffa
fault_str_q:
    str  q1, [x19]
    b fail

after_str_q:
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
    and  x3, x2, #0x3f
    cmp  x3, #0x07
    b.ne fail_from_handler

    adr  x11, phase
    ldr  x12, [x11]
    cbz  x12, check_ldr_d
    cmp  x12, #1
    b.eq check_str_d
    cmp  x12, #2
    b.eq check_ldr_q
    cmp  x12, #3
    b.eq check_str_q
    b fail_from_handler

check_far_common:
    mrs  x5, far_el1
    movz x6, #0x4000, lsl #16
    add  x6, x6, #0x1000
    cmp  x5, x6
    b.ne fail_from_handler
    ret

check_ldr_d:
    tbnz x2, #6, fail_from_handler
    bl check_far_common
    mrs  x6, elr_el1
    adr  x7, fault_ldr_d
    cmp  x6, x7
    b.ne fail_from_handler
    adr  x8, after_ldr_d
    msr  elr_el1, x8
    eret

check_str_d:
    tbz  x2, #6, fail_from_handler
    bl check_far_common
    mrs  x6, elr_el1
    adr  x7, fault_str_d
    cmp  x6, x7
    b.ne fail_from_handler
    adr  x8, after_str_d
    msr  elr_el1, x8
    eret

check_ldr_q:
    tbnz x2, #6, fail_from_handler
    bl check_far_common
    mrs  x6, elr_el1
    adr  x7, fault_ldr_q
    cmp  x6, x7
    b.ne fail_from_handler
    adr  x8, after_ldr_q
    msr  elr_el1, x8
    eret

check_str_q:
    tbz  x2, #6, fail_from_handler
    bl check_far_common
    mrs  x6, elr_el1
    adr  x7, fault_str_q
    cmp  x6, x7
    b.ne fail_from_handler
    adr  x8, after_str_q
    msr  elr_el1, x8
    eret

fail_from_handler:
    adr  x8, fail
    msr  elr_el1, x8
    eret

    .data
    .balign 8
phase:
    .quad 0
