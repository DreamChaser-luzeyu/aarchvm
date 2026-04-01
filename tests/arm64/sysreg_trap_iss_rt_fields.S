    .text
    .global _start
_start:
    movz x1, #0x0000
    movk x1, #0x0900, lsl #16      // UART

    adr  x10, vectors
    msr  vbar_el1, x10

    adr  x11, flags
    stp  xzr, xzr, [x11]

    mrs  x2, sctlr_el1
    movz x3, #1
    lsl  x4, x3, #15               // UCT
    tst  x2, x4
    b.ne fail0
    lsl  x4, x3, #26               // UCI
    tst  x2, x4
    b.ne fail0
    lsl  x4, x3, #14               // DZE
    tst  x2, x4
    b.ne fail0

    adr  x3, el0_mrs_ctr
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret

after_mrs_ctr:
    adr  x11, flags
    ldr  x12, [x11]
    tbz  x12, #0, fail1

    adr  x3, el0_msr_tcr
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret

after_msr_tcr:
    adr  x11, flags
    ldr  x12, [x11]
    tbz  x12, #1, fail2

    adr  x3, el0_ic_ivau
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret

after_ic_ivau:
    adr  x11, flags
    ldr  x12, [x11]
    tbz  x12, #2, fail3

    adr  x3, el0_dc_zva
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret

after_dc_zva:
    adr  x11, flags
    ldr  x12, [x11]
    tbz  x12, #3, fail4

    movz x0, #'I'
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
failh:
    movz x0, #'H'
    b out
out:
    str  x0, [x1]
    brk  #0

el0_mrs_ctr:
el0_mrs_ctr_insn:
    mrs  x5, ctr_el0
    svc  #0x11

el0_msr_tcr:
    movz x9, #1
el0_msr_tcr_insn:
    msr  tcr_el1, x9
    svc  #0x12

el0_ic_ivau:
    adr  x17, scratch
el0_ic_ivau_insn:
    ic   ivau, x17
    svc  #0x13

el0_dc_zva:
    adr  x13, scratch
    add  x13, x13, #32
el0_dc_zva_insn:
    dc   zva, x13
    svc  #0x14

    .balign 2048
vectors:
    .space 0x400
sync_lower:
    b lower_sync_handler

lower_sync_handler:
    mrs  x20, esr_el1
    tbz  x20, #25, failh_from_handler
    ubfx x21, x20, #26, #6
    movz x22, #0x18
    cmp  x21, x22
    b.ne failh_from_handler

    mrs  x23, far_el1
    cbnz x23, failh_from_handler

    mrs  x24, elr_el1
    adr  x25, el0_mrs_ctr_insn
    cmp  x24, x25
    b.eq trap_mrs_ctr
    adr  x25, el0_msr_tcr_insn
    cmp  x24, x25
    b.eq trap_msr_tcr
    adr  x25, el0_ic_ivau_insn
    cmp  x24, x25
    b.eq trap_ic_ivau
    adr  x25, el0_dc_zva_insn
    cmp  x24, x25
    b.eq trap_dc_zva
    b failh_from_handler

trap_mrs_ctr:
    movz x24, #0xffff
    movk x24, #0x01ff, lsl #16
    and  x25, x20, x24
    movz x24, #0xc0a1
    movk x24, #0x0032, lsl #16
    cmp  x25, x24
    b.ne fail1
    adr  x24, flags
    ldr  x25, [x24]
    orr  x25, x25, #1
    str  x25, [x24]
    adr  x26, after_mrs_ctr
    b return_to_el1

trap_msr_tcr:
    movz x24, #0xffff
    movk x24, #0x01ff, lsl #16
    and  x25, x20, x24
    movz x24, #0x0920
    movk x24, #0x0034, lsl #16
    cmp  x25, x24
    b.ne fail2
    adr  x24, flags
    ldr  x25, [x24]
    orr  x25, x25, #2
    str  x25, [x24]
    adr  x26, after_msr_tcr
    b return_to_el1

trap_ic_ivau:
    movz x24, #0xffff
    movk x24, #0x01ff, lsl #16
    and  x25, x20, x24
    movz x24, #0xde2a
    movk x24, #0x0012, lsl #16
    cmp  x25, x24
    b.ne fail3
    adr  x24, flags
    ldr  x25, [x24]
    orr  x25, x25, #4
    str  x25, [x24]
    adr  x26, after_ic_ivau
    b return_to_el1

trap_dc_zva:
    movz x24, #0xffff
    movk x24, #0x01ff, lsl #16
    and  x25, x20, x24
    movz x24, #0xdda8
    movk x24, #0x0012, lsl #16
    cmp  x25, x24
    b.ne fail4
    adr  x24, flags
    ldr  x25, [x24]
    orr  x25, x25, #8
    str  x25, [x24]
    adr  x26, after_dc_zva
    b return_to_el1

return_to_el1:
    movz x27, #0x5
    msr  spsr_el1, x27
    msr  elr_el1, x26
    eret

failh_from_handler:
    adr  x26, failh
    movz x27, #0x5
    msr  spsr_el1, x27
    msr  elr_el1, x26
    eret

    .data
    .balign 16
flags:
    .quad 0
    .quad 0
    .balign 64
scratch:
    .space 64
