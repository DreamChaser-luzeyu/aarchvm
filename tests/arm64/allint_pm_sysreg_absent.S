    .text
    .global _start

    .macro LOAD64 reg, imm0, imm1, imm2, imm3
    movz \reg, #\imm0
    movk \reg, #\imm1, lsl #16
    movk \reg, #\imm2, lsl #32
    movk \reg, #\imm3, lsl #48
    .endm

    .macro CHECK_COUNT value, fail_label
    adr  x11, trap_count
    ldr  x12, [x11]
    movz x13, #\value
    cmp  x12, x13
    b.ne \fail_label
    .endm

    .macro CHECK_SAVED_EL0 fail_label
    mrs  x24, spsr_el1
    tbnz x24, #20, \fail_label
    and  x25, x24, #0xf
    cbnz x25, \fail_label
    ubfx x25, x24, #28, #4
    cmp  x25, #0x9
    b.ne \fail_label
    ubfx x25, x24, #6, #4
    cmp  x25, #0xa
    b.ne \fail_label
    tbz  x24, #22, \fail_label
    .endm

    .macro HANDLE_CASE_EL1 insn_label, after_label, count, imm0, imm1, imm2, imm3
    adr  x23, \insn_label
    cmp  x22, x23
    b.ne .Lskip\@
    LOAD64 x24, \imm0, \imm1, \imm2, \imm3
    cmp  x0, x24
    b.ne handler_fail
    adr  x25, trap_count
    movz x26, #\count
    str  x26, [x25]
    adr  x27, \after_label
    b    return_to_el1
.Lskip\@:
    .endm

    .macro HANDLE_CASE_EL0 insn_label, after_label, count, imm0, imm1, imm2, imm3
    adr  x23, \insn_label
    cmp  x22, x23
    b.ne .Lskip\@
    CHECK_SAVED_EL0 handler_fail
    LOAD64 x24, \imm0, \imm1, \imm2, \imm3
    cmp  x0, x24
    b.ne handler_fail
    adr  x25, trap_count
    movz x26, #\count
    str  x26, [x25]
    adr  x27, \after_label
    b    return_to_el1
.Lskip\@:
    .endm

_start:
    movz x1, #0x0000
    movk x1, #0x0900, lsl #16      // UART

    adr  x10, vectors
    msr  vbar_el1, x10

    adr  x11, trap_count
    str  xzr, [x11]

    movz x4, #0x0280
    movk x4, #0x9040, lsl #16      // NZCV=1001, PAN=1, D=1, I=1, EL0t.

    LOAD64 x0, 0x1101, 0x1102, 0x1103, 0x1104
el1_mrs_allint_insn:
    .inst 0xD5384300                  // mrs x0, allint
    b    fail1
after_el1_mrs_allint:
    CHECK_COUNT 1, fail1

    LOAD64 x0, 0x1201, 0x1202, 0x1203, 0x1204
el1_msr_allint_insn:
    .inst 0xD5184300                  // msr allint, x0
    b    fail2
after_el1_msr_allint:
    CHECK_COUNT 2, fail2

    LOAD64 x0, 0x1301, 0x1302, 0x1303, 0x1304
el1_mrs_pm_insn:
    .inst 0xD5384320                  // mrs x0, pm
    b    fail3
after_el1_mrs_pm:
    CHECK_COUNT 3, fail3

    LOAD64 x0, 0x1401, 0x1402, 0x1403, 0x1404
el1_msr_pm_insn:
    .inst 0xD5184320                  // msr pm, x0
    b    fail4
after_el1_msr_pm:
    CHECK_COUNT 4, fail4

    adr  x3, el0_mrs_allint_entry
    msr  elr_el1, x3
    msr  spsr_el1, x4
    eret
after_el0_mrs_allint:
    CHECK_COUNT 5, fail5

    adr  x3, el0_msr_allint_entry
    msr  elr_el1, x3
    msr  spsr_el1, x4
    eret
after_el0_msr_allint:
    CHECK_COUNT 6, fail6

    adr  x3, el0_mrs_pm_entry
    msr  elr_el1, x3
    msr  spsr_el1, x4
    eret
after_el0_mrs_pm:
    CHECK_COUNT 7, fail7

    adr  x3, el0_msr_pm_entry
    msr  elr_el1, x3
    msr  spsr_el1, x4
    eret
