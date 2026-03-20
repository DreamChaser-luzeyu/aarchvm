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

    mrs  x2, cntkctl_el1
    cbnz x2, fail0

    adr  x3, el0_cntvct_denied
    msr  elr_el1, x3
    msr  spsr_el1, xzr             // EL0t
    eret
after_cntvct_denied:
    adr  x11, trap_flags
    ldr  x12, [x11]
    tbz  x12, #0, fail1

    adr  x3, el0_cntpct_denied
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret
after_cntpct_denied:
    adr  x11, trap_flags
    ldr  x12, [x11]
    tbz  x12, #1, fail2

    adr  x3, el0_cntv_tval_denied
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret
after_cntv_tval_denied:
    adr  x11, trap_flags
    ldr  x12, [x11]
    tbz  x12, #2, fail3

    adr  x3, el0_cntp_ctl_denied
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret
after_cntp_ctl_denied:
    adr  x11, trap_flags
    ldr  x12, [x11]
    tbz  x12, #3, fail4

    movz x4, #0x0303               // EL0PCTEN | EL0VCTEN | EL0VTEN | EL0PTEN
    msr  cntkctl_el1, x4
    mrs  x5, cntkctl_el1
    cmp  x5, x4
    b.ne fail5

    adr  x3, el0_allowed
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret
after_allowed:
    adr  x11, allow_done
    ldr  x12, [x11]
    cbz  x12, fail6

    movz x1, #0x0000
    movk x1, #0x0900, lsl #16
    movz x0, #'K'
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

out:
    movz x1, #0x0000
    movk x1, #0x0900, lsl #16
    str  x0, [x1]
    brk  #0

el0_cntvct_denied:
el0_cntvct_insn:
    mrs  x0, cntvct_el0
    svc  #0x11

el0_cntpct_denied:
el0_cntpct_insn:
    mrs  x0, cntpct_el0
    svc  #0x12

el0_cntv_tval_denied:
el0_cntv_tval_insn:
    mrs  x0, cntv_tval_el0
    svc  #0x13

el0_cntp_ctl_denied:
    movz x0, #1
el0_cntp_ctl_insn:
    msr  cntp_ctl_el0, x0
    svc  #0x14

el0_allowed:
    mrs  x0, cntvct_el0
    mrs  x1, cntpct_el0
    subs xzr, x1, x0
    b.lo el0_fail

    movz x6, #64
    msr  cntv_tval_el0, x6
    movz x2, #3
    msr  cntv_ctl_el0, x2
    mrs  x3, cntv_ctl_el0
    and  x3, x3, #3
    cmp  x3, x2
    b.ne el0_fail

    movz x8, #64
    msr  cntp_tval_el0, x8
    movz x4, #3
    msr  cntp_ctl_el0, x4
    mrs  x5, cntp_ctl_el0
    and  x5, x5, #3
    cmp  x5, x4
    b.ne el0_fail

    mrs  x7, cntv_tval_el0
    cbz  x7, el0_fail
    cmp  x7, x6
    b.hi el0_fail

    mrs  x9, cntp_tval_el0
    cbz  x9, el0_fail
    cmp  x9, x8
    b.hi el0_fail

    movn x10, #0
    msr  cntv_tval_el0, x10
    mrs  x11, cntv_tval_el0
    lsr  x12, x11, #32
    cbnz x12, el0_fail
    tbz  x11, #31, el0_fail

    movn x13, #0
    msr  cntp_tval_el0, x13
    mrs  x14, cntp_tval_el0
    lsr  x15, x14, #32
    cbnz x15, el0_fail
    tbz  x14, #31, el0_fail

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
    lsr  x21, x20, #26
    movz x22, #0x18
    cmp  x21, x22
    b.eq handle_sys_trap
    movz x22, #0x15
    cmp  x21, x22
    b.eq handle_svc
    b handler_fail

handle_sys_trap:
    mrs  x23, elr_el1
    adr  x24, el0_cntvct_insn
    cmp  x23, x24
    b.eq trap_cntvct
    adr  x24, el0_cntpct_insn
    cmp  x23, x24
    b.eq trap_cntpct
    adr  x24, el0_cntv_tval_insn
    cmp  x23, x24
    b.eq trap_cntv_tval
    adr  x24, el0_cntp_ctl_insn
    cmp  x23, x24
    b.eq trap_cntp_ctl
    b handler_fail

trap_cntvct:
    movz x24, #0xffff
    movk x24, #0x01ff, lsl #16
    and  x25, x20, x24
    movz x24, #0xf801
    movk x24, #0x0034, lsl #16
    cmp  x25, x24
    b.ne fail1
    adr  x24, trap_flags
    ldr  x25, [x24]
    orr  x25, x25, #1
    str  x25, [x24]
    adr  x26, after_cntvct_denied
    b return_to_el1

trap_cntpct:
    movz x24, #0xffff
    movk x24, #0x01ff, lsl #16
    and  x25, x20, x24
    movz x24, #0xf801
    movk x24, #0x0032, lsl #16
    cmp  x25, x24
    b.ne fail2
    adr  x24, trap_flags
    ldr  x25, [x24]
    orr  x25, x25, #2
    str  x25, [x24]
    adr  x26, after_cntpct_denied
    b return_to_el1

trap_cntv_tval:
    movz x24, #0xffff
    movk x24, #0x01ff, lsl #16
    and  x25, x20, x24
    movz x24, #0xf807
    movk x24, #0x0030, lsl #16
    cmp  x25, x24
    b.ne fail3
    adr  x24, trap_flags
    ldr  x25, [x24]
    orr  x25, x25, #4
    str  x25, [x24]
    adr  x26, after_cntv_tval_denied
    b return_to_el1

trap_cntp_ctl:
    movz x24, #0xffff
    movk x24, #0x01ff, lsl #16
    and  x25, x20, x24
    movz x24, #0xf804
    movk x24, #0x0032, lsl #16
    cmp  x25, x24
    b.ne fail4
    adr  x24, trap_flags
    ldr  x25, [x24]
    orr  x25, x25, #8
    str  x25, [x24]
    adr  x26, after_cntp_ctl_denied
    b return_to_el1

handle_svc:
    and  x23, x20, #0xffff
    movz x24, #0x20
    cmp  x23, x24
    b.eq svc_allowed_done
    b handler_fail

svc_allowed_done:
    adr  x24, allow_done
    movz x25, #1
    str  x25, [x24]
    adr  x26, after_allowed

return_to_el1:
    msr  elr_el1, x26
    movz x27, #0x5                 // EL1h
    msr  spsr_el1, x27
    eret

handler_fail:
    adr  x26, out
    msr  elr_el1, x26
    movz x27, #0x5
    msr  spsr_el1, x27
    movz x0, #'F'
    eret

    .data
    .balign 8
trap_flags:
    .quad 0
allow_done:
    .quad 0
