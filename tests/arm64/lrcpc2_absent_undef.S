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
    ldr  x22, [x25]
    movz x23, #\value
    cmp  x22, x23
    b.ne fail
    .endm

    .macro CHECK_REG_IMM reg, imm0, imm1, imm2, imm3
    LOAD_IMM64 x14, \imm0, \imm1, \imm2, \imm3
    cmp  \reg, x14
    b.ne fail
    .endm

    .macro CHECK_REG_LABEL reg, label
    adr  x14, \label
    cmp  \reg, x14
    b.ne fail
    .endm

    .macro CHECK_MEM64 label, imm0, imm1, imm2, imm3
    adr  x14, \label
    ldr  x15, [x14]
    LOAD_IMM64 x14, \imm0, \imm1, \imm2, \imm3
    cmp  x15, x14
    b.ne fail
    .endm

    .macro HANDLE_CASE insn_label, after_label, value
    adr  x27, \insn_label
    cmp  x26, x27
    b.ne .Lskip\@
    movz x29, #\value
    str  x29, [x25]
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

    adr  x25, handled
    str  xzr, [x25]

    LOAD_IMM64 x0, 0x1111, 0x2222, 0x3333, 0x4444
    adr  x1, data_byte0
el1_ldapurb_insn:
    .inst 0x19400020                // LDAPURB W0, [X1]
    b    fail
after_el1_ldapurb:
    CHECK_HANDLED 1
    CHECK_REG_IMM x0, 0x1111, 0x2222, 0x3333, 0x4444
    CHECK_REG_LABEL x1, data_byte0

    LOAD_IMM64 x2, 0x5555, 0x6666, 0x7777, 0x8888
    adr  x3, data_byte1
el1_ldapursb_w_insn:
    .inst 0x19C00062                // LDAPURSB W2, [X3]
    b    fail
after_el1_ldapursb_w:
    CHECK_HANDLED 2
    CHECK_REG_IMM x2, 0x5555, 0x6666, 0x7777, 0x8888
    CHECK_REG_LABEL x3, data_byte1

    LOAD_IMM64 x4, 0x9999, 0xAAAA, 0xBBBB, 0xCCCC
    adr  x5, data_byte2
el1_ldapursb_x_insn:
    .inst 0x198000A4                // LDAPURSB X4, [X5]
    b    fail
after_el1_ldapursb_x:
    CHECK_HANDLED 3
    CHECK_REG_IMM x4, 0x9999, 0xAAAA, 0xBBBB, 0xCCCC
    CHECK_REG_LABEL x5, data_byte2

    LOAD_IMM64 x6, 0xDDDD, 0xEEEE, 0x1357, 0x2468
    adr  x7, data_half0
el1_ldapurh_insn:
    .inst 0x594000E6                // LDAPURH W6, [X7]
    b    fail
after_el1_ldapurh:
    CHECK_HANDLED 4
    CHECK_REG_IMM x6, 0xDDDD, 0xEEEE, 0x1357, 0x2468
    CHECK_REG_LABEL x7, data_half0

    LOAD_IMM64 x8, 0x3141, 0x5926, 0x5358, 0x9793
    adr  x9, data_half1
el1_ldapursh_w_insn:
    .inst 0x59C00128                // LDAPURSH W8, [X9]
    b    fail
after_el1_ldapursh_w:
    CHECK_HANDLED 5
    CHECK_REG_IMM x8, 0x3141, 0x5926, 0x5358, 0x9793
    CHECK_REG_LABEL x9, data_half1

    LOAD_IMM64 x10, 0x2718, 0x2818, 0x2845, 0x9045
    adr  x11, data_half2
el1_ldapursh_x_insn:
    .inst 0x5980016A                // LDAPURSH X10, [X11]
    b    fail
after_el1_ldapursh_x:
    CHECK_HANDLED 6
    CHECK_REG_IMM x10, 0x2718, 0x2818, 0x2845, 0x9045
    CHECK_REG_LABEL x11, data_half2

    LOAD_IMM64 x12, 0x1618, 0x0339, 0x8874, 0x9894
    adr  x13, data_word0
