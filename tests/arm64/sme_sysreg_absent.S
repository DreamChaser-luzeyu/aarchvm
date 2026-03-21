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
el1_svcr_mrs_insn:
    .inst 0xD53B4240                  // mrs x0, svcr
    b    fail
after_el1_svcr_mrs:
    CHECK_HANDLED 1

    LOAD_EXPECT x0, 0x5555, 0x6666, 0x7777, 0x8888
el1_svcr_msr_insn:
    .inst 0xD51B4240                  // msr svcr, x0
    b    fail
after_el1_svcr_msr:
    CHECK_HANDLED 2

    LOAD_EXPECT x0, 0x9999, 0xAAAA, 0xBBBB, 0xCCCC
el1_smcr_mrs_insn:
    .inst 0xD53812C0                  // mrs x0, smcr_el1
    b    fail
after_el1_smcr_mrs:
    CHECK_HANDLED 3

    LOAD_EXPECT x0, 0xDDDD, 0xEEEE, 0x1357, 0x2468
el1_smcr_msr_insn:
    .inst 0xD51812C0                  // msr smcr_el1, x0
    b    fail
after_el1_smcr_msr:
    CHECK_HANDLED 4

    adr  x3, el0_svcr_mrs_entry
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret
after_el0_svcr_mrs:
    CHECK_HANDLED 5

    adr  x3, el0_svcr_msr_entry
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret
after_el0_svcr_msr:
    CHECK_HANDLED 6

    adr  x3, el0_smcr_mrs_entry
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret
after_el0_smcr_mrs:
    CHECK_HANDLED 7

    adr  x3, el0_smcr_msr_entry
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret
after_el0_smcr_msr:
    CHECK_HANDLED 8

    adr  x3, el0_smcr_el2_mrs_entry
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret
after_el0_smcr_el2_mrs:
    CHECK_HANDLED 9

    adr  x3, el0_smcr_el2_msr_entry
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret
after_el0_smcr_el2_msr:
    CHECK_HANDLED 10

    adr  x3, el0_smcr_el3_mrs_entry
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret
after_el0_smcr_el3_mrs:
    CHECK_HANDLED 11

    adr  x3, el0_smcr_el3_msr_entry
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret
after_el0_smcr_el3_msr:
    CHECK_HANDLED 12

    movz x0, #'Z'
    str  x0, [x15]
    movz x0, #'\n'
    str  x0, [x15]
    b    exit_ok

el0_svcr_mrs_entry:
    LOAD_EXPECT x0, 0xA001, 0xA002, 0xA003, 0xA004
el0_svcr_mrs_insn:
    .inst 0xD53B4240                  // mrs x0, svcr
    b    fail

el0_svcr_msr_entry:
    LOAD_EXPECT x0, 0xB001, 0xB002, 0xB003, 0xB004
el0_svcr_msr_insn:
    .inst 0xD51B4240                  // msr svcr, x0
    b    fail

el0_smcr_mrs_entry:
    LOAD_EXPECT x0, 0xC001, 0xC002, 0xC003, 0xC004
el0_smcr_mrs_insn:
    .inst 0xD53812C0                  // mrs x0, smcr_el1
    b    fail

el0_smcr_msr_entry:
    LOAD_EXPECT x0, 0xD001, 0xD002, 0xD003, 0xD004
el0_smcr_msr_insn:
    .inst 0xD51812C0                  // msr smcr_el1, x0
    b    fail

el0_smcr_el2_mrs_entry:
    LOAD_EXPECT x0, 0xE001, 0xE002, 0xE003, 0xE004
el0_smcr_el2_mrs_insn:
    .inst 0xD53C12C0                  // mrs x0, smcr_el2
    b    fail

el0_smcr_el2_msr_entry:
    LOAD_EXPECT x0, 0xE101, 0xE102, 0xE103, 0xE104
el0_smcr_el2_msr_insn:
    .inst 0xD51C12C0                  // msr smcr_el2, x0
    b    fail

el0_smcr_el3_mrs_entry:
    LOAD_EXPECT x0, 0xE201, 0xE202, 0xE203, 0xE204
el0_smcr_el3_mrs_insn:
    .inst 0xD53E12C0                  // mrs x0, smcr_el3
    b    fail

el0_smcr_el3_msr_entry:
    LOAD_EXPECT x0, 0xE301, 0xE302, 0xE303, 0xE304
el0_smcr_el3_msr_insn:
    .inst 0xD51E12C0                  // msr smcr_el3, x0
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

    HANDLE_EL1_CASE el1_svcr_mrs_insn, after_el1_svcr_mrs, 1, 0x1111, 0x2222, 0x3333, 0x4444
    HANDLE_EL1_CASE el1_svcr_msr_insn, after_el1_svcr_msr, 2, 0x5555, 0x6666, 0x7777, 0x8888
    HANDLE_EL1_CASE el1_smcr_mrs_insn, after_el1_smcr_mrs, 3, 0x9999, 0xAAAA, 0xBBBB, 0xCCCC
    HANDLE_EL1_CASE el1_smcr_msr_insn, after_el1_smcr_msr, 4, 0xDDDD, 0xEEEE, 0x1357, 0x2468
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

    HANDLE_EL0_CASE el0_svcr_mrs_insn, after_el0_svcr_mrs, 5, 0xA001, 0xA002, 0xA003, 0xA004
    HANDLE_EL0_CASE el0_svcr_msr_insn, after_el0_svcr_msr, 6, 0xB001, 0xB002, 0xB003, 0xB004
    HANDLE_EL0_CASE el0_smcr_mrs_insn, after_el0_smcr_mrs, 7, 0xC001, 0xC002, 0xC003, 0xC004
    HANDLE_EL0_CASE el0_smcr_msr_insn, after_el0_smcr_msr, 8, 0xD001, 0xD002, 0xD003, 0xD004
    HANDLE_EL0_CASE el0_smcr_el2_mrs_insn, after_el0_smcr_el2_mrs, 9, 0xE001, 0xE002, 0xE003, 0xE004
    HANDLE_EL0_CASE el0_smcr_el2_msr_insn, after_el0_smcr_el2_msr, 10, 0xE101, 0xE102, 0xE103, 0xE104
    HANDLE_EL0_CASE el0_smcr_el3_mrs_insn, after_el0_smcr_el3_mrs, 11, 0xE201, 0xE202, 0xE203, 0xE204
    HANDLE_EL0_CASE el0_smcr_el3_msr_insn, after_el0_smcr_el3_msr, 12, 0xE301, 0xE302, 0xE303, 0xE304
    b    fail

return_to_el1:
    movz x29, #0x5
    msr  spsr_el1, x29
    msr  elr_el1, x30
    eret

    .bss
    .balign 8
handled:
    .skip 8
