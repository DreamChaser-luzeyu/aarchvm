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
el1_pmbidr_mrs_insn:
    .inst 0xD5389AE0                  // mrs x0, pmbidr_el1
    b    fail
after_el1_pmbidr_mrs:
    CHECK_HANDLED 1

    LOAD_EXPECT x0, 0x1201, 0x1202, 0x1203, 0x1204
el1_pmblimitr_mrs_insn:
    .inst 0xD5389A00                  // mrs x0, pmblimitr_el1
    b    fail
after_el1_pmblimitr_mrs:
    CHECK_HANDLED 2

    LOAD_EXPECT x0, 0x1301, 0x1302, 0x1303, 0x1304
el1_pmblimitr_msr_insn:
    .inst 0xD5189A00                  // msr pmblimitr_el1, x0
    b    fail
after_el1_pmblimitr_msr:
    CHECK_HANDLED 3

    LOAD_EXPECT x0, 0x1401, 0x1402, 0x1403, 0x1404
el1_pmbmar_mrs_insn:
    .inst 0xD5389AA0                  // mrs x0, pmbmar_el1
    b    fail
after_el1_pmbmar_mrs:
    CHECK_HANDLED 4

    LOAD_EXPECT x0, 0x1501, 0x1502, 0x1503, 0x1504
el1_pmbmar_msr_insn:
    .inst 0xD5189AA0                  // msr pmbmar_el1, x0
    b    fail
after_el1_pmbmar_msr:
    CHECK_HANDLED 5

    LOAD_EXPECT x0, 0x1601, 0x1602, 0x1603, 0x1604
el1_pmbptr_mrs_insn:
    .inst 0xD5389A20                  // mrs x0, pmbptr_el1
    b    fail
after_el1_pmbptr_mrs:
    CHECK_HANDLED 6

    LOAD_EXPECT x0, 0x1701, 0x1702, 0x1703, 0x1704
el1_pmbptr_msr_insn:
    .inst 0xD5189A20                  // msr pmbptr_el1, x0
    b    fail
after_el1_pmbptr_msr:
    CHECK_HANDLED 7

    LOAD_EXPECT x0, 0x1801, 0x1802, 0x1803, 0x1804
el1_pmbsr_mrs_insn:
    .inst 0xD5389A60                  // mrs x0, pmbsr_el1
    b    fail
after_el1_pmbsr_mrs:
    CHECK_HANDLED 8

    LOAD_EXPECT x0, 0x1901, 0x1902, 0x1903, 0x1904
el1_pmbsr_msr_insn:
    .inst 0xD5189A60                  // msr pmbsr_el1, x0
    b    fail
after_el1_pmbsr_msr:
    CHECK_HANDLED 9

    LOAD_EXPECT x0, 0x1A01, 0x1A02, 0x1A03, 0x1A04
el1_pmbsr_el2_mrs_insn:
    .inst 0xD53C9A60                  // mrs x0, pmbsr_el2
    b    fail
after_el1_pmbsr_el2_mrs:
    CHECK_HANDLED 10

    LOAD_EXPECT x0, 0x1B01, 0x1B02, 0x1B03, 0x1B04
el1_pmbsr_el2_msr_insn:
    .inst 0xD51C9A60                  // msr pmbsr_el2, x0
    b    fail
after_el1_pmbsr_el2_msr:
    CHECK_HANDLED 11

    LOAD_EXPECT x0, 0x1C01, 0x1C02, 0x1C03, 0x1C04
el1_pmbsr_el3_mrs_insn:
    .inst 0xD53E9A60                  // mrs x0, pmbsr_el3
    b    fail
after_el1_pmbsr_el3_mrs:
    CHECK_HANDLED 12

    LOAD_EXPECT x0, 0x1D01, 0x1D02, 0x1D03, 0x1D04
el1_pmbsr_el3_msr_insn:
    .inst 0xD51E9A60                  // msr pmbsr_el3, x0
    b    fail
after_el1_pmbsr_el3_msr:
    CHECK_HANDLED 13

    adr  x3, el0_pmbidr_mrs_entry
    msr  elr_el1, x3
    msr  spsr_el1, x4
    eret
after_el0_pmbidr_mrs:
    CHECK_HANDLED 14

    adr  x3, el0_pmblimitr_msr_entry
    msr  elr_el1, x3
    msr  spsr_el1, x4
    eret
after_el0_pmblimitr_msr:
    CHECK_HANDLED 15

    adr  x3, el0_pmbmar_mrs_entry
    msr  elr_el1, x3
    msr  spsr_el1, x4
    eret
after_el0_pmbmar_mrs:
    CHECK_HANDLED 16

    adr  x3, el0_pmbptr_msr_entry
    msr  elr_el1, x3
    msr  spsr_el1, x4
    eret
after_el0_pmbptr_msr:
    CHECK_HANDLED 17

    adr  x3, el0_pmbsr_el1_mrs_entry
    msr  elr_el1, x3
    msr  spsr_el1, x4
    eret
after_el0_pmbsr_el1_mrs:
    CHECK_HANDLED 18

    adr  x3, el0_pmbsr_el2_msr_entry
    msr  elr_el1, x3
    msr  spsr_el1, x4
    eret
