    .text
    .global _start

    .macro fp_enable_el1
    movz x25, #0x0030, lsl #16
    msr  cpacr_el1, x25
    isb
    .endm

    .macro fp_trap_all
    msr  cpacr_el1, xzr
    isb
    .endm

    .macro load_v0_sentinel
    adr  x19, v0_sentinel
    ldr  q0, [x19]
    .endm

    .macro check_v0_unchanged
    adr  x19, v0_sentinel
    ldr  x21, [x19]
    ldr  x22, [x19, #8]
    umov x23, v0.d[0]
    umov x24, v0.d[1]
    cmp  x23, x21
    b.ne fail
    cmp  x24, x22
    b.ne fail
    .endm

    .macro check_handled value
    ldr  x12, [x11]
    movz x13, #\value
    cmp  x12, x13
    b.ne fail
    .endm

    .macro handle_case insn_label, after_label, value
    adr  x27, \insn_label
    cmp  x26, x27
    b.ne .Lskip\@
    adr  x9, handled
    movz x10, #\value
    str  x10, [x9]
    adr  x24, \after_label
    msr  elr_el1, x24
    eret
.Lskip\@:
    .endm

_start:
    movz x20, #0
    movk x20, #0x0900, lsl #16

    adr  x22, vectors
    msr  vbar_el1, x22
    isb

    fp_enable_el1
    adr  x19, fp_inputs
    ldr  q1, [x19]
    ldr  q2, [x19, #16]
    ldr  q3, [x19, #32]

    adr  x11, handled
    str  xzr, [x11]

    load_v0_sentinel
case_fmlal_vec:
    fp_trap_all
insn_fmlal_vec:
    .inst 0x0E22EC20                // FMLAL V0.2S, V1.2H, V2.2H
    b    fail
after_fmlal_vec:
    fp_enable_el1
    check_handled 1
    check_v0_unchanged

    load_v0_sentinel
case_fmlal_idx:
    fp_trap_all
insn_fmlal_idx:
    .inst 0x0F820020                // FMLAL V0.2S, V1.2H, V2.H[0]
    b    fail
after_fmlal_idx:
    fp_enable_el1
    check_handled 2
    check_v0_unchanged

    load_v0_sentinel
case_ummla:
    fp_trap_all
insn_ummla:
    .inst 0x6E82A420                // UMMLA V0.4S, V1.16B, V2.16B
    b    fail
after_ummla:
    fp_enable_el1
    check_handled 3
    check_v0_unchanged

    load_v0_sentinel
case_usdot_vec:
    fp_trap_all
insn_usdot_vec:
    .inst 0x4E829C20                // USDOT V0.4S, V1.16B, V2.16B
    b    fail
after_usdot_vec:
    fp_enable_el1
    check_handled 4
    check_v0_unchanged

    load_v0_sentinel
case_sudot_idx:
    fp_trap_all
insn_sudot_idx:
    .inst 0x4F22F020                // SUDOT V0.4S, V1.16B, V2.4B[1]
    b    fail
after_sudot_idx:
    fp_enable_el1
    check_handled 5
    check_v0_unchanged

    load_v0_sentinel
case_eor3:
    fp_trap_all
insn_eor3:
    .inst 0xCE020C20                // EOR3 V0.16B, V1.16B, V2.16B, V3.16B
    b    fail
after_eor3:
    fp_enable_el1
    check_handled 6
    check_v0_unchanged

    load_v0_sentinel
case_xar:
    fp_trap_all
insn_xar:
    .inst 0xCE821C20                // XAR V0.2D, V1.2D, V2.2D, #7
    b    fail
after_xar:
    fp_enable_el1
    check_handled 7
    check_v0_unchanged

    load_v0_sentinel
case_sm3ss1:
    fp_trap_all
insn_sm3ss1:
    .inst 0xCE420C20                // SM3SS1 V0.4S, V1.4S, V2.4S, V3.4S
    b    fail
after_sm3ss1:
    fp_enable_el1
    check_handled 8
    check_v0_unchanged

    load_v0_sentinel
case_sm3tt1a:
    fp_trap_all
insn_sm3tt1a:
    .inst 0xCE428020                // SM3TT1A V0.4S, V1.4S, V2.S[0]
    b    fail
after_sm3tt1a:
    fp_enable_el1
    check_handled 9
    check_v0_unchanged

    load_v0_sentinel
case_sm3partw1:
    fp_trap_all
insn_sm3partw1:
    .inst 0xCE62C020                // SM3PARTW1 V0.4S, V1.4S, V2.4S
    b    fail
after_sm3partw1:
    fp_enable_el1
    check_handled 10
    check_v0_unchanged

    load_v0_sentinel
case_sm4e:
    fp_trap_all
insn_sm4e:
    .inst 0xCEC08420                // SM4E V0.4S, V1.4S
    b    fail
after_sm4e:
    fp_enable_el1
    check_handled 11
    check_v0_unchanged

    load_v0_sentinel
case_sm4ekey:
    fp_trap_all
insn_sm4ekey:
    .inst 0xCE62C820                // SM4EKEY V0.4S, V1.4S, V2.4S
    b    fail
after_sm4ekey:
    fp_enable_el1
    check_handled 12
    check_v0_unchanged

    load_v0_sentinel
case_sha512h:
    fp_trap_all
insn_sha512h:
    .inst 0xCE628020                // SHA512H Q0, Q1, V2.2D
    b    fail
after_sha512h:
    fp_enable_el1
    check_handled 13
    check_v0_unchanged

    load_v0_sentinel
case_sha512su0:
    fp_trap_all
insn_sha512su0:
    .inst 0xCEC08020                // SHA512SU0 V0.2D, V1.2D
    b    fail
after_sha512su0:
    fp_enable_el1
    check_handled 14
    check_v0_unchanged

    load_v0_sentinel
case_sqrdmlah_vec_h:
    fp_trap_all
insn_sqrdmlah_vec_h:
    .inst 0x6E428420                // SQRDMLAH V0.8H, V1.8H, V2.8H
    b    fail
after_sqrdmlah_vec_h:
    fp_enable_el1
    check_handled 15
    check_v0_unchanged

    load_v0_sentinel
case_sqrdmlsh_scalar:
    fp_trap_all
insn_sqrdmlsh_scalar:
    .inst 0x7E828C20                // SQRDMLSH S0, S1, S2
    b    fail
after_sqrdmlsh_scalar:
    fp_enable_el1
    check_handled 16
    check_v0_unchanged

    load_v0_sentinel
case_sqrdmlah_idx_scalar:
    fp_trap_all
insn_sqrdmlah_idx_scalar:
    .inst 0x7F83D020                // SQRDMLAH S0, S1, V3.S[0]
    b    fail
after_sqrdmlah_idx_scalar:
    fp_enable_el1
    check_handled 17
    check_v0_unchanged

    load_v0_sentinel
case_fcmla_idx_single:
    fp_trap_all
insn_fcmla_idx_single:
    .inst 0x6F823820                // FCMLA V0.4S, V1.4S, V2.S[1], #90
    b    fail
after_fcmla_idx_single:
    fp_enable_el1
    check_handled 18
    check_v0_unchanged

    load_v0_sentinel
case_fcmla_idx_half:
    fp_trap_all
insn_fcmla_idx_half:
    .inst 0x6F423020                // FCMLA V0.8H, V1.8H, V2.H[0], #90
    b    fail
after_fcmla_idx_half:
    fp_enable_el1
    check_handled 19
    check_v0_unchanged

    load_v0_sentinel
case_pmul:
    fp_trap_all
insn_pmul:
    .inst 0x2E229C20                // PMUL V0.8B, V1.8B, V2.8B
    b    fail
after_pmul:
    fp_enable_el1
    check_handled 20
    check_v0_unchanged

    load_v0_sentinel
case_pmull:
    fp_trap_all
insn_pmull:
    .inst 0x0E22E020                // PMULL V0.8H, V1.8B, V2.8B
    b    fail
after_pmull:
    fp_enable_el1
    check_handled 21
    check_v0_unchanged

    load_v0_sentinel
case_frint32z_scalar:
    fp_trap_all
insn_frint32z_scalar:
    .inst 0x1E284020                // FRINT32Z S0, S1
    b    fail
after_frint32z_scalar:
    fp_enable_el1
    check_handled 22
    check_v0_unchanged

    load_v0_sentinel
case_frint64x_vec:
    fp_trap_all
insn_frint64x_vec:
    .inst 0x6E61F820                // FRINT64X V0.2D, V1.2D
    b    fail
after_frint64x_vec:
    fp_enable_el1
    check_handled 23
    check_v0_unchanged

    load_v0_sentinel
case_fadd_half_scalar:
    fp_trap_all
insn_fadd_half_scalar:
    .inst 0x1EE22820                // FADD H0, H1, H2
    b    fail
after_fadd_half_scalar:
    fp_enable_el1
    check_handled 24
    check_v0_unchanged

    load_v0_sentinel
case_fmov_half_imm:
    fp_trap_all
insn_fmov_half_imm:
    .inst 0x1EEE1000                // FMOV H0, #1.0
    b    fail
after_fmov_half_imm:
    fp_enable_el1
    check_handled 25
    check_v0_unchanged

    load_v0_sentinel
case_fcsel_half:
    fp_trap_all
insn_fcsel_half:
    .inst 0x1EE20C20                // FCSEL H0, H1, H2, EQ
    b    fail
after_fcsel_half:
    fp_enable_el1
    check_handled 26
    check_v0_unchanged

    load_v0_sentinel
case_fadd_half_vector:
    fp_trap_all
insn_fadd_half_vector:
    .inst 0x4E421420                // FADD V0.8H, V1.8H, V2.8H
    b    fail
after_fadd_half_vector:
    fp_enable_el1
    check_handled 27
    check_v0_unchanged

    load_v0_sentinel
case_sha512su1:
    fp_trap_all
insn_sha512su1:
    .inst 0xCE628820                // SHA512SU1 V0.2D, V1.2D, V2.2D
    b    fail
after_sha512su1:
    fp_enable_el1
    check_handled 28
    check_v0_unchanged

    load_v0_sentinel
case_fmlal2_idx:
    fp_trap_all
insn_fmlal2_idx:
    .inst 0x4F820020                // FMLAL2 V0.4S, V1.4H, V2.H[0]
    b    fail
after_fmlal2_idx:
    fp_enable_el1
    check_handled 29
    check_v0_unchanged

    load_v0_sentinel
case_fmlsl2_idx:
    fp_trap_all
insn_fmlsl2_idx:
    .inst 0x4F824020                // FMLSL2 V0.4S, V1.4H, V2.H[1]
    b    fail
after_fmlsl2_idx:
    fp_enable_el1
    check_handled 30
    check_v0_unchanged

    mov  x0, #'N'
    b    done

fail:
    mov  x0, #'F'
    b    done

done:
    str  x0, [x20]
    brk  #0

    .balign 2048
vectors:
    b .
    .space 0x200 - 4, 0
sync_current_spx:
    mrs  x2, esr_el1
    lsr  x3, x2, #26
    cbnz x3, fail_from_handler
    ubfx x4, x2, #25, #1
    cmp  x4, #1
    b.ne fail_from_handler
    ubfx x5, x2, #0, #25
    cbnz x5, fail_from_handler
    mrs  x6, far_el1
    cbnz x6, fail_from_handler

    mrs  x26, elr_el1
    handle_case insn_fmlal_vec, after_fmlal_vec, 1
    handle_case insn_fmlal_idx, after_fmlal_idx, 2
    handle_case insn_ummla, after_ummla, 3
    handle_case insn_usdot_vec, after_usdot_vec, 4
    handle_case insn_sudot_idx, after_sudot_idx, 5
    handle_case insn_eor3, after_eor3, 6
    handle_case insn_xar, after_xar, 7
    handle_case insn_sm3ss1, after_sm3ss1, 8
    handle_case insn_sm3tt1a, after_sm3tt1a, 9
    handle_case insn_sm3partw1, after_sm3partw1, 10
    handle_case insn_sm4e, after_sm4e, 11
    handle_case insn_sm4ekey, after_sm4ekey, 12
    handle_case insn_sha512h, after_sha512h, 13
    handle_case insn_sha512su0, after_sha512su0, 14
    handle_case insn_sqrdmlah_vec_h, after_sqrdmlah_vec_h, 15
    handle_case insn_sqrdmlsh_scalar, after_sqrdmlsh_scalar, 16
    handle_case insn_sqrdmlah_idx_scalar, after_sqrdmlah_idx_scalar, 17
    handle_case insn_fcmla_idx_single, after_fcmla_idx_single, 18
    handle_case insn_fcmla_idx_half, after_fcmla_idx_half, 19
    handle_case insn_pmul, after_pmul, 20
    handle_case insn_pmull, after_pmull, 21
    handle_case insn_frint32z_scalar, after_frint32z_scalar, 22
    handle_case insn_frint64x_vec, after_frint64x_vec, 23
    handle_case insn_fadd_half_scalar, after_fadd_half_scalar, 24
    handle_case insn_fmov_half_imm, after_fmov_half_imm, 25
    handle_case insn_fcsel_half, after_fcsel_half, 26
    handle_case insn_fadd_half_vector, after_fadd_half_vector, 27
    handle_case insn_sha512su1, after_sha512su1, 28
    handle_case insn_fmlal2_idx, after_fmlal2_idx, 29
    handle_case insn_fmlsl2_idx, after_fmlsl2_idx, 30

fail_from_handler:
    mov  x0, #'H'
    adr  x24, done
    msr  elr_el1, x24
    eret

    .data
    .balign 16
v0_sentinel:
    .quad 0x1122334455667788
    .quad 0x99AABBCCDDEEFF00
fp_inputs:
    .quad 0x3ff0000000000000
    .quad 0x4000000000000000
    .quad 0x4008000000000000
    .quad 0x4010000000000000
    .quad 0x4014000000000000
    .quad 0x4018000000000000
handled:
    .quad 0
