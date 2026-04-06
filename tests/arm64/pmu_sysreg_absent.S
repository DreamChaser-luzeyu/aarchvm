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

    adr  x10, vectors
    msr  vbar_el1, x10

    adr  x21, handled
    str  xzr, [x21]

    movz x4, #0x0280
    movk x4, #0x9040, lsl #16      // NZCV=1001, PAN=1, D=1, I=1, EL0t.

    LOAD_EXPECT x0, 0x1101, 0x1102, 0x1103, 0x1104
el1_pmcr_mrs_insn:
    .inst 0xD53B9C00                  // mrs x0, pmcr_el0
    b    fail
after_el1_pmcr_mrs:
    CHECK_HANDLED 1

    LOAD_EXPECT x0, 0x1201, 0x1202, 0x1203, 0x1204
el1_pmcr_msr_insn:
    .inst 0xD51B9C00                  // msr pmcr_el0, x0
    b    fail
after_el1_pmcr_msr:
    CHECK_HANDLED 2

    adr  x3, el0_pmccfiltr_mrs_entry
    msr  elr_el1, x3
    msr  spsr_el1, x4
    eret
after_el0_pmccfiltr_mrs:
    CHECK_HANDLED 3

    adr  x3, el0_pmccntr_mrs_entry
    msr  elr_el1, x3
    msr  spsr_el1, x4
    eret
after_el0_pmccntr_mrs:
    CHECK_HANDLED 4

    adr  x3, el0_pmceid0_mrs_entry
    msr  elr_el1, x3
    msr  spsr_el1, x4
    eret
after_el0_pmceid0_mrs:
    CHECK_HANDLED 5

    adr  x3, el0_pmceid1_mrs_entry
    msr  elr_el1, x3
    msr  spsr_el1, x4
    eret
after_el0_pmceid1_mrs:
    CHECK_HANDLED 6

    adr  x3, el0_pmcntenclr_mrs_entry
    msr  elr_el1, x3
    msr  spsr_el1, x4
    eret
after_el0_pmcntenclr_mrs:
    CHECK_HANDLED 7

    adr  x3, el0_pmcntenset_msr_entry
    msr  elr_el1, x3
    msr  spsr_el1, x4
    eret
after_el0_pmcntenset_msr:
    CHECK_HANDLED 8

    adr  x3, el0_pmcr_mrs_entry
    msr  elr_el1, x3
    msr  spsr_el1, x4
    eret
after_el0_pmcr_mrs:
    CHECK_HANDLED 9

    adr  x3, el0_pmintenclr_mrs_entry
    msr  elr_el1, x3
    msr  spsr_el1, x4
    eret
after_el0_pmintenclr_mrs:
    CHECK_HANDLED 10

    adr  x3, el0_pmintenset_msr_entry
    msr  elr_el1, x3
    msr  spsr_el1, x4
    eret
after_el0_pmintenset_msr:
    CHECK_HANDLED 11

    adr  x3, el0_pmmir_mrs_entry
    msr  elr_el1, x3
    msr  spsr_el1, x4
    eret
after_el0_pmmir_mrs:
    CHECK_HANDLED 12

    adr  x3, el0_pmovsclr_msr_entry
    msr  elr_el1, x3
    msr  spsr_el1, x4
    eret
after_el0_pmovsclr_msr:
    CHECK_HANDLED 13

    adr  x3, el0_pmovsset_mrs_entry
    msr  elr_el1, x3
    msr  spsr_el1, x4
    eret
after_el0_pmovsset_mrs:
    CHECK_HANDLED 14

    adr  x3, el0_pmselr_msr_entry
    msr  elr_el1, x3
    msr  spsr_el1, x4
    eret
after_el0_pmselr_msr:
    CHECK_HANDLED 15

    adr  x3, el0_pmswinc_msr_entry
    msr  elr_el1, x3
    msr  spsr_el1, x4
    eret
after_el0_pmswinc_msr:
    CHECK_HANDLED 16

    adr  x3, el0_pmxevcntr_mrs_entry
    msr  elr_el1, x3
    msr  spsr_el1, x4
    eret
after_el0_pmxevcntr_mrs:
    CHECK_HANDLED 17

    adr  x3, el0_pmxevtyper_msr_entry
    msr  elr_el1, x3
    msr  spsr_el1, x4
    eret
after_el0_pmxevtyper_msr:
    CHECK_HANDLED 18

    movz x0, #'P'
    str  x0, [x15]
    movz x0, #'\n'
    str  x0, [x15]
1:
    wfi
    b    1b

el0_pmccfiltr_mrs_entry:
    LOAD_EXPECT x0, 0x2101, 0x2102, 0x2103, 0x2104
