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

    adr  x11, handled
    str  xzr, [x11]

    load_v0_sentinel
case_bfcvt_scalar:
    .inst 0x1e634020                // BFCVT H0, S1
    mov  x0, #'a'
    b    done
after_bfcvt_scalar:
    ldr  x12, [x11]
    movz x13, #1
    cmp  x12, x13
    b.ne fail1
    check_v0_unchanged fail2

    load_v0_sentinel
case_bfcvtn:
    .inst 0x0ea16820                // BFCVTN V0.4H, V1.4S
    mov  x0, #'b'
    b    done
after_bfcvtn:
    ldr  x12, [x11]
    movz x13, #2
    cmp  x12, x13
    b.ne fail3
    check_v0_unchanged fail4

    load_v0_sentinel
case_bfcvtn2:
    .inst 0x4ea16820                // BFCVTN2 V0.8H, V1.4S
    mov  x0, #'c'
    b    done
after_bfcvtn2:
    ldr  x12, [x11]
    movz x13, #3
    cmp  x12, x13
    b.ne fail5
    check_v0_unchanged fail6

    load_v0_sentinel
case_bfmlalb:
    .inst 0x2ec2fc20                // BFMLALB V0.4S, V1.8H, V2.8H
    mov  x0, #'d'
    b    done
after_bfmlalb:
    ldr  x12, [x11]
    movz x13, #4
    cmp  x12, x13
    b.ne fail7
    check_v0_unchanged fail8

    load_v0_sentinel
case_bfdot:
    .inst 0x6e42fc20                // BFDOT V0.4S, V1.8H, V2.8H
    mov  x0, #'e'
    b    done
after_bfdot:
    ldr  x12, [x11]
    movz x13, #5
    cmp  x12, x13
    b.ne fail9
    check_v0_unchanged failA

    load_v0_sentinel
case_bfmmla:
    .inst 0x6e42ec20                // BFMMLA V0.4S, V1.8H, V2.8H
    mov  x0, #'f'
    b    done
after_bfmmla:
    ldr  x12, [x11]
    movz x13, #6
    cmp  x12, x13
    b.ne failB
    check_v0_unchanged failC

    mov  x0, #'F'
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
    adr  x8, case_bfcvt_scalar
    cmp  x7, x8
    b.eq handle_bfcvt_scalar
    adr  x8, case_bfcvtn
    cmp  x7, x8
    b.eq handle_bfcvtn
    adr  x8, case_bfmlalb
    cmp  x7, x8
    b.eq handle_bfmlalb
    adr  x8, case_bfcvtn2
    cmp  x7, x8
    b.eq handle_bfcvtn2
    adr  x8, case_bfdot
    cmp  x7, x8
    b.eq handle_bfdot
    adr  x8, case_bfmmla
    cmp  x7, x8
    b.eq handle_bfmmla
    b    trap_fail5

handle_bfcvt_scalar:
    adr  x9, handled
    movz x10, #1
    str  x10, [x9]
    adr  x24, after_bfcvt_scalar
    msr  elr_el1, x24
    eret

handle_bfcvtn:
    adr  x9, handled
    movz x10, #2
    str  x10, [x9]
    adr  x24, after_bfcvtn
    msr  elr_el1, x24
    eret

handle_bfmlalb:
    adr  x9, handled
    movz x10, #4
    str  x10, [x9]
    adr  x24, after_bfmlalb
    msr  elr_el1, x24
    eret

handle_bfcvtn2:
    adr  x9, handled
    movz x10, #3
    str  x10, [x9]
    adr  x24, after_bfcvtn2
    msr  elr_el1, x24
    eret

handle_bfdot:
    adr  x9, handled
    movz x10, #5
    str  x10, [x9]
    adr  x24, after_bfdot
    msr  elr_el1, x24
    eret

handle_bfmmla:
    adr  x9, handled
    movz x10, #6
    str  x10, [x9]
    adr  x24, after_bfmmla
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
    .quad 0x99aabbccddeeff00
fp_inputs:
    .quad 0x3f80000040000000
    .quad 0x4040000040800000
    .quad 0x40003f0042004100
    .quad 0x4400430046004500

    .balign 8
handled:
    .quad 0
