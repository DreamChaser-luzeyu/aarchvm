    .text
    .global _start

    .macro CHECK_HANDLED value
    ldr  x12, [x21]
    movz x13, #\value
    cmp  x12, x13
    b.ne fail
    .endm

    .macro LOAD_EXPECT reg, imm0, imm1, imm2, imm3
    movz \reg, #\imm0
    movk \reg, #\imm1, lsl #16
    movk \reg, #\imm2, lsl #32
    movk \reg, #\imm3, lsl #48
    .endm

    .macro HANDLE_EL1_CASE insn_label, after_label, value, imm0, imm1, imm2, imm3
    adr  x27, \insn_label
    cmp  x26, x27
    b.ne .Lskip_el1\@
    LOAD_EXPECT x24, \imm0, \imm1, \imm2, \imm3
    cmp  x0, x24
    b.ne fail
    adr  x28, handled
    movz x29, #\value
    str  x29, [x28]
    adr  x30, \after_label
    msr  elr_el1, x30
    eret
.Lskip_el1\@:
    .endm

    .macro HANDLE_EL0_CASE insn_label, after_label, value, imm0, imm1, imm2, imm3
    adr  x27, \insn_label
    cmp  x26, x27
    b.ne .Lskip_el0\@
    LOAD_EXPECT x24, \imm0, \imm1, \imm2, \imm3
    cmp  x0, x24
    b.ne fail
    adr  x28, handled
    movz x29, #\value
    str  x29, [x28]
    adr  x30, \after_label
    b    return_to_el1
.Lskip_el0\@:
    .endm

_start:
    movz x15, #0x0000
    movk x15, #0x0900, lsl #16      // UART
    movz x16, #0x0000
    movk x16, #0x0902, lsl #16      // PERF mailbox

    adr  x10, vectors
    msr  vbar_el1, x10

    adr  x21, handled
    str  xzr, [x21]

    LOAD_EXPECT x0, 0x1111, 0x2222, 0x3333, 0x4444
el1_scxtnum0_mrs_insn:
    .inst 0xD53BD0E0                  // mrs x0, scxtnum_el0
    b    fail
after_el1_scxtnum0_mrs:
    CHECK_HANDLED 1

    LOAD_EXPECT x0, 0x5555, 0x6666, 0x7777, 0x8888
el1_scxtnum0_msr_insn:
    .inst 0xD51BD0E0                  // msr scxtnum_el0, x0
    b    fail
after_el1_scxtnum0_msr:
    CHECK_HANDLED 2

    LOAD_EXPECT x0, 0x9999, 0xAAAA, 0xBBBB, 0xCCCC
el1_scxtnum1_mrs_insn:
    .inst 0xD538D0E0                  // mrs x0, scxtnum_el1
    b    fail
after_el1_scxtnum1_mrs:
    CHECK_HANDLED 3

    LOAD_EXPECT x0, 0xDDDD, 0xEEEE, 0x1357, 0x2468
el1_scxtnum1_msr_insn:
    .inst 0xD518D0E0                  // msr scxtnum_el1, x0
    b    fail
after_el1_scxtnum1_msr:
    CHECK_HANDLED 4

    LOAD_EXPECT x0, 0xABCD, 0xBCDE, 0xCDEF, 0xDEF0
el1_por_mrs_insn:
    .inst 0xD53BA280                  // mrs x0, por_el0
    b    fail
after_el1_por_mrs:
    CHECK_HANDLED 5

    LOAD_EXPECT x0, 0x0FED, 0x1E2D, 0x3C4B, 0x5A69
el1_por_msr_insn:
    .inst 0xD51BA280                  // msr por_el0, x0
    b    fail
after_el1_por_msr:
    CHECK_HANDLED 6

    LOAD_EXPECT x0, 0x1020, 0x3040, 0x5060, 0x7080
el1_trfcr_mrs_insn:
    .inst 0xD5381220                  // mrs x0, trfcr_el1
    b    fail
after_el1_trfcr_mrs:
    CHECK_HANDLED 7

    LOAD_EXPECT x0, 0x89AB, 0xCDEF, 0x0123, 0x4567
el1_trfcr_msr_insn:
    .inst 0xD5181220                  // msr trfcr_el1, x0
    b    fail
after_el1_trfcr_msr:
    CHECK_HANDLED 8

    adr  x3, el0_scxtnum0_mrs_entry
    msr  elr_el1, x3
    msr  spsr_el1, xzr               // EL0t
    eret
