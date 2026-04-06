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
el1_pmccntsvr_mrs_insn:
    .inst 0xD530EBE0                  // mrs x0, pmccntsvr_el1
    b    fail
after_el1_pmccntsvr_mrs:
    CHECK_HANDLED 1

    LOAD_EXPECT x0, 0x1201, 0x1202, 0x1203, 0x1204
el1_pmicfiltr_mrs_insn:
    .inst 0xD53B9600                  // mrs x0, pmicfiltr_el0
    b    fail
after_el1_pmicfiltr_mrs:
    CHECK_HANDLED 2

    LOAD_EXPECT x0, 0x1301, 0x1302, 0x1303, 0x1304
el1_pmicfiltr_msr_insn:
    .inst 0xD51B9600                  // msr pmicfiltr_el0, x0
    b    fail
after_el1_pmicfiltr_msr:
    CHECK_HANDLED 3

    LOAD_EXPECT x0, 0x1401, 0x1402, 0x1403, 0x1404
el1_pmicntr_mrs_insn:
    .inst 0xD53B9400                  // mrs x0, pmicntr_el0
    b    fail
after_el1_pmicntr_mrs:
    CHECK_HANDLED 4

    LOAD_EXPECT x0, 0x1501, 0x1502, 0x1503, 0x1504
el1_pmicntr_msr_insn:
    .inst 0xD51B9400                  // msr pmicntr_el0, x0
    b    fail
after_el1_pmicntr_msr:
    CHECK_HANDLED 5

    LOAD_EXPECT x0, 0x1601, 0x1602, 0x1603, 0x1604
el1_pmicntsvr_mrs_insn:
    .inst 0xD530EC00                  // mrs x0, pmicntsvr_el1
    b    fail
after_el1_pmicntsvr_mrs:
    CHECK_HANDLED 6

    LOAD_EXPECT x0, 0x1701, 0x1702, 0x1703, 0x1704
el1_pmuacr_mrs_insn:
    .inst 0xD5389E80                  // mrs x0, pmuacr_el1
    b    fail
after_el1_pmuacr_mrs:
    CHECK_HANDLED 7

    LOAD_EXPECT x0, 0x1801, 0x1802, 0x1803, 0x1804
el1_pmuacr_msr_insn:
    .inst 0xD5189E80                  // msr pmuacr_el1, x0
    b    fail
after_el1_pmuacr_msr:
    CHECK_HANDLED 8

    LOAD_EXPECT x0, 0x1901, 0x1902, 0x1903, 0x1904
el1_pmzr_msr_insn:
    .inst 0xD51B9D80                  // msr pmzr_el0, x0
    b    fail
after_el1_pmzr_msr:
    CHECK_HANDLED 9

    adr  x3, el0_pmccntsvr_mrs_entry
    msr  elr_el1, x3
    msr  spsr_el1, x4
    eret
after_el0_pmccntsvr_mrs:
    CHECK_HANDLED 10

    adr  x3, el0_pmicfiltr_mrs_entry
    msr  elr_el1, x3
    msr  spsr_el1, x4
    eret
after_el0_pmicfiltr_mrs:
    CHECK_HANDLED 11

    adr  x3, el0_pmicfiltr_msr_entry
    msr  elr_el1, x3
    msr  spsr_el1, x4
    eret
after_el0_pmicfiltr_msr:
    CHECK_HANDLED 12

    adr  x3, el0_pmicntr_mrs_entry
    msr  elr_el1, x3
    msr  spsr_el1, x4
    eret
after_el0_pmicntr_mrs:
    CHECK_HANDLED 13

    adr  x3, el0_pmicntr_msr_entry
    msr  elr_el1, x3
    msr  spsr_el1, x4
    eret
after_el0_pmicntr_msr:
    CHECK_HANDLED 14

    adr  x3, el0_pmicntsvr_mrs_entry
    msr  elr_el1, x3
    msr  spsr_el1, x4
    eret
after_el0_pmicntsvr_mrs:
    CHECK_HANDLED 15

    adr  x3, el0_pmuacr_mrs_entry
    msr  elr_el1, x3
    msr  spsr_el1, x4
    eret
after_el0_pmuacr_mrs:
    CHECK_HANDLED 16

    adr  x3, el0_pmuacr_msr_entry
    msr  elr_el1, x3
    msr  spsr_el1, x4
    eret
after_el0_pmuacr_msr:
    CHECK_HANDLED 17

    adr  x3, el0_pmzr_msr_entry
    msr  elr_el1, x3
    msr  spsr_el1, x4
    eret
after_el0_pmzr_msr:
    CHECK_HANDLED 18

    movz x0, #'R'
    str  x0, [x15]
    movz x0, #'\n'
    str  x0, [x15]
1:
    wfi
    b    1b

el0_pmccntsvr_mrs_entry:
    LOAD_EXPECT x0, 0x2101, 0x2102, 0x2103, 0x2104
el0_pmccntsvr_mrs_insn:
    .inst 0xD530EBE0                  // mrs x0, pmccntsvr_el1
    b    fail

el0_pmicfiltr_mrs_entry:
    LOAD_EXPECT x0, 0x2201, 0x2202, 0x2203, 0x2204
