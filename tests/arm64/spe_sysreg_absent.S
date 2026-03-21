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

    // EL1: PMECR_EL1 (absent -> UNDEFINED)
    LOAD_EXPECT x0, 0x1101, 0x1102, 0x1103, 0x1104
el1_pmecr_mrs_insn:
    .inst 0xD5389EA0                  // mrs x0, pmecr_el1
    b    fail
after_el1_pmecr_mrs:
    CHECK_HANDLED 1

    LOAD_EXPECT x0, 0x1201, 0x1202, 0x1203, 0x1204
el1_pmecr_msr_insn:
    .inst 0xD5189EA0                  // msr pmecr_el1, x0
    b    fail
after_el1_pmecr_msr:
    CHECK_HANDLED 2

    // EL1: PMIAR_EL1 (absent -> UNDEFINED)
    LOAD_EXPECT x0, 0x1301, 0x1302, 0x1303, 0x1304
el1_pmiar_mrs_insn:
    .inst 0xD5389EE0                  // mrs x0, pmiar_el1
    b    fail
after_el1_pmiar_mrs:
    CHECK_HANDLED 3

    LOAD_EXPECT x0, 0x1401, 0x1402, 0x1403, 0x1404
el1_pmiar_msr_insn:
    .inst 0xD5189EE0                  // msr pmiar_el1, x0
    b    fail
after_el1_pmiar_msr:
    CHECK_HANDLED 4

    // EL1: PMSCR_EL1 (absent -> UNDEFINED)
    LOAD_EXPECT x0, 0x1501, 0x1502, 0x1503, 0x1504
el1_pmscr_mrs_insn:
    .inst 0xD5389900                  // mrs x0, pmscr_el1
    b    fail
after_el1_pmscr_mrs:
    CHECK_HANDLED 5

    LOAD_EXPECT x0, 0x1601, 0x1602, 0x1603, 0x1604
el1_pmscr_msr_insn:
    .inst 0xD5189900                  // msr pmscr_el1, x0
    b    fail
after_el1_pmscr_msr:
    CHECK_HANDLED 6

    // EL1: PMSCR_EL2 (absent -> UNDEFINED)
    LOAD_EXPECT x0, 0x1701, 0x1702, 0x1703, 0x1704
el1_pmscr_el2_mrs_insn:
    .inst 0xD53C9900                  // mrs x0, pmscr_el2
    b    fail
after_el1_pmscr_el2_mrs:
    CHECK_HANDLED 7

    LOAD_EXPECT x0, 0x1801, 0x1802, 0x1803, 0x1804
el1_pmscr_el2_msr_insn:
    .inst 0xD51C9900                  // msr pmscr_el2, x0
    b    fail
after_el1_pmscr_el2_msr:
    CHECK_HANDLED 8

    // EL1: PMSDSFR_EL1 (absent -> UNDEFINED)
    LOAD_EXPECT x0, 0x1901, 0x1902, 0x1903, 0x1904
el1_pmsdsfr_mrs_insn:
    .inst 0xD5389A80                  // mrs x0, pmsdsfr_el1
    b    fail
after_el1_pmsdsfr_mrs:
    CHECK_HANDLED 9

    LOAD_EXPECT x0, 0x1A01, 0x1A02, 0x1A03, 0x1A04
el1_pmsdsfr_msr_insn:
    .inst 0xD5189A80                  // msr pmsdsfr_el1, x0
    b    fail
after_el1_pmsdsfr_msr:
    CHECK_HANDLED 10

    // EL1: PMSEVFR_EL1 (absent -> UNDEFINED)
    LOAD_EXPECT x0, 0x1B01, 0x1B02, 0x1B03, 0x1B04
el1_pmsevfr_mrs_insn:
    .inst 0xD5389920                  // mrs x0, pmsevfr_el1
    b    fail
after_el1_pmsevfr_mrs:
    CHECK_HANDLED 11

    LOAD_EXPECT x0, 0x1C01, 0x1C02, 0x1C03, 0x1C04
el1_pmsevfr_msr_insn:
    .inst 0xD5189920                  // msr pmsevfr_el1, x0
    b    fail
after_el1_pmsevfr_msr:
    CHECK_HANDLED 12

    // EL1: PMSFCR_EL1 (absent -> UNDEFINED)
    LOAD_EXPECT x0, 0x1D01, 0x1D02, 0x1D03, 0x1D04
