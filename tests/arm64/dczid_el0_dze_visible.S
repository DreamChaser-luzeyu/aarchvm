    .text
    .global _start
_start:
    movz x20, #0
    movk x20, #0x0900, lsl #16      // UART

    adr  x10, vectors
    msr  vbar_el1, x10

    mrs  x0, sctlr_el1
    movz x1, #1
    lsl  x2, x1, #14                // DZE
    bic  x3, x0, x2
    msr  sctlr_el1, x3
    isb

    adr  x4, el0_read_disabled
    msr  elr_el1, x4
    msr  spsr_el1, xzr              // EL0t
    eret

after_disabled:
    adr  x11, result_value
    ldr  x12, [x11]
    movz x13, #0x14                 // DZP=1, BS=4
    cmp  x12, x13
    b.ne fail1

    orr  x3, x3, x2
    msr  sctlr_el1, x3
    isb

    adr  x4, el0_read_enabled
    msr  elr_el1, x4
    msr  spsr_el1, xzr
    eret

after_enabled:
    adr  x11, result_value
    ldr  x12, [x11]
    movz x13, #0x4                  // DZP=0, BS=4
    cmp  x12, x13
    b.ne fail2

    movz x0, #'D'
    str  x0, [x20]
    movz x0, #'\n'
    str  x0, [x20]
    brk  #0

fail1:
    movz x0, #'1'
    b out
fail2:
    movz x0, #'2'
    b out
fail3:
    movz x0, #'3'
    b out

out:
    str  x0, [x20]
    movz x0, #'F'
    str  x0, [x20]
    brk  #0

el0_read_disabled:
    mrs  x0, dczid_el0
    svc  #0x31

el0_read_enabled:
    mrs  x0, dczid_el0
    svc  #0x32

    .balign 2048
vectors:
    .space 0x400
sync_lower:
    b lower_sync_handler

lower_sync_handler:
    mrs  x21, esr_el1
    ubfx x22, x21, #26, #6
    movz x23, #0x15                 // SVC from AArch64
    cmp  x22, x23
    b.ne fail3

    adr  x24, result_value
    str  x0, [x24]
    and  x22, x21, #0xffff
    movz x23, #0x31
    cmp  x22, x23
    b.eq return_after_disabled
    movz x23, #0x32
    cmp  x22, x23
    b.ne fail3
    adr  x25, after_enabled
    b    return_to_el1

return_after_disabled:
    adr  x25, after_disabled

return_to_el1:
    movz x26, #0x5                  // EL1h
    msr  spsr_el1, x26
    msr  elr_el1, x25
    eret

    .balign 8
result_value:
    .quad 0
