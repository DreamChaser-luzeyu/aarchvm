    .text
    .global _start

    .macro load_v0_sentinel
    adr  x19, v0_sentinel
    ldr  q0, [x19]
    .endm

    .macro check_v0_unchanged fail_label
    adr  x19, v0_sentinel
    ldr  x25, [x19]
    ldr  x26, [x19, #8]
    umov x23, v0.d[0]
    umov x24, v0.d[1]
    cmp  x23, x25
    b.ne \fail_label
    cmp  x24, x26
    b.ne \fail_label
    .endm

_start:
    movz x20, #0
    movk x20, #0x0900, lsl #16

    adr  x22, vectors
    msr  vbar_el1, x22
    isb

    movz x25, #0x0030, lsl #16
    msr  cpacr_el1, x25
    isb

    adr  x19, fp_inputs
    ldr  q1, [x19]
    ldr  q2, [x19, #16]
    ldr  q3, [x19, #32]

    adr  x11, handled
    str  xzr, [x11]

    load_v0_sentinel
case_sqrdmlah_vec:
    .inst 0x6E428420                // SQRDMLAH V0.8H, V1.8H, V2.8H
    mov  x0, #'a'
    b    done
after_sqrdmlah_vec:
    ldr  x12, [x11]
    movz x13, #1
    cmp  x12, x13
    b.ne fail1
    check_v0_unchanged fail2

    load_v0_sentinel
case_sqrdmlsh_scalar:
    .inst 0x7E828C20                // SQRDMLSH S0, S1, S2
    mov  x0, #'b'
    b    done
after_sqrdmlsh_scalar:
    ldr  x12, [x11]
    movz x13, #2
    cmp  x12, x13
    b.ne fail3
    check_v0_unchanged fail4

    load_v0_sentinel
case_sqrdmlah_idx_vec:
    .inst 0x6FA2D020                // SQRDMLAH V0.4S, V1.4S, V2.S[1]
    mov  x0, #'c'
    b    done
after_sqrdmlah_idx_vec:
    ldr  x12, [x11]
    movz x13, #3
    cmp  x12, x13
    b.ne fail5
    check_v0_unchanged fail6

    load_v0_sentinel
case_sqrdmlah_idx_scalar:
    .inst 0x7F83D020                // SQRDMLAH S0, S1, V3.S[0]
    mov  x0, #'d'
    b    done
after_sqrdmlah_idx_scalar:
    ldr  x12, [x11]
    movz x13, #4
    cmp  x12, x13
    b.ne fail7
    check_v0_unchanged fail8

    mov  x0, #'R'
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
    cbnz x3, trap_fail1
    ubfx x4, x2, #25, #1
    cmp  x4, #1
    b.ne trap_fail2
    ubfx x5, x2, #0, #25
    cbnz x5, trap_fail3
    mrs  x6, far_el1
    cbnz x6, trap_fail4

    mrs  x7, elr_el1
    adr  x8, case_sqrdmlah_vec
    cmp  x7, x8
    b.eq handle_sqrdmlah_vec
    adr  x8, case_sqrdmlsh_scalar
    cmp  x7, x8
    b.eq handle_sqrdmlsh_scalar
    adr  x8, case_sqrdmlah_idx_vec
    cmp  x7, x8
    b.eq handle_sqrdmlah_idx_vec
    adr  x8, case_sqrdmlah_idx_scalar
    cmp  x7, x8
    b.eq handle_sqrdmlah_idx_scalar
    b    trap_fail5

handle_sqrdmlah_vec:
    adr  x9, handled
    movz x10, #1
    str  x10, [x9]
    adr  x24, after_sqrdmlah_vec
    msr  elr_el1, x24
    eret

handle_sqrdmlsh_scalar:
    adr  x9, handled
    movz x10, #2
    str  x10, [x9]
    adr  x24, after_sqrdmlsh_scalar
    msr  elr_el1, x24
    eret

handle_sqrdmlah_idx_vec:
    adr  x9, handled
    movz x10, #3
    str  x10, [x9]
    adr  x24, after_sqrdmlah_idx_vec
    msr  elr_el1, x24
    eret

handle_sqrdmlah_idx_scalar:
    adr  x9, handled
    movz x10, #4
    str  x10, [x9]
    adr  x24, after_sqrdmlah_idx_scalar
    msr  elr_el1, x24
    eret

trap_fail1:
    mov  x0, #'x'
    b    done
trap_fail2:
    mov  x0, #'y'
    b    done
trap_fail3:
    mov  x0, #'z'
    b    done
trap_fail4:
    mov  x0, #'w'
    b    done
trap_fail5:
    mov  x0, #'v'
    b    done

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
