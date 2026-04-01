    .arch armv8.8-a+mops
    .text
    .global _start

    .macro LOAD_IMM64 reg, imm0, imm1, imm2, imm3
    movz \reg, #\imm0
    movk \reg, #\imm1, lsl #16
    movk \reg, #\imm2, lsl #32
    movk \reg, #\imm3, lsl #48
    .endm

    .macro CHECK_HANDLED value
    ldr  x24, [x23]
    movz x25, #\value
    cmp  x24, x25
    b.ne fail
    .endm

    .macro CHECK_REG_IMM reg, imm0, imm1, imm2, imm3
    LOAD_IMM64 x24, \imm0, \imm1, \imm2, \imm3
    cmp  \reg, x24
    b.ne fail
    .endm

    .macro CHECK_ADDR reg, label
    adr  x24, \label
    cmp  \reg, x24
    b.ne fail
    .endm

    .macro HANDLE_CASE insn_label, after_label, value
    adr  x27, \insn_label
    cmp  x26, x27
    b.ne .Lskip\@
    movz x30, #\value
    str  x30, [x23]
    movz x24, #0x5
    msr  spsr_el1, x24
    adr  x30, \after_label
    msr  elr_el1, x30
    eret
.Lskip\@:
    .endm

_start:
    movz x20, #0x0000
    movk x20, #0x0900, lsl #16      // UART
    movz x21, #0x0000
    movk x21, #0x0902, lsl #16      // PERF mailbox

    adr  x22, vectors
    msr  vbar_el1, x22

    adr  x23, handled
    str  xzr, [x23]

    adr  x0, set_dst0
    LOAD_IMM64 x1, 0x1111, 0x2222, 0x3333, 0x4444
    LOAD_IMM64 x2, 0x5555, 0x6666, 0x7777, 0x8888
el1_setp_insn:
    .inst 0x19C20420                // SETP [X0]!, X1!, X2
    b    fail
after_el1_setp:
    CHECK_HANDLED 1
    CHECK_ADDR x0, set_dst0
    CHECK_REG_IMM x1, 0x1111, 0x2222, 0x3333, 0x4444
    CHECK_REG_IMM x2, 0x5555, 0x6666, 0x7777, 0x8888

    adr  x0, set_dst1
    LOAD_IMM64 x1, 0x9999, 0xAAAA, 0xBBBB, 0xCCCC
    LOAD_IMM64 x2, 0xD00D, 0xFACE, 0x2468, 0x1357
el1_setm_insn:
    .inst 0x19C24420                // SETM [X0]!, X1!, X2
    b    fail
after_el1_setm:
    CHECK_HANDLED 2
    CHECK_ADDR x0, set_dst1
    CHECK_REG_IMM x1, 0x9999, 0xAAAA, 0xBBBB, 0xCCCC
    CHECK_REG_IMM x2, 0xD00D, 0xFACE, 0x2468, 0x1357

    adr  x0, set_dst2
    LOAD_IMM64 x1, 0x3141, 0x5926, 0x5358, 0x9793
    LOAD_IMM64 x2, 0x2718, 0x2818, 0x2845, 0x9045
el1_sete_insn:
    .inst 0x19C28420                // SETE [X0]!, X1!, X2
    b    fail
after_el1_sete:
    CHECK_HANDLED 3
    CHECK_ADDR x0, set_dst2
    CHECK_REG_IMM x1, 0x3141, 0x5926, 0x5358, 0x9793
    CHECK_REG_IMM x2, 0x2718, 0x2818, 0x2845, 0x9045

    adr  x3, copy_dst0
    adr  x4, copy_src0
    LOAD_IMM64 x5, 0x1618, 0x0339, 0x8874, 0x9894
el1_cpyp_insn:
    .inst 0x1D0404A3                // CPYP [X3]!, [X4]!, X5!
    b    fail
