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
    movz x4, #0x0280
    movk x4, #0x9040, lsl #16       // NZCV=1001, PAN=1, D=1, I=1, EL0t.

    adr  x10, vectors
    msr  vbar_el1, x10

    adr  x21, handled
    str  xzr, [x21]

    LOAD_EXPECT x0, 0x1101, 0x1102, 0x1103, 0x1104
el1_amcfgr_mrs_insn:
    .inst 0xD53BD220                  // mrs x0, amcfgr_el0
    b    fail
after_el1_amcfgr_mrs:
    CHECK_HANDLED 1

    LOAD_EXPECT x0, 0x1201, 0x1202, 0x1203, 0x1204
el1_amcgcr_mrs_insn:
    .inst 0xD53BD240                  // mrs x0, amcgcr_el0
    b    fail
after_el1_amcgcr_mrs:
    CHECK_HANDLED 2

    LOAD_EXPECT x0, 0x1301, 0x1302, 0x1303, 0x1304
el1_amcg1idr_mrs_insn:
    .inst 0xD53BD2C0                  // mrs x0, amcg1idr_el0
    b    fail
after_el1_amcg1idr_mrs:
    CHECK_HANDLED 3

    LOAD_EXPECT x0, 0x1401, 0x1402, 0x1403, 0x1404
el1_amcr_mrs_insn:
    .inst 0xD53BD200                  // mrs x0, amcr_el0
    b    fail
after_el1_amcr_mrs:
    CHECK_HANDLED 4

    LOAD_EXPECT x0, 0x1501, 0x1502, 0x1503, 0x1504
el1_amcr_msr_insn:
    .inst 0xD51BD200                  // msr amcr_el0, x0
    b    fail
after_el1_amcr_msr:
    CHECK_HANDLED 5

    LOAD_EXPECT x0, 0x1601, 0x1602, 0x1603, 0x1604
el1_amcntenclr0_mrs_insn:
    .inst 0xD53BD280                  // mrs x0, amcntenclr0_el0
    b    fail
after_el1_amcntenclr0_mrs:
    CHECK_HANDLED 6

    LOAD_EXPECT x0, 0x1701, 0x1702, 0x1703, 0x1704
el1_amcntenclr0_msr_insn:
    .inst 0xD51BD280                  // msr amcntenclr0_el0, x0
    b    fail
after_el1_amcntenclr0_msr:
    CHECK_HANDLED 7

    LOAD_EXPECT x0, 0x1801, 0x1802, 0x1803, 0x1804
el1_amcntenset0_mrs_insn:
    .inst 0xD53BD2A0                  // mrs x0, amcntenset0_el0
    b    fail
after_el1_amcntenset0_mrs:
    CHECK_HANDLED 8

    LOAD_EXPECT x0, 0x1901, 0x1902, 0x1903, 0x1904
el1_amcntenset0_msr_insn:
    .inst 0xD51BD2A0                  // msr amcntenset0_el0, x0
    b    fail
after_el1_amcntenset0_msr:
    CHECK_HANDLED 9

    LOAD_EXPECT x0, 0x1A01, 0x1A02, 0x1A03, 0x1A04
el1_amcntenclr1_mrs_insn:
    .inst 0xD53BD300                  // mrs x0, amcntenclr1_el0
    b    fail
after_el1_amcntenclr1_mrs:
    CHECK_HANDLED 10

    LOAD_EXPECT x0, 0x1B01, 0x1B02, 0x1B03, 0x1B04
el1_amcntenclr1_msr_insn:
    .inst 0xD51BD300                  // msr amcntenclr1_el0, x0
    b    fail
after_el1_amcntenclr1_msr:
    CHECK_HANDLED 11

    LOAD_EXPECT x0, 0x1C01, 0x1C02, 0x1C03, 0x1C04
el1_amcntenset1_mrs_insn:
    .inst 0xD53BD320                  // mrs x0, amcntenset1_el0
    b    fail
after_el1_amcntenset1_mrs:
    CHECK_HANDLED 12

    LOAD_EXPECT x0, 0x1D01, 0x1D02, 0x1D03, 0x1D04
el1_amcntenset1_msr_insn:
    .inst 0xD51BD320                  // msr amcntenset1_el0, x0
    b    fail
