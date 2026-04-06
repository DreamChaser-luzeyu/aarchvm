    .arch armv8.2-a+fp16
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

    .macro set_nzcv_sentinel
    movz x14, #0xA000, lsl #16
    msr  nzcv, x14
    .endm

    .macro check_nzcv_unchanged
    mrs  x15, nzcv
    cmp  x15, x14
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
    fp_trap_all
insn_fmov_half_scalar:
    .inst 0x1EE04020                // FMOV H0, H1
    b    fail
after_fmov_half_scalar:
    fp_enable_el1
    check_handled 1
    check_v0_unchanged

    load_v0_sentinel
    fp_trap_all
insn_fabs_half_scalar:
    .inst 0x1EE0C020                // FABS H0, H1
    b    fail
after_fabs_half_scalar:
    fp_enable_el1
    check_handled 2
    check_v0_unchanged

    load_v0_sentinel
    fp_trap_all
insn_fneg_half_scalar:
    .inst 0x1EE14020                // FNEG H0, H1
    b    fail
after_fneg_half_scalar:
    fp_enable_el1
    check_handled 3
    check_v0_unchanged

    load_v0_sentinel
    fp_trap_all
insn_frinti_half_scalar:
    .inst 0x1EE7C020                // FRINTI H0, H1
    b    fail
after_frinti_half_scalar:
    fp_enable_el1
    check_handled 4
    check_v0_unchanged

    set_nzcv_sentinel
    fp_trap_all
insn_fcmp_half_scalar:
    .inst 0x1EE22020                // FCMP H1, H2
    b    fail
after_fcmp_half_scalar:
    fp_enable_el1
    check_handled 5

    set_nzcv_sentinel
    fp_trap_all
insn_fcmpe_half_zero:
    .inst 0x1EE02038                // FCMPE H1, #0.0
    b    fail
after_fcmpe_half_zero:
    fp_enable_el1
    check_handled 6

    set_nzcv_sentinel
    fp_trap_all
insn_fccmp_half_scalar:
    .inst 0x1EE20420                // FCCMP H1, H2, #0, EQ
    b    fail
after_fccmp_half_scalar:
    fp_enable_el1
    check_handled 7

    set_nzcv_sentinel
    fp_trap_all
insn_fccmpe_half_scalar:
    .inst 0x1EE20430                // FCCMPE H1, H2, #0, EQ
    b    fail
after_fccmpe_half_scalar:
    fp_enable_el1
    check_handled 8

    load_v0_sentinel
    set_nzcv_sentinel
    fp_trap_all
insn_fcsel_half_scalar:
    .inst 0x1EE20C20                // FCSEL H0, H1, H2, EQ
    b    fail
after_fcsel_half_scalar:
    fp_enable_el1
    check_handled 9
    check_v0_unchanged

    load_v0_sentinel
    fp_trap_all
insn_fmadd_half_scalar:
    .inst 0x1FC20C20                // FMADD H0, H1, H2, H3
    b    fail
after_fmadd_half_scalar:
    fp_enable_el1
    check_handled 10
    check_v0_unchanged

    movz w1, #0x5678
    movk w1, #0x1234, lsl #16
    load_v0_sentinel
    fp_trap_all
insn_scvtf_half_from_w:
    .inst 0x1EE20020                // SCVTF H0, W1
    b    fail
after_scvtf_half_from_w:
    fp_enable_el1
    check_handled 11
    check_v0_unchanged

    movz w1, #0x5678
    movk w1, #0x1234, lsl #16
    load_v0_sentinel
    fp_trap_all
insn_ucvtf_half_from_w:
    .inst 0x1EE30020                // UCVTF H0, W1
    b    fail
after_ucvtf_half_from_w:
    fp_enable_el1
    check_handled 12
    check_v0_unchanged

    movz w0, #0x2468
    movk w0, #0x1357, lsl #16
    fp_trap_all
insn_fmov_w_from_half:
    .inst 0x1EE60020                // FMOV W0, H1
    b    fail
after_fmov_w_from_half:
    fp_enable_el1
    check_handled 13
    movz w16, #0x2468
    movk w16, #0x1357, lsl #16
    cmp  w0, w16
    b.ne fail

    movz w1, #0x5678
    movk w1, #0x1234, lsl #16
    load_v0_sentinel
    fp_trap_all
insn_fmov_half_from_w:
    .inst 0x1EE70020                // FMOV H0, W1
    b    fail
after_fmov_half_from_w:
    fp_enable_el1
    check_handled 14
    check_v0_unchanged

    load_v0_sentinel
    fp_trap_all
insn_fabd_half_scalar:
    .inst 0x7EC21420                // FABD H0, H1, H2
    b    fail
