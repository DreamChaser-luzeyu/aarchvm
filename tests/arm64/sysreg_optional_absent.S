    .text
    .global _start
_start:
    movz x1, #0x0000
    movk x1, #0x0900, lsl #16      // UART

    adr  x10, vectors
    msr  vbar_el1, x10

    adr  x11, trap_flags
    str  xzr, [x11]

    movz x4, #0x0280
    movk x4, #0x9040, lsl #16      // NZCV=1001, PAN=1, D=1, I=1, EL0t.

    // EL1: TPIDR2_EL0 is absent without FEAT_SME.
    movz x0, #0x7788
    movk x0, #0x5566, lsl #16
    movk x0, #0x3344, lsl #32
    movk x0, #0x1122, lsl #48
el1_tpidr2_mrs_insn:
    .inst 0xD53BD0A0                  // mrs x0, tpidr2_el0
    brk  #0

after_el1_tpidr2_mrs:
    movz x2, #0x7788
    movk x2, #0x5566, lsl #16
    movk x2, #0x3344, lsl #32
    movk x2, #0x1122, lsl #48
    cmp  x0, x2
    b.ne fail1
    adr  x11, trap_flags
    ldr  x12, [x11]
    tbz  x12, #0, fail2

    movz x0, #0xbeef
    movk x0, #0xcafe, lsl #16
    movk x0, #0x5678, lsl #32
    movk x0, #0x1234, lsl #48
el1_tpidr2_msr_insn:
    .inst 0xD51BD0A0                  // msr tpidr2_el0, x0
    brk  #0

after_el1_tpidr2_msr:
    adr  x11, trap_flags
    ldr  x12, [x11]
    tbz  x12, #1, fail3

    // EL1: GCSPR_EL0 is absent without FEAT_GCS.
    movz x0, #0x4321
    movk x0, #0x8765, lsl #16
    movk x0, #0xcba9, lsl #32
    movk x0, #0x0fed, lsl #48
el1_gcspr_mrs_insn:
    .inst 0xD53B2520                  // mrs x0, gcspr_el0
    brk  #0

after_el1_gcspr_mrs:
    movz x2, #0x4321
    movk x2, #0x8765, lsl #16
    movk x2, #0xcba9, lsl #32
    movk x2, #0x0fed, lsl #48
    cmp  x0, x2
    b.ne fail4
    adr  x11, trap_flags
    ldr  x12, [x11]
    tbz  x12, #2, fail5

    movz x0, #0x55aa
    movk x0, #0xa55a, lsl #16
el1_gcspr_msr_insn:
    .inst 0xD51B2520                  // msr gcspr_el0, x0
    brk  #0

after_el1_gcspr_msr:
    adr  x11, trap_flags
    ldr  x12, [x11]
    tbz  x12, #3, fail6

    // EL1: PMUSERENR_EL0 is absent without FEAT_PMUv3.
    movz x0, #0x2468
    movk x0, #0x1357, lsl #16
    movk x0, #0x9bdf, lsl #32
    movk x0, #0xace0, lsl #48
el1_pmuserenr_mrs_insn:
    .inst 0xD53B9E00                  // mrs x0, pmuserenr_el0
    brk  #0

after_el1_pmuserenr_mrs:
    movz x2, #0x2468
    movk x2, #0x1357, lsl #16
    movk x2, #0x9bdf, lsl #32
    movk x2, #0xace0, lsl #48
    cmp  x0, x2
    b.ne fail7
    adr  x11, trap_flags
    ldr  x12, [x11]
    tbz  x12, #4, fail8

    movz x0, #0x1111
    movk x0, #0x2222, lsl #16
el1_pmuserenr_msr_insn:
    .inst 0xD51B9E00                  // msr pmuserenr_el0, x0
    brk  #0

after_el1_pmuserenr_msr:
    adr  x11, trap_flags
    ldr  x12, [x11]
    tbz  x12, #5, fail9

    // EL1: AMUSERENR_EL0 is absent without FEAT_AMUv1.
    movz x0, #0xaaaa
    movk x0, #0xbbbb, lsl #16
    movk x0, #0xcccc, lsl #32
    movk x0, #0xdddd, lsl #48