el1_ldapur_w_insn:
    .inst 0x994001AC                // LDAPUR W12, [X13]
    b    fail
after_el1_ldapur_w:
    CHECK_HANDLED 7
    CHECK_REG_IMM x12, 0x1618, 0x0339, 0x8874, 0x9894
    CHECK_REG_LABEL x13, data_word0

    LOAD_IMM64 x14, 0x4242, 0x3131, 0x2020, 0x1010
    adr  x15, data_word1
el1_ldapursw_x_insn:
    .inst 0x998001EE                // LDAPURSW X14, [X15]
    b    fail
after_el1_ldapursw_x:
    CHECK_HANDLED 8
    CHECK_REG_IMM x14, 0x4242, 0x3131, 0x2020, 0x1010
    CHECK_REG_LABEL x15, data_word1

    LOAD_IMM64 x16, 0xABCD, 0xBCDE, 0xCDEF, 0xDEF0
    adr  x17, store_byte
el1_stlurb_insn:
    .inst 0x19000230                // STLURB W16, [X17]
    b    fail
after_el1_stlurb:
    CHECK_HANDLED 9
    CHECK_REG_IMM x16, 0xABCD, 0xBCDE, 0xCDEF, 0xDEF0
    CHECK_REG_LABEL x17, store_byte
    CHECK_MEM64 store_byte, 0x2211, 0x4433, 0x6655, 0x8877

    LOAD_IMM64 x18, 0x0BAD, 0x1DEA, 0x2FAB, 0x3CED
    adr  x19, store_half
el1_stlurh_insn:
    .inst 0x59000272                // STLURH W18, [X19]
    b    fail
after_el1_stlurh:
    CHECK_HANDLED 10
    CHECK_REG_IMM x18, 0x0BAD, 0x1DEA, 0x2FAB, 0x3CED
    CHECK_REG_LABEL x19, store_half
    CHECK_MEM64 store_half, 0x2211, 0x4433, 0x6655, 0x8877

    LOAD_IMM64 x20, 0x7654, 0xFEDC, 0xBA98, 0x0123
    adr  x21, store_word
el1_stlur_w_insn:
    .inst 0x990002B4                // STLUR W20, [X21]
    b    fail