el0_pmicfiltr_mrs_insn:
    .inst 0xD53B9600                  // mrs x0, pmicfiltr_el0
    b    fail

el0_pmicfiltr_msr_entry:
    LOAD_EXPECT x0, 0x2301, 0x2302, 0x2303, 0x2304
el0_pmicfiltr_msr_insn:
    .inst 0xD51B9600                  // msr pmicfiltr_el0, x0
    b    fail

el0_pmicntr_mrs_entry:
    LOAD_EXPECT x0, 0x2401, 0x2402, 0x2403, 0x2404
el0_pmicntr_mrs_insn:
    .inst 0xD53B9400                  // mrs x0, pmicntr_el0
    b    fail

el0_pmicntr_msr_entry:
    LOAD_EXPECT x0, 0x2501, 0x2502, 0x2503, 0x2504
el0_pmicntr_msr_insn:
    .inst 0xD51B9400                  // msr pmicntr_el0, x0
    b    fail

el0_pmicntsvr_mrs_entry:
    LOAD_EXPECT x0, 0x2601, 0x2602, 0x2603, 0x2604
el0_pmicntsvr_mrs_insn:
    .inst 0xD530EC00                  // mrs x0, pmicntsvr_el1
    b    fail

el0_pmuacr_mrs_entry:
    LOAD_EXPECT x0, 0x2701, 0x2702, 0x2703, 0x2704
el0_pmuacr_mrs_insn:
    .inst 0xD5389E80                  // mrs x0, pmuacr_el1
    b    fail

el0_pmuacr_msr_entry:
    LOAD_EXPECT x0, 0x2801, 0x2802, 0x2803, 0x2804
el0_pmuacr_msr_insn:
    .inst 0xD5189E80                  // msr pmuacr_el1, x0
    b    fail

el0_pmzr_msr_entry:
    LOAD_EXPECT x0, 0x2901, 0x2902, 0x2903, 0x2904
el0_pmzr_msr_insn:
    .inst 0xD51B9D80                  // msr pmzr_el0, x0
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
    HANDLE_EL1_CASE el1_pmccntsvr_mrs_insn, after_el1_pmccntsvr_mrs, 1, 0x1101, 0x1102, 0x1103, 0x1104
    HANDLE_EL1_CASE el1_pmicfiltr_mrs_insn, after_el1_pmicfiltr_mrs, 2, 0x1201, 0x1202, 0x1203, 0x1204
    HANDLE_EL1_CASE el1_pmicfiltr_msr_insn, after_el1_pmicfiltr_msr, 3, 0x1301, 0x1302, 0x1303, 0x1304
    HANDLE_EL1_CASE el1_pmicntr_mrs_insn, after_el1_pmicntr_mrs, 4, 0x1401, 0x1402, 0x1403, 0x1404
    HANDLE_EL1_CASE el1_pmicntr_msr_insn, after_el1_pmicntr_msr, 5, 0x1501, 0x1502, 0x1503, 0x1504
    HANDLE_EL1_CASE el1_pmicntsvr_mrs_insn, after_el1_pmicntsvr_mrs, 6, 0x1601, 0x1602, 0x1603, 0x1604
    HANDLE_EL1_CASE el1_pmuacr_mrs_insn, after_el1_pmuacr_mrs, 7, 0x1701, 0x1702, 0x1703, 0x1704
    HANDLE_EL1_CASE el1_pmuacr_msr_insn, after_el1_pmuacr_msr, 8, 0x1801, 0x1802, 0x1803, 0x1804
    HANDLE_EL1_CASE el1_pmzr_msr_insn, after_el1_pmzr_msr, 9, 0x1901, 0x1902, 0x1903, 0x1904
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
    HANDLE_EL0_CASE el0_pmccntsvr_mrs_insn, after_el0_pmccntsvr_mrs, 10, 0x2101, 0x2102, 0x2103, 0x2104
    HANDLE_EL0_CASE el0_pmicfiltr_mrs_insn, after_el0_pmicfiltr_mrs, 11, 0x2201, 0x2202, 0x2203, 0x2204
    HANDLE_EL0_CASE el0_pmicfiltr_msr_insn, after_el0_pmicfiltr_msr, 12, 0x2301, 0x2302, 0x2303, 0x2304
    HANDLE_EL0_CASE el0_pmicntr_mrs_insn, after_el0_pmicntr_mrs, 13, 0x2401, 0x2402, 0x2403, 0x2404
    HANDLE_EL0_CASE el0_pmicntr_msr_insn, after_el0_pmicntr_msr, 14, 0x2501, 0x2502, 0x2503, 0x2504
    HANDLE_EL0_CASE el0_pmicntsvr_mrs_insn, after_el0_pmicntsvr_mrs, 15, 0x2601, 0x2602, 0x2603, 0x2604
    HANDLE_EL0_CASE el0_pmuacr_mrs_insn, after_el0_pmuacr_mrs, 16, 0x2701, 0x2702, 0x2703, 0x2704
    HANDLE_EL0_CASE el0_pmuacr_msr_insn, after_el0_pmuacr_msr, 17, 0x2801, 0x2802, 0x2803, 0x2804
    HANDLE_EL0_CASE el0_pmzr_msr_insn, after_el0_pmzr_msr, 18, 0x2901, 0x2902, 0x2903, 0x2904
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
