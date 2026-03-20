    .text
    .global _start
_start:
    movz x1, #0x0000
    movk x1, #0x0900, lsl #16      // UART
    movz x2, #0x0000
    movk x2, #0x0902, lsl #16      // PERF mailbox

    adr  x10, vectors
    msr  vbar_el1, x10

    adr  x21, handled
    str  xzr, [x21]

el1_sb_insn:
    .inst 0xD50330FF                  // sb
after_el1_sb:
    ldr  x12, [x21]
    movz x13, #1
    cmp  x12, x13
    b.ne fail

el1_wfet_insn:
    .inst 0xD503100F                  // wfet x15
after_el1_wfet:
    ldr  x12, [x21]
    movz x13, #2
    cmp  x12, x13
    b.ne fail

el1_wfit_insn:
    .inst 0xD5031030                  // wfit x16
after_el1_wfit:
    ldr  x12, [x21]
    movz x13, #3
    cmp  x12, x13
    b.ne fail

el1_cfp_insn:
    .inst 0xD50B7391                  // cfp rctx, x17
after_el1_cfp:
    ldr  x12, [x21]
    movz x13, #4
    cmp  x12, x13
    b.ne fail

el1_dvp_insn:
    .inst 0xD50B73B2                  // dvp rctx, x18
after_el1_dvp:
    ldr  x12, [x21]
    movz x13, #5
    cmp  x12, x13
    b.ne fail

el1_cosp_insn:
    .inst 0xD50B73D3                  // cosp rctx, x19
after_el1_cosp:
    ldr  x12, [x21]
    movz x13, #6
    cmp  x12, x13
    b.ne fail

el1_cpp_insn:
    .inst 0xD50B73F4                  // cpp rctx, x20
after_el1_cpp:
    ldr  x12, [x21]
    movz x13, #7
    cmp  x12, x13
    b.ne fail

    adr  x3, el0_sb_entry
    msr  elr_el1, x3
    msr  spsr_el1, xzr               // EL0t
    eret

after_el0_sb:
    ldr  x12, [x21]
    movz x13, #8
    cmp  x12, x13
    b.ne fail

    adr  x3, el0_wfet_entry
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret

after_el0_wfet:
    ldr  x12, [x21]
    movz x13, #9
    cmp  x12, x13
    b.ne fail

    adr  x3, el0_wfit_entry
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret

after_el0_wfit:
    ldr  x12, [x21]
    movz x13, #10
    cmp  x12, x13
    b.ne fail

    adr  x3, el0_cfp_entry
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret

after_el0_cfp:
    ldr  x12, [x21]
    movz x13, #11
    cmp  x12, x13
    b.ne fail

    adr  x3, el0_dvp_entry
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret

after_el0_dvp:
    ldr  x12, [x21]
    movz x13, #12
    cmp  x12, x13
    b.ne fail

    adr  x3, el0_cosp_entry
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret

after_el0_cosp:
    ldr  x12, [x21]
    movz x13, #13
    cmp  x12, x13
    b.ne fail

    adr  x3, el0_cpp_entry
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret

after_el0_cpp:
    ldr  x12, [x21]
    movz x13, #14
    cmp  x12, x13
    b.ne fail

    movz x0, #'W'
    str  x0, [x1]
    movz x0, #'\n'
    str  x0, [x1]
    b    exit_ok

el0_sb_entry:
el0_sb_insn:
    .inst 0xD50330FF                  // sb
    b    fail

el0_wfet_entry:
el0_wfet_insn:
    .inst 0xD503100F                  // wfet x15
    b    fail

el0_wfit_entry:
el0_wfit_insn:
    .inst 0xD5031030                  // wfit x16
    b    fail

el0_cfp_entry:
el0_cfp_insn:
    .inst 0xD50B7391                  // cfp rctx, x17
    b    fail

el0_dvp_entry:
el0_dvp_insn:
    .inst 0xD50B73B2                  // dvp rctx, x18
    b    fail

el0_cosp_entry:
el0_cosp_insn:
    .inst 0xD50B73D3                  // cosp rctx, x19
    b    fail

el0_cpp_entry:
el0_cpp_insn:
    .inst 0xD50B73F4                  // cpp rctx, x20
    b    fail

fail:
    movz x0, #'F'
    str  x0, [x1]
    movz x0, #'\n'
    str  x0, [x1]