el1_amuserenr_mrs_insn:
    .inst 0xD53BD260                  // mrs x0, amuserenr_el0
    brk  #0

after_el1_amuserenr_mrs:
    movz x2, #0xaaaa
    movk x2, #0xbbbb, lsl #16
    movk x2, #0xcccc, lsl #32
    movk x2, #0xdddd, lsl #48
    cmp  x0, x2
    b.ne failA
    adr  x11, trap_flags
    ldr  x12, [x11]
    tbz  x12, #6, failB

    movz x0, #0x9999
    movk x0, #0x8888, lsl #16
el1_amuserenr_msr_insn:
    .inst 0xD51BD260                  // msr amuserenr_el0, x0
    brk  #0

after_el1_amuserenr_msr:
    adr  x11, trap_flags
    ldr  x12, [x11]
    tbz  x12, #7, failC

    // EL0: absent-feature sysregs must also be UNDEFINED, not EC=0x18 traps.
    adr  x3, el0_tpidr2_mrs
    msr  elr_el1, x3
    msr  spsr_el1, x4
    eret

after_el0_tpidr2_mrs:
    movz x2, #0x1020
    movk x2, #0x3040, lsl #16
    movk x2, #0x5060, lsl #32
    movk x2, #0x7080, lsl #48
    cmp  x0, x2
    b.ne failD
    adr  x11, trap_flags
    ldr  x12, [x11]
    tbz  x12, #8, failE

    adr  x3, el0_gcspr_mrs
    msr  elr_el1, x3
    msr  spsr_el1, x4
    eret

after_el0_gcspr_mrs:
    movz x2, #0x1122
    movk x2, #0x3344, lsl #16
    movk x2, #0x5566, lsl #32
    movk x2, #0x7788, lsl #48
    cmp  x0, x2
    b.ne failF
    adr  x11, trap_flags
    ldr  x12, [x11]
    tbz  x12, #9, failG

    adr  x3, el0_pmuserenr_mrs
    msr  elr_el1, x3
    msr  spsr_el1, x4
    eret

after_el0_pmuserenr_mrs:
    movz x2, #0x89ab
    movk x2, #0xcdef, lsl #16
    movk x2, #0x0123, lsl #32
    movk x2, #0x4567, lsl #48
    cmp  x0, x2
    b.ne failH
    adr  x11, trap_flags
    ldr  x12, [x11]
    tbz  x12, #10, failI

    adr  x3, el0_amuserenr_mrs
    msr  elr_el1, x3
    msr  spsr_el1, x4
    eret

after_el0_amuserenr_mrs:
    movz x2, #0xfedc
    movk x2, #0xba98, lsl #16
    movk x2, #0x7654, lsl #32
    movk x2, #0x3210, lsl #48
    cmp  x0, x2
    b.ne failJ
    adr  x11, trap_flags
    ldr  x12, [x11]
    tbz  x12, #11, failK

    movz x0, #'U'
    str  x0, [x1]
    movz x0, #'\n'
    str  x0, [x1]
    brk  #0

fail1:
    movz x0, #'1'
    b out
fail2:
    movz x0, #'2'
    b out
fail3:
    movz x0, #'3'
    b out
fail4:
    movz x0, #'4'
    b out
fail5:
    movz x0, #'5'
    b out
fail6:
    movz x0, #'6'
    b out
fail7:
    movz x0, #'7'
    b out
fail8:
    movz x0, #'8'
    b out
fail9:
    movz x0, #'9'
    b out
failA:
    movz x0, #'A'
    b out
failB:
    movz x0, #'B'
    b out
failC:
    movz x0, #'C'
    b out
failD:
    movz x0, #'D'
    b out
failE:
    movz x0, #'E'
    b out
failF:
    movz x0, #'F'
    b out
failG:
    movz x0, #'G'
    b out
failH:
    movz x0, #'H'
    b out
failI:
    movz x0, #'I'
    b out
