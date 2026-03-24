    .text
    .global _start
_start:
    movz x1, #0x0000
    movk x1, #0x0900, lsl #16      // UART
    movz x28, #0x0000
    movk x28, #0x0902, lsl #16     // PERF mailbox

    adr  x10, vectors
    msr  vbar_el1, x10

    ldr  x16, expected_x16
    ldr  x17, expected_x17

    .inst 0xD50320DF               // DGH
    .inst 0xD503221F               // ESB
    .inst 0xD503225F               // TSB CSYNC
    .inst 0xD503227F               // GCSB DSYNC
    .inst 0xD50322DF               // CLRBHB
    .inst 0xD503245F               // BTI c
    .inst 0xD50324DF               // BTI jc
    .inst 0xD503251F               // CHKFEAT X16
    .inst 0xD503261F               // STSHH KEEP
    .inst 0xD503263F               // STSHH STRM

    ldr  x20, expected_x16
    cmp  x16, x20
    b.ne fail
    ldr  x20, expected_x17
    cmp  x17, x20
    b.ne fail

    adr  x3, el0_entry
    msr  elr_el1, x3
    msr  spsr_el1, xzr             // EL0t
    eret

after_el0:
    ldr  x20, expected_x16
    cmp  x16, x20
    b.ne fail
    ldr  x20, expected_x17
    cmp  x17, x20
    b.ne fail

    movz x0, #'K'
    str  x0, [x1]
    movz x0, #'\n'
    str  x0, [x1]
    b    exit_ok

el0_entry:
    .inst 0xD50320DF               // DGH
    .inst 0xD503221F               // ESB
    .inst 0xD503225F               // TSB CSYNC
    .inst 0xD503227F               // GCSB DSYNC
    .inst 0xD50322DF               // CLRBHB
    .inst 0xD503245F               // BTI c
    .inst 0xD50324DF               // BTI jc
    .inst 0xD503251F               // CHKFEAT X16
    .inst 0xD503261F               // STSHH KEEP
    .inst 0xD503263F               // STSHH STRM
    svc  #0x55
    b    fail

fail:
    movz x0, #'F'
    str  x0, [x1]
    movz x0, #'\n'
    str  x0, [x1]

exit_ok:
    movz x0, #3
    str  x0, [x28, #0x08]
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
    cmp  x23, #0x15               // SVC from EL0 only
    b.ne fail_from_handler
    and  x24, x22, #0xFFFF
    movz x25, #0x55
    cmp  x24, x25
    b.ne fail_from_handler
    movz x26, #0x5                // EL1h
    msr  spsr_el1, x26
    adr  x27, after_el0
    msr  elr_el1, x27
    eret

fail_from_handler:
    movz x26, #0x5
    msr  spsr_el1, x26
    adr  x27, fail
    msr  elr_el1, x27
    eret

    .data
    .balign 8
expected_x16:
    .quad 0x0123456789abcdef
expected_x17:
    .quad 0xfedcba9876543210
