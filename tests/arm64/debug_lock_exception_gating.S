    .text
    .global _start

    .macro LOAD64 reg, imm0, imm1, imm2, imm3
    movz \reg, #\imm0
    movk \reg, #\imm1, lsl #16
    movk \reg, #\imm2, lsl #32
    movk \reg, #\imm3, lsl #48
    .endm

_start:
    movz x1, #0x0000
    movk x1, #0x0900, lsl #16

    adr  x2, vectors
    msr  vbar_el1, x2

    adr  x3, handled
    str  xzr, [x3]

    // Case 1: OS Lock suppresses EL1 hardware breakpoint exceptions.
    movz x20, #0
    movz x0, #1
    msr  oslar_el1, x0
    isb
    adr  x4, case1_break_insn
    msr  dbgbvr0_el1, x4
    movz x5, #0x1e3              // BAS=0xf, execute, EL1, enabled
    msr  dbgbcr0_el1, x5
    movz x5, #0xa000             // KDE|MDE
    msr  mdscr_el1, x5
case1_break_insn:
    movz x20, #0x1234
after_case1:
    ldr  x4, [x3]
    cbnz x4, fail1
    movz x5, #0x1234
    cmp  x20, x5
    b.ne fail2
    msr  dbgbcr0_el1, xzr
    msr  oslar_el1, xzr
    isb

    // Case 2: OS Double Lock suppresses EL1 watchpoint exceptions.
    str  xzr, [x3]
    adr  x4, watch_store_data
    msr  dbgwvr0_el1, x4
    movz x5, #0x1ff3             // BAS=0xff, store, EL1, enabled
    msr  dbgwcr0_el1, x5
    movz x5, #0xa000             // KDE|MDE
    msr  mdscr_el1, x5
    movz x0, #1
    msr  osdlr_el1, x0
    isb
    LOAD64 x24, 0xabcd, 0xef01, 0x2345, 0x6789
    adr  x25, watch_store_data
case2_store_insn:
    str  x24, [x25]
after_case2:
    ldr  x4, [x3]
    cbnz x4, fail3
    adr  x4, watch_store_data
    ldr  x5, [x4]
    cmp  x5, x24
    b.ne fail4
    msr  dbgwcr0_el1, xzr
    msr  osdlr_el1, xzr
    isb

    // Case 3: CORENPDRQ=1 disables the OS Double Lock, so breakpoints work.
    str  xzr, [x3]
    movz x22, #0
    movz x0, #1
    msr  osdlr_el1, x0
    msr  dbgprcr_el1, x0         // CORENPDRQ=1
    isb
    adr  x4, case3_break_insn
    msr  dbgbvr0_el1, x4
    movz x5, #0x1e3
    msr  dbgbcr0_el1, x5
    movz x5, #0xa000
    msr  mdscr_el1, x5
case3_break_insn:
    movz x22, #0x3333
after_case3:
    ldr  x4, [x3]
    movz x5, #1
    cmp  x4, x5
    b.ne fail5
    cbnz x22, fail6
    msr  dbgbcr0_el1, xzr
    msr  dbgprcr_el1, xzr
    msr  osdlr_el1, xzr
    isb

    // Case 4: OS Lock suppresses software step exceptions.
    str  xzr, [x3]
    movz x21, #0
    movz x0, #1
    msr  oslar_el1, x0
    isb
    msr  daifset, #8
    movz x0, #0x2001            // KDE|SS
    msr  mdscr_el1, x0
    msr  daifclr, #8
case4_step_target:
    movz x21, #0x5678
after_case4:
    ldr  x4, [x3]
    cbnz x4, fail7
    movz x5, #0x5678
    cmp  x21, x5
    b.ne fail8
    msr  mdscr_el1, xzr
    msr  oslar_el1, xzr
    isb

    // Case 5: BRK instruction exceptions are not masked by OS Lock.
    str  xzr, [x3]
    movz x0, #1
    msr  oslar_el1, x0
    isb
case5_brk_insn:
    brk  #0x321
after_case5:
    ldr  x4, [x3]
    movz x5, #2
    cmp  x4, x5
    b.ne fail9
    msr  oslar_el1, xzr
    isb

    movz x0, #'L'
    str  x0, [x1]
    movz x0, #'\n'
    str  x0, [x1]
success_spin:
    b    success_spin

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
    b out
fail5:
    movz x0, #'5'
    b out
fail6:
    movz x0, #'6'
    b out
fail7:
    movz x0, #'7'
    b out
fail8:
    movz x0, #'8'
    b out
fail9:
    movz x0, #'9'
    b out
failA:
    movz x0, #'A'
    b out
failB:
    movz x0, #'B'
    b out
failC:
    movz x0, #'C'
    b out
failD:
    movz x0, #'D'
    b out

out:
    str  x0, [x1]
out_spin:
    b    out_spin

    .balign 8
handled:
    .quad 0
watch_store_data:
    .quad 0x1122334455667788

    .balign 2048
vectors:
    .space 0x200
sync_spx:
    b sync_spx_handler
    .space 0x1fc
sync_lower:
    b sync_lower_handler

sync_spx_handler:
    mrs  x10, esr_el1
    mrs  x11, elr_el1
    mrs  x12, far_el1
    cbnz x12, fail_from_handler

    adr  x13, case3_break_insn
    cmp  x11, x13
    b.eq handle_case3_break

    adr  x13, case5_brk_insn
    cmp  x11, x13
    b.eq handle_case5_brk

    b fail_from_handler

handle_case3_break:
    ubfx x14, x10, #26, #6
    movz x15, #0x31
    cmp  x14, x15
    b.ne fail_from_handler
    adr  x16, handled
    movz x17, #1
    str  x17, [x16]
    adr  x18, after_case3
    msr  elr_el1, x18
    eret

handle_case5_brk:
    ubfx x14, x10, #26, #6
    movz x15, #0x3c
    cmp  x14, x15
    b.ne fail_from_handler
    and  x14, x10, #0xffff
    movz x15, #0x321
    cmp  x14, x15
    b.ne fail_from_handler
    adr  x16, handled
    movz x17, #2
    str  x17, [x16]
    adr  x18, after_case5
    msr  elr_el1, x18
    eret

sync_lower_handler:
    b fail_from_handler

fail_from_handler:
    movz x0, #'X'
    b out
