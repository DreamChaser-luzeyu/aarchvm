    .text
    .global _start

    .macro LOAD_IMM64 reg, imm0, imm1, imm2, imm3
    movz \reg, #\imm0
    movk \reg, #\imm1, lsl #16
    movk \reg, #\imm2, lsl #32
    movk \reg, #\imm3, lsl #48
    .endm

    .macro CHECK_HANDLED value
    ldr  x23, [x27]
    movz x24, #\value
    cmp  x23, x24
    b.ne fail
    .endm

    .macro HANDLE_CASE insn_label, after_label, value
    adr  x26, \insn_label
    cmp  x25, x26
    b.ne .Lskip\@
    movz x30, #\value
    str  x30, [x27]
    movz x24, #0x5
    msr  spsr_el1, x24
    adr  x30, \after_label
    msr  elr_el1, x30
    eret
.Lskip\@:
    .endm

_start:
    movz x28, #0x0000
    movk x28, #0x0900, lsl #16      // UART
    movz x29, #0x0000
    movk x29, #0x0902, lsl #16      // PERF mailbox

    adr  x26, vectors
    msr  vbar_el1, x26

    adr  x27, handled
    str  xzr, [x27]

    LOAD_IMM64 x0, 0x1111, 0x2222, 0x3333, 0x4444
    adr  x1, data0
el1_ld64b_insn:
    .inst 0xF83FD020                // LD64B X0, [X1]
    b    fail
after_el1_ld64b:
    CHECK_HANDLED 1
    LOAD_IMM64 x23, 0x1111, 0x2222, 0x3333, 0x4444
    cmp  x0, x23
    b.ne fail
    adr  x23, data0
    cmp  x1, x23
    b.ne fail

    LOAD_IMM64 x2, 0x5555, 0x6666, 0x7777, 0x8888
    adr  x3, data1
el1_st64b_insn:
    .inst 0xF83F9062                // ST64B X2, [X3]
    b    fail
after_el1_st64b:
    CHECK_HANDLED 2
    LOAD_IMM64 x23, 0x5555, 0x6666, 0x7777, 0x8888
    cmp  x2, x23
    b.ne fail
    adr  x23, data1
    cmp  x3, x23
    b.ne fail

    LOAD_IMM64 x8, 0x9999, 0xAAAA, 0xBBBB, 0xCCCC
    adr  x10, data2
el1_st64bv_insn:
    .inst 0xF828B142                // ST64BV X8, X2, [X10]
    b    fail
after_el1_st64bv:
    CHECK_HANDLED 3
    LOAD_IMM64 x23, 0x9999, 0xAAAA, 0xBBBB, 0xCCCC
    cmp  x8, x23
    b.ne fail
    adr  x23, data2
    cmp  x10, x23
    b.ne fail

    LOAD_IMM64 x12, 0xD00D, 0xFACE, 0x2468, 0x1357
    adr  x13, data3
el1_st64bv0_insn:
    .inst 0xF82CA1A0                // ST64BV0 X12, X0, [X13]
    b    fail
after_el1_st64bv0:
    CHECK_HANDLED 4
    LOAD_IMM64 x23, 0xD00D, 0xFACE, 0x2468, 0x1357
    cmp  x12, x23
    b.ne fail
    adr  x23, data3
    cmp  x13, x23
    b.ne fail

    adr  x3, el0_ld64b_entry
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret
after_el0_ld64b:
    CHECK_HANDLED 5
    LOAD_IMM64 x23, 0x3141, 0x5926, 0x5358, 0x9793
    cmp  x14, x23
    b.ne fail
    adr  x23, data0
    cmp  x15, x23
    b.ne fail

    adr  x3, el0_st64b_entry
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret
after_el0_st64b:
    CHECK_HANDLED 6
    LOAD_IMM64 x23, 0x2718, 0x2818, 0x2845, 0x9045
    cmp  x16, x23
    b.ne fail
    adr  x23, data1
    cmp  x17, x23
    b.ne fail

    adr  x3, el0_st64bv_entry
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret
after_el0_st64bv:
    CHECK_HANDLED 7
    LOAD_IMM64 x23, 0x1618, 0x0339, 0x8874, 0x9894
    cmp  x18, x23
    b.ne fail
    adr  x23, data2
    cmp  x19, x23
    b.ne fail

    adr  x3, el0_st64bv0_entry
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret
after_el0_st64bv0:
    CHECK_HANDLED 8
    LOAD_IMM64 x23, 0x2643, 0x3832, 0x7950, 0x2884
    cmp  x20, x23
    b.ne fail
    adr  x23, data3
    cmp  x22, x23
    b.ne fail

    movz x0, #'S'
    str  x0, [x28]
    movz x0, #'\n'
    str  x0, [x28]
    b    exit_ok

el0_ld64b_entry:
    LOAD_IMM64 x14, 0x3141, 0x5926, 0x5358, 0x9793
    adr  x15, data0
el0_ld64b_insn:
    .inst 0xF83FD1EE                // LD64B X14, [X15]
    b    fail

el0_st64b_entry:
    LOAD_IMM64 x16, 0x2718, 0x2818, 0x2845, 0x9045
    adr  x17, data1
el0_st64b_insn:
    .inst 0xF83F9230                // ST64B X16, [X17]
    b    fail

el0_st64bv_entry:
    LOAD_IMM64 x18, 0x1618, 0x0339, 0x8874, 0x9894
    adr  x19, data2
el0_st64bv_insn:
    .inst 0xF832B260                // ST64BV X18, X0, [X19]
    b    fail

el0_st64bv0_entry:
    LOAD_IMM64 x20, 0x2643, 0x3832, 0x7950, 0x2884
    adr  x22, data3
el0_st64bv0_insn:
    .inst 0xF834A2C2                // ST64BV0 X20, X2, [X22]
    b    fail

fail:
    movz x0, #'F'
    str  x0, [x28]
    movz x0, #'\n'
    str  x0, [x28]

exit_ok:
    movz x0, #3
    str  x0, [x29, #0x08]
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
    mrs  x23, esr_el1
    ubfx x24, x23, #26, #6
    cbnz x24, fail_from_handler
    tbz  x23, #25, fail_from_handler
    and  x24, x23, #0x1ffffff
    cbnz x24, fail_from_handler
    mrs  x24, far_el1
    cbnz x24, fail_from_handler

    mrs  x25, elr_el1
    HANDLE_CASE el1_ld64b_insn, after_el1_ld64b, 1
    HANDLE_CASE el1_st64b_insn, after_el1_st64b, 2
    HANDLE_CASE el1_st64bv_insn, after_el1_st64bv, 3
    HANDLE_CASE el1_st64bv0_insn, after_el1_st64bv0, 4
    HANDLE_CASE el0_ld64b_insn, after_el0_ld64b, 5
    HANDLE_CASE el0_st64b_insn, after_el0_st64b, 6
    HANDLE_CASE el0_st64bv_insn, after_el0_st64bv, 7
    HANDLE_CASE el0_st64bv0_insn, after_el0_st64bv0, 8

fail_from_handler:
    movz x24, #0x5
    msr  spsr_el1, x24
    adr  x30, fail
    msr  elr_el1, x30
    eret

    .data
    .balign 8
handled:
    .quad 0

    .balign 64
data0:
    .space 64
    .balign 64
data1:
    .space 64
    .balign 64
data2:
    .space 64
    .balign 64
data3:
    .space 64
