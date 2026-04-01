    .arch armv8.4-a+rcpc
    .text
    .global _start

    .macro LOAD_IMM64 reg, imm0, imm1, imm2, imm3
    movz \reg, #\imm0
    movk \reg, #\imm1, lsl #16
    movk \reg, #\imm2, lsl #32
    movk \reg, #\imm3, lsl #48
    .endm

    .macro CHECK_HANDLED value
    ldr  x22, [x21]
    movz x23, #\value
    cmp  x22, x23
    b.ne fail
    .endm

    .macro HANDLE_CASE insn_label, after_label, value
    adr  x27, \insn_label
    cmp  x26, x27
    b.ne .Lskip\@
    movz x29, #\value
    str  x29, [x21]
    movz x30, #0x5
    msr  spsr_el1, x30
    adr  x30, \after_label
    msr  elr_el1, x30
    eret
.Lskip\@:
    .endm

_start:
    movz x24, #0x0000
    movk x24, #0x0900, lsl #16      // UART
    movz x28, #0x0000
    movk x28, #0x0902, lsl #16      // PERF mailbox

    adr  x25, vectors
    msr  vbar_el1, x25

    adr  x21, handled
    str  xzr, [x21]

    LOAD_IMM64 x0, 0x1111, 0x2222, 0x3333, 0x4444
    adr  x1, data_word0
el1_ldapr_w_insn:
    .inst 0xB8BFC020                // LDAPR W0, [X1]
    b    fail
after_el1_ldapr_w:
    CHECK_HANDLED 1
    LOAD_IMM64 x14, 0x1111, 0x2222, 0x3333, 0x4444
    cmp  x0, x14
    b.ne fail
    adr  x14, data_word0
    cmp  x1, x14
    b.ne fail

    LOAD_IMM64 x2, 0x5555, 0x6666, 0x7777, 0x8888
    adr  x3, data_word1
el1_ldapr_x_insn:
    .inst 0xF8BFC062                // LDAPR X2, [X3]
    b    fail
after_el1_ldapr_x:
    CHECK_HANDLED 2
    LOAD_IMM64 x14, 0x5555, 0x6666, 0x7777, 0x8888
    cmp  x2, x14
    b.ne fail
    adr  x14, data_word1
    cmp  x3, x14
    b.ne fail

    LOAD_IMM64 x4, 0x9999, 0xAAAA, 0xBBBB, 0xCCCC
    adr  x5, data_byte0
el1_ldaprb_w_insn:
    .inst 0x38BFC0A4                // LDAPRB W4, [X5]
    b    fail
after_el1_ldaprb_w:
    CHECK_HANDLED 3
    LOAD_IMM64 x14, 0x9999, 0xAAAA, 0xBBBB, 0xCCCC
    cmp  x4, x14
    b.ne fail
    adr  x14, data_byte0
    cmp  x5, x14
    b.ne fail

    LOAD_IMM64 x6, 0xDDDD, 0xEEEE, 0x1357, 0x2468
    adr  x7, data_half0
el1_ldaprh_w_insn:
    .inst 0x78BFC0E6                // LDAPRH W6, [X7]
    b    fail