after_el0_scxtnum0_mrs:
    CHECK_HANDLED 9

    adr  x3, el0_scxtnum0_msr_entry
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret
after_el0_scxtnum0_msr:
    CHECK_HANDLED 10

    adr  x3, el0_scxtnum1_mrs_entry
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret
after_el0_scxtnum1_mrs:
    CHECK_HANDLED 11

    adr  x3, el0_scxtnum1_msr_entry
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret
after_el0_scxtnum1_msr:
    CHECK_HANDLED 12

    adr  x3, el0_por_mrs_entry
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret
after_el0_por_mrs:
    CHECK_HANDLED 13

    adr  x3, el0_por_msr_entry
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret
after_el0_por_msr:
    CHECK_HANDLED 14

    adr  x3, el0_trfcr_mrs_entry
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret
after_el0_trfcr_mrs:
    CHECK_HANDLED 15

    adr  x3, el0_trfcr_msr_entry
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret
after_el0_trfcr_msr:
    CHECK_HANDLED 16

    adr  x3, el0_rcwmask_mrs_entry
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret
after_el0_rcwmask_mrs:
    CHECK_HANDLED 17

    adr  x3, el0_rcwmask_msr_entry
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret
after_el0_rcwmask_msr:
    CHECK_HANDLED 18

    adr  x3, el0_rcwsmask_mrs_entry
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret
after_el0_rcwsmask_mrs:
    CHECK_HANDLED 19

    adr  x3, el0_rcwsmask_msr_entry
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret
after_el0_rcwsmask_msr:
    CHECK_HANDLED 20

    movz x0, #'O'
    str  x0, [x15]
    movz x0, #'\n'
    str  x0, [x15]
    b    exit_ok

el0_scxtnum0_mrs_entry:
    LOAD_EXPECT x0, 0xA001, 0xA002, 0xA003, 0xA004
el0_scxtnum0_mrs_insn:
    .inst 0xD53BD0E0                  // mrs x0, scxtnum_el0
    b    fail

el0_scxtnum0_msr_entry:
    LOAD_EXPECT x0, 0xB001, 0xB002, 0xB003, 0xB004
el0_scxtnum0_msr_insn:
    .inst 0xD51BD0E0                  // msr scxtnum_el0, x0
    b    fail

el0_scxtnum1_mrs_entry:
    LOAD_EXPECT x0, 0xC001, 0xC002, 0xC003, 0xC004
el0_scxtnum1_mrs_insn:
    .inst 0xD538D0E0                  // mrs x0, scxtnum_el1
    b    fail

el0_scxtnum1_msr_entry:
    LOAD_EXPECT x0, 0xD001, 0xD002, 0xD003, 0xD004
el0_scxtnum1_msr_insn:
    .inst 0xD518D0E0                  // msr scxtnum_el1, x0
    b    fail

el0_por_mrs_entry:
    LOAD_EXPECT x0, 0xE001, 0xE002, 0xE003, 0xE004
el0_por_mrs_insn:
    .inst 0xD53BA280                  // mrs x0, por_el0
    b    fail

el0_por_msr_entry:
    LOAD_EXPECT x0, 0xF001, 0xF002, 0xF003, 0xF004
el0_por_msr_insn:
    .inst 0xD51BA280                  // msr por_el0, x0
    b    fail

el0_trfcr_mrs_entry:
    LOAD_EXPECT x0, 0x1234, 0x2345, 0x3456, 0x4567
el0_trfcr_mrs_insn:
    .inst 0xD5381220                  // mrs x0, trfcr_el1
    b    fail

el0_trfcr_msr_entry:
    LOAD_EXPECT x0, 0x5678, 0x6789, 0x789A, 0x89AB
el0_trfcr_msr_insn:
    .inst 0xD5181220                  // msr trfcr_el1, x0
    b    fail

el0_rcwmask_mrs_entry:
    LOAD_EXPECT x0, 0x2101, 0x2102, 0x2103, 0x2104
el0_rcwmask_mrs_insn:
    .inst 0xD538D0C0                  // mrs x0, rcwmask_el1
    b    fail

el0_rcwmask_msr_entry:
    LOAD_EXPECT x0, 0x2201, 0x2202, 0x2203, 0x2204
el0_rcwmask_msr_insn:
    .inst 0xD518D0C0                  // msr rcwmask_el1, x0
    b    fail

el0_rcwsmask_mrs_entry:
    LOAD_EXPECT x0, 0x2301, 0x2302, 0x2303, 0x2304
el0_rcwsmask_mrs_insn:
    .inst 0xD538D060                  // mrs x0, rcwsmask_el1
    b    fail

el0_rcwsmask_msr_entry:
    LOAD_EXPECT x0, 0x2401, 0x2402, 0x2403, 0x2404
el0_rcwsmask_msr_insn:
    .inst 0xD518D060                  // msr rcwsmask_el1, x0
    b    fail