el1_pmsfcr_mrs_insn:
    .inst 0xD5389980                  // mrs x0, pmsfcr_el1
    b    fail
after_el1_pmsfcr_mrs:
    CHECK_HANDLED 13

    LOAD_EXPECT x0, 0x1E01, 0x1E02, 0x1E03, 0x1E04
el1_pmsfcr_msr_insn:
    .inst 0xD5189980                  // msr pmsfcr_el1, x0
    b    fail
after_el1_pmsfcr_msr:
    CHECK_HANDLED 14

    // EL1: PMSICR_EL1 (absent -> UNDEFINED)
    LOAD_EXPECT x0, 0x1F01, 0x1F02, 0x1F03, 0x1F04
el1_pmsicr_mrs_insn:
    .inst 0xD5389940                  // mrs x0, pmsicr_el1
    b    fail
after_el1_pmsicr_mrs:
    CHECK_HANDLED 15

    LOAD_EXPECT x0, 0x2001, 0x2002, 0x2003, 0x2004
el1_pmsicr_msr_insn:
    .inst 0xD5189940                  // msr pmsicr_el1, x0
    b    fail
after_el1_pmsicr_msr:
    CHECK_HANDLED 16

    // EL1: PMSIDR_EL1 (absent -> UNDEFINED)
    LOAD_EXPECT x0, 0x2101, 0x2102, 0x2103, 0x2104
el1_pmsidr_mrs_insn:
    .inst 0xD53899E0                  // mrs x0, pmsidr_el1
    b    fail
after_el1_pmsidr_mrs:
    CHECK_HANDLED 17

    // EL1: PMSIRR_EL1 (absent -> UNDEFINED)
    LOAD_EXPECT x0, 0x2201, 0x2202, 0x2203, 0x2204
el1_pmsirr_mrs_insn:
    .inst 0xD5389960                  // mrs x0, pmsirr_el1
    b    fail
after_el1_pmsirr_mrs:
    CHECK_HANDLED 18

    LOAD_EXPECT x0, 0x2301, 0x2302, 0x2303, 0x2304
el1_pmsirr_msr_insn:
    .inst 0xD5189960                  // msr pmsirr_el1, x0
    b    fail
after_el1_pmsirr_msr:
    CHECK_HANDLED 19

    // EL1: PMSLATFR_EL1 (absent -> UNDEFINED)
    LOAD_EXPECT x0, 0x2401, 0x2402, 0x2403, 0x2404
el1_pmslatfr_mrs_insn:
    .inst 0xD53899C0                  // mrs x0, pmslatfr_el1
    b    fail
after_el1_pmslatfr_mrs:
    CHECK_HANDLED 20

    LOAD_EXPECT x0, 0x2501, 0x2502, 0x2503, 0x2504
el1_pmslatfr_msr_insn:
    .inst 0xD51899C0                  // msr pmslatfr_el1, x0
    b    fail
after_el1_pmslatfr_msr:
    CHECK_HANDLED 21

    // EL1: PMSNEVFR_EL1 (absent -> UNDEFINED)
    LOAD_EXPECT x0, 0x2601, 0x2602, 0x2603, 0x2604
el1_pmsnevfr_mrs_insn:
    .inst 0xD5389840                  // mrs x0, pmsnevfr_el1
    b    fail
after_el1_pmsnevfr_mrs:
    CHECK_HANDLED 22

    LOAD_EXPECT x0, 0x2701, 0x2702, 0x2703, 0x2704
el1_pmsnevfr_msr_insn:
    .inst 0xD5189840                  // msr pmsnevfr_el1, x0
    b    fail
after_el1_pmsnevfr_msr:
    CHECK_HANDLED 23

    // Now repeat a representative subset from EL0.
    adr  x3, el0_pmscr_mrs_entry
    msr  elr_el1, x3
    msr  spsr_el1, xzr               // EL0t
    eret