el0_pmccfiltr_mrs_insn:
    .inst 0xD53BEFE0                  // mrs x0, pmccfiltr_el0
    b    fail

el0_pmccntr_mrs_entry:
    LOAD_EXPECT x0, 0x2201, 0x2202, 0x2203, 0x2204
el0_pmccntr_mrs_insn:
    .inst 0xD53B9D00                  // mrs x0, pmccntr_el0
    b    fail

el0_pmceid0_mrs_entry:
    LOAD_EXPECT x0, 0x2301, 0x2302, 0x2303, 0x2304
el0_pmceid0_mrs_insn:
    .inst 0xD53B9CC0                  // mrs x0, pmceid0_el0
    b    fail

el0_pmceid1_mrs_entry:
    LOAD_EXPECT x0, 0x2401, 0x2402, 0x2403, 0x2404
el0_pmceid1_mrs_insn:
    .inst 0xD53B9CE0                  // mrs x0, pmceid1_el0
    b    fail

el0_pmcntenclr_mrs_entry:
    LOAD_EXPECT x0, 0x2501, 0x2502, 0x2503, 0x2504
el0_pmcntenclr_mrs_insn:
    .inst 0xD53B9C40                  // mrs x0, pmcntenclr_el0
    b    fail

el0_pmcntenset_msr_entry:
    LOAD_EXPECT x0, 0x2601, 0x2602, 0x2603, 0x2604
el0_pmcntenset_msr_insn:
    .inst 0xD51B9C20                  // msr pmcntenset_el0, x0
    b    fail

el0_pmcr_mrs_entry:
    LOAD_EXPECT x0, 0x2701, 0x2702, 0x2703, 0x2704
el0_pmcr_mrs_insn:
    .inst 0xD53B9C00                  // mrs x0, pmcr_el0
    b    fail

el0_pmintenclr_mrs_entry:
    LOAD_EXPECT x0, 0x2801, 0x2802, 0x2803, 0x2804
el0_pmintenclr_mrs_insn:
    .inst 0xD5389E40                  // mrs x0, pmintenclr_el1
    b    fail

el0_pmintenset_msr_entry:
    LOAD_EXPECT x0, 0x2901, 0x2902, 0x2903, 0x2904
el0_pmintenset_msr_insn:
    .inst 0xD5189E20                  // msr pmintenset_el1, x0
    b    fail

el0_pmmir_mrs_entry:
    LOAD_EXPECT x0, 0x2A01, 0x2A02, 0x2A03, 0x2A04
el0_pmmir_mrs_insn:
    .inst 0xD5389EC0                  // mrs x0, pmmir_el1
    b    fail

el0_pmovsclr_msr_entry:
    LOAD_EXPECT x0, 0x2B01, 0x2B02, 0x2B03, 0x2B04
el0_pmovsclr_msr_insn:
    .inst 0xD51B9C60                  // msr pmovsclr_el0, x0
    b    fail

el0_pmovsset_mrs_entry:
    LOAD_EXPECT x0, 0x2C01, 0x2C02, 0x2C03, 0x2C04
el0_pmovsset_mrs_insn:
    .inst 0xD53B9E60                  // mrs x0, pmovsset_el0
    b    fail

el0_pmselr_msr_entry:
    LOAD_EXPECT x0, 0x2D01, 0x2D02, 0x2D03, 0x2D04
el0_pmselr_msr_insn:
    .inst 0xD51B9CA0                  // msr pmselr_el0, x0
    b    fail

el0_pmswinc_msr_entry:
    LOAD_EXPECT x0, 0x2E01, 0x2E02, 0x2E03, 0x2E04
el0_pmswinc_msr_insn:
    .inst 0xD51B9C80                  // msr pmswinc_el0, x0
    b    fail

el0_pmxevcntr_mrs_entry:
    LOAD_EXPECT x0, 0x2F01, 0x2F02, 0x2F03, 0x2F04
el0_pmxevcntr_mrs_insn:
    .inst 0xD53B9D40                  // mrs x0, pmxevcntr_el0
    b    fail

el0_pmxevtyper_msr_entry:
    LOAD_EXPECT x0, 0x3001, 0x3002, 0x3003, 0x3004
el0_pmxevtyper_msr_insn:
    .inst 0xD51B9D20                  // msr pmxevtyper_el0, x0
    b    fail

fail:
    movz x0, #'F'
    str  x0, [x15]
    movz x0, #'\n'
    str  x0, [x15]
2:
    wfi
    b    2b

    .balign 2048
vectors:
    .space 0x200
sync_spx:
    b sync_handler
    .space 0x1fc
sync_lower:
    b lower_sync_handler

