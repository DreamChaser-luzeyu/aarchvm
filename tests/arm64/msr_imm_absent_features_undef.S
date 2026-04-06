    .text
    .global _start

    .macro CHECK_COUNT value
    adr  x11, trap_count
    ldr  x12, [x11]
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

    .macro HANDLE_CASE insn_label, after_label, count, imm0, imm1, imm2, imm3
    adr  x23, \insn_label
    cmp  x22, x23
    b.ne .Lskip\@
    LOAD_EXPECT x24, \imm0, \imm1, \imm2, \imm3
    cmp  x0, x24
    b.ne fail
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

    adr  x3, el0_msr_uao
    msr  elr_el1, x3
    msr  spsr_el1, x4
    eret
after_uao:
    CHECK_COUNT 1

    adr  x3, el0_msr_dit
    msr  elr_el1, x3
    msr  spsr_el1, x4
    eret
after_dit:
    CHECK_COUNT 2

    adr  x3, el0_msr_ssbs
    msr  elr_el1, x3
    msr  spsr_el1, x4
    eret
after_ssbs:
    CHECK_COUNT 3

    adr  x3, el0_msr_tco
    msr  elr_el1, x3
    msr  spsr_el1, x4
    eret
after_tco:
    CHECK_COUNT 4

    adr  x3, el0_msr_allint
    msr  elr_el1, x3
    msr  spsr_el1, x4
    eret
after_allint:
    CHECK_COUNT 5

    adr  x3, el0_msr_pm
    msr  elr_el1, x3
    msr  spsr_el1, x4
    eret
after_pm:
    CHECK_COUNT 6

    adr  x3, el0_smstart_sm
    msr  elr_el1, x3
    msr  spsr_el1, x4
    eret
after_smstart_sm:
    CHECK_COUNT 7

    adr  x3, el0_smstart_za
    msr  elr_el1, x3
    msr  spsr_el1, x4
    eret
after_smstart_za:
    CHECK_COUNT 8

    adr  x3, el0_smstart_all
    msr  elr_el1, x3
    msr  spsr_el1, x4
    eret
after_smstart_all:
    CHECK_COUNT 9

    movz x0, #'M'
    b    out

el0_msr_uao:
    LOAD_EXPECT x0, 0x1101, 0x1102, 0x1103, 0x1104
el0_msr_uao_insn:
    .inst 0xD500417F                  // msr uao, #1
    svc  #0x11

el0_msr_dit:
    LOAD_EXPECT x0, 0x1201, 0x1202, 0x1203, 0x1204
el0_msr_dit_insn:
    .inst 0xD503415F                  // msr dit, #1
    svc  #0x12

el0_msr_ssbs:
    LOAD_EXPECT x0, 0x1301, 0x1302, 0x1303, 0x1304
el0_msr_ssbs_insn:
    .inst 0xD503413F                  // msr ssbs, #1
    svc  #0x13

el0_msr_tco:
    LOAD_EXPECT x0, 0x1401, 0x1402, 0x1403, 0x1404
el0_msr_tco_insn:
    .inst 0xD503419F                  // msr tco, #1
    svc  #0x14

el0_msr_allint:
    LOAD_EXPECT x0, 0x1501, 0x1502, 0x1503, 0x1504
el0_msr_allint_insn:
    .inst 0xD501411F                  // msr allint, #1
    svc  #0x15

el0_msr_pm:
    LOAD_EXPECT x0, 0x1601, 0x1602, 0x1603, 0x1604
el0_msr_pm_insn:
    .inst 0xD501431F                  // msr pm, #1
    svc  #0x16

el0_smstart_sm:
    LOAD_EXPECT x0, 0x1701, 0x1702, 0x1703, 0x1704
el0_smstart_sm_insn:
    .inst 0xD503437F                  // smstart sm
    svc  #0x17

el0_smstart_za:
    LOAD_EXPECT x0, 0x1801, 0x1802, 0x1803, 0x1804
el0_smstart_za_insn:
    .inst 0xD503457F                  // smstart za
    svc  #0x18

el0_smstart_all:
    LOAD_EXPECT x0, 0x1901, 0x1902, 0x1903, 0x1904
el0_smstart_all_insn:
    .inst 0xD503477F                  // smstart
    svc  #0x19

fail:
    movz x0, #'F'
    b out

out:
    movz x1, #0x0000
    movk x1, #0x0900, lsl #16
    str  x0, [x1]
1:
    wfi
    b    1b

    .balign 2048
vectors:
    .space 0x400
sync_lower:
    b lower_sync_handler

lower_sync_handler:
    mrs  x20, esr_el1
    ubfx x21, x20, #26, #6
    cbnz x21, handler_fail
    tbz  x20, #25, handler_fail
    and  x24, x20, #0x1ffffff
    cbnz x24, handler_fail
    mrs  x24, far_el1
    cbnz x24, handler_fail
    CHECK_SAVED_EL0 handler_fail

    mrs  x22, elr_el1
    HANDLE_CASE el0_msr_uao_insn, after_uao, 1, 0x1101, 0x1102, 0x1103, 0x1104
    HANDLE_CASE el0_msr_dit_insn, after_dit, 2, 0x1201, 0x1202, 0x1203, 0x1204
    HANDLE_CASE el0_msr_ssbs_insn, after_ssbs, 3, 0x1301, 0x1302, 0x1303, 0x1304
    HANDLE_CASE el0_msr_tco_insn, after_tco, 4, 0x1401, 0x1402, 0x1403, 0x1404
    HANDLE_CASE el0_msr_allint_insn, after_allint, 5, 0x1501, 0x1502, 0x1503, 0x1504
    HANDLE_CASE el0_msr_pm_insn, after_pm, 6, 0x1601, 0x1602, 0x1603, 0x1604
    HANDLE_CASE el0_smstart_sm_insn, after_smstart_sm, 7, 0x1701, 0x1702, 0x1703, 0x1704
    HANDLE_CASE el0_smstart_za_insn, after_smstart_za, 8, 0x1801, 0x1802, 0x1803, 0x1804
    HANDLE_CASE el0_smstart_all_insn, after_smstart_all, 9, 0x1901, 0x1902, 0x1903, 0x1904
    b    handler_fail

return_to_el1:
    movz x28, #0x5                // EL1h
    msr  spsr_el1, x28
    msr  elr_el1, x27
    eret

handler_fail:
    movz x0, #'E'
    adr  x27, out
    movz x28, #0x5
    msr  spsr_el1, x28
    msr  elr_el1, x27
    eret

    .data
    .balign 8
trap_count:
    .quad 0
