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

dcps1_insn:
    dcps1 #0x1234

after_dcps1:
    ldr  x12, [x11]
    movz x13, #1
    cmp  x12, x13
    b.ne fail

dcps2_insn:
    dcps2 #0x2345

after_dcps2:
    ldr  x12, [x11]
    movz x13, #2
    cmp  x12, x13
    b.ne fail

dcps3_insn:
    dcps3 #0x3456

after_dcps3:
    ldr  x12, [x11]
    movz x13, #3
    cmp  x12, x13
    b.ne fail

drps_insn:
    drps

after_drps:
    ldr  x12, [x11]
    movz x13, #4
    cmp  x12, x13
    b.ne fail

    movz x0, #'D'
    str  x0, [x1]
    movz x0, #'\n'
    str  x0, [x1]
    b    exit_ok

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

    adr  x25, dcps1_insn
    cmp  x24, x25
    b.eq handle_dcps1
    adr  x25, dcps2_insn
    cmp  x24, x25
    b.eq handle_dcps2
    adr  x25, dcps3_insn
    cmp  x24, x25
    b.eq handle_dcps3
    adr  x25, drps_insn
    cmp  x24, x25
    b.eq handle_drps
    b fail_from_handler

handle_dcps1:
    adr  x26, handled
    movz x27, #1
    str  x27, [x26]
    adr  x28, after_dcps1
    b    return_to_el1

handle_dcps2:
    adr  x26, handled
    movz x27, #2
    str  x27, [x26]
    adr  x28, after_dcps2
    b    return_to_el1

handle_dcps3:
    adr  x26, handled
    movz x27, #3
    str  x27, [x26]
    adr  x28, after_dcps3
    b    return_to_el1

handle_drps:
    adr  x26, handled
    movz x27, #4
    str  x27, [x26]
    adr  x28, after_drps

return_to_el1:
    movz x29, #0x5                // EL1h
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