after_el1_amcntenset1_msr:
    CHECK_HANDLED 13

    LOAD_EXPECT x0, 0x1E01, 0x1E02, 0x1E03, 0x1E04
el1_amevcntr00_mrs_insn:
    .inst 0xD53BD400                  // mrs x0, amevcntr00_el0
    b    fail
after_el1_amevcntr00_mrs:
    CHECK_HANDLED 14

    LOAD_EXPECT x0, 0x1F01, 0x1F02, 0x1F03, 0x1F04
el1_amevcntr00_msr_insn:
    .inst 0xD51BD400                  // msr amevcntr00_el0, x0
    b    fail
after_el1_amevcntr00_msr:
    CHECK_HANDLED 15

    LOAD_EXPECT x0, 0x2001, 0x2002, 0x2003, 0x2004
el1_amevtyper00_mrs_insn:
    .inst 0xD53BD600                  // mrs x0, amevtyper00_el0
    b    fail
after_el1_amevtyper00_mrs:
    CHECK_HANDLED 16

    LOAD_EXPECT x0, 0x2101, 0x2102, 0x2103, 0x2104
el1_amevcntr10_mrs_insn:
    .inst 0xD53BDC00                  // mrs x0, amevcntr10_el0
    b    fail
after_el1_amevcntr10_mrs:
    CHECK_HANDLED 17

    LOAD_EXPECT x0, 0x2201, 0x2202, 0x2203, 0x2204
el1_amevcntr10_msr_insn:
    .inst 0xD51BDC00                  // msr amevcntr10_el0, x0
    b    fail
after_el1_amevcntr10_msr:
    CHECK_HANDLED 18

    LOAD_EXPECT x0, 0x2301, 0x2302, 0x2303, 0x2304
el1_amevtyper10_mrs_insn:
    .inst 0xD53BDE00                  // mrs x0, amevtyper10_el0
    b    fail
after_el1_amevtyper10_mrs:
    CHECK_HANDLED 19

    LOAD_EXPECT x0, 0x2401, 0x2402, 0x2403, 0x2404
el1_amevcntvoff00_mrs_insn:
    .inst 0xD53CD800                  // mrs x0, amevcntvoff00_el2
    b    fail
after_el1_amevcntvoff00_mrs:
    CHECK_HANDLED 20

    LOAD_EXPECT x0, 0x2501, 0x2502, 0x2503, 0x2504
el1_amevcntvoff00_msr_insn:
    .inst 0xD51CD800                  // msr amevcntvoff00_el2, x0
    b    fail
after_el1_amevcntvoff00_msr:
    CHECK_HANDLED 21

    LOAD_EXPECT x0, 0x2601, 0x2602, 0x2603, 0x2604
el1_amevcntvoff10_mrs_insn:
    .inst 0xD53CDA00                  // mrs x0, amevcntvoff10_el2
    b    fail
after_el1_amevcntvoff10_mrs:
    CHECK_HANDLED 22

    LOAD_EXPECT x0, 0x2701, 0x2702, 0x2703, 0x2704
el1_amevcntvoff10_msr_insn:
    .inst 0xD51CDA00                  // msr amevcntvoff10_el2, x0
    b    fail
after_el1_amevcntvoff10_msr:
    CHECK_HANDLED 23

    adr  x3, el0_amcfgr_mrs_entry
    msr  elr_el1, x3
    msr  spsr_el1, x4
    eret
after_el0_amcfgr_mrs:
    CHECK_HANDLED 24

    adr  x3, el0_amcr_mrs_entry
    msr  elr_el1, x3
    msr  spsr_el1, x4
    eret
after_el0_amcr_mrs:
    CHECK_HANDLED 25

    adr  x3, el0_amevcntr00_mrs_entry
    msr  elr_el1, x3
    msr  spsr_el1, x4
    eret
after_el0_amevcntr00_mrs:
    CHECK_HANDLED 26

    adr  x3, el0_amevtyper00_mrs_entry
    msr  elr_el1, x3
    msr  spsr_el1, x4
    eret
after_el0_amevtyper00_mrs:
    CHECK_HANDLED 27

    adr  x3, el0_amcg1idr_mrs_entry
    msr  elr_el1, x3
    msr  spsr_el1, x4
    eret
after_el0_amcg1idr_mrs:
    CHECK_HANDLED 28

    adr  x3, el0_amevcntvoff00_mrs_entry
    msr  elr_el1, x3
    msr  spsr_el1, x4
    eret