after_el1_stlur_w:
    CHECK_HANDLED 11
    CHECK_REG_IMM x20, 0x7654, 0xFEDC, 0xBA98, 0x0123
    CHECK_REG_LABEL x21, store_word
    CHECK_MEM64 store_word, 0x2211, 0x4433, 0x6655, 0x8877

    adr  x3, el0_ldapurb_entry
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret
after_el0_ldapurb:
    CHECK_HANDLED 12
    CHECK_REG_IMM x0, 0x1111, 0x2222, 0x3333, 0x4444
    CHECK_REG_LABEL x1, data_byte0

    adr  x3, el0_ldapursb_w_entry
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret
after_el0_ldapursb_w:
    CHECK_HANDLED 13
    CHECK_REG_IMM x2, 0x5555, 0x6666, 0x7777, 0x8888
    CHECK_REG_LABEL x3, data_byte1

    adr  x3, el0_ldapursb_x_entry
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret
after_el0_ldapursb_x:
    CHECK_HANDLED 14
    CHECK_REG_IMM x4, 0x9999, 0xAAAA, 0xBBBB, 0xCCCC
    CHECK_REG_LABEL x5, data_byte2

    adr  x3, el0_ldapurh_entry
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret
after_el0_ldapurh:
    CHECK_HANDLED 15
    CHECK_REG_IMM x6, 0xDDDD, 0xEEEE, 0x1357, 0x2468
    CHECK_REG_LABEL x7, data_half0

    adr  x3, el0_ldapursh_w_entry
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret
after_el0_ldapursh_w:
    CHECK_HANDLED 16
    CHECK_REG_IMM x8, 0x3141, 0x5926, 0x5358, 0x9793
    CHECK_REG_LABEL x9, data_half1

    adr  x3, el0_ldapursh_x_entry
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret
after_el0_ldapursh_x:
    CHECK_HANDLED 17
    CHECK_REG_IMM x10, 0x2718, 0x2818, 0x2845, 0x9045
    CHECK_REG_LABEL x11, data_half2

    adr  x3, el0_ldapur_w_entry
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret
after_el0_ldapur_w:
    CHECK_HANDLED 18
    CHECK_REG_IMM x12, 0x1618, 0x0339, 0x8874, 0x9894
    CHECK_REG_LABEL x13, data_word0

    adr  x3, el0_ldapursw_x_entry
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret
after_el0_ldapursw_x:
    CHECK_HANDLED 19
    CHECK_REG_IMM x14, 0x4242, 0x3131, 0x2020, 0x1010
    CHECK_REG_LABEL x15, data_word1

    adr  x3, el0_stlurb_entry
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret
after_el0_stlurb:
    CHECK_HANDLED 20
    CHECK_REG_IMM x16, 0xABCD, 0xBCDE, 0xCDEF, 0xDEF0
    CHECK_REG_LABEL x17, store_byte
    CHECK_MEM64 store_byte, 0x2211, 0x4433, 0x6655, 0x8877

    adr  x3, el0_stlurh_entry
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret
after_el0_stlurh:
    CHECK_HANDLED 21
    CHECK_REG_IMM x18, 0x0BAD, 0x1DEA, 0x2FAB, 0x3CED
    CHECK_REG_LABEL x19, store_half
    CHECK_MEM64 store_half, 0x2211, 0x4433, 0x6655, 0x8877

    adr  x3, el0_stlur_w_entry
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret
after_el0_stlur_w:
    CHECK_HANDLED 22
    CHECK_REG_IMM x20, 0x7654, 0xFEDC, 0xBA98, 0x0123
    CHECK_REG_LABEL x21, store_word
    CHECK_MEM64 store_word, 0x2211, 0x4433, 0x6655, 0x8877

    movz x0, #'2'
    str  x0, [x24]
    movz x0, #'\n'
    str  x0, [x24]
    b    exit_ok

el0_ldapurb_entry:
    LOAD_IMM64 x0, 0x1111, 0x2222, 0x3333, 0x4444
    adr  x1, data_byte0
el0_ldapurb_insn:
    .inst 0x19400020                // LDAPURB W0, [X1]
    b    fail

el0_ldapursb_w_entry:
    LOAD_IMM64 x2, 0x5555, 0x6666, 0x7777, 0x8888
    adr  x3, data_byte1
el0_ldapursb_w_insn:
    .inst 0x19C00062                // LDAPURSB W2, [X3]
    b    fail

el0_ldapursb_x_entry:
    LOAD_IMM64 x4, 0x9999, 0xAAAA, 0xBBBB, 0xCCCC
    adr  x5, data_byte2
el0_ldapursb_x_insn:
    .inst 0x198000A4                // LDAPURSB X4, [X5]
    b    fail

el0_ldapurh_entry:
    LOAD_IMM64 x6, 0xDDDD, 0xEEEE, 0x1357, 0x2468
    adr  x7, data_half0
el0_ldapurh_insn:
    .inst 0x594000E6                // LDAPURH W6, [X7]
    b    fail

el0_ldapursh_w_entry:
    LOAD_IMM64 x8, 0x3141, 0x5926, 0x5358, 0x9793
    adr  x9, data_half1
el0_ldapursh_w_insn:
    .inst 0x59C00128                // LDAPURSH W8, [X9]
    b    fail

el0_ldapursh_x_entry:
    LOAD_IMM64 x10, 0x2718, 0x2818, 0x2845, 0x9045
    adr  x11, data_half2
el0_ldapursh_x_insn:
    .inst 0x5980016A                // LDAPURSH X10, [X11]
    b    fail

el0_ldapur_w_entry:
    LOAD_IMM64 x12, 0x1618, 0x0339, 0x8874, 0x9894
    adr  x13, data_word0