after_el1_ldaprh_w:
    CHECK_HANDLED 4
    LOAD_IMM64 x14, 0xDDDD, 0xEEEE, 0x1357, 0x2468
    cmp  x6, x14
    b.ne fail
    adr  x14, data_half0
    cmp  x7, x14
    b.ne fail

    adr  x3, el0_ldapr_w_entry
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret
after_el0_ldapr_w:
    CHECK_HANDLED 5
    LOAD_IMM64 x14, 0x3141, 0x5926, 0x5358, 0x9793
    cmp  x8, x14
    b.ne fail
    adr  x14, data_word0
    cmp  x9, x14
    b.ne fail

    adr  x3, el0_ldapr_x_entry
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret
after_el0_ldapr_x:
    CHECK_HANDLED 6
    LOAD_IMM64 x14, 0x2718, 0x2818, 0x2845, 0x9045
    cmp  x10, x14
    b.ne fail
    adr  x14, data_word1
    cmp  x11, x14
    b.ne fail

    adr  x3, el0_ldaprb_w_entry
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret
after_el0_ldaprb_w:
    CHECK_HANDLED 7
    LOAD_IMM64 x14, 0x1618, 0x0339, 0x8874, 0x9894
    cmp  x12, x14
    b.ne fail
    adr  x14, data_byte0
    cmp  x13, x14
    b.ne fail

    adr  x3, el0_ldaprh_w_entry
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret
after_el0_ldaprh_w:
    CHECK_HANDLED 8
    LOAD_IMM64 x14, 0x2643, 0x3832, 0x7950, 0x2884
    cmp  x15, x14
    b.ne fail
    adr  x14, data_half0
    cmp  x16, x14
    b.ne fail

    movz x0, #'R'
    str  x0, [x24]
    movz x0, #'\n'
    str  x0, [x24]
    b    exit_ok

el0_ldapr_w_entry:
    LOAD_IMM64 x8, 0x3141, 0x5926, 0x5358, 0x9793
    adr  x9, data_word0
el0_ldapr_w_insn:
    .inst 0xB8BFC028                // LDAPR W8, [X9]
    b    fail

el0_ldapr_x_entry:
    LOAD_IMM64 x10, 0x2718, 0x2818, 0x2845, 0x9045
    adr  x11, data_word1
el0_ldapr_x_insn:
    .inst 0xF8BFC16A                // LDAPR X10, [X11]
    b    fail

el0_ldaprb_w_entry:
    LOAD_IMM64 x12, 0x1618, 0x0339, 0x8874, 0x9894
    adr  x13, data_byte0
el0_ldaprb_w_insn:
    .inst 0x38BFC1AC                // LDAPRB W12, [X13]
    b    fail

el0_ldaprh_w_entry:
    LOAD_IMM64 x15, 0x2643, 0x3832, 0x7950, 0x2884
    adr  x16, data_half0
el0_ldaprh_w_insn:
    .inst 0x78BFC20F                // LDAPRH W15, [X16]
    b    fail

fail:
    movz x0, #'F'
    str  x0, [x24]
    movz x0, #'\n'
    str  x0, [x24]

exit_ok:
    movz x0, #3
    str  x0, [x28, #0x08]
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
    mrs  x22, esr_el1
    ubfx x23, x22, #26, #6
    cbnz x23, fail_from_handler
    tbz  x22, #25, fail_from_handler
    and  x23, x22, #0x1ffffff
    cbnz x23, fail_from_handler
    mrs  x23, far_el1
    cbnz x23, fail_from_handler

    mrs  x26, elr_el1
    HANDLE_CASE el1_ldapr_w_insn, after_el1_ldapr_w, 1
    HANDLE_CASE el1_ldapr_x_insn, after_el1_ldapr_x, 2
    HANDLE_CASE el1_ldaprb_w_insn, after_el1_ldaprb_w, 3
    HANDLE_CASE el1_ldaprh_w_insn, after_el1_ldaprh_w, 4
    HANDLE_CASE el0_ldapr_w_insn, after_el0_ldapr_w, 5
    HANDLE_CASE el0_ldapr_x_insn, after_el0_ldapr_x, 6
    HANDLE_CASE el0_ldaprb_w_insn, after_el0_ldaprb_w, 7
    HANDLE_CASE el0_ldaprh_w_insn, after_el0_ldaprh_w, 8

fail_from_handler:
    movz x29, #0x5
    msr  spsr_el1, x29
    adr  x30, fail
    msr  elr_el1, x30
    eret

    .data
    .balign 8
handled:
    .quad 0
data_word0:
    .word 0x89ABCDEF
data_word1:
    .word 0x01234567
data_byte0:
    .byte 0x80
    .space 1
data_half0:
    .hword 0x8001