after_el0_amevcntvoff00_mrs:
    CHECK_HANDLED 29

    movz x0, #'A'
    str  x0, [x15]
    movz x0, #'\n'
    str  x0, [x15]
    b    exit_ok

el0_amcfgr_mrs_entry:
    LOAD_EXPECT x0, 0xA001, 0xA002, 0xA003, 0xA004
el0_amcfgr_mrs_insn:
    .inst 0xD53BD220                  // mrs x0, amcfgr_el0
    b    fail

el0_amcr_mrs_entry:
    LOAD_EXPECT x0, 0xA101, 0xA102, 0xA103, 0xA104
el0_amcr_mrs_insn:
    .inst 0xD53BD200                  // mrs x0, amcr_el0
    b    fail

el0_amevcntr00_mrs_entry:
    LOAD_EXPECT x0, 0xA201, 0xA202, 0xA203, 0xA204
el0_amevcntr00_mrs_insn:
    .inst 0xD53BD400                  // mrs x0, amevcntr00_el0
    b    fail

el0_amevtyper00_mrs_entry:
    LOAD_EXPECT x0, 0xA301, 0xA302, 0xA303, 0xA304
el0_amevtyper00_mrs_insn:
    .inst 0xD53BD600                  // mrs x0, amevtyper00_el0
    b    fail

el0_amcg1idr_mrs_entry:
    LOAD_EXPECT x0, 0xA401, 0xA402, 0xA403, 0xA404
el0_amcg1idr_mrs_insn:
    .inst 0xD53BD2C0                  // mrs x0, amcg1idr_el0
    b    fail

el0_amevcntvoff00_mrs_entry:
    LOAD_EXPECT x0, 0xA501, 0xA502, 0xA503, 0xA504
el0_amevcntvoff00_mrs_insn:
    .inst 0xD53CD800                  // mrs x0, amevcntvoff00_el2
    b    fail

fail:
    movz x0, #'F'
    str  x0, [x15]
    movz x0, #'\n'
    str  x0, [x15]
