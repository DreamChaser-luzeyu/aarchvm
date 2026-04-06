    .arch armv8.2-a+fp16
    .text
    .global _start

    .macro fp_enable_el1
    movz x25, #0x0030, lsl #16
    msr  cpacr_el1, x25
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

    adr  x11, handled
    str  xzr, [x11]

    load_v0_sentinel
insn_fmul_half_scalar:
    .inst 0x1EE20820                // FMUL H0, H1, H2
    b    fail
after_fmul_half_scalar:
    check_handled 1
    check_v0_unchanged

    load_v0_sentinel
insn_fdiv_half_scalar:
    .inst 0x1EE21820                // FDIV H0, H1, H2
    b    fail
after_fdiv_half_scalar:
    check_handled 2
    check_v0_unchanged

    load_v0_sentinel
insn_fmax_half_scalar:
    .inst 0x1EE24820                // FMAX H0, H1, H2
    b    fail
after_fmax_half_scalar:
    check_handled 3
    check_v0_unchanged

    load_v0_sentinel
insn_fminnm_half_scalar:
    .inst 0x1EE27820                // FMINNM H0, H1, H2
    b    fail
after_fminnm_half_scalar:
    check_handled 4
    check_v0_unchanged

    load_v0_sentinel
insn_fnmul_half_scalar:
    .inst 0x1EE28820                // FNMUL H0, H1, H2
    b    fail
after_fnmul_half_scalar:
    check_handled 5
    check_v0_unchanged

    movz x0, #0x1357
    movk x0, #0x2468, lsl #16
insn_fcvtas_half_to_w:
    .inst 0x1EE40020                // FCVTAS W0, H1
    b    fail
after_fcvtas_half_to_w:
    check_handled 6
    movz x16, #0x1357
    movk x16, #0x2468, lsl #16
    cmp  x0, x16
    b.ne fail

    load_v0_sentinel
insn_fmul_half_vector:
    .inst 0x6E421C20                // FMUL V0.8H, V1.8H, V2.8H
    b    fail
after_fmul_half_vector:
    check_handled 7
    check_v0_unchanged

    load_v0_sentinel
insn_fdiv_half_vector:
    .inst 0x6E423C20                // FDIV V0.8H, V1.8H, V2.8H
    b    fail
after_fdiv_half_vector:
    check_handled 8
    check_v0_unchanged

    load_v0_sentinel
insn_fmax_half_vector:
    .inst 0x4E423420                // FMAX V0.8H, V1.8H, V2.8H
    b    fail
after_fmax_half_vector:
    check_handled 9
    check_v0_unchanged

    load_v0_sentinel
insn_fminnm_half_vector:
    .inst 0x4EC20420                // FMINNM V0.8H, V1.8H, V2.8H
    b    fail
after_fminnm_half_vector:
    check_handled 10
    check_v0_unchanged

    load_v0_sentinel
insn_fmulx_half_scalar:
    .inst 0x5E421C20                // FMULX H0, H1, H2
    b    fail
after_fmulx_half_scalar:
    check_handled 11
    check_v0_unchanged

    load_v0_sentinel
insn_frecpe_half_scalar:
    .inst 0x5EF9D820                // FRECPE H0, H1
    b    fail
after_frecpe_half_scalar:
    check_handled 12
    check_v0_unchanged

    load_v0_sentinel
insn_frecps_half_scalar:
    .inst 0x5E423C20                // FRECPS H0, H1, H2
    b    fail
after_frecps_half_scalar:
    check_handled 13
    check_v0_unchanged

    load_v0_sentinel
insn_frecpx_half_scalar:
    .inst 0x5EF9F820                // FRECPX H0, H1
    b    fail
after_frecpx_half_scalar:
    check_handled 14
    check_v0_unchanged

    load_v0_sentinel
insn_frsqrte_half_scalar:
    .inst 0x7EF9D820                // FRSQRTE H0, H1
    b    fail
after_frsqrte_half_scalar:
    check_handled 15
    check_v0_unchanged

    load_v0_sentinel
insn_frsqrts_half_scalar:
    .inst 0x5EC23C20                // FRSQRTS H0, H1, H2
    b    fail
after_frsqrts_half_scalar:
    check_handled 16
    check_v0_unchanged

    load_v0_sentinel
insn_fmaxnm_half_vector_4h:
    .inst 0x0E420420                // FMAXNM V0.4H, V1.4H, V2.4H
    b    fail
after_fmaxnm_half_vector_4h:
    check_handled 17
    check_v0_unchanged

    load_v0_sentinel
insn_fmulx_half_vector_indexed:
    .inst 0x6F029020                // FMULX V0.8H, V1.8H, V2.H[0]
    b    fail
after_fmulx_half_vector_indexed:
    check_handled 18
    check_v0_unchanged

    mov  x0, #'G'
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
    handle_case insn_fmul_half_scalar, after_fmul_half_scalar, 1
    handle_case insn_fdiv_half_scalar, after_fdiv_half_scalar, 2
    handle_case insn_fmax_half_scalar, after_fmax_half_scalar, 3
    handle_case insn_fminnm_half_scalar, after_fminnm_half_scalar, 4
    handle_case insn_fnmul_half_scalar, after_fnmul_half_scalar, 5
    handle_case insn_fcvtas_half_to_w, after_fcvtas_half_to_w, 6
    handle_case insn_fmul_half_vector, after_fmul_half_vector, 7
    handle_case insn_fdiv_half_vector, after_fdiv_half_vector, 8
    handle_case insn_fmax_half_vector, after_fmax_half_vector, 9
    handle_case insn_fminnm_half_vector, after_fminnm_half_vector, 10
    handle_case insn_fmulx_half_scalar, after_fmulx_half_scalar, 11
    handle_case insn_frecpe_half_scalar, after_frecpe_half_scalar, 12
    handle_case insn_frecps_half_scalar, after_frecps_half_scalar, 13
    handle_case insn_frecpx_half_scalar, after_frecpx_half_scalar, 14
    handle_case insn_frsqrte_half_scalar, after_frsqrte_half_scalar, 15
    handle_case insn_frsqrts_half_scalar, after_frsqrts_half_scalar, 16
    handle_case insn_fmaxnm_half_vector_4h, after_fmaxnm_half_vector_4h, 17
    handle_case insn_fmulx_half_vector_indexed, after_fmulx_half_vector_indexed, 18

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
handled:
    .quad 0
