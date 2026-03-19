    .text
    .global _start
_start:
    movz x1, #0x0000
    movk x1, #0x0900, lsl #16      // UART

    adr  x10, vectors
    msr  vbar_el1, x10

    adr  x11, trap_flags
    stp  xzr, xzr, [x11]
    str  xzr, [x11, #16]

    mrs  x2, sctlr_el1
    movz x3, #1
    lsl  x4, x3, #9                // UMA
    tst  x2, x4
    b.ne fail0

    adr  x3, el0_nzcv_allowed
    msr  elr_el1, x3
    msr  spsr_el1, xzr             // EL0t
    eret
after_nzcv_allowed:
    adr  x11, stage_done
    ldr  x12, [x11]
    movz x13, #1
    cmp  x12, x13
    b.ne fail1

    adr  x3, el0_mrs_daif
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret
after_mrs_daif_trap:
    adr  x11, trap_flags
    ldr  x12, [x11]
    tbz  x12, #0, fail2

    adr  x3, el0_msr_daif
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret
after_msr_daif_trap:
    adr  x11, trap_flags
    ldr  x12, [x11]
    tbz  x12, #1, fail3

    adr  x3, el0_msr_daifset
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret
after_msr_daifset_trap:
    adr  x11, trap_flags
    ldr  x12, [x11]
    tbz  x12, #2, fail4

    adr  x3, el0_msr_daifclr
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret
after_msr_daifclr_trap:
    adr  x11, trap_flags
    ldr  x12, [x11]
    tbz  x12, #3, fail5

    mrs  x2, sctlr_el1
    movz x3, #1
    lsl  x4, x3, #9                // UMA
    orr  x2, x2, x4
    msr  sctlr_el1, x2
    isb
    mrs  x5, sctlr_el1
    tst  x5, x4
    b.eq fail6

    adr  x3, el0_daif_allowed
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret
after_daif_allowed:
    adr  x11, stage_done
    ldr  x12, [x11]
    movz x13, #2
    cmp  x12, x13
    b.ne fail7

    movz x0, #'U'
    str  x0, [x1]
    movz x0, #'\n'
    str  x0, [x1]
    brk  #0

fail0:
    movz x0, #'0'
    b out
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
failh:
    movz x0, #'H'
    b out

out:
    movz x1, #0x0000
    movk x1, #0x0900, lsl #16
    str  x0, [x1]
    brk  #0

el0_nzcv_allowed:
    movz x0, #0xb000, lsl #16
    msr  nzcv, x0
    mrs  x2, nzcv
    subs xzr, x2, x0
    b.ne el0_fail
    svc  #0x10

el0_mrs_daif:
el0_mrs_daif_insn:
    mrs  x0, daif
    svc  #0x11

el0_msr_daif:
    movz x0, #0x03c0
el0_msr_daif_insn:
    msr  daif, x0
    svc  #0x12

el0_msr_daifset:
el0_msr_daifset_insn:
    msr  daifset, #0xf
    svc  #0x13

el0_msr_daifclr:
el0_msr_daifclr_insn:
    msr  daifclr, #0xf
    svc  #0x14

el0_daif_allowed:
    movz x0, #0
    msr  daif, x0
    mrs  x2, daif
    cbnz x2, el0_fail

    msr  daifset, #0xa
    mrs  x2, daif
    movz x3, #0x0280
    subs xzr, x2, x3
    b.ne el0_fail

    msr  daifclr, #0x8
    mrs  x2, daif
    movz x3, #0x0080
    subs xzr, x2, x3
    b.ne el0_fail

    movz x0, #0x03c0
    msr  daif, x0
    mrs  x2, daif
    subs xzr, x2, x0
    b.ne el0_fail
    svc  #0x20

el0_fail:
    svc  #0x2f

    .balign 2048
vectors:
    .space 0x400
sync_lower:
    b lower_sync_handler

lower_sync_handler:
    mrs  x20, esr_el1
    ubfx x21, x20, #26, #6
    movz x22, #0x18
    cmp  x21, x22
    b.eq handle_sys_trap
    movz x22, #0x15
    cmp  x21, x22
    b.eq handle_svc
    b handler_fail

handle_sys_trap:
    mrs  x23, elr_el1
    adr  x24, el0_mrs_daif_insn
    cmp  x23, x24
    b.eq trap_mrs_daif
    adr  x24, el0_msr_daif_insn
    cmp  x23, x24
    b.eq trap_msr_daif
    adr  x24, el0_msr_daifset_insn
    cmp  x23, x24
    b.eq trap_msr_daifset
    adr  x24, el0_msr_daifclr_insn
    cmp  x23, x24
    b.eq trap_msr_daifclr
    b handler_fail

trap_mrs_daif:
    adr  x24, trap_flags
    ldr  x25, [x24]
    orr  x25, x25, #1
    str  x25, [x24]
    adr  x26, after_mrs_daif_trap
    b return_to_el1

trap_msr_daif:
    adr  x24, trap_flags
    ldr  x25, [x24]
    orr  x25, x25, #2
    str  x25, [x24]
    adr  x26, after_msr_daif_trap
    b return_to_el1

trap_msr_daifset:
    adr  x24, trap_flags
    ldr  x25, [x24]
    orr  x25, x25, #4
    str  x25, [x24]
    adr  x26, after_msr_daifset_trap
    b return_to_el1

trap_msr_daifclr:
    adr  x24, trap_flags
    ldr  x25, [x24]
    orr  x25, x25, #8
    str  x25, [x24]
    adr  x26, after_msr_daifclr_trap
    b return_to_el1

handle_svc:
    and  x23, x20, #0xffff
    movz x24, #0x10
    cmp  x23, x24
    b.eq svc_nzcv_ok
    movz x24, #0x20
    cmp  x23, x24
    b.eq svc_daif_ok
    b handler_fail

svc_nzcv_ok:
    adr  x24, stage_done
    movz x25, #1
    str  x25, [x24]
    adr  x26, after_nzcv_allowed
    b return_to_el1

svc_daif_ok:
    adr  x24, stage_done
    movz x25, #2
    str  x25, [x24]
    adr  x26, after_daif_allowed
    b return_to_el1

handler_fail:
    adr  x26, failh

return_to_el1:
    msr  elr_el1, x26
    movz x27, #0x5               // EL1h
    msr  spsr_el1, x27
    eret

    .data
    .balign 8
trap_flags:
    .quad 0
    .quad 0
stage_done:
    .quad 0