exit_ok:
    movz x0, #3
    str  x0, [x2, #0x08]
1:
    wfi
    b    1b

    .balign 2048
vectors:
    .space 0x200
sync_spx:
    b sync_handler
    .space 0x1fc
sync_lower:
    b sync_handler

sync_handler:
    mrs  x22, esr_el1
    ubfx x23, x22, #26, #6
    cbnz x23, fail_from_handler
    tbz  x22, #25, fail_from_handler
    and  x24, x22, #0x1ffffff
    cbnz x24, fail_from_handler

    mrs  x25, far_el1
    cbnz x25, fail_from_handler
    mrs  x26, elr_el1

    adr  x27, el1_sb_insn
    cmp  x26, x27
    b.eq handle_el1_sb
    adr  x27, el1_wfet_insn
    cmp  x26, x27
    b.eq handle_el1_wfet
    adr  x27, el1_wfit_insn
    cmp  x26, x27
    b.eq handle_el1_wfit
    adr  x27, el1_cfp_insn
    cmp  x26, x27
    b.eq handle_el1_cfp
    adr  x27, el1_dvp_insn
    cmp  x26, x27
    b.eq handle_el1_dvp
    adr  x27, el1_cosp_insn
    cmp  x26, x27
    b.eq handle_el1_cosp
    adr  x27, el1_cpp_insn
    cmp  x26, x27
    b.eq handle_el1_cpp
    adr  x27, el0_sb_insn
    cmp  x26, x27
    b.eq handle_el0_sb
    adr  x27, el0_wfet_insn
    cmp  x26, x27
    b.eq handle_el0_wfet
    adr  x27, el0_wfit_insn
    cmp  x26, x27
    b.eq handle_el0_wfit
    adr  x27, el0_cfp_insn
    cmp  x26, x27
    b.eq handle_el0_cfp
    adr  x27, el0_dvp_insn
    cmp  x26, x27
    b.eq handle_el0_dvp
    adr  x27, el0_cosp_insn
    cmp  x26, x27
    b.eq handle_el0_cosp
    adr  x27, el0_cpp_insn
    cmp  x26, x27
    b.eq handle_el0_cpp
    b    fail_from_handler

handle_el1_sb:
    adr  x28, handled
    movz x29, #1
    str  x29, [x28]
    adr  x30, after_el1_sb
    msr  elr_el1, x30
    eret

handle_el1_wfet:
    adr  x28, handled
    movz x29, #2
    str  x29, [x28]
    adr  x30, after_el1_wfet
    msr  elr_el1, x30
    eret

handle_el1_wfit:
    adr  x28, handled
    movz x29, #3
    str  x29, [x28]
    adr  x30, after_el1_wfit
    msr  elr_el1, x30
    eret

handle_el1_cfp:
    adr  x28, handled
    movz x29, #4
    str  x29, [x28]
    adr  x30, after_el1_cfp
    msr  elr_el1, x30
    eret

handle_el1_dvp:
    adr  x28, handled
    movz x29, #5
    str  x29, [x28]
    adr  x30, after_el1_dvp
    msr  elr_el1, x30
    eret

handle_el1_cosp:
    adr  x28, handled
    movz x29, #6
    str  x29, [x28]
    adr  x30, after_el1_cosp
    msr  elr_el1, x30
    eret

handle_el1_cpp:
    adr  x28, handled
    movz x29, #7
    str  x29, [x28]
    adr  x30, after_el1_cpp
    msr  elr_el1, x30
    eret

handle_el0_sb:
    adr  x28, handled
    movz x29, #8
    str  x29, [x28]
    adr  x30, after_el0_sb
    b    return_to_el1

handle_el0_wfet:
    adr  x28, handled
    movz x29, #9
    str  x29, [x28]
    adr  x30, after_el0_wfet
    b    return_to_el1

handle_el0_wfit:
    adr  x28, handled
    movz x29, #10
    str  x29, [x28]
    adr  x30, after_el0_wfit
    b    return_to_el1

handle_el0_cfp:
    adr  x28, handled
    movz x29, #11
    str  x29, [x28]
    adr  x30, after_el0_cfp
    b    return_to_el1

handle_el0_dvp:
    adr  x28, handled
    movz x29, #12
    str  x29, [x28]
    adr  x30, after_el0_dvp
    b    return_to_el1

handle_el0_cosp:
    adr  x28, handled
    movz x29, #13
    str  x29, [x28]
    adr  x30, after_el0_cosp
    b    return_to_el1

handle_el0_cpp:
    adr  x28, handled
    movz x29, #14
    str  x29, [x28]
    adr  x30, after_el0_cpp

return_to_el1:
    movz x29, #0x5                  // EL1h
    msr  spsr_el1, x29
    msr  elr_el1, x30
    eret

fail_from_handler:
    adr  x30, fail
    movz x29, #0x5
    msr  spsr_el1, x29
    msr  elr_el1, x30
    eret

    .data
    .balign 8
handled:
    .quad 0