fail:
    movz x0, #'F'
    str  x0, [x15]
    movz x0, #'\n'
    str  x0, [x15]

exit_ok:
    movz x0, #3
    str  x0, [x16, #0x08]
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
    b lower_sync_handler

sync_handler:
    mrs  x22, esr_el1
    ubfx x23, x22, #26, #6
    cbnz x23, fail
    tbz  x22, #25, fail
    and  x24, x22, #0x1ffffff
    cbnz x24, fail
    mrs  x25, far_el1
    cbnz x25, fail
    mrs  x26, elr_el1

    HANDLE_EL1_CASE el1_scxtnum0_mrs_insn, after_el1_scxtnum0_mrs, 1, 0x1111, 0x2222, 0x3333, 0x4444
    HANDLE_EL1_CASE el1_scxtnum0_msr_insn, after_el1_scxtnum0_msr, 2, 0x5555, 0x6666, 0x7777, 0x8888
    HANDLE_EL1_CASE el1_scxtnum1_mrs_insn, after_el1_scxtnum1_mrs, 3, 0x9999, 0xAAAA, 0xBBBB, 0xCCCC
    HANDLE_EL1_CASE el1_scxtnum1_msr_insn, after_el1_scxtnum1_msr, 4, 0xDDDD, 0xEEEE, 0x1357, 0x2468
    HANDLE_EL1_CASE el1_por_mrs_insn, after_el1_por_mrs, 5, 0xABCD, 0xBCDE, 0xCDEF, 0xDEF0
    HANDLE_EL1_CASE el1_por_msr_insn, after_el1_por_msr, 6, 0x0FED, 0x1E2D, 0x3C4B, 0x5A69
    HANDLE_EL1_CASE el1_trfcr_mrs_insn, after_el1_trfcr_mrs, 7, 0x1020, 0x3040, 0x5060, 0x7080
    HANDLE_EL1_CASE el1_trfcr_msr_insn, after_el1_trfcr_msr, 8, 0x89AB, 0xCDEF, 0x0123, 0x4567
    b    fail

lower_sync_handler:
    mrs  x22, esr_el1
    ubfx x23, x22, #26, #6
    cbnz x23, fail
    tbz  x22, #25, fail
    and  x24, x22, #0x1ffffff
    cbnz x24, fail
    mrs  x25, far_el1
    cbnz x25, fail
    mrs  x26, elr_el1

    HANDLE_EL0_CASE el0_scxtnum0_mrs_insn, after_el0_scxtnum0_mrs, 9, 0xA001, 0xA002, 0xA003, 0xA004
    HANDLE_EL0_CASE el0_scxtnum0_msr_insn, after_el0_scxtnum0_msr, 10, 0xB001, 0xB002, 0xB003, 0xB004
    HANDLE_EL0_CASE el0_scxtnum1_mrs_insn, after_el0_scxtnum1_mrs, 11, 0xC001, 0xC002, 0xC003, 0xC004
    HANDLE_EL0_CASE el0_scxtnum1_msr_insn, after_el0_scxtnum1_msr, 12, 0xD001, 0xD002, 0xD003, 0xD004
    HANDLE_EL0_CASE el0_por_mrs_insn, after_el0_por_mrs, 13, 0xE001, 0xE002, 0xE003, 0xE004
    HANDLE_EL0_CASE el0_por_msr_insn, after_el0_por_msr, 14, 0xF001, 0xF002, 0xF003, 0xF004
    HANDLE_EL0_CASE el0_trfcr_mrs_insn, after_el0_trfcr_mrs, 15, 0x1234, 0x2345, 0x3456, 0x4567
    HANDLE_EL0_CASE el0_trfcr_msr_insn, after_el0_trfcr_msr, 16, 0x5678, 0x6789, 0x789A, 0x89AB
    HANDLE_EL0_CASE el0_rcwmask_mrs_insn, after_el0_rcwmask_mrs, 17, 0x2101, 0x2102, 0x2103, 0x2104
    HANDLE_EL0_CASE el0_rcwmask_msr_insn, after_el0_rcwmask_msr, 18, 0x2201, 0x2202, 0x2203, 0x2204
    HANDLE_EL0_CASE el0_rcwsmask_mrs_insn, after_el0_rcwsmask_mrs, 19, 0x2301, 0x2302, 0x2303, 0x2304
    HANDLE_EL0_CASE el0_rcwsmask_msr_insn, after_el0_rcwsmask_msr, 20, 0x2401, 0x2402, 0x2403, 0x2404
    b    fail

return_to_el1:
    movz x29, #0x5                  // EL1h
    msr  spsr_el1, x29
    msr  elr_el1, x30
    eret

    .bss
    .balign 8
handled:
    .skip 8