failJ:
    movz x0, #'J'
    b out
failK:
    movz x0, #'K'
    b out

out:
    str  x0, [x1]
    brk  #0

el0_tpidr2_mrs:
    movz x0, #0x1020
    movk x0, #0x3040, lsl #16
    movk x0, #0x5060, lsl #32
    movk x0, #0x7080, lsl #48
el0_tpidr2_mrs_insn:
    .inst 0xD53BD0A0                  // mrs x0, tpidr2_el0
    svc  #0x70

el0_gcspr_mrs:
    movz x0, #0x1122
    movk x0, #0x3344, lsl #16
    movk x0, #0x5566, lsl #32
    movk x0, #0x7788, lsl #48
el0_gcspr_mrs_insn:
    .inst 0xD53B2520                  // mrs x0, gcspr_el0
    svc  #0x71

el0_pmuserenr_mrs:
    movz x0, #0x89ab
    movk x0, #0xcdef, lsl #16
    movk x0, #0x0123, lsl #32
    movk x0, #0x4567, lsl #48
el0_pmuserenr_mrs_insn:
    .inst 0xD53B9E00                  // mrs x0, pmuserenr_el0
    svc  #0x72

el0_amuserenr_mrs:
    movz x0, #0xfedc
    movk x0, #0xba98, lsl #16
    movk x0, #0x7654, lsl #32
    movk x0, #0x3210, lsl #48
el0_amuserenr_mrs_insn:
    .inst 0xD53BD260                  // mrs x0, amuserenr_el0
    svc  #0x73

    .balign 2048
vectors:
    .space 0x200
sync_spx:
    b sync_handler
    .space 0x400 - 0x200 - 4
sync_lower:
    b lower_sync_handler

sync_handler:
    mrs  x20, esr_el1
    tbz  x20, #25, handler_fail
    ubfx x21, x20, #26, #6
    cbnz x21, handler_fail
    and  x21, x20, #0x1ffffff
    cbnz x21, handler_fail
    mrs  x21, far_el1
    cbnz x21, handler_fail

    mrs  x22, elr_el1
    adr  x23, el1_tpidr2_mrs_insn
    cmp  x22, x23
    b.eq trap_el1_tpidr2_mrs
    adr  x23, el1_tpidr2_msr_insn
    cmp  x22, x23
    b.eq trap_el1_tpidr2_msr
    adr  x23, el1_gcspr_mrs_insn
    cmp  x22, x23
    b.eq trap_el1_gcspr_mrs
    adr  x23, el1_gcspr_msr_insn
    cmp  x22, x23
    b.eq trap_el1_gcspr_msr
    adr  x23, el1_pmuserenr_mrs_insn
    cmp  x22, x23
    b.eq trap_el1_pmuserenr_mrs
    adr  x23, el1_pmuserenr_msr_insn
    cmp  x22, x23
    b.eq trap_el1_pmuserenr_msr
    adr  x23, el1_amuserenr_mrs_insn
    cmp  x22, x23
    b.eq trap_el1_amuserenr_mrs
    adr  x23, el1_amuserenr_msr_insn
    cmp  x22, x23
    b.eq trap_el1_amuserenr_msr
    b handler_fail

lower_sync_handler:
    mrs  x20, esr_el1
    tbz  x20, #25, handler_fail
    ubfx x21, x20, #26, #6
    cbnz x21, handler_fail
    and  x21, x20, #0x1ffffff
    cbnz x21, handler_fail
    mrs  x21, far_el1
    cbnz x21, handler_fail

    mrs  x21, spsr_el1
    tbnz x21, #20, handler_fail
    and  x22, x21, #0xf
    cbnz x22, handler_fail
    ubfx x22, x21, #28, #4
    cmp  x22, #0x9
    b.ne handler_fail
    ubfx x22, x21, #6, #4
    cmp  x22, #0xa
    b.ne handler_fail
    tbz  x21, #22, handler_fail

    mrs  x22, elr_el1
    adr  x23, el0_tpidr2_mrs_insn
    cmp  x22, x23
    b.eq trap_el0_tpidr2_mrs
    adr  x23, el0_gcspr_mrs_insn
    cmp  x22, x23
    b.eq trap_el0_gcspr_mrs
    adr  x23, el0_pmuserenr_mrs_insn
    cmp  x22, x23
    b.eq trap_el0_pmuserenr_mrs
    adr  x23, el0_amuserenr_mrs_insn
    cmp  x22, x23
    b.eq trap_el0_amuserenr_mrs
    b handler_fail

