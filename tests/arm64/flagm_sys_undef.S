    .text
    .global _start
_start:
    movz x1, #0x0000
    movk x1, #0x0900, lsl #16      // UART
    movz x2, #0x0000
    movk x2, #0x0902, lsl #16      // PERF mailbox

    adr  x10, vectors
    msr  vbar_el1, x10

    adr  x11, handled
    str  xzr, [x11]

el1_cfinv_insn:
    .inst 0xD500401F                  // cfinv

after_el1_cfinv:
    ldr  x12, [x11]
    movz x13, #1
    cmp  x12, x13
    b.ne fail

el1_axflag_insn:
    .inst 0xD500405F                  // axflag

after_el1_axflag:
    ldr  x12, [x11]
    movz x13, #2
    cmp  x12, x13
    b.ne fail

el1_xaflag_insn:
    .inst 0xD500403F                  // xaflag

after_el1_xaflag:
    ldr  x12, [x11]
    movz x13, #3
    cmp  x12, x13
    b.ne fail

    adr  x3, el0_cfinv_entry
    msr  elr_el1, x3
    msr  spsr_el1, xzr               // EL0t
    eret

after_el0_cfinv:
    ldr  x12, [x11]
    movz x13, #4
    cmp  x12, x13
    b.ne fail

    adr  x3, el0_axflag_entry
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret

after_el0_axflag:
    ldr  x12, [x11]
    movz x13, #5
    cmp  x12, x13
    b.ne fail

    adr  x3, el0_xaflag_entry
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret

after_el0_xaflag:
    ldr  x12, [x11]
    movz x13, #6
    cmp  x12, x13
    b.ne fail

    movz x0, #'G'
    str  x0, [x1]
    movz x0, #'\n'
    str  x0, [x1]
    b    exit_ok

el0_cfinv_entry:
el0_cfinv_insn:
    .inst 0xD500401F                  // cfinv
    b    fail

el0_axflag_entry:
el0_axflag_insn:
    .inst 0xD500405F                  // axflag
    b    fail

el0_xaflag_entry:
el0_xaflag_insn:
    .inst 0xD500403F                  // xaflag
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
    mrs  x20, esr_el1
    ubfx x21, x20, #26, #6
    cbnz x21, fail_from_handler
    tbz  x20, #25, fail_from_handler
    and  x22, x20, #0x1ffffff
    cbnz x22, fail_from_handler

    mrs  x23, far_el1
    cbnz x23, fail_from_handler
    mrs  x24, elr_el1

    adr  x25, el1_cfinv_insn
    cmp  x24, x25
    b.eq handle_el1_cfinv
    adr  x25, el1_axflag_insn
    cmp  x24, x25
    b.eq handle_el1_axflag
    adr  x25, el1_xaflag_insn
    cmp  x24, x25
    b.eq handle_el1_xaflag
    adr  x25, el0_cfinv_insn
    cmp  x24, x25
    b.eq handle_el0_cfinv
    adr  x25, el0_axflag_insn
    cmp  x24, x25
    b.eq handle_el0_axflag
    adr  x25, el0_xaflag_insn
    cmp  x24, x25
    b.eq handle_el0_xaflag
    b fail_from_handler

handle_el1_cfinv:
    adr  x26, handled
    movz x27, #1
    str  x27, [x26]
    adr  x28, after_el1_cfinv
    msr  elr_el1, x28
    eret

handle_el1_axflag:
    adr  x26, handled
    movz x27, #2
    str  x27, [x26]
    adr  x28, after_el1_axflag
    msr  elr_el1, x28
    eret

handle_el1_xaflag:
    adr  x26, handled
    movz x27, #3
    str  x27, [x26]
    adr  x28, after_el1_xaflag
    msr  elr_el1, x28
    eret

handle_el0_cfinv:
    adr  x26, handled
    movz x27, #4
    str  x27, [x26]
    adr  x28, after_el0_cfinv
    b    return_to_el1

handle_el0_axflag:
    adr  x26, handled
    movz x27, #5
    str  x27, [x26]
    adr  x28, after_el0_axflag
    b    return_to_el1

handle_el0_xaflag:
    adr  x26, handled
    movz x27, #6
    str  x27, [x26]
    adr  x28, after_el0_xaflag

return_to_el1:
    movz x29, #0x5                  // EL1h
    msr  spsr_el1, x29
    msr  elr_el1, x28
    eret

fail_from_handler:
    adr  x28, fail
    movz x29, #0x5
    msr  spsr_el1, x29
    msr  elr_el1, x28
    eret

    .data
    .balign 8
handled:
    .quad 0
