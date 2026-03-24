    .text
    .global _start
_start:
    movz x1, #0x0000
    movk x1, #0x0900, lsl #16      // UART
    movz x2, #0x0000
    movk x2, #0x0902, lsl #16      // PERF mailbox

    adr  x10, vectors
    msr  vbar_el1, x10

    adr  x11, fail_seen
    str  xzr, [x11]

el1_pacm_insn:
    .inst 0xD50324FF                  // pacm

after_el1_pacm:
    ldr  x12, [x11]
    cbnz x12, fail

    adr  x3, el0_pacm_entry
    msr  elr_el1, x3
    msr  spsr_el1, xzr               // EL0t
    eret

after_el0_pacm:
    ldr  x12, [x11]
    cbnz x12, fail

    movz x0, #'M'
    str  x0, [x1]
    movz x0, #'\n'
    str  x0, [x1]
    b    exit_ok

el0_pacm_entry:
el0_pacm_insn:
    .inst 0xD50324FF                  // pacm
el0_svc_insn:
    svc  #0
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
    mrs  x24, elr_el1
    cmp  x21, #0x15                  // SVC from EL0
    b.eq handle_el0_svc

    cmp  x21, #0x00                  // unexpected UNDEFINED
    b.ne fail_from_handler
    tbz  x20, #25, fail_from_handler
    and  x22, x20, #0x1ffffff
    cbnz x22, fail_from_handler
    mrs  x23, far_el1
    cbnz x23, fail_from_handler

    adr  x25, el1_pacm_insn
    cmp  x24, x25
    b.eq unexpected_pacm_undef
    adr  x25, el0_pacm_insn
    cmp  x24, x25
    b.eq unexpected_pacm_undef
    b fail_from_handler

handle_el0_svc:
    adr  x28, after_el0_pacm
    movz x29, #0x5                  // EL1h
    msr  spsr_el1, x29
    msr  elr_el1, x28
    eret

unexpected_pacm_undef:
    adr  x26, fail_seen
    movz x27, #1
    str  x27, [x26]
    b fail_from_handler

fail_from_handler:
    adr  x28, fail
    movz x29, #0x5
    msr  spsr_el1, x29
    msr  elr_el1, x28
    eret

    .data
    .balign 8
fail_seen:
    .quad 0