trap_el1_tpidr2_mrs:
    adr  x24, trap_flags
    ldr  x25, [x24]
    orr  x25, x25, #1
    str  x25, [x24]
    adr  x26, after_el1_tpidr2_mrs
    b return_to_el1

trap_el1_tpidr2_msr:
    adr  x24, trap_flags
    ldr  x25, [x24]
    orr  x25, x25, #2
    str  x25, [x24]
    adr  x26, after_el1_tpidr2_msr
    b return_to_el1

trap_el1_gcspr_mrs:
    adr  x24, trap_flags
    ldr  x25, [x24]
    orr  x25, x25, #4
    str  x25, [x24]
    adr  x26, after_el1_gcspr_mrs
    b return_to_el1

trap_el1_gcspr_msr:
    adr  x24, trap_flags
    ldr  x25, [x24]
    orr  x25, x25, #8
    str  x25, [x24]
    adr  x26, after_el1_gcspr_msr
    b return_to_el1

trap_el1_pmuserenr_mrs:
    adr  x24, trap_flags
    ldr  x25, [x24]
    orr  x25, x25, #16
    str  x25, [x24]
    adr  x26, after_el1_pmuserenr_mrs
    b return_to_el1

trap_el1_pmuserenr_msr:
    adr  x24, trap_flags
    ldr  x25, [x24]
    orr  x25, x25, #32
    str  x25, [x24]
    adr  x26, after_el1_pmuserenr_msr
    b return_to_el1

trap_el1_amuserenr_mrs:
    adr  x24, trap_flags
    ldr  x25, [x24]
    orr  x25, x25, #64
    str  x25, [x24]
    adr  x26, after_el1_amuserenr_mrs
    b return_to_el1

trap_el1_amuserenr_msr:
    adr  x24, trap_flags
    ldr  x25, [x24]
    orr  x25, x25, #128
    str  x25, [x24]
    adr  x26, after_el1_amuserenr_msr
    b return_to_el1

trap_el0_tpidr2_mrs:
    adr  x24, trap_flags
    ldr  x25, [x24]
    movz x28, #1
    lsl  x28, x28, #8
    orr  x25, x25, x28
    str  x25, [x24]
    adr  x26, after_el0_tpidr2_mrs
    b return_to_el1

trap_el0_gcspr_mrs:
    adr  x24, trap_flags
    ldr  x25, [x24]
    movz x28, #1
    lsl  x28, x28, #9
    orr  x25, x25, x28
    str  x25, [x24]
    adr  x26, after_el0_gcspr_mrs
    b return_to_el1

trap_el0_pmuserenr_mrs:
    adr  x24, trap_flags
    ldr  x25, [x24]
    movz x28, #1
    lsl  x28, x28, #10
    orr  x25, x25, x28
    str  x25, [x24]
    adr  x26, after_el0_pmuserenr_mrs
    b return_to_el1

trap_el0_amuserenr_mrs:
    adr  x24, trap_flags
    ldr  x25, [x24]
    movz x28, #1
    lsl  x28, x28, #11
    orr  x25, x25, x28
    str  x25, [x24]
    adr  x26, after_el0_amuserenr_mrs
    b return_to_el1

return_to_el1:
    movz x27, #0x5                // EL1h
    msr  spsr_el1, x27
    msr  elr_el1, x26
    eret

handler_fail:
    movz x0, #'X'
    adr  x26, out
    movz x27, #0x5
    msr  spsr_el1, x27
    msr  elr_el1, x26
    eret

    .data
    .balign 8
trap_flags:
    .quad 0
