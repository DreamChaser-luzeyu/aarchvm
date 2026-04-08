    .text
    .global _start
_start:
    movz x1, #0
    movk x1, #0x0900, lsl #16      // UART

    adr  x2, vectors
    msr  vbar_el1, x2

    adr  x3, handled
    str  xzr, [x3]

    // Breakpoint exceptions require OS Lock/OS Double Lock to be open.
    msr  oslar_el1, xzr
    msr  osdlr_el1, xzr
    msr  dbgprcr_el1, xzr
    isb

    adr  x4, break_insn
    msr  dbgbvr0_el1, x4
    movz x5, #0x003               // BAS programmed as 0; AArch64-only model must treat BAS[3:0] as RES1.
    msr  dbgbcr0_el1, x5
    mrs  x6, dbgbcr0_el1
    ubfx x7, x6, #5, #4
    movz x8, #0xF
    cmp  x7, x8
    b.ne fail

    movz x5, #0xA000              // KDE|MDE
    msr  mdscr_el1, x5
    isb

    movz x20, #0
break_insn:
    movz x20, #0x1234
after_break:
    ldr  x4, [x3]
    movz x5, #1
    cmp  x4, x5
    b.ne fail
    cbnz x20, fail

    movz x0, #'R'
    str  x0, [x1]
    movz x0, #'\n'
    str  x0, [x1]
    brk  #0

fail:
    movz x0, #'F'
    str  x0, [x1]
    brk  #0

    .balign 2048
vectors:
    .space 0x200
sync_spx:
    b sync_spx_handler

sync_spx_handler:
    mrs  x10, esr_el1
    mrs  x11, elr_el1
    mrs  x12, far_el1

    ubfx x13, x10, #26, #6
    cmp  x13, #0x31
    b.ne fail_from_handler
    and  x13, x10, #0x1ffffff
    movz x14, #0x22
    cmp  x13, x14
    b.ne fail_from_handler
    cbnz x12, fail_from_handler
    adr  x13, break_insn
    cmp  x11, x13
    b.ne fail_from_handler
    cbnz x20, fail_from_handler

    adr  x13, handled
    movz x14, #1
    str  x14, [x13]
    msr  dbgbcr0_el1, xzr
    msr  mdscr_el1, xzr
    adr  x13, after_break
    msr  elr_el1, x13
    eret

fail_from_handler:
    adr  x13, fail
    msr  elr_el1, x13
    eret

    .data
    .balign 8
handled:
    .quad 0