after_el0_pmbsr_el2_msr:
    CHECK_HANDLED 19

    movz x0, #'B'
    str  x0, [x15]
    movz x0, #'\n'
    str  x0, [x15]
    b    exit_ok

el0_pmbidr_mrs_entry:
    LOAD_EXPECT x0, 0xA001, 0xA002, 0xA003, 0xA004
el0_pmbidr_mrs_insn:
    .inst 0xD5389AE0                  // mrs x0, pmbidr_el1
    b    fail

el0_pmblimitr_msr_entry:
    LOAD_EXPECT x0, 0xB001, 0xB002, 0xB003, 0xB004
el0_pmblimitr_msr_insn:
    .inst 0xD5189A00                  // msr pmblimitr_el1, x0
    b    fail

el0_pmbmar_mrs_entry:
    LOAD_EXPECT x0, 0xC001, 0xC002, 0xC003, 0xC004
el0_pmbmar_mrs_insn:
    .inst 0xD5389AA0                  // mrs x0, pmbmar_el1
    b    fail

el0_pmbptr_msr_entry:
    LOAD_EXPECT x0, 0xD001, 0xD002, 0xD003, 0xD004
el0_pmbptr_msr_insn:
    .inst 0xD5189A20                  // msr pmbptr_el1, x0
    b    fail

el0_pmbsr_el1_mrs_entry:
    LOAD_EXPECT x0, 0xE001, 0xE002, 0xE003, 0xE004
el0_pmbsr_el1_mrs_insn:
    .inst 0xD5389A60                  // mrs x0, pmbsr_el1
    b    fail

el0_pmbsr_el2_msr_entry:
    LOAD_EXPECT x0, 0xE101, 0xE102, 0xE103, 0xE104
el0_pmbsr_el2_msr_insn:
    .inst 0xD51C9A60                  // msr pmbsr_el2, x0
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
    HANDLE_EL1_CASE el1_pmbidr_mrs_insn, after_el1_pmbidr_mrs, 1, 0x1101, 0x1102, 0x1103, 0x1104
    HANDLE_EL1_CASE el1_pmblimitr_mrs_insn, after_el1_pmblimitr_mrs, 2, 0x1201, 0x1202, 0x1203, 0x1204
    HANDLE_EL1_CASE el1_pmblimitr_msr_insn, after_el1_pmblimitr_msr, 3, 0x1301, 0x1302, 0x1303, 0x1304
    HANDLE_EL1_CASE el1_pmbmar_mrs_insn, after_el1_pmbmar_mrs, 4, 0x1401, 0x1402, 0x1403, 0x1404
    HANDLE_EL1_CASE el1_pmbmar_msr_insn, after_el1_pmbmar_msr, 5, 0x1501, 0x1502, 0x1503, 0x1504
    HANDLE_EL1_CASE el1_pmbptr_mrs_insn, after_el1_pmbptr_mrs, 6, 0x1601, 0x1602, 0x1603, 0x1604
    HANDLE_EL1_CASE el1_pmbptr_msr_insn, after_el1_pmbptr_msr, 7, 0x1701, 0x1702, 0x1703, 0x1704
    HANDLE_EL1_CASE el1_pmbsr_mrs_insn, after_el1_pmbsr_mrs, 8, 0x1801, 0x1802, 0x1803, 0x1804
    HANDLE_EL1_CASE el1_pmbsr_msr_insn, after_el1_pmbsr_msr, 9, 0x1901, 0x1902, 0x1903, 0x1904
    HANDLE_EL1_CASE el1_pmbsr_el2_mrs_insn, after_el1_pmbsr_el2_mrs, 10, 0x1A01, 0x1A02, 0x1A03, 0x1A04
    HANDLE_EL1_CASE el1_pmbsr_el2_msr_insn, after_el1_pmbsr_el2_msr, 11, 0x1B01, 0x1B02, 0x1B03, 0x1B04
    HANDLE_EL1_CASE el1_pmbsr_el3_mrs_insn, after_el1_pmbsr_el3_mrs, 12, 0x1C01, 0x1C02, 0x1C03, 0x1C04
    HANDLE_EL1_CASE el1_pmbsr_el3_msr_insn, after_el1_pmbsr_el3_msr, 13, 0x1D01, 0x1D02, 0x1D03, 0x1D04
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
    HANDLE_EL0_CASE el0_pmbidr_mrs_insn, after_el0_pmbidr_mrs, 14, 0xA001, 0xA002, 0xA003, 0xA004
    HANDLE_EL0_CASE el0_pmblimitr_msr_insn, after_el0_pmblimitr_msr, 15, 0xB001, 0xB002, 0xB003, 0xB004
    HANDLE_EL0_CASE el0_pmbmar_mrs_insn, after_el0_pmbmar_mrs, 16, 0xC001, 0xC002, 0xC003, 0xC004
    HANDLE_EL0_CASE el0_pmbptr_msr_insn, after_el0_pmbptr_msr, 17, 0xD001, 0xD002, 0xD003, 0xD004
    HANDLE_EL0_CASE el0_pmbsr_el1_mrs_insn, after_el0_pmbsr_el1_mrs, 18, 0xE001, 0xE002, 0xE003, 0xE004
    HANDLE_EL0_CASE el0_pmbsr_el2_msr_insn, after_el0_pmbsr_el2_msr, 19, 0xE101, 0xE102, 0xE103, 0xE104
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
