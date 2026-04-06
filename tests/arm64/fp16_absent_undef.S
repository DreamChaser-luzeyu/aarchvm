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

    .macro check_v0_unchanged fail_label
    adr  x19, v0_sentinel
    ldr  x21, [x19]
    ldr  x22, [x19, #8]
    umov x23, v0.d[0]
    umov x24, v0.d[1]
    cmp  x23, x21
    b.ne \fail_label
    cmp  x24, x22
    b.ne \fail_label
    .endm

    .macro check_handled expected, fail_label
    adr  x11, handled
    ldr  x12, [x11]
    movz x13, #\expected
    cmp  x12, x13
    b.ne \fail_label
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
    ldr  q5, [x19]
    ldr  q6, [x19, #16]

    adr  x11, handled
    str  xzr, [x11]

    load_v0_sentinel
case_fadd_h:
insn_fadd_h:
    .inst 0x1EE22820                // FADD H0, H1, H2
    b    fail1
after_fadd_h:
    check_handled 1, fail2
    check_v0_unchanged fail3

    load_v0_sentinel
case_fabs_h:
insn_fabs_h:
    .inst 0x1EE0C083                // FABS H3, H4
    b    fail4
after_fabs_h:
    check_handled 2, fail5
    check_v0_unchanged fail6

    movz x14, #0xa000, lsl #16
    msr  nzcv, x14
    isb
case_fcmp_h:
insn_fcmp_h:
    .inst 0x1EE620A0                // FCMP H5, H6
    b    fail7
after_fcmp_h:
    mrs  x15, nzcv
    check_handled 3, fail8
    cmp  x15, x14
    b.ne fail9

    load_v0_sentinel
case_fadd_v4h:
insn_fadd_v4h:
    .inst 0x0E491507                // FADD V7.4H, V8.4H, V9.4H
    b    failA
after_fadd_v4h:
    check_handled 4, failB
    check_v0_unchanged failC

    load_v0_sentinel
case_fmov_h_h:
insn_fmov_h_h:
    .inst 0x1EE04020                // FMOV H0, H1
    b    failD
after_fmov_h_h:
    check_handled 5, failE
    check_v0_unchanged failF

    load_v0_sentinel
case_fmov_h_imm:
insn_fmov_h_imm:
    .inst 0x1EEE1000                // FMOV H0, #1.0
    b    failG
after_fmov_h_imm:
    check_handled 6, failH
    check_v0_unchanged failI

    movz x0, #0x1357
    movk x0, #0x2468, lsl #16
case_fmov_w_h:
insn_fmov_w_h:
    .inst 0x1EE60020                // FMOV W0, H1
    b    failJ
after_fmov_w_h:
    check_handled 7, failK
    movz x16, #0x1357
    movk x16, #0x2468, lsl #16
    cmp  x0, x16
    b.ne failL

    load_v0_sentinel
case_fmov_h_w:
insn_fmov_h_w:
    .inst 0x1EE70020                // FMOV H0, W1
    b    failM
after_fmov_h_w:
    check_handled 8, failN
    check_v0_unchanged failO

    load_v0_sentinel
case_scvtf_h_w:
insn_scvtf_h_w:
    .inst 0x1EE20020                // SCVTF H0, W1
    b    failQ
after_scvtf_h_w:
    check_handled 9, failR
    check_v0_unchanged failS

    movz x0, #0x55aa
    movk x0, #0xaa55, lsl #16
case_fcvtzs_w_h:
insn_fcvtzs_w_h:
    .inst 0x1EF80020                // FCVTZS W0, H1
    b    failT
after_fcvtzs_w_h:
    check_handled 10, failV
    movz x16, #0x55aa
    movk x16, #0xaa55, lsl #16
    cmp  x0, x16
    b.ne failW

    movz x14, #0x9000, lsl #16
    msr  nzcv, x14
    isb
case_fcmp_h_zero:
insn_fcmp_h_zero:
    .inst 0x1EE02008                // FCMP H0, #0.0
    b    failX
after_fcmp_h_zero:
    mrs  x15, nzcv
    check_handled 11, failY
    cmp  x15, x14
    b.ne failZ

    load_v0_sentinel
case_fsqrt_h:
insn_fsqrt_h:
    .inst 0x1EE1C020                // FSQRT H0, H1
    b    failAA
after_fsqrt_h:
    check_handled 12, failAB
    check_v0_unchanged failAC

    load_v0_sentinel
case_frinta_h:
insn_frinta_h:
    .inst 0x1EE64020                // FRINTA H0, H1
    b    failAD
after_frinta_h:
    check_handled 13, failAE
    check_v0_unchanged failAF

    load_v0_sentinel
case_frinti_h:
insn_frinti_h:
    .inst 0x1EE7C020                // FRINTI H0, H1
    b    failAG
after_frinti_h:
    check_handled 14, failAH
    check_v0_unchanged failAI

    load_v0_sentinel
case_ucvtf_h_w:
insn_ucvtf_h_w:
    .inst 0x1EE30020                // UCVTF H0, W1
    b    failAJ
after_ucvtf_h_w:
    check_handled 15, failAK
    check_v0_unchanged failAL

    movz x0, #0x2468
    movk x0, #0x1357, lsl #16
case_fcvtzu_w_h:
insn_fcvtzu_w_h:
    .inst 0x1EF90020                // FCVTZU W0, H1
    b    failAM
after_fcvtzu_w_h:
    check_handled 16, failAN
    movz x16, #0x2468
    movk x16, #0x1357, lsl #16
    cmp  x0, x16
    b.ne failAO

    load_v0_sentinel
case_fcsel_h:
insn_fcsel_h:
    .inst 0x1EE20C20                // FCSEL H0, H1, H2, EQ
    b    failAP
after_fcsel_h:
    check_handled 17, failAQ
    check_v0_unchanged failAR

    movz x14, #0x6000, lsl #16
    msr  nzcv, x14
    isb
case_fccmp_h:
insn_fccmp_h:
    .inst 0x1EE21425                // FCCMP H1, H2, #5, NE
    b    failAS
after_fccmp_h:
    mrs  x15, nzcv
    check_handled 18, failAT
    cmp  x15, x14
    b.ne failAU

    mov  x0, #'U'
    b    done

fail1:
    mov  x0, #'1'
    b    done
fail2:
    mov  x0, #'2'
    b    done
fail3:
    mov  x0, #'3'
    b    done
fail4:
    mov  x0, #'4'
    b    done
fail5:
    mov  x0, #'5'
    b    done
fail6:
    mov  x0, #'6'
    b    done
fail7:
    mov  x0, #'7'
    b    done
fail8:
    mov  x0, #'8'
    b    done
fail9:
    mov  x0, #'9'
    b    done
failA:
    mov  x0, #'A'
    b    done
failB:
    mov  x0, #'B'
    b    done
failC:
    mov  x0, #'C'
    b    done
failD:
    mov  x0, #'D'
    b    done
failE:
    mov  x0, #'E'
    b    done
failF:
    mov  x0, #'F'
    b    done
failG:
    mov  x0, #'G'
    b    done
failH:
    mov  x0, #'H'
    b    done
failI:
    mov  x0, #'I'
    b    done
failJ:
    mov  x0, #'J'
    b    done
failK:
    mov  x0, #'K'
    b    done
failL:
    mov  x0, #'L'
    b    done
failM:
    mov  x0, #'M'
    b    done
failN:
    mov  x0, #'N'
    b    done
failO:
    mov  x0, #'O'
    b    done
failQ:
    mov  x0, #'Q'
    b    done
failR:
    mov  x0, #'R'
    b    done
failS:
    mov  x0, #'S'
    b    done
failT:
    mov  x0, #'T'
    b    done
failV:
    mov  x0, #'V'
    b    done
failW:
    mov  x0, #'W'
    b    done
failX:
    mov  x0, #'X'
    b    done
failY:
    mov  x0, #'Y'
    b    done
failZ:
    mov  x0, #'Z'
    b    done
failAA:
    mov  x0, #'a'
    b    done
failAB:
    mov  x0, #'b'
    b    done
failAC:
    mov  x0, #'c'
    b    done
failAD:
    mov  x0, #'d'
    b    done
failAE:
    mov  x0, #'e'
    b    done
failAF:
    mov  x0, #'f'
    b    done
failAG:
    mov  x0, #'g'
    b    done
failAH:
    mov  x0, #'h'
    b    done
failAI:
    mov  x0, #'i'
    b    done
failAJ:
    mov  x0, #'j'
    b    done
failAK:
    mov  x0, #'k'
    b    done
failAL:
    mov  x0, #'l'
    b    done
failAM:
    mov  x0, #'m'
    b    done
failAN:
    mov  x0, #'n'
    b    done
failAO:
    mov  x0, #'o'
    b    done
failAP:
    mov  x0, #'p'
    b    done
failAQ:
    mov  x0, #'q'
    b    done
failAR:
    mov  x0, #'r'
    b    done
failAS:
    mov  x0, #'s'
    b    done
failAT:
    mov  x0, #'t'
    b    done
failAU:
    mov  x0, #'u'
    b    done
fail_from_handler:
    mov  x0, #'P'

done:
    str  x0, [x20]
    brk  #0

    .balign 2048
vectors:
    .space 0x200
sync_spx:
    b current_sync_handler
    .space 0x1fc
sync_lower:
    b fail_from_handler

current_sync_handler:
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

    mrs  x7, elr_el1
    adr  x8, insn_fadd_h
    cmp  x7, x8
    b.eq handle_fadd_h
    adr  x8, insn_fabs_h
    cmp  x7, x8
    b.eq handle_fabs_h
    adr  x8, insn_fcmp_h
    cmp  x7, x8
    b.eq handle_fcmp_h
    adr  x8, insn_fadd_v4h
    cmp  x7, x8
    b.eq handle_fadd_v4h
    adr  x8, insn_fmov_h_h
    cmp  x7, x8
    b.eq handle_fmov_h_h
    adr  x8, insn_fmov_h_imm
    cmp  x7, x8
    b.eq handle_fmov_h_imm
    adr  x8, insn_fmov_w_h
    cmp  x7, x8
    b.eq handle_fmov_w_h
    adr  x8, insn_fmov_h_w
    cmp  x7, x8
    b.eq handle_fmov_h_w
    adr  x8, insn_scvtf_h_w
    cmp  x7, x8
    b.eq handle_scvtf_h_w
    adr  x8, insn_fcvtzs_w_h
    cmp  x7, x8
    b.eq handle_fcvtzs_w_h
    adr  x8, insn_fcmp_h_zero
    cmp  x7, x8
    b.eq handle_fcmp_h_zero
    adr  x8, insn_fsqrt_h
    cmp  x7, x8
    b.eq handle_fsqrt_h
    adr  x8, insn_frinta_h
    cmp  x7, x8
    b.eq handle_frinta_h
    adr  x8, insn_frinti_h
    cmp  x7, x8
    b.eq handle_frinti_h
    adr  x8, insn_ucvtf_h_w
    cmp  x7, x8
    b.eq handle_ucvtf_h_w
    adr  x8, insn_fcvtzu_w_h
    cmp  x7, x8
    b.eq handle_fcvtzu_w_h
    adr  x8, insn_fcsel_h
    cmp  x7, x8
    b.eq handle_fcsel_h
    adr  x8, insn_fccmp_h
    cmp  x7, x8
    b.eq handle_fccmp_h
    b    fail_from_handler

handle_fadd_h:
    adr  x9, handled
    movz x10, #1
    str  x10, [x9]
    adr  x24, after_fadd_h
    msr  elr_el1, x24
    eret

handle_fabs_h:
    adr  x9, handled
    movz x10, #2
    str  x10, [x9]
    adr  x24, after_fabs_h
    msr  elr_el1, x24
    eret

handle_fcmp_h:
    adr  x9, handled
    movz x10, #3
    str  x10, [x9]
    adr  x24, after_fcmp_h
    msr  elr_el1, x24
    eret

handle_fadd_v4h:
    adr  x9, handled
    movz x10, #4
    str  x10, [x9]
    adr  x24, after_fadd_v4h
    msr  elr_el1, x24
    eret

handle_fmov_h_h:
    adr  x9, handled
    movz x10, #5
    str  x10, [x9]
    adr  x24, after_fmov_h_h
    msr  elr_el1, x24
    eret

handle_fmov_h_imm:
    adr  x9, handled
    movz x10, #6
    str  x10, [x9]
    adr  x24, after_fmov_h_imm
    msr  elr_el1, x24
    eret

handle_fmov_w_h:
    adr  x9, handled
    movz x10, #7
    str  x10, [x9]
    adr  x24, after_fmov_w_h
    msr  elr_el1, x24
    eret

handle_fmov_h_w:
    adr  x9, handled
    movz x10, #8
    str  x10, [x9]
    adr  x24, after_fmov_h_w
    msr  elr_el1, x24
    eret

handle_scvtf_h_w:
    adr  x9, handled
    movz x10, #9
    str  x10, [x9]
    adr  x24, after_scvtf_h_w
    msr  elr_el1, x24
    eret

handle_fcvtzs_w_h:
    adr  x9, handled
    movz x10, #10
    str  x10, [x9]
    adr  x24, after_fcvtzs_w_h
    msr  elr_el1, x24
    eret

handle_fcmp_h_zero:
    adr  x9, handled
    movz x10, #11
    str  x10, [x9]
    adr  x24, after_fcmp_h_zero
    msr  elr_el1, x24
    eret

handle_fsqrt_h:
    adr  x9, handled
    movz x10, #12
    str  x10, [x9]
    adr  x24, after_fsqrt_h
    msr  elr_el1, x24
    eret

handle_frinta_h:
    adr  x9, handled
    movz x10, #13
    str  x10, [x9]
    adr  x24, after_frinta_h
    msr  elr_el1, x24
    eret

handle_frinti_h:
    adr  x9, handled
    movz x10, #14
    str  x10, [x9]
    adr  x24, after_frinti_h
    msr  elr_el1, x24
    eret

handle_ucvtf_h_w:
    adr  x9, handled
    movz x10, #15
    str  x10, [x9]
    adr  x24, after_ucvtf_h_w
    msr  elr_el1, x24
    eret

handle_fcvtzu_w_h:
    adr  x9, handled
    movz x10, #16
    str  x10, [x9]
    adr  x24, after_fcvtzu_w_h
    msr  elr_el1, x24
    eret

handle_fcsel_h:
    adr  x9, handled
    movz x10, #17
    str  x10, [x9]
    adr  x24, after_fcsel_h
    msr  elr_el1, x24
    eret

handle_fccmp_h:
    adr  x9, handled
    movz x10, #18
    str  x10, [x9]
    adr  x24, after_fccmp_h
    msr  elr_el1, x24
    eret

    .data
    .balign 16
handled:
    .quad 0
v0_sentinel:
    .quad 0x1122334455667788, 0x99aabbccddeeff00
fp_inputs:
    .quad 0x400040003c003800, 0x7c0070000400c000
    .quad 0x3c00300020001800, 0x4400420041003e00