sync_handler:
    mrs  x20, esr_el1
    ubfx x22, x20, #26, #6
    cbnz x22, handler_fail
    tbz  x20, #25, handler_fail
    and  x23, x20, #0x1ffffff
    cbnz x23, handler_fail
    mrs  x23, far_el1
    cbnz x23, handler_fail

    mrs  x26, elr_el1
    HANDLE_EL1_CASE el1_pmcr_mrs_insn, after_el1_pmcr_mrs, 1, 0x1101, 0x1102, 0x1103, 0x1104
    HANDLE_EL1_CASE el1_pmcr_msr_insn, after_el1_pmcr_msr, 2, 0x1201, 0x1202, 0x1203, 0x1204
    b    handler_fail

lower_sync_handler:
    mrs  x20, esr_el1
    ubfx x22, x20, #26, #6
    cbnz x22, handler_fail
    tbz  x20, #25, handler_fail
    and  x23, x20, #0x1ffffff
    cbnz x23, handler_fail
    mrs  x23, far_el1
    cbnz x23, handler_fail
    mrs  x23, spsr_el1
    tbnz x23, #20, handler_fail
    and  x24, x23, #0xf
    cbnz x24, handler_fail
    ubfx x24, x23, #28, #4
    cmp  x24, #0x9
    b.ne handler_fail
    ubfx x24, x23, #6, #4
    cmp  x24, #0xa
    b.ne handler_fail
    tbz  x23, #22, handler_fail

    mrs  x26, elr_el1
    HANDLE_EL0_CASE el0_pmccfiltr_mrs_insn, after_el0_pmccfiltr_mrs, 3, 0x2101, 0x2102, 0x2103, 0x2104
    HANDLE_EL0_CASE el0_pmccntr_mrs_insn, after_el0_pmccntr_mrs, 4, 0x2201, 0x2202, 0x2203, 0x2204
    HANDLE_EL0_CASE el0_pmceid0_mrs_insn, after_el0_pmceid0_mrs, 5, 0x2301, 0x2302, 0x2303, 0x2304
    HANDLE_EL0_CASE el0_pmceid1_mrs_insn, after_el0_pmceid1_mrs, 6, 0x2401, 0x2402, 0x2403, 0x2404
    HANDLE_EL0_CASE el0_pmcntenclr_mrs_insn, after_el0_pmcntenclr_mrs, 7, 0x2501, 0x2502, 0x2503, 0x2504
    HANDLE_EL0_CASE el0_pmcntenset_msr_insn, after_el0_pmcntenset_msr, 8, 0x2601, 0x2602, 0x2603, 0x2604
    HANDLE_EL0_CASE el0_pmcr_mrs_insn, after_el0_pmcr_mrs, 9, 0x2701, 0x2702, 0x2703, 0x2704
    HANDLE_EL0_CASE el0_pmintenclr_mrs_insn, after_el0_pmintenclr_mrs, 10, 0x2801, 0x2802, 0x2803, 0x2804
    HANDLE_EL0_CASE el0_pmintenset_msr_insn, after_el0_pmintenset_msr, 11, 0x2901, 0x2902, 0x2903, 0x2904
    HANDLE_EL0_CASE el0_pmmir_mrs_insn, after_el0_pmmir_mrs, 12, 0x2A01, 0x2A02, 0x2A03, 0x2A04
    HANDLE_EL0_CASE el0_pmovsclr_msr_insn, after_el0_pmovsclr_msr, 13, 0x2B01, 0x2B02, 0x2B03, 0x2B04
    HANDLE_EL0_CASE el0_pmovsset_mrs_insn, after_el0_pmovsset_mrs, 14, 0x2C01, 0x2C02, 0x2C03, 0x2C04
    HANDLE_EL0_CASE el0_pmselr_msr_insn, after_el0_pmselr_msr, 15, 0x2D01, 0x2D02, 0x2D03, 0x2D04
    HANDLE_EL0_CASE el0_pmswinc_msr_insn, after_el0_pmswinc_msr, 16, 0x2E01, 0x2E02, 0x2E03, 0x2E04
    HANDLE_EL0_CASE el0_pmxevcntr_mrs_insn, after_el0_pmxevcntr_mrs, 17, 0x2F01, 0x2F02, 0x2F03, 0x2F04
    HANDLE_EL0_CASE el0_pmxevtyper_msr_insn, after_el0_pmxevtyper_msr, 18, 0x3001, 0x3002, 0x3003, 0x3004
    b    handler_fail

return_to_el1:
    movz x28, #0x5                    // EL1h
    msr  spsr_el1, x28
    msr  elr_el1, x30
    eret

handler_fail:
    movz x0, #'E'
    str  x0, [x15]
    movz x0, #'\n'
    str  x0, [x15]
3:
    wfi
    b    3b

    .data
    .balign 8
handled:
    .quad 0