after_el0_pmscr_mrs:
    CHECK_HANDLED 24

    adr  x3, el0_pmscr_msr_entry
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret
after_el0_pmscr_msr:
    CHECK_HANDLED 25

    adr  x3, el0_pmsicr_mrs_entry
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret
after_el0_pmsicr_mrs:
    CHECK_HANDLED 26

    adr  x3, el0_pmsicr_msr_entry
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret
after_el0_pmsicr_msr:
    CHECK_HANDLED 27

    adr  x3, el0_pmslatfr_mrs_entry
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret
after_el0_pmslatfr_mrs:
    CHECK_HANDLED 28

    adr  x3, el0_pmslatfr_msr_entry
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret
after_el0_pmslatfr_msr:
    CHECK_HANDLED 29

    adr  x3, el0_pmecr_mrs_entry
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret
after_el0_pmecr_mrs:
    CHECK_HANDLED 30

    adr  x3, el0_pmiar_mrs_entry
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret
after_el0_pmiar_mrs:
    CHECK_HANDLED 31

    movz x0, #'S'
    str  x0, [x15]
    movz x0, #'\n'
    str  x0, [x15]
    b    exit_ok

el0_pmscr_mrs_entry:
    LOAD_EXPECT x0, 0xA001, 0xA002, 0xA003, 0xA004
el0_pmscr_mrs_insn:
    .inst 0xD5389900                  // mrs x0, pmscr_el1
    b    fail

el0_pmscr_msr_entry:
    LOAD_EXPECT x0, 0xB001, 0xB002, 0xB003, 0xB004
el0_pmscr_msr_insn:
    .inst 0xD5189900                  // msr pmscr_el1, x0
    b    fail

el0_pmsicr_mrs_entry:
    LOAD_EXPECT x0, 0xC001, 0xC002, 0xC003, 0xC004
el0_pmsicr_mrs_insn:
    .inst 0xD5389940                  // mrs x0, pmsicr_el1
    b    fail

el0_pmsicr_msr_entry:
    LOAD_EXPECT x0, 0xD001, 0xD002, 0xD003, 0xD004
el0_pmsicr_msr_insn:
    .inst 0xD5189940                  // msr pmsicr_el1, x0
    b    fail

el0_pmslatfr_mrs_entry:
    LOAD_EXPECT x0, 0xE001, 0xE002, 0xE003, 0xE004
el0_pmslatfr_mrs_insn:
    .inst 0xD53899C0                  // mrs x0, pmslatfr_el1
    b    fail

el0_pmslatfr_msr_entry:
    LOAD_EXPECT x0, 0xE101, 0xE102, 0xE103, 0xE104
el0_pmslatfr_msr_insn:
    .inst 0xD51899C0                  // msr pmslatfr_el1, x0
    b    fail

el0_pmecr_mrs_entry:
    LOAD_EXPECT x0, 0xE201, 0xE202, 0xE203, 0xE204
el0_pmecr_mrs_insn:
    .inst 0xD5389EA0                  // mrs x0, pmecr_el1
    b    fail

el0_pmiar_mrs_entry:
    LOAD_EXPECT x0, 0xE301, 0xE302, 0xE303, 0xE304
