    .text
    .global _start

    .macro LOAD_IMM64 reg, imm0, imm1, imm2, imm3
    movz \reg, #\imm0
    movk \reg, #\imm1, lsl #16
    movk \reg, #\imm2, lsl #32
    movk \reg, #\imm3, lsl #48
    .endm

    .macro CHECK_HANDLED value
    ldr  x26, [x25]
    movz x27, #\value
    cmp  x26, x27
    b.ne fail
    .endm

    .macro CHECK_REG_IMM reg, imm0, imm1, imm2, imm3
    LOAD_IMM64 x26, \imm0, \imm1, \imm2, \imm3
    cmp  \reg, x26
    b.ne fail
    .endm

    .macro CHECK_REG_LABEL reg, label
    adr  x30, \label
    cmp  \reg, x30
    b.ne fail
    .endm

    .macro CHECK_MEM128 label, pattern
    adr  x26, \label
    adr  x27, \pattern
    ldr  x29, [x26]
    ldr  x30, [x27]
    cmp  x29, x30
    b.ne fail
    ldr  x29, [x26, #8]
    ldr  x30, [x27, #8]
    cmp  x29, x30
    b.ne fail
    .endm

    .macro CHECK_QREG qidx, pattern
    adr  x26, qscratch
    str  q\qidx, [x26]
    CHECK_MEM128 qscratch, \pattern
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

    adr  x26, vectors
    msr  vbar_el1, x26

    movz x0, #0x30, lsl #16        // CPACR_EL1.FPEN = 0b11
    msr  cpacr_el1, x0
    isb

    adr  x25, handled
    str  xzr, [x25]

    LOAD_IMM64 x0, 0x1111, 0x2222, 0x3333, 0x4444
    LOAD_IMM64 x1, 0x5555, 0x6666, 0x7777, 0x8888
    adr  x2, pair_load0
el1_ldiapp_w_insn:
    .inst 0x99411840                // LDIAPP W0, W1, [X2]
    b    fail
after_el1_ldiapp_w:
    CHECK_HANDLED 1
    CHECK_REG_IMM x0, 0x1111, 0x2222, 0x3333, 0x4444
    CHECK_REG_IMM x1, 0x5555, 0x6666, 0x7777, 0x8888
    CHECK_REG_LABEL x2, pair_load0

    LOAD_IMM64 x3, 0x9999, 0xAAAA, 0xBBBB, 0xCCCC
    LOAD_IMM64 x4, 0xDDDD, 0xEEEE, 0x1357, 0x2468
    adr  x5, pair_load1
el1_ldiapp_w_post_insn:
    .inst 0x994408A3                // LDIAPP W3, W4, [X5], #8
    b    fail
after_el1_ldiapp_w_post:
    CHECK_HANDLED 2
    CHECK_REG_IMM x3, 0x9999, 0xAAAA, 0xBBBB, 0xCCCC
    CHECK_REG_IMM x4, 0xDDDD, 0xEEEE, 0x1357, 0x2468
    CHECK_REG_LABEL x5, pair_load1

    LOAD_IMM64 x6, 0x3141, 0x5926, 0x5358, 0x9793
    LOAD_IMM64 x7, 0x2718, 0x2818, 0x2845, 0x9045
    adr  x8, pair_load2
el1_ldiapp_x_insn:
    .inst 0xD9471906                // LDIAPP X6, X7, [X8]
    b    fail
after_el1_ldiapp_x:
    CHECK_HANDLED 3
    CHECK_REG_IMM x6, 0x3141, 0x5926, 0x5358, 0x9793
    CHECK_REG_IMM x7, 0x2718, 0x2818, 0x2845, 0x9045
    CHECK_REG_LABEL x8, pair_load2

    LOAD_IMM64 x9, 0x1618, 0x0339, 0x8874, 0x9894
    LOAD_IMM64 x10, 0x4242, 0x3131, 0x2020, 0x1010
    adr  x11, pair_load3
el1_ldiapp_x_post_insn:
    .inst 0xD94A0969                // LDIAPP X9, X10, [X11], #16
    b    fail
after_el1_ldiapp_x_post:
    CHECK_HANDLED 4
    CHECK_REG_IMM x9, 0x1618, 0x0339, 0x8874, 0x9894
    CHECK_REG_IMM x10, 0x4242, 0x3131, 0x2020, 0x1010
    CHECK_REG_LABEL x11, pair_load3

    LOAD_IMM64 x12, 0xABCD, 0xBCDE, 0xCDEF, 0xDEF0
    LOAD_IMM64 x13, 0x0BAD, 0x1DEA, 0x2FAB, 0x3CED
    adr  x14, pair_store0
el1_stilp_w_insn:
    .inst 0x990D19CC                // STILP W12, W13, [X14]
    b    fail
after_el1_stilp_w:
    CHECK_HANDLED 5
    CHECK_REG_IMM x12, 0xABCD, 0xBCDE, 0xCDEF, 0xDEF0
    CHECK_REG_IMM x13, 0x0BAD, 0x1DEA, 0x2FAB, 0x3CED
    CHECK_REG_LABEL x14, pair_store0
    CHECK_MEM128 pair_store0, pair_store_pat0

    LOAD_IMM64 x15, 0x7654, 0xFEDC, 0xBA98, 0x0123
    LOAD_IMM64 x16, 0x1111, 0x0000, 0x2222, 0x3333
    adr  x17, pair_store1
el1_stilp_w_pre_insn:
    .inst 0x99100A2F                // STILP W15, W16, [X17, #-8]!
    b    fail
after_el1_stilp_w_pre:
    CHECK_HANDLED 6
    CHECK_REG_IMM x15, 0x7654, 0xFEDC, 0xBA98, 0x0123
    CHECK_REG_IMM x16, 0x1111, 0x0000, 0x2222, 0x3333
    CHECK_REG_LABEL x17, pair_store1
    CHECK_MEM128 pair_store1, pair_store_pat1

    LOAD_IMM64 x18, 0xCAFE, 0xBABE, 0xD00D, 0xFACE
    LOAD_IMM64 x19, 0x1357, 0x2468, 0x369C, 0x48AD
    adr  x20, pair_store2
el1_stilp_x_insn:
    .inst 0xD9131A92                // STILP X18, X19, [X20]
    b    fail
after_el1_stilp_x:
    CHECK_HANDLED 7
    CHECK_REG_IMM x18, 0xCAFE, 0xBABE, 0xD00D, 0xFACE
    CHECK_REG_IMM x19, 0x1357, 0x2468, 0x369C, 0x48AD
    CHECK_REG_LABEL x20, pair_store2
    CHECK_MEM128 pair_store2, pair_store_pat2

    LOAD_IMM64 x2, 0xAAAA, 0x5555, 0xBBBB, 0x6666
    LOAD_IMM64 x3, 0xCCCC, 0x7777, 0xDDDD, 0x8888
    adr  x4, pair_store3
el1_stilp_x_pre_insn:
    .inst 0xD9030882                // STILP X2, X3, [X4, #-16]!
    b    fail
after_el1_stilp_x_pre:
    CHECK_HANDLED 8
    CHECK_REG_IMM x2, 0xAAAA, 0x5555, 0xBBBB, 0x6666
    CHECK_REG_IMM x3, 0xCCCC, 0x7777, 0xDDDD, 0x8888
    CHECK_REG_LABEL x4, pair_store3
    CHECK_MEM128 pair_store3, pair_store_pat3

    adr  x14, vec_init0
    ldr  q0, [x14]
    adr  x1, vec_load0
el1_ldapur_b_insn:
    .inst 0x1D400820                // LDAPUR B0, [X1]
    b    fail
after_el1_ldapur_b:
    CHECK_HANDLED 9
    CHECK_QREG 0, vec_init0
    CHECK_REG_LABEL x1, vec_load0

    adr  x14, vec_init1
    ldr  q2, [x14]
    adr  x3, vec_load1
el1_ldapur_h_insn:
    .inst 0x5D400862                // LDAPUR H2, [X3]
    b    fail
after_el1_ldapur_h:
    CHECK_HANDLED 10
    CHECK_QREG 2, vec_init1
    CHECK_REG_LABEL x3, vec_load1

    adr  x14, vec_init2
    ldr  q4, [x14]
    adr  x5, vec_load2
el1_ldapur_s_insn:
    .inst 0x9D4008A4                // LDAPUR S4, [X5]
    b    fail
after_el1_ldapur_s:
    CHECK_HANDLED 11
    CHECK_QREG 4, vec_init2
    CHECK_REG_LABEL x5, vec_load2

    adr  x14, vec_init3
    ldr  q6, [x14]
    adr  x7, vec_load3
el1_ldapur_d_insn:
    .inst 0xDD4008E6                // LDAPUR D6, [X7]
    b    fail
after_el1_ldapur_d:
    CHECK_HANDLED 12
    CHECK_QREG 6, vec_init3
    CHECK_REG_LABEL x7, vec_load3

    adr  x14, vec_init4
    ldr  q8, [x14]
    adr  x9, vec_load4
el1_ldapur_q_insn:
    .inst 0x1DC00928                // LDAPUR Q8, [X9]
    b    fail
after_el1_ldapur_q:
    CHECK_HANDLED 13
    CHECK_QREG 8, vec_init4
    CHECK_REG_LABEL x9, vec_load4

    adr  x14, vec_init5
    ldr  q10, [x14]
    adr  x11, vec_store0
el1_stlur_b_insn:
    .inst 0x1D00096A                // STLUR B10, [X11]
    b    fail
after_el1_stlur_b:
    CHECK_HANDLED 14
    CHECK_QREG 10, vec_init5
    CHECK_REG_LABEL x11, vec_store0
    CHECK_MEM128 vec_store0, vec_store_pat0

    adr  x14, vec_init6
    ldr  q12, [x14]
    adr  x13, vec_store1
el1_stlur_h_insn:
    .inst 0x5D0009AC                // STLUR H12, [X13]
    b    fail
after_el1_stlur_h:
    CHECK_HANDLED 15
    CHECK_QREG 12, vec_init6
    CHECK_REG_LABEL x13, vec_store1
    CHECK_MEM128 vec_store1, vec_store_pat1

    adr  x14, vec_init7
    ldr  q14, [x14]
    adr  x15, vec_store2
el1_stlur_s_insn:
    .inst 0x9D0009EE                // STLUR S14, [X15]
    b    fail
after_el1_stlur_s:
    CHECK_HANDLED 16
    CHECK_QREG 14, vec_init7
    CHECK_REG_LABEL x15, vec_store2
    CHECK_MEM128 vec_store2, vec_store_pat2

    adr  x14, vec_init8
    ldr  q16, [x14]
    adr  x17, vec_store3
el1_stlur_d_insn:
    .inst 0xDD000A30                // STLUR D16, [X17]
    b    fail
after_el1_stlur_d:
    CHECK_HANDLED 17
    CHECK_QREG 16, vec_init8
    CHECK_REG_LABEL x17, vec_store3
    CHECK_MEM128 vec_store3, vec_store_pat3

    adr  x14, vec_init9
    ldr  q18, [x14]
    adr  x19, vec_store4
el1_stlur_q_insn:
    .inst 0x1D800A72                // STLUR Q18, [X19]
    b    fail
after_el1_stlur_q:
    CHECK_HANDLED 18
    CHECK_QREG 18, vec_init9
    CHECK_REG_LABEL x19, vec_store4
    CHECK_MEM128 vec_store4, vec_store_pat4

    adr  x3, el0_ldiapp_w_entry
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret
after_el0_ldiapp_w:
    CHECK_HANDLED 19
    CHECK_REG_IMM x0, 0x1111, 0x2222, 0x3333, 0x4444
    CHECK_REG_IMM x1, 0x5555, 0x6666, 0x7777, 0x8888
    CHECK_REG_LABEL x2, pair_load0

    adr  x3, el0_stilp_x_entry
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret
after_el0_stilp_x:
    CHECK_HANDLED 20
    CHECK_REG_IMM x18, 0xCAFE, 0xBABE, 0xD00D, 0xFACE
    CHECK_REG_IMM x19, 0x1357, 0x2468, 0x369C, 0x48AD
    CHECK_REG_LABEL x20, pair_store2
    CHECK_MEM128 pair_store2, pair_store_pat2

    adr  x3, el0_ldapur_q_entry
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret
after_el0_ldapur_q:
    CHECK_HANDLED 21
    CHECK_QREG 8, vec_init4
    CHECK_REG_LABEL x9, vec_load4

    adr  x3, el0_stlur_d_entry
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret
after_el0_stlur_d:
    CHECK_HANDLED 22
    CHECK_QREG 16, vec_init8
    CHECK_REG_LABEL x17, vec_store3
    CHECK_MEM128 vec_store3, vec_store_pat3

    movz x0, #'3'
    str  x0, [x24]
    movz x0, #'\n'
    str  x0, [x24]
    b    exit_ok

el0_ldiapp_w_entry:
    LOAD_IMM64 x0, 0x1111, 0x2222, 0x3333, 0x4444
    LOAD_IMM64 x1, 0x5555, 0x6666, 0x7777, 0x8888
    adr  x2, pair_load0
el0_ldiapp_w_insn:
    .inst 0x99411840                // LDIAPP W0, W1, [X2]
    b    fail

el0_stilp_x_entry:
    LOAD_IMM64 x18, 0xCAFE, 0xBABE, 0xD00D, 0xFACE
    LOAD_IMM64 x19, 0x1357, 0x2468, 0x369C, 0x48AD
    adr  x20, pair_store2
el0_stilp_x_insn:
    .inst 0xD9131A92                // STILP X18, X19, [X20]
    b    fail

el0_ldapur_q_entry:
    adr  x14, vec_init4
    ldr  q8, [x14]
    adr  x9, vec_load4
el0_ldapur_q_insn:
    .inst 0x1DC00928                // LDAPUR Q8, [X9]
    b    fail

el0_stlur_d_entry:
    adr  x14, vec_init8
    ldr  q16, [x14]
    adr  x17, vec_store3
el0_stlur_d_insn:
    .inst 0xDD000A30                // STLUR D16, [X17]
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
    HANDLE_CASE el1_ldiapp_w_insn, after_el1_ldiapp_w, 1
    HANDLE_CASE el1_ldiapp_w_post_insn, after_el1_ldiapp_w_post, 2
    HANDLE_CASE el1_ldiapp_x_insn, after_el1_ldiapp_x, 3
    HANDLE_CASE el1_ldiapp_x_post_insn, after_el1_ldiapp_x_post, 4
    HANDLE_CASE el1_stilp_w_insn, after_el1_stilp_w, 5
    HANDLE_CASE el1_stilp_w_pre_insn, after_el1_stilp_w_pre, 6
    HANDLE_CASE el1_stilp_x_insn, after_el1_stilp_x, 7
    HANDLE_CASE el1_stilp_x_pre_insn, after_el1_stilp_x_pre, 8
    HANDLE_CASE el1_ldapur_b_insn, after_el1_ldapur_b, 9
    HANDLE_CASE el1_ldapur_h_insn, after_el1_ldapur_h, 10
    HANDLE_CASE el1_ldapur_s_insn, after_el1_ldapur_s, 11
    HANDLE_CASE el1_ldapur_d_insn, after_el1_ldapur_d, 12
    HANDLE_CASE el1_ldapur_q_insn, after_el1_ldapur_q, 13
    HANDLE_CASE el1_stlur_b_insn, after_el1_stlur_b, 14
    HANDLE_CASE el1_stlur_h_insn, after_el1_stlur_h, 15
    HANDLE_CASE el1_stlur_s_insn, after_el1_stlur_s, 16
    HANDLE_CASE el1_stlur_d_insn, after_el1_stlur_d, 17
    HANDLE_CASE el1_stlur_q_insn, after_el1_stlur_q, 18
    HANDLE_CASE el0_ldiapp_w_insn, after_el0_ldiapp_w, 19
    HANDLE_CASE el0_stilp_x_insn, after_el0_stilp_x, 20
    HANDLE_CASE el0_ldapur_q_insn, after_el0_ldapur_q, 21
    HANDLE_CASE el0_stlur_d_insn, after_el0_stlur_d, 22
    b    fail_from_handler

fail_from_handler:
    b fail

    .balign 16
handled:
    .quad 0
.balign 16
qscratch:
    .quad 0
    .quad 0

pair_load0:
    .quad 0x8877665544332211
    .quad 0x1122334455667788
pair_load1:
    .quad 0xCAFEBABEDEADBEEF
    .quad 0x0102030405060708
pair_load2:
    .quad 0x13579BDF2468ACE0
    .quad 0x0F1E2D3C4B5A6978
pair_load3:
    .quad 0xFFEEDDCCBBAA9988
    .quad 0x7766554433221100

pair_store0:
    .quad 0x1111222233334444
    .quad 0x5555666677778888
pair_store_pat0:
    .quad 0x1111222233334444
    .quad 0x5555666677778888
pair_store1:
    .quad 0x9999AAAABBBBCCCC
    .quad 0xDDDDEEEEFFFF0001
pair_store_pat1:
    .quad 0x9999AAAABBBBCCCC
    .quad 0xDDDDEEEEFFFF0001
pair_store2:
    .quad 0x2468246824682468
    .quad 0x1357135713571357
pair_store_pat2:
    .quad 0x2468246824682468
    .quad 0x1357135713571357
pair_store3:
    .quad 0xAAAABBBBCCCCDDDD
    .quad 0x1111222233334444
pair_store_pat3:
    .quad 0xAAAABBBBCCCCDDDD
    .quad 0x1111222233334444

.balign 16
vec_init0:
    .quad 0x1011121314151617
    .quad 0x18191A1B1C1D1E1F
vec_init1:
    .quad 0x2122232425262728
    .quad 0x292A2B2C2D2E2F30
vec_init2:
    .quad 0x3132333435363738
    .quad 0x393A3B3C3D3E3F40
vec_init3:
    .quad 0x4142434445464748
    .quad 0x494A4B4C4D4E4F50
vec_init4:
    .quad 0x5152535455565758
    .quad 0x595A5B5C5D5E5F60
vec_init5:
    .quad 0x6162636465666768
    .quad 0x696A6B6C6D6E6F70
vec_init6:
    .quad 0x7172737475767778
    .quad 0x797A7B7C7D7E7F80
vec_init7:
    .quad 0x8182838485868788
    .quad 0x898A8B8C8D8E8F90
vec_init8:
    .quad 0x9192939495969798
    .quad 0x999A9B9C9D9E9FA0
vec_init9:
    .quad 0xA1A2A3A4A5A6A7A8
    .quad 0xA9AAABACADAEAFB0

vec_load0:
    .quad 0xFFFFFFFFFFFFFFFF
    .quad 0x0000000000000000
vec_load1:
    .quad 0x0123456789ABCDEF
    .quad 0xFEDCBA9876543210
vec_load2:
    .quad 0x0BADF00DDEADC0DE
    .quad 0x1234567890ABCDEF
vec_load3:
    .quad 0xFFEEDDCCBBAA0099
    .quad 0x8877665544332211
vec_load4:
    .quad 0x1020304050607080
    .quad 0x90A0B0C0D0E0F000

vec_store0:
    .quad 0xAAAABBBBCCCCDDDD
    .quad 0x1111222233334444
vec_store_pat0:
    .quad 0xAAAABBBBCCCCDDDD
    .quad 0x1111222233334444
vec_store1:
    .quad 0x5555666677778888
    .quad 0x9999AAAABBBBCCCC
vec_store_pat1:
    .quad 0x5555666677778888
    .quad 0x9999AAAABBBBCCCC
vec_store2:
    .quad 0xDEADBEEFCAFEBABE
    .quad 0x0101010102020202
vec_store_pat2:
    .quad 0xDEADBEEFCAFEBABE
    .quad 0x0101010102020202
vec_store3:
    .quad 0x13579BDF2468ACE0
    .quad 0x02468ACE13579BDF
vec_store_pat3:
    .quad 0x13579BDF2468ACE0
    .quad 0x02468ACE13579BDF
vec_store4:
    .quad 0xFEEDFACECAFED00D
    .quad 0x1234123412341234
vec_store_pat4:
    .quad 0xFEEDFACECAFED00D
    .quad 0x1234123412341234
