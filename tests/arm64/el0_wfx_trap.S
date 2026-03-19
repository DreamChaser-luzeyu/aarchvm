    .text
    .global _start
_start:
    movz x1, #0x0000
    movk x1, #0x0900, lsl #16      // UART

    adr  x10, vectors
    msr  vbar_el1, x10

    adr  x11, trap_flags
    str  xzr, [x11]
    adr  x11, stage_done
    str  xzr, [x11]
    adr  x11, irq_count
    str  xzr, [x11]

    // Enable the virtual timer PPI and CPU interface so the allowed-WFI path
    // can be validated with a real EL0 IRQ delivery rather than depending on
    // reset-state MMIO timer wiring.
    movz x15, #0x0000
    movk x15, #0x080b, lsl #16     // GICR SGI frame base
    movz x16, #0
    movk x16, #0x0800, lsl #16     // bit 27: virtual timer PPI
    str  w16, [x15, #0x100]

    movz x16, #0x00ff
    msr  icc_pmr_el1, x16
    movz x16, #1
    msr  icc_sre_el1, x16
    msr  icc_igrpen1_el1, x16

    mrs  x3, sctlr_el1
    movz x4, #1
    lsl  x5, x4, #16               // nTWI
    tst  x3, x5
    b.ne fail0
    lsl  x5, x4, #18               // nTWE
    tst  x3, x5
    b.ne fail0

    adr  x3, el0_wfe_denied
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret
after_wfe_denied:
    adr  x11, trap_flags
    ldr  x12, [x11]
    tbz  x12, #0, fail1

    adr  x3, el0_wfi_denied
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret
after_wfi_denied:
    adr  x11, trap_flags
    ldr  x12, [x11]
    tbz  x12, #1, fail2

    mrs  x3, sctlr_el1
    movz x4, #1
    lsl  x5, x4, #16               // nTWI
    orr  x3, x3, x5
    lsl  x5, x4, #18               // nTWE
    orr  x3, x3, x5
    msr  sctlr_el1, x3
    isb
    mrs  x6, sctlr_el1
    movz x4, #1
    lsl  x5, x4, #16
    tst  x6, x5
    b.eq fail3
    lsl  x5, x4, #18
    tst  x6, x5
    b.eq fail3

    adr  x3, el0_wfe_allowed
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret
after_wfe_allowed:
    adr  x11, stage_done
    ldr  x12, [x11]
    movz x13, #1
    cmp  x12, x13
    b.ne fail4

    mrs  x14, cntvct_el0
    add  x14, x14, #30
    msr  cntv_cval_el0, x14
    movz x15, #1
    msr  cntv_ctl_el0, x15

    adr  x3, el0_wfi_allowed
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret
after_wfi_allowed:
    adr  x11, stage_done
    ldr  x12, [x11]
    movz x13, #2
    cmp  x12, x13
    b.ne fail5
    adr  x11, irq_count
    ldr  x12, [x11]
    cmp  x12, #1
    b.ne fail6

    movz x0, #'T'
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

el0_wfe_denied:
el0_wfe_denied_insn:
    wfe
    svc  #0x11

el0_wfi_denied:
el0_wfi_denied_insn:
    wfi
    svc  #0x12

el0_wfe_allowed:
    sevl
    wfe
    svc  #0x20

el0_wfi_allowed:
el0_wfi_allowed_insn:
    wfi
    svc  #0x21

    .balign 2048
vectors:
    .space 0x400
sync_lower:
    b lower_sync_handler
    .space (0x480 - 0x400 - 4)
irq_vector:
    b irq_handler

irq_handler:
    mrs  x28, icc_iar1_el1
    movz x29, #27
    cmp  x28, x29
    b.ne handler_fail
    msr  cntv_ctl_el0, xzr
    msr  icc_eoir1_el1, x28
    msr  icc_dir_el1, x28

    adr  x24, irq_count
    ldr  x25, [x24]
    add  x25, x25, #1
    str  x25, [x24]
    eret

lower_sync_handler:
    mrs  x20, esr_el1
    ubfx x21, x20, #26, #6
    movz x22, #0x01
    cmp  x21, x22
    b.eq handle_wfx_trap
    movz x22, #0x15
    cmp  x21, x22
    b.eq handle_svc
    b handler_fail

handle_wfx_trap:
    mrs  x23, elr_el1
    adr  x24, el0_wfe_denied_insn
    cmp  x23, x24
    b.eq trap_wfe
    adr  x24, el0_wfi_denied_insn
    cmp  x23, x24
    b.eq trap_wfi
    b handler_fail

trap_wfe:
    and  x24, x20, #0x3
    cmp  x24, #1
    b.ne handler_fail
    adr  x24, trap_flags
    ldr  x25, [x24]
    orr  x25, x25, #1
    str  x25, [x24]
    adr  x26, after_wfe_denied
    b return_to_el1

trap_wfi:
    and  x24, x20, #0x3
    cbnz x24, handler_fail
    adr  x24, trap_flags
    ldr  x25, [x24]
    orr  x25, x25, #2
    str  x25, [x24]
    adr  x26, after_wfi_denied
    b return_to_el1

handle_svc:
    and  x22, x20, #0xffff
    movz x23, #0x20
    cmp  x22, x23
    b.eq svc_wfe_ok
    movz x23, #0x21
    cmp  x22, x23
    b.eq svc_wfi_ok
    b handler_fail

svc_wfe_ok:
    adr  x24, stage_done
    movz x25, #1
    str  x25, [x24]
    adr  x26, after_wfe_allowed
    b return_to_el1

svc_wfi_ok:
    adr  x24, stage_done
    movz x25, #2
    str  x25, [x24]
    adr  x26, after_wfi_allowed

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
stage_done:
    .quad 0
irq_count:
    .quad 0