el0_ldapur_w_insn:
    .inst 0x994001AC                // LDAPUR W12, [X13]
    b    fail

el0_ldapursw_x_entry:
    LOAD_IMM64 x14, 0x4242, 0x3131, 0x2020, 0x1010
    adr  x15, data_word1
el0_ldapursw_x_insn:
    .inst 0x998001EE                // LDAPURSW X14, [X15]
    b    fail

el0_stlurb_entry:
    LOAD_IMM64 x16, 0xABCD, 0xBCDE, 0xCDEF, 0xDEF0
    adr  x17, store_byte
el0_stlurb_insn:
    .inst 0x19000230                // STLURB W16, [X17]
    b    fail

el0_stlurh_entry:
    LOAD_IMM64 x18, 0x0BAD, 0x1DEA, 0x2FAB, 0x3CED
    adr  x19, store_half
el0_stlurh_insn:
    .inst 0x59000272                // STLURH W18, [X19]
    b    fail

el0_stlur_w_entry:
    LOAD_IMM64 x20, 0x7654, 0xFEDC, 0xBA98, 0x0123
    adr  x21, store_word
el0_stlur_w_insn:
    .inst 0x990002B4                // STLUR W20, [X21]
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
    HANDLE_CASE el1_ldapurb_insn, after_el1_ldapurb, 1
    HANDLE_CASE el1_ldapursb_w_insn, after_el1_ldapursb_w, 2
    HANDLE_CASE el1_ldapursb_x_insn, after_el1_ldapursb_x, 3
    HANDLE_CASE el1_ldapurh_insn, after_el1_ldapurh, 4
    HANDLE_CASE el1_ldapursh_w_insn, after_el1_ldapursh_w, 5
    HANDLE_CASE el1_ldapursh_x_insn, after_el1_ldapursh_x, 6
    HANDLE_CASE el1_ldapur_w_insn, after_el1_ldapur_w, 7
    HANDLE_CASE el1_ldapursw_x_insn, after_el1_ldapursw_x, 8
    HANDLE_CASE el1_stlurb_insn, after_el1_stlurb, 9
    HANDLE_CASE el1_stlurh_insn, after_el1_stlurh, 10
    HANDLE_CASE el1_stlur_w_insn, after_el1_stlur_w, 11
    HANDLE_CASE el0_ldapurb_insn, after_el0_ldapurb, 12
    HANDLE_CASE el0_ldapursb_w_insn, after_el0_ldapursb_w, 13
    HANDLE_CASE el0_ldapursb_x_insn, after_el0_ldapursb_x, 14
    HANDLE_CASE el0_ldapurh_insn, after_el0_ldapurh, 15
    HANDLE_CASE el0_ldapursh_w_insn, after_el0_ldapursh_w, 16
    HANDLE_CASE el0_ldapursh_x_insn, after_el0_ldapursh_x, 17
    HANDLE_CASE el0_ldapur_w_insn, after_el0_ldapur_w, 18
    HANDLE_CASE el0_ldapursw_x_insn, after_el0_ldapursw_x, 19
    HANDLE_CASE el0_stlurb_insn, after_el0_stlurb, 20
    HANDLE_CASE el0_stlurh_insn, after_el0_stlurh, 21
    HANDLE_CASE el0_stlur_w_insn, after_el0_stlur_w, 22

fail_from_handler:
    adr  x30, fail
    movz x29, #0x5
    msr  spsr_el1, x29
    msr  elr_el1, x30
    eret

    .data
    .balign 8
handled:
    .quad 0
store_byte:
    .quad 0x8877665544332211
store_half:
    .quad 0x8877665544332211
store_word:
    .quad 0x8877665544332211
data_byte0:
    .byte 0x5A
data_byte1:
    .byte 0xA5
data_byte2:
    .byte 0xC3
    .balign 2
data_half0:
    .hword 0x1234
data_half1:
    .hword 0x89AB
data_half2:
    .hword 0xFEDC
    .balign 4
data_word0:
    .word 0x76543210
data_word1:
    .word 0x89ABCDEF