after_el1_cpyp:
    CHECK_HANDLED 4
    CHECK_ADDR x3, copy_dst0
    CHECK_ADDR x4, copy_src0
    CHECK_REG_IMM x5, 0x1618, 0x0339, 0x8874, 0x9894

    adr  x3, copy_dst1
    adr  x4, copy_src1
    LOAD_IMM64 x5, 0x2643, 0x3832, 0x7950, 0x2884
el1_cpym_insn:
    .inst 0x1D4404A3                // CPYM [X3]!, [X4]!, X5!
    b    fail
after_el1_cpym:
    CHECK_HANDLED 5
    CHECK_ADDR x3, copy_dst1
    CHECK_ADDR x4, copy_src1
    CHECK_REG_IMM x5, 0x2643, 0x3832, 0x7950, 0x2884

    adr  x3, copy_dst2
    adr  x4, copy_src2
    LOAD_IMM64 x5, 0xA1B2, 0xC3D4, 0xE5F6, 0x1357
el1_cpye_insn:
    .inst 0x1D8404A3                // CPYE [X3]!, [X4]!, X5!
    b    fail
after_el1_cpye:
    CHECK_HANDLED 6
    CHECK_ADDR x3, copy_dst2
    CHECK_ADDR x4, copy_src2
    CHECK_REG_IMM x5, 0xA1B2, 0xC3D4, 0xE5F6, 0x1357

    adr  x24, el0_setp_entry
    msr  elr_el1, x24
    msr  spsr_el1, xzr
    eret
after_el0_setp:
    CHECK_HANDLED 7
    CHECK_ADDR x8, set_dst0
    CHECK_REG_IMM x9, 0x0A0A, 0x0B0B, 0x0C0C, 0x0D0D
    CHECK_REG_IMM x10, 0x0E0E, 0x0F0F, 0x1010, 0x1111

    adr  x24, el0_setm_entry
    msr  elr_el1, x24
    msr  spsr_el1, xzr
    eret
after_el0_setm:
    CHECK_HANDLED 8
    CHECK_ADDR x8, set_dst1
    CHECK_REG_IMM x9, 0x1212, 0x1313, 0x1414, 0x1515
    CHECK_REG_IMM x10, 0x1616, 0x1717, 0x1818, 0x1919

    adr  x24, el0_sete_entry
    msr  elr_el1, x24
    msr  spsr_el1, xzr
    eret
after_el0_sete:
    CHECK_HANDLED 9
    CHECK_ADDR x8, set_dst2
    CHECK_REG_IMM x9, 0x1A1A, 0x1B1B, 0x1C1C, 0x1D1D
    CHECK_REG_IMM x10, 0x1E1E, 0x1F1F, 0x2020, 0x2121

    adr  x24, el0_cpyp_entry
    msr  elr_el1, x24
    msr  spsr_el1, xzr
    eret
after_el0_cpyp:
    CHECK_HANDLED 10
    CHECK_ADDR x11, copy_dst0
    CHECK_ADDR x12, copy_src0
    CHECK_REG_IMM x13, 0x2222, 0x2323, 0x2424, 0x2525

    adr  x24, el0_cpym_entry
    msr  elr_el1, x24
    msr  spsr_el1, xzr
    eret
after_el0_cpym:
    CHECK_HANDLED 11
    CHECK_ADDR x11, copy_dst1
    CHECK_ADDR x12, copy_src1
    CHECK_REG_IMM x13, 0x2626, 0x2727, 0x2828, 0x2929

    adr  x24, el0_cpye_entry
    msr  elr_el1, x24
    msr  spsr_el1, xzr
    eret
after_el0_cpye:
    CHECK_HANDLED 12
    CHECK_ADDR x11, copy_dst2
    CHECK_ADDR x12, copy_src2
    CHECK_REG_IMM x13, 0x2A2A, 0x2B2B, 0x2C2C, 0x2D2D

    movz x0, #'O'
    str  x0, [x20]
    movz x0, #'\n'
    str  x0, [x20]
    b    exit_ok

el0_setp_entry:
    adr  x8, set_dst0
    LOAD_IMM64 x9, 0x0A0A, 0x0B0B, 0x0C0C, 0x0D0D
    LOAD_IMM64 x10, 0x0E0E, 0x0F0F, 0x1010, 0x1111