after_fabd_half_scalar:
    fp_enable_el1
    check_handled 15
    check_v0_unchanged

    load_v0_sentinel
    fp_trap_all
insn_fcmeq_half_scalar:
    .inst 0x5E422420                // FCMEQ H0, H1, H2
    b    fail
after_fcmeq_half_scalar:
    fp_enable_el1
    check_handled 16
    check_v0_unchanged

    load_v0_sentinel
    fp_trap_all
insn_facge_half_scalar:
    .inst 0x7E422C20                // FACGE H0, H1, H2
    b    fail
after_facge_half_scalar:
    fp_enable_el1
    check_handled 17
    check_v0_unchanged

    load_v0_sentinel
    fp_trap_all
insn_fcmeq_half_zero_scalar:
    .inst 0x5EF8D820                // FCMEQ H0, H1, #0.0
    b    fail
after_fcmeq_half_zero_scalar:
    fp_enable_el1
    check_handled 18
    check_v0_unchanged

    load_v0_sentinel
    fp_trap_all
insn_fcmlt_half_zero_scalar:
    .inst 0x5EF8E820                // FCMLT H0, H1, #0.0
    b    fail
after_fcmlt_half_zero_scalar:
    fp_enable_el1
    check_handled 19
    check_v0_unchanged

    load_v0_sentinel
    fp_trap_all
insn_fabd_half_vector:
    .inst 0x6EC21420                // FABD V0.8H, V1.8H, V2.8H
    b    fail
after_fabd_half_vector:
    fp_enable_el1
    check_handled 20
    check_v0_unchanged

    load_v0_sentinel
    fp_trap_all
insn_fcmeq_half_vector:
    .inst 0x4E422420                // FCMEQ V0.8H, V1.8H, V2.8H
    b    fail
after_fcmeq_half_vector:
    fp_enable_el1
    check_handled 21
    check_v0_unchanged

    load_v0_sentinel
    fp_trap_all
insn_fcmeq_half_zero_vector:
    .inst 0x4EF8D820                // FCMEQ V0.8H, V1.8H, #0.0
    b    fail
after_fcmeq_half_zero_vector:
    fp_enable_el1
    check_handled 22
    check_v0_unchanged

    mov  x0, #'P'
    b    done

fail:
    adr  x9, handled
    ldr  x10, [x9]
    cmp  x10, #9
    b.gt fail_alpha
    add  x0, x10, #'0'
    b    done

fail_alpha:
    sub  x10, x10, #10
    add  x0, x10, #'A'
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
    handle_case insn_fmov_half_scalar, after_fmov_half_scalar, 1
    handle_case insn_fabs_half_scalar, after_fabs_half_scalar, 2
    handle_case insn_fneg_half_scalar, after_fneg_half_scalar, 3
    handle_case insn_frinti_half_scalar, after_frinti_half_scalar, 4
    handle_case insn_fcmp_half_scalar, after_fcmp_half_scalar, 5
    handle_case insn_fcmpe_half_zero, after_fcmpe_half_zero, 6
    handle_case insn_fccmp_half_scalar, after_fccmp_half_scalar, 7
    handle_case insn_fccmpe_half_scalar, after_fccmpe_half_scalar, 8
    handle_case insn_fcsel_half_scalar, after_fcsel_half_scalar, 9
    handle_case insn_fmadd_half_scalar, after_fmadd_half_scalar, 10
    handle_case insn_scvtf_half_from_w, after_scvtf_half_from_w, 11
    handle_case insn_ucvtf_half_from_w, after_ucvtf_half_from_w, 12
    handle_case insn_fmov_w_from_half, after_fmov_w_from_half, 13
    handle_case insn_fmov_half_from_w, after_fmov_half_from_w, 14
    handle_case insn_fabd_half_scalar, after_fabd_half_scalar, 15
    handle_case insn_fcmeq_half_scalar, after_fcmeq_half_scalar, 16
    handle_case insn_facge_half_scalar, after_facge_half_scalar, 17
    handle_case insn_fcmeq_half_zero_scalar, after_fcmeq_half_zero_scalar, 18
    handle_case insn_fcmlt_half_zero_scalar, after_fcmlt_half_zero_scalar, 19
    handle_case insn_fabd_half_vector, after_fabd_half_vector, 20
    handle_case insn_fcmeq_half_vector, after_fcmeq_half_vector, 21
    handle_case insn_fcmeq_half_zero_vector, after_fcmeq_half_zero_vector, 22

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
handled:
    .quad 0x0
fp_inputs:
    .quad 0x3ff0000000000000
    .quad 0x0
    .quad 0x4000000000000000
    .quad 0x0
    .quad 0x4008000000000000
    .quad 0x0
