    .text
    .global _start
_start:
    movz x1, #0x0000
    movk x1, #0x0900, lsl #16      // UART
    movz x2, #0x0000
    movk x2, #0x0902, lsl #16      // PERF mailbox

    adr  x10, vectors
    msr  vbar_el1, x10

    movz x5, #0x1234
    movz x6, #0x00f0
    movz x7, #0x8001

el1_rmif_setup:
    adr  x9, after_el1_rmif
el1_rmif_insn:
    .inst 0xBA0684A9                // RMIF X5, #13, #9
    b    fail
after_el1_rmif:
el1_setf8_setup:
    adr  x9, after_el1_setf8
el1_setf8_insn:
    .inst 0x3A0008CD                // SETF8 W6
    b    fail
after_el1_setf8:
el1_setf16_setup:
    adr  x9, after_el1_setf16
el1_setf16_insn:
    .inst 0x3A0048ED                // SETF16 W7
    b    fail
after_el1_setf16:
    adr  x3, el0_rmif_entry
    msr  elr_el1, x3
    msr  spsr_el1, xzr              // EL0t
    eret

after_el0_rmif:
    adr  x3, el0_setf8_entry
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret

after_el0_setf8:
    adr  x3, el0_setf16_entry
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret

after_el0_setf16:
    movz x0, #'m'
    str  x0, [x1]
    movz x0, #'\n'
    str  x0, [x1]
    b    exit_ok

el0_rmif_entry:
el0_rmif_insn:
    .inst 0xBA0684A9                // RMIF X5, #13, #9
    b    fail

el0_setf8_entry:
el0_setf8_insn:
    .inst 0x3A0008CD                // SETF8 W6
    b    fail

el0_setf16_entry:
el0_setf16_insn:
    .inst 0x3A0048ED                // SETF16 W7
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
    mrs  x21, esr_el1
    ubfx x22, x21, #26, #6
    cbnz x22, fail_from_handler
    tbz  x21, #25, fail_from_handler
    and  x23, x21, #0x1ffffff
    cbnz x23, fail_from_handler
    mrs  x24, far_el1
    cbnz x24, fail_from_handler
    mrs  x25, elr_el1

    adr  x26, el1_rmif_insn
    cmp  x25, x26
    b.eq resume_el1
    adr  x26, el1_setf8_insn
    cmp  x25, x26
    b.eq resume_el1
    adr  x26, el1_setf16_insn
    cmp  x25, x26
    b.eq resume_el1
    adr  x26, el0_rmif_insn
    cmp  x25, x26
    b.eq resume_el0_rmif
    adr  x26, el0_setf8_insn
    cmp  x25, x26
    b.eq resume_el0_setf8
    adr  x26, el0_setf16_insn
    cmp  x25, x26
    b.eq resume_el0_setf16
    b fail_from_handler

resume_el1:
    msr  elr_el1, x9
    eret

resume_el0_rmif:
    adr  x27, after_el0_rmif
    b    return_to_el1

resume_el0_setf8:
    adr  x27, after_el0_setf8
    b    return_to_el1

resume_el0_setf16:
    adr  x27, after_el0_setf16

return_to_el1:
    movz x28, #0x5                  // EL1h
    msr  spsr_el1, x28
    msr  elr_el1, x27
    eret

fail_from_handler:
    adr  x27, fail
    movz x28, #0x5
    msr  spsr_el1, x28
    msr  elr_el1, x27
    eret