el0_pmiar_mrs_insn:
    .inst 0xD5389EE0                  // mrs x0, pmiar_el1
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
    HANDLE_EL1_CASE el1_pmecr_mrs_insn, after_el1_pmecr_mrs, 1, 0x1101, 0x1102, 0x1103, 0x1104
    HANDLE_EL1_CASE el1_pmecr_msr_insn, after_el1_pmecr_msr, 2, 0x1201, 0x1202, 0x1203, 0x1204
    HANDLE_EL1_CASE el1_pmiar_mrs_insn, after_el1_pmiar_mrs, 3, 0x1301, 0x1302, 0x1303, 0x1304
    HANDLE_EL1_CASE el1_pmiar_msr_insn, after_el1_pmiar_msr, 4, 0x1401, 0x1402, 0x1403, 0x1404
    HANDLE_EL1_CASE el1_pmscr_mrs_insn, after_el1_pmscr_mrs, 5, 0x1501, 0x1502, 0x1503, 0x1504
    HANDLE_EL1_CASE el1_pmscr_msr_insn, after_el1_pmscr_msr, 6, 0x1601, 0x1602, 0x1603, 0x1604
    HANDLE_EL1_CASE el1_pmscr_el2_mrs_insn, after_el1_pmscr_el2_mrs, 7, 0x1701, 0x1702, 0x1703, 0x1704
    HANDLE_EL1_CASE el1_pmscr_el2_msr_insn, after_el1_pmscr_el2_msr, 8, 0x1801, 0x1802, 0x1803, 0x1804
    HANDLE_EL1_CASE el1_pmsdsfr_mrs_insn, after_el1_pmsdsfr_mrs, 9, 0x1901, 0x1902, 0x1903, 0x1904
    HANDLE_EL1_CASE el1_pmsdsfr_msr_insn, after_el1_pmsdsfr_msr, 10, 0x1A01, 0x1A02, 0x1A03, 0x1A04
    HANDLE_EL1_CASE el1_pmsevfr_mrs_insn, after_el1_pmsevfr_mrs, 11, 0x1B01, 0x1B02, 0x1B03, 0x1B04
    HANDLE_EL1_CASE el1_pmsevfr_msr_insn, after_el1_pmsevfr_msr, 12, 0x1C01, 0x1C02, 0x1C03, 0x1C04
    HANDLE_EL1_CASE el1_pmsfcr_mrs_insn, after_el1_pmsfcr_mrs, 13, 0x1D01, 0x1D02, 0x1D03, 0x1D04
    HANDLE_EL1_CASE el1_pmsfcr_msr_insn, after_el1_pmsfcr_msr, 14, 0x1E01, 0x1E02, 0x1E03, 0x1E04
    HANDLE_EL1_CASE el1_pmsicr_mrs_insn, after_el1_pmsicr_mrs, 15, 0x1F01, 0x1F02, 0x1F03, 0x1F04
    HANDLE_EL1_CASE el1_pmsicr_msr_insn, after_el1_pmsicr_msr, 16, 0x2001, 0x2002, 0x2003, 0x2004
    HANDLE_EL1_CASE el1_pmsidr_mrs_insn, after_el1_pmsidr_mrs, 17, 0x2101, 0x2102, 0x2103, 0x2104
    HANDLE_EL1_CASE el1_pmsirr_mrs_insn, after_el1_pmsirr_mrs, 18, 0x2201, 0x2202, 0x2203, 0x2204
    HANDLE_EL1_CASE el1_pmsirr_msr_insn, after_el1_pmsirr_msr, 19, 0x2301, 0x2302, 0x2303, 0x2304
    HANDLE_EL1_CASE el1_pmslatfr_mrs_insn, after_el1_pmslatfr_mrs, 20, 0x2401, 0x2402, 0x2403, 0x2404
    HANDLE_EL1_CASE el1_pmslatfr_msr_insn, after_el1_pmslatfr_msr, 21, 0x2501, 0x2502, 0x2503, 0x2504
    HANDLE_EL1_CASE el1_pmsnevfr_mrs_insn, after_el1_pmsnevfr_mrs, 22, 0x2601, 0x2602, 0x2603, 0x2604
    HANDLE_EL1_CASE el1_pmsnevfr_msr_insn, after_el1_pmsnevfr_msr, 23, 0x2701, 0x2702, 0x2703, 0x2704
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

    mrs  x26, elr_el1
    HANDLE_EL0_CASE el0_pmscr_mrs_insn, after_el0_pmscr_mrs, 24, 0xA001, 0xA002, 0xA003, 0xA004
    HANDLE_EL0_CASE el0_pmscr_msr_insn, after_el0_pmscr_msr, 25, 0xB001, 0xB002, 0xB003, 0xB004
    HANDLE_EL0_CASE el0_pmsicr_mrs_insn, after_el0_pmsicr_mrs, 26, 0xC001, 0xC002, 0xC003, 0xC004
    HANDLE_EL0_CASE el0_pmsicr_msr_insn, after_el0_pmsicr_msr, 27, 0xD001, 0xD002, 0xD003, 0xD004
    HANDLE_EL0_CASE el0_pmslatfr_mrs_insn, after_el0_pmslatfr_mrs, 28, 0xE001, 0xE002, 0xE003, 0xE004
    HANDLE_EL0_CASE el0_pmslatfr_msr_insn, after_el0_pmslatfr_msr, 29, 0xE101, 0xE102, 0xE103, 0xE104
    HANDLE_EL0_CASE el0_pmecr_mrs_insn, after_el0_pmecr_mrs, 30, 0xE201, 0xE202, 0xE203, 0xE204
    HANDLE_EL0_CASE el0_pmiar_mrs_insn, after_el0_pmiar_mrs, 31, 0xE301, 0xE302, 0xE303, 0xE304
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

