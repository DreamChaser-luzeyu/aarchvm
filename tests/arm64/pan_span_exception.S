    .arch armv8.1-a
    .text
    .global _start
_start:
    movz x20, #0x0000
    movk x20, #0x0900, lsl #16      // UART

    adr  x10, vectors
    msr  vbar_el1, x10

    // Case 1: SPAN=0 forces PAN=1 on exception entry to EL1.
    mrs  x11, sctlr_el1
    movz x12, #0x80, lsl #16        // bit 23
    bic  x11, x11, x12
    msr  sctlr_el1, x11
    isb
    msr  pan, #0
    movz x27, #1
    adr  x0, el0_case1
    msr  elr_el1, x0
    msr  spsr_el1, xzr              // EL0t
    eret

after_case1:
    mrs  x0, pan
    cbnz x0, fail1                  // exception return must restore PAN=0

    // Case 2: SPAN=1 leaves PAN unchanged on exception entry to EL1.
    mrs  x11, sctlr_el1
    movz x12, #0x80, lsl #16        // bit 23
    orr  x11, x11, x12
    msr  sctlr_el1, x11
    isb
    msr  pan, #0
    movz x27, #2
    adr  x0, el0_case2
    msr  elr_el1, x0
    msr  spsr_el1, xzr              // EL0t
    eret

after_case2:
    mrs  x0, pan
    cbnz x0, fail2

pass:
    movz x0, #'S'
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
fail4:
    movz x0, #'4'
out:
    str  x0, [x20]
    brk  #0

el0_case1:
    svc  #0
    b fail3

el0_case2:
    svc  #0
    b fail4

    .balign 2048
vectors:
    .space 0x200
sync_spx:
    b fail3
    .space 0x1fc
sync_lower:
    mrs  x1, esr_el1
    ubfx x2, x1, #26, #6
    cmp  x2, #0x15                 // SVC from AArch64
    b.ne fail_from_handler
    mrs  x3, pan
    cmp  x27, #1
    b.eq check_span_zero
    cmp  x27, #2
    b.eq check_span_one
    b fail_from_handler

check_span_zero:
    cmp  x3, #1
    b.ne fail_from_handler
    adr  x4, after_case1
    b return_el1

check_span_one:
    cbnz x3, fail_from_handler
    adr  x4, after_case2

return_el1:
    movz x5, #0x5                  // EL1h
    msr  spsr_el1, x5
    msr  elr_el1, x4
    eret

fail_from_handler:
    adr  x4, fail4
    movz x5, #0x5
    msr  spsr_el1, x5
    msr  elr_el1, x4
    eret
