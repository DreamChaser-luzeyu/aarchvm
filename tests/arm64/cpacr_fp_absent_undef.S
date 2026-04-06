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

case_fjcvtzs_el1:
    fp_trap_all
    movz x0, #0x1111
insn_fjcvtzs_el1:
    .inst 0x1E7E0020                // FJCVTZS W0, D1
    mov  x0, #'a'
    b    done
after_fjcvtzs_el1:
    fp_enable_el1
    ldr  x12, [x11]
    movz x13, #1
    cmp  x12, x13
    b.ne fail1
    movz x14, #0x1111
    cmp  x0, x14
    b.ne fail2

case_bfcvtn_el1:
    load_v0_sentinel
    fp_trap_all
insn_bfcvtn_el1:
    .inst 0x0EA16820                // BFCVTN V0.4H, V1.4S
    mov  x0, #'b'
    b    done
after_bfcvtn_el1:
    fp_enable_el1
    ldr  x12, [x11]
    movz x13, #2
    cmp  x12, x13
    b.ne fail3
    check_v0_unchanged fail4

case_fcadd_el1:
    load_v0_sentinel
    fp_trap_all
insn_fcadd_el1:
    .inst 0x6E82E420                // FCADD V0.4S, V1.4S, V2.4S, #90
    mov  x0, #'c'
    b    done
after_fcadd_el1:
    fp_enable_el1
    ldr  x12, [x11]
    movz x13, #3
    cmp  x12, x13
    b.ne fail5
    check_v0_unchanged fail6

case_aese_el1:
    load_v0_sentinel
    fp_trap_all
insn_aese_el1:
    .inst 0x4E284820                // AESE V0.16B, V1.16B
    mov  x0, #'d'
    b    done
after_aese_el1:
    fp_enable_el1
    ldr  x12, [x11]
    movz x13, #4
    cmp  x12, x13
    b.ne fail7
    check_v0_unchanged fail8

case_udot_el1:
    load_v0_sentinel
    fp_trap_all
insn_udot_el1:
    .inst 0x6E829420                // UDOT V0.4S, V1.16B, V2.16B
    mov  x0, #'e'
    b    done
after_udot_el1:
    fp_enable_el1
    ldr  x12, [x11]
    movz x13, #5
    cmp  x12, x13
    b.ne fail9
    check_v0_unchanged failA

    load_v0_sentinel
    movz x25, #0x0010, lsl #16
    msr  cpacr_el1, x25
    isb
    adr  x3, el0_fcadd_absent
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret

after_fcadd_el0:
    fp_enable_el1
    adr  x11, handled
    ldr  x12, [x11]
    movz x13, #6
    cmp  x12, x13
    b.ne failB
    check_v0_unchanged failC

    mov  x0, #'Q'
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
failH:
    mov  x0, #'H'

done:
    str  x0, [x20]
    brk  #0

el0_fcadd_absent:
insn_fcadd_el0:
    .inst 0x6E82E420                // FCADD V0.4S, V1.4S, V2.4S, #90
    svc  #0x3f

    .balign 2048
vectors:
    .space 0x200
sync_spx:
    b current_sync_handler
    .space 0x1fc
sync_lower:
    b lower_sync_handler

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
    adr  x8, insn_fjcvtzs_el1
    cmp  x7, x8
    b.eq handle_fjcvtzs_el1
    adr  x8, insn_bfcvtn_el1
    cmp  x7, x8
    b.eq handle_bfcvtn_el1
    adr  x8, insn_fcadd_el1
    cmp  x7, x8
    b.eq handle_fcadd_el1
    adr  x8, insn_aese_el1
    cmp  x7, x8
    b.eq handle_aese_el1
    adr  x8, insn_udot_el1
    cmp  x7, x8
    b.eq handle_udot_el1
    b    fail_from_handler

handle_fjcvtzs_el1:
    adr  x9, handled
    movz x10, #1
    str  x10, [x9]
    adr  x24, after_fjcvtzs_el1
    msr  elr_el1, x24
    eret

handle_bfcvtn_el1:
    adr  x9, handled
    movz x10, #2
    str  x10, [x9]
    adr  x24, after_bfcvtn_el1
    msr  elr_el1, x24
    eret

handle_fcadd_el1:
    adr  x9, handled
    movz x10, #3
    str  x10, [x9]
    adr  x24, after_fcadd_el1
    msr  elr_el1, x24
    eret

handle_aese_el1:
    adr  x9, handled
    movz x10, #4
    str  x10, [x9]
    adr  x24, after_aese_el1
    msr  elr_el1, x24
    eret

handle_udot_el1:
    adr  x9, handled
    movz x10, #5
    str  x10, [x9]
    adr  x24, after_udot_el1
    msr  elr_el1, x24
    eret

lower_sync_handler:
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
    adr  x8, insn_fcadd_el0
    cmp  x7, x8
    b.ne fail_from_handler
    adr  x9, handled
    movz x10, #6
    str  x10, [x9]
    adr  x24, after_fcadd_el0
    msr  elr_el1, x24
    movz x11, #0x5
    msr  spsr_el1, x11
    eret

fail_from_handler:
    adr  x24, failH
    msr  elr_el1, x24
    movz x11, #0x5
    msr  spsr_el1, x11
    eret

    .data
    .balign 16
fp_inputs:
    .quad 0x3f80000040000000, 0x40a0000040c00000
    .quad 0x3f0000003f400000, 0x3f8000003fc00000
v0_sentinel:
    .quad 0x1122334455667788, 0x99aabbccddeeff00
handled:
    .quad 0
