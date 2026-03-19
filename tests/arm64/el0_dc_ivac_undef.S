    .text
    .global _start
_start:
    movz x1, #0x0000
    movk x1, #0x0900, lsl #16

    adr  x10, vectors
    msr  vbar_el1, x10

    mrs  x2, sctlr_el1
    movz x3, #1
    lsl  x3, x3, #26
    orr  x2, x2, x3
    msr  sctlr_el1, x2
    isb

    mrs  x4, sctlr_el1
    tst  x4, x3
    b.eq fail0

    adr  x5, el0_dc_ivac
    msr  elr_el1, x5
    msr  spsr_el1, xzr
    eret

after_undef:
    movz x0, #'V'
    str  x0, [x1]
    movz x0, #'\n'
    str  x0, [x1]
    brk  #0

fail0:
    movz x0, #'0'
    b out

out:
    str  x0, [x1]
    brk  #0

el0_dc_ivac:
    adr  x0, data_buf
el0_dc_ivac_insn:
    dc   ivac, x0
    svc  #0x11

    .balign 2048
vectors:
    .space 0x400
sync_lower:
    b lower_sync_handler

lower_sync_handler:
    mrs  x20, esr_el1
    ubfx x21, x20, #26, #6
    cbnz x21, handler_fail

    mrs  x22, elr_el1
    adr  x23, el0_dc_ivac_insn
    cmp  x22, x23
    b.ne handler_fail

    adr  x24, after_undef
    movz x25, #0x5
    msr  spsr_el1, x25
    msr  elr_el1, x24
    eret

handler_fail:
    movz x0, #'F'
    str  x0, [x1]
    brk  #0

    .data
    .balign 64
data_buf:
    .quad 0x0123456789abcdef