after_el0_msr_pm:
    CHECK_COUNT 8, fail8

    movz x0, #'I'
    b    out

fail1:
    movz x0, #'1'
    b    out
fail2:
    movz x0, #'2'
    b    out
fail3:
    movz x0, #'3'
    b    out
fail4:
    movz x0, #'4'
    b    out
fail5:
    movz x0, #'5'
    b    out
fail6:
    movz x0, #'6'
    b    out
fail7:
    movz x0, #'7'
    b    out
fail8:
    movz x0, #'8'
    b    out

out:
    str  x0, [x1]
    brk  #0

el0_mrs_allint_entry:
    LOAD64 x0, 0x1501, 0x1502, 0x1503, 0x1504
el0_mrs_allint_insn:
    .inst 0xD5384300                  // mrs x0, allint
    b    fail5

el0_msr_allint_entry:
    LOAD64 x0, 0x1601, 0x1602, 0x1603, 0x1604
el0_msr_allint_insn:
    .inst 0xD5184300                  // msr allint, x0
    b    fail6

el0_mrs_pm_entry:
    LOAD64 x0, 0x1701, 0x1702, 0x1703, 0x1704
el0_mrs_pm_insn:
    .inst 0xD5384320                  // mrs x0, pm
    b    fail7

el0_msr_pm_entry:
    LOAD64 x0, 0x1801, 0x1802, 0x1803, 0x1804
el0_msr_pm_insn:
    .inst 0xD5184320                  // msr pm, x0
    b    fail8

    .balign 2048
vectors:
    .space 0x200
sync_spx:
    b sync_handler
    .space 0x400 - 0x200 - 4
sync_lower:
    b lower_sync_handler

sync_handler:
    mrs  x20, esr_el1
    ubfx x21, x20, #26, #6
    cbnz x21, handler_fail
    tbz  x20, #25, handler_fail
    and  x21, x20, #0x1ffffff
    cbnz x21, handler_fail
    mrs  x21, far_el1
    cbnz x21, handler_fail

    mrs  x22, elr_el1
    HANDLE_CASE_EL1 el1_mrs_allint_insn, after_el1_mrs_allint, 1, 0x1101, 0x1102, 0x1103, 0x1104
    HANDLE_CASE_EL1 el1_msr_allint_insn, after_el1_msr_allint, 2, 0x1201, 0x1202, 0x1203, 0x1204
    HANDLE_CASE_EL1 el1_mrs_pm_insn, after_el1_mrs_pm, 3, 0x1301, 0x1302, 0x1303, 0x1304
    HANDLE_CASE_EL1 el1_msr_pm_insn, after_el1_msr_pm, 4, 0x1401, 0x1402, 0x1403, 0x1404
    b    handler_fail

lower_sync_handler:
    mrs  x20, esr_el1
    ubfx x21, x20, #26, #6
    cbnz x21, handler_fail
    tbz  x20, #25, handler_fail
    and  x21, x20, #0x1ffffff
    cbnz x21, handler_fail
    mrs  x21, far_el1
    cbnz x21, handler_fail

    mrs  x22, elr_el1
    HANDLE_CASE_EL0 el0_mrs_allint_insn, after_el0_mrs_allint, 5, 0x1501, 0x1502, 0x1503, 0x1504
    HANDLE_CASE_EL0 el0_msr_allint_insn, after_el0_msr_allint, 6, 0x1601, 0x1602, 0x1603, 0x1604
    HANDLE_CASE_EL0 el0_mrs_pm_insn, after_el0_mrs_pm, 7, 0x1701, 0x1702, 0x1703, 0x1704
    HANDLE_CASE_EL0 el0_msr_pm_insn, after_el0_msr_pm, 8, 0x1801, 0x1802, 0x1803, 0x1804
    b    handler_fail

return_to_el1:
    movz x28, #0x5
    msr  spsr_el1, x28
    msr  elr_el1, x27
    eret

handler_fail:
    movz x0, #'X'
    adr  x27, out
    movz x28, #0x5
    msr  spsr_el1, x28
    msr  elr_el1, x27
    eret

    .data
    .balign 8
trap_count:
    .quad 0
