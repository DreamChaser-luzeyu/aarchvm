    .text
    .global _start

    .macro CHECK_HANDLED value
    ldr  x12, [x21]
    movz x13, #\value
    cmp  x12, x13
    b.ne fail
    .endm

_start:
    movz x1, #0x0000
    movk x1, #0x0900, lsl #16      // UART
    movz x2, #0x0000
    movk x2, #0x0902, lsl #16      // PERF mailbox

    adr  x10, vectors
    msr  vbar_el1, x10

    adr  x21, handled
    str  xzr, [x21]

el1_gcspushm_insn:
    .inst 0xD50B7701                  // gcspushm x1
after_el1_gcspushm:
    CHECK_HANDLED 1

el1_gcsss1_insn:
    .inst 0xD50B7742                  // gcsss1 x2
after_el1_gcsss1:
    CHECK_HANDLED 2

el1_gcsss2_insn:
    .inst 0xD52B7763                  // gcsss2 x3
after_el1_gcsss2:
    CHECK_HANDLED 3

el1_gcspopm_insn:
    .inst 0xD52B7724                  // gcspopm x4
after_el1_gcspopm:
    CHECK_HANDLED 4

el1_gcspushx_insn:
    .inst 0xD508779F                  // gcspushx
after_el1_gcspushx:
    CHECK_HANDLED 5

el1_gcspopx_insn:
    .inst 0xD50877DF                  // gcspopx
after_el1_gcspopx:
    CHECK_HANDLED 6

el1_gcspopcx_insn:
    .inst 0xD50877BF                  // gcspopcx
after_el1_gcspopcx:
    CHECK_HANDLED 7

    adr  x3, el0_gcspushm_entry
    msr  elr_el1, x3
    msr  spsr_el1, xzr               // EL0t
    eret

after_el0_gcspushm:
    CHECK_HANDLED 8

    adr  x3, el0_gcsss1_entry
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret

after_el0_gcsss1:
    CHECK_HANDLED 9

    adr  x3, el0_gcsss2_entry
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret

after_el0_gcsss2:
    CHECK_HANDLED 10

    adr  x3, el0_gcspopm_entry
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret

after_el0_gcspopm:
    CHECK_HANDLED 11

    adr  x3, el0_gcspushx_entry
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret

after_el0_gcspushx:
    CHECK_HANDLED 12

    adr  x3, el0_gcspopx_entry
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret

after_el0_gcspopx:
    CHECK_HANDLED 13

    adr  x3, el0_gcspopcx_entry
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret

after_el0_gcspopcx:
    CHECK_HANDLED 14

    movz x0, #'Q'
    str  x0, [x1]
    movz x0, #'\n'
    str  x0, [x1]
    b    exit_ok

el0_gcspushm_entry:
el0_gcspushm_insn:
    .inst 0xD50B7701                  // gcspushm x1
    b    fail

el0_gcsss1_entry:
el0_gcsss1_insn:
    .inst 0xD50B7742                  // gcsss1 x2
    b    fail

el0_gcsss2_entry:
el0_gcsss2_insn:
    .inst 0xD52B7763                  // gcsss2 x3
    b    fail

el0_gcspopm_entry:
el0_gcspopm_insn:
    .inst 0xD52B7724                  // gcspopm x4
    b    fail

el0_gcspushx_entry:
el0_gcspushx_insn:
    .inst 0xD508779F                  // gcspushx
    b    fail

el0_gcspopx_entry:
el0_gcspopx_insn:
    .inst 0xD50877DF                  // gcspopx
    b    fail

el0_gcspopcx_entry:
el0_gcspopcx_insn:
    .inst 0xD50877BF                  // gcspopcx
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

    .macro HANDLE_CASE insn_label, after_label, value
    adr  x27, \insn_label
    cmp  x26, x27
    b.ne .Lhandle_skip\@
    adr  x28, handled
    movz x29, #\value
    str  x29, [x28]
    adr  x30, \after_label
    msr  elr_el1, x30
    eret
.Lhandle_skip\@:
    .endm

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

    HANDLE_CASE el1_gcspushm_insn, after_el1_gcspushm, 1
    HANDLE_CASE el1_gcsss1_insn, after_el1_gcsss1, 2
    HANDLE_CASE el1_gcsss2_insn, after_el1_gcsss2, 3
    HANDLE_CASE el1_gcspopm_insn, after_el1_gcspopm, 4
    HANDLE_CASE el1_gcspushx_insn, after_el1_gcspushx, 5
    HANDLE_CASE el1_gcspopx_insn, after_el1_gcspopx, 6
    HANDLE_CASE el1_gcspopcx_insn, after_el1_gcspopcx, 7
    adr  x27, el0_gcspushm_insn
    cmp  x26, x27
    b.eq handle_el0_gcspushm
    adr  x27, el0_gcsss1_insn
    cmp  x26, x27
    b.eq handle_el0_gcsss1
    adr  x27, el0_gcsss2_insn
    cmp  x26, x27
    b.eq handle_el0_gcsss2
    adr  x27, el0_gcspopm_insn
    cmp  x26, x27
    b.eq handle_el0_gcspopm
    adr  x27, el0_gcspushx_insn
    cmp  x26, x27
    b.eq handle_el0_gcspushx
    adr  x27, el0_gcspopx_insn
    cmp  x26, x27
    b.eq handle_el0_gcspopx
    adr  x27, el0_gcspopcx_insn
    cmp  x26, x27
    b.eq handle_el0_gcspopcx

fail_from_handler:
    b fail

handle_el0_gcspushm:
    adr  x28, handled
    movz x29, #8
    str  x29, [x28]
    adr  x30, after_el0_gcspushm
    b    return_to_el1

handle_el0_gcsss1:
    adr  x28, handled
    movz x29, #9
    str  x29, [x28]
    adr  x30, after_el0_gcsss1
    b    return_to_el1

handle_el0_gcsss2:
    adr  x28, handled
    movz x29, #10
    str  x29, [x28]
    adr  x30, after_el0_gcsss2
    b    return_to_el1

handle_el0_gcspopm:
    adr  x28, handled
    movz x29, #11
    str  x29, [x28]
    adr  x30, after_el0_gcspopm
    b    return_to_el1

handle_el0_gcspushx:
    adr  x28, handled
    movz x29, #12
    str  x29, [x28]
    adr  x30, after_el0_gcspushx
    b    return_to_el1

handle_el0_gcspopx:
    adr  x28, handled
    movz x29, #13
    str  x29, [x28]
    adr  x30, after_el0_gcspopx
    b    return_to_el1

handle_el0_gcspopcx:
    adr  x28, handled
    movz x29, #14
    str  x29, [x28]
    adr  x30, after_el0_gcspopcx

return_to_el1:
    movz x29, #0x5
    msr  spsr_el1, x29
    msr  elr_el1, x30
    eret

    .bss
    .balign 8
handled:
    .skip 8
