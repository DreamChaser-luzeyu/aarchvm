    .text
    .global _start
_start:
    movz x1, #0x0000
    movk x1, #0x0900, lsl #16      // UART

    adr  x10, vectors
    msr  vbar_el1, x10

    adr  x11, trap_flags
    str  xzr, [x11]
    adr  x11, allow_done
    str  xzr, [x11]

    adr  x3, el0_allowed
    msr  elr_el1, x3
    msr  spsr_el1, xzr             // EL0t
    eret

after_allowed:
    adr  x11, allow_done
    ldr  x12, [x11]
    cbz  x12, fail0

    adr  x3, el0_mrs_far
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret

after_mrs_far:
    adr  x11, trap_flags
    ldr  x12, [x11]
    tbz  x12, #0, fail1

    adr  x3, el0_msr_tcr
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret

after_msr_tcr:
    adr  x11, trap_flags
    ldr  x12, [x11]
    tbz  x12, #1, fail2

    adr  x3, el0_msr_daifset
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret

after_msr_daifset:
    adr  x11, trap_flags
    ldr  x12, [x11]
    tbz  x12, #2, fail3

    movz x0, #'R'
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

out:
    str  x0, [x1]
    brk  #0

el0_allowed:
    movz x0, #0xbeef
    movk x0, #0xcafe, lsl #16
    movk x0, #0x5678, lsl #32
    movk x0, #0x1234, lsl #48
    msr  tpidr_el0, x0
    mrs  x2, tpidr_el0
    subs xzr, x2, x0
    b.ne el0_fail

    mrs  x3, ctr_el0
    cbz  x3, el0_fail

    mrs  x4, dczid_el0
    and  x5, x4, #0x1f
    movz x6, #4
    subs xzr, x5, x6
    b.ne el0_fail

    svc  #0x20

el0_fail:
    svc  #0x2f

el0_mrs_far:
el0_mrs_far_insn:
    mrs  x0, far_el1
    svc  #0x11

el0_msr_tcr:
    movz x0, #1
el0_msr_tcr_insn:
    msr  tcr_el1, x0
    svc  #0x12

el0_msr_daifset:
el0_msr_daifset_insn:
    msr  daifset, #0xf
    svc  #0x13

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
    adr  x24, el0_mrs_far_insn
    cmp  x23, x24
    b.eq trap_mrs_far
    adr  x24, el0_msr_tcr_insn
    cmp  x23, x24
    b.eq trap_msr_tcr
    adr  x24, el0_msr_daifset_insn
    cmp  x23, x24
    b.eq trap_msr_daifset
    b handler_fail

trap_mrs_far:
    adr  x24, trap_flags
    ldr  x25, [x24]
    orr  x25, x25, #1
    str  x25, [x24]
    adr  x26, after_mrs_far
    b return_to_el1

trap_msr_tcr:
    adr  x24, trap_flags
    ldr  x25, [x24]
    orr  x25, x25, #2
    str  x25, [x24]
    adr  x26, after_msr_tcr
    b return_to_el1

trap_msr_daifset:
    adr  x24, trap_flags
    ldr  x25, [x24]
    orr  x25, x25, #4
    str  x25, [x24]
    adr  x26, after_msr_daifset
    b return_to_el1

handle_svc:
    and  x22, x20, #0xffff
    movz x23, #0x20
    cmp  x22, x23
    b.eq svc_ok
    b handler_fail

svc_ok:
    adr  x24, allow_done
    movz x25, #1
    str  x25, [x24]
    adr  x26, after_allowed

return_to_el1:
    movz x27, #0x5                // EL1h
    msr  spsr_el1, x27
    msr  elr_el1, x26
    eret

handler_fail:
    movz x0, #'F'
    adr  x26, out
    movz x27, #0x5
    msr  spsr_el1, x27
    msr  elr_el1, x26
    eret

    .data
    .balign 8
trap_flags:
    .quad 0
allow_done:
    .quad 0