2:
    wfi
    b    2b

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
    mrs  x20, esr_el1
    ubfx x22, x20, #26, #6
    cbnz x22, handler_fail
    tbz  x20, #25, handler_fail
    and  x23, x20, #0x1ffffff
    cbnz x23, handler_fail
    mrs  x23, far_el1
    cbnz x23, handler_fail

    mrs  x26, elr_el1
    HANDLE_EL1_CASE el1_amcfgr_mrs_insn, after_el1_amcfgr_mrs, 1, 0x1101, 0x1102, 0x1103, 0x1104
    HANDLE_EL1_CASE el1_amcgcr_mrs_insn, after_el1_amcgcr_mrs, 2, 0x1201, 0x1202, 0x1203, 0x1204
    HANDLE_EL1_CASE el1_amcg1idr_mrs_insn, after_el1_amcg1idr_mrs, 3, 0x1301, 0x1302, 0x1303, 0x1304
    HANDLE_EL1_CASE el1_amcr_mrs_insn, after_el1_amcr_mrs, 4, 0x1401, 0x1402, 0x1403, 0x1404
    HANDLE_EL1_CASE el1_amcr_msr_insn, after_el1_amcr_msr, 5, 0x1501, 0x1502, 0x1503, 0x1504
    HANDLE_EL1_CASE el1_amcntenclr0_mrs_insn, after_el1_amcntenclr0_mrs, 6, 0x1601, 0x1602, 0x1603, 0x1604
    HANDLE_EL1_CASE el1_amcntenclr0_msr_insn, after_el1_amcntenclr0_msr, 7, 0x1701, 0x1702, 0x1703, 0x1704
    HANDLE_EL1_CASE el1_amcntenset0_mrs_insn, after_el1_amcntenset0_mrs, 8, 0x1801, 0x1802, 0x1803, 0x1804
    HANDLE_EL1_CASE el1_amcntenset0_msr_insn, after_el1_amcntenset0_msr, 9, 0x1901, 0x1902, 0x1903, 0x1904
    HANDLE_EL1_CASE el1_amcntenclr1_mrs_insn, after_el1_amcntenclr1_mrs, 10, 0x1A01, 0x1A02, 0x1A03, 0x1A04
    HANDLE_EL1_CASE el1_amcntenclr1_msr_insn, after_el1_amcntenclr1_msr, 11, 0x1B01, 0x1B02, 0x1B03, 0x1B04
    HANDLE_EL1_CASE el1_amcntenset1_mrs_insn, after_el1_amcntenset1_mrs, 12, 0x1C01, 0x1C02, 0x1C03, 0x1C04
    HANDLE_EL1_CASE el1_amcntenset1_msr_insn, after_el1_amcntenset1_msr, 13, 0x1D01, 0x1D02, 0x1D03, 0x1D04
    HANDLE_EL1_CASE el1_amevcntr00_mrs_insn, after_el1_amevcntr00_mrs, 14, 0x1E01, 0x1E02, 0x1E03, 0x1E04
    HANDLE_EL1_CASE el1_amevcntr00_msr_insn, after_el1_amevcntr00_msr, 15, 0x1F01, 0x1F02, 0x1F03, 0x1F04
    HANDLE_EL1_CASE el1_amevtyper00_mrs_insn, after_el1_amevtyper00_mrs, 16, 0x2001, 0x2002, 0x2003, 0x2004
    HANDLE_EL1_CASE el1_amevcntr10_mrs_insn, after_el1_amevcntr10_mrs, 17, 0x2101, 0x2102, 0x2103, 0x2104
    HANDLE_EL1_CASE el1_amevcntr10_msr_insn, after_el1_amevcntr10_msr, 18, 0x2201, 0x2202, 0x2203, 0x2204
    HANDLE_EL1_CASE el1_amevtyper10_mrs_insn, after_el1_amevtyper10_mrs, 19, 0x2301, 0x2302, 0x2303, 0x2304
    HANDLE_EL1_CASE el1_amevcntvoff00_mrs_insn, after_el1_amevcntvoff00_mrs, 20, 0x2401, 0x2402, 0x2403, 0x2404
    HANDLE_EL1_CASE el1_amevcntvoff00_msr_insn, after_el1_amevcntvoff00_msr, 21, 0x2501, 0x2502, 0x2503, 0x2504
    HANDLE_EL1_CASE el1_amevcntvoff10_mrs_insn, after_el1_amevcntvoff10_mrs, 22, 0x2601, 0x2602, 0x2603, 0x2604
    HANDLE_EL1_CASE el1_amevcntvoff10_msr_insn, after_el1_amevcntvoff10_msr, 23, 0x2701, 0x2702, 0x2703, 0x2704
    b    handler_fail

lower_sync_handler:
    mrs  x20, esr_el1
    mrs  x25, spsr_el1
    tbnz x25, #20, handler_fail
    and  x24, x25, #0xf
    cbnz x24, handler_fail
    ubfx x24, x25, #28, #4
    cmp  x24, #0x9
    b.ne handler_fail
    ubfx x24, x25, #6, #4
    cmp  x24, #0xa
    b.ne handler_fail
    tbz  x25, #22, handler_fail
    ubfx x22, x20, #26, #6
    cbnz x22, handler_fail
    tbz  x20, #25, handler_fail
    and  x23, x20, #0x1ffffff
    cbnz x23, handler_fail
    mrs  x23, far_el1
    cbnz x23, handler_fail

    mrs  x26, elr_el1
    HANDLE_EL0_CASE el0_amcfgr_mrs_insn, after_el0_amcfgr_mrs, 24, 0xA001, 0xA002, 0xA003, 0xA004
    HANDLE_EL0_CASE el0_amcr_mrs_insn, after_el0_amcr_mrs, 25, 0xA101, 0xA102, 0xA103, 0xA104
    HANDLE_EL0_CASE el0_amevcntr00_mrs_insn, after_el0_amevcntr00_mrs, 26, 0xA201, 0xA202, 0xA203, 0xA204
    HANDLE_EL0_CASE el0_amevtyper00_mrs_insn, after_el0_amevtyper00_mrs, 27, 0xA301, 0xA302, 0xA303, 0xA304
    HANDLE_EL0_CASE el0_amcg1idr_mrs_insn, after_el0_amcg1idr_mrs, 28, 0xA401, 0xA402, 0xA403, 0xA404
    HANDLE_EL0_CASE el0_amevcntvoff00_mrs_insn, after_el0_amevcntvoff00_mrs, 29, 0xA501, 0xA502, 0xA503, 0xA504
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