el0_setp_insn:
    .inst 0x19CA0528                // SETP [X8]!, X9!, X10
    b    fail

el0_setm_entry:
    adr  x8, set_dst1
    LOAD_IMM64 x9, 0x1212, 0x1313, 0x1414, 0x1515
    LOAD_IMM64 x10, 0x1616, 0x1717, 0x1818, 0x1919
el0_setm_insn:
    .inst 0x19CA4528                // SETM [X8]!, X9!, X10
    b    fail

el0_sete_entry:
    adr  x8, set_dst2
    LOAD_IMM64 x9, 0x1A1A, 0x1B1B, 0x1C1C, 0x1D1D
    LOAD_IMM64 x10, 0x1E1E, 0x1F1F, 0x2020, 0x2121
el0_sete_insn:
    .inst 0x19CA8528                // SETE [X8]!, X9!, X10
    b    fail

el0_cpyp_entry:
    adr  x11, copy_dst0
    adr  x12, copy_src0
    LOAD_IMM64 x13, 0x2222, 0x2323, 0x2424, 0x2525
el0_cpyp_insn:
    .inst 0x1D0C05AB                // CPYP [X11]!, [X12]!, X13!
    b    fail

el0_cpym_entry:
    adr  x11, copy_dst1
    adr  x12, copy_src1
    LOAD_IMM64 x13, 0x2626, 0x2727, 0x2828, 0x2929
el0_cpym_insn:
    .inst 0x1D4C05AB                // CPYM [X11]!, [X12]!, X13!
    b    fail

el0_cpye_entry:
    adr  x11, copy_dst2
    adr  x12, copy_src2
    LOAD_IMM64 x13, 0x2A2A, 0x2B2B, 0x2C2C, 0x2D2D
el0_cpye_insn:
    .inst 0x1D8C05AB                // CPYE [X11]!, [X12]!, X13!
    b    fail

fail:
    movz x0, #'F'
    str  x0, [x20]
    movz x0, #'\n'
    str  x0, [x20]

exit_ok:
    movz x0, #3
    str  x0, [x21, #0x08]
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

sync_handler:
    mrs  x24, esr_el1
    ubfx x25, x24, #26, #6
    cbnz x25, fail_from_handler
    tbz  x24, #25, fail_from_handler
    and  x25, x24, #0x1ffffff
    cbnz x25, fail_from_handler
    mrs  x25, far_el1
    cbnz x25, fail_from_handler

    mrs  x26, elr_el1
    HANDLE_CASE el1_setp_insn, after_el1_setp, 1
    HANDLE_CASE el1_setm_insn, after_el1_setm, 2
    HANDLE_CASE el1_sete_insn, after_el1_sete, 3
    HANDLE_CASE el1_cpyp_insn, after_el1_cpyp, 4
    HANDLE_CASE el1_cpym_insn, after_el1_cpym, 5
    HANDLE_CASE el1_cpye_insn, after_el1_cpye, 6
    HANDLE_CASE el0_setp_insn, after_el0_setp, 7
    HANDLE_CASE el0_setm_insn, after_el0_setm, 8
    HANDLE_CASE el0_sete_insn, after_el0_sete, 9
    HANDLE_CASE el0_cpyp_insn, after_el0_cpyp, 10
    HANDLE_CASE el0_cpym_insn, after_el0_cpym, 11
    HANDLE_CASE el0_cpye_insn, after_el0_cpye, 12

fail_from_handler:
    movz x27, #0x5
    msr  spsr_el1, x27
    adr  x30, fail
    msr  elr_el1, x30
    eret

    .data
    .balign 8
handled:
    .quad 0

    .balign 16
set_dst0:
    .space 16
set_dst1:
    .space 16
set_dst2:
    .space 16
copy_dst0:
    .space 16
copy_dst1:
    .space 16
copy_dst2:
    .space 16
copy_src0:
    .space 16
copy_src1:
    .space 16
copy_src2:
    .space 16
