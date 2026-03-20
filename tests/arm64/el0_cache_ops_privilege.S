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

    mrs  x2, sctlr_el1
    movz x3, #1
    lsl  x4, x3, #26               // UCI
    tst  x2, x4
    b.ne fail0
    lsl  x4, x3, #14               // DZE
    tst  x2, x4
    b.ne fail0

    adr  x3, el0_ic_ivau_denied
    msr  elr_el1, x3
    msr  spsr_el1, xzr             // EL0t
    eret
after_ic_ivau_denied:
    adr  x11, trap_flags
    ldr  x12, [x11]
    tbz  x12, #0, fail1

    adr  x3, el0_at_denied
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret
after_at_denied:
    adr  x11, trap_flags
    ldr  x12, [x11]
    tbz  x12, #1, fail2

    adr  x3, el0_dc_zva_denied
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret
after_dc_zva_denied:
    adr  x11, trap_flags
    ldr  x12, [x11]
    tbz  x12, #2, fail3

    adr  x3, el0_tlbi_denied
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret
after_tlbi_denied:
    adr  x11, trap_flags
    ldr  x12, [x11]
    tbz  x12, #3, fail4

    mrs  x2, sctlr_el1
    movz x3, #1
    lsl  x4, x3, #26               // UCI
    orr  x2, x2, x4
    lsl  x4, x3, #14               // DZE
    orr  x2, x2, x4
    msr  sctlr_el1, x2
    isb
    mrs  x5, sctlr_el1
    movz x6, #1
    lsl  x7, x6, #26
    tst  x5, x7
    b.eq fail6
    lsl  x7, x6, #14
    tst  x5, x7
    b.eq fail6

    adr  x3, el0_at_enabled_undef
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret
after_at_enabled_undef:
    adr  x11, trap_flags
    ldr  x12, [x11]
    tbz  x12, #4, fail5

    adr  x3, el0_allowed
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret
after_allowed:
    adr  x11, allow_done
    ldr  x12, [x11]
    cbz  x12, fail7

    adr  x16, zva_buf
    ldr  x17, [x16, #0]
    cbnz x17, fail8
    ldr  x17, [x16, #8]
    cbnz x17, fail8
    ldr  x17, [x16, #16]
    cbnz x17, fail8
    ldr  x17, [x16, #24]
    cbnz x17, fail8
    ldr  x17, [x16, #32]
    cbnz x17, fail8
    ldr  x17, [x16, #40]
    cbnz x17, fail8
    ldr  x17, [x16, #48]
    cbnz x17, fail8
    ldr  x17, [x16, #56]
    cbnz x17, fail8

    movz x0, #'C'
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
fail8:
    movz x0, #'8'
    b out

out:
    movz x1, #0x0000
    movk x1, #0x0900, lsl #16
    str  x0, [x1]
    brk  #0

el0_ic_ivau_denied:
    adr  x0, zva_buf
el0_ic_ivau_insn:
    ic   ivau, x0
    svc  #0x11

el0_at_denied:
    adr  x0, zva_buf
el0_at_insn:
    at   s1e1r, x0
    svc  #0x12

el0_at_enabled_undef:
    adr  x0, zva_buf
el0_at_enabled_insn:
    at   s1e1w, x0
    svc  #0x16

el0_dc_zva_denied:
    adr  x0, zva_buf
    add  x0, x0, #32
el0_dc_zva_insn:
    dc   zva, x0
    svc  #0x13

el0_tlbi_denied:
el0_tlbi_insn:
    tlbi vmalle1
    svc  #0x14

el0_allowed:
    adr  x0, zva_buf
    ic   ivau, x0
    add  x0, x0, #32
    dc   zva, x0
    svc  #0x20

    .balign 2048
vectors:
    .space 0x400
sync_lower:
    b lower_sync_handler

lower_sync_handler:
    mrs  x20, esr_el1
    ubfx x21, x20, #26, #6
    cbz  x21, handle_undef
    movz x22, #0x18
    cmp  x21, x22
    b.eq handle_sys_trap
    movz x22, #0x15
    cmp  x21, x22
    b.eq handle_svc
    b handler_fail

handle_sys_trap:
    mrs  x23, elr_el1
    adr  x24, el0_ic_ivau_insn
    cmp  x23, x24
    b.eq trap_ic_ivau
    adr  x24, el0_dc_zva_insn
    cmp  x23, x24
    b.eq trap_dc_zva
    adr  x24, el0_tlbi_insn
    cmp  x23, x24
    b.eq trap_tlbi
    b handler_fail

trap_ic_ivau:
    movz x24, #0xffff
    movk x24, #0x01ff, lsl #16
    and  x25, x20, x24
    movz x24, #0xdc0a
    movk x24, #0x0012, lsl #16
    cmp  x25, x24
    b.ne fail1
    adr  x24, trap_flags
    ldr  x25, [x24]
    orr  x25, x25, #1
    str  x25, [x24]
    adr  x26, after_ic_ivau_denied
    b return_to_el1

trap_dc_zva:
    movz x24, #0xffff
    movk x24, #0x01ff, lsl #16
    and  x25, x20, x24
    movz x24, #0xdc08
    movk x24, #0x0012, lsl #16
    cmp  x25, x24
    b.ne fail3
    adr  x24, trap_flags
    ldr  x25, [x24]
    orr  x25, x25, #4
    str  x25, [x24]
    adr  x26, after_dc_zva_denied
    b return_to_el1

trap_tlbi:
    adr  x24, trap_flags
    ldr  x25, [x24]
    orr  x25, x25, #8
    str  x25, [x24]
    adr  x26, after_tlbi_denied
    b return_to_el1

handle_undef:
    mrs  x23, elr_el1
    adr  x24, el0_at_insn
    cmp  x23, x24
    b.eq undef_at_denied
    adr  x24, el0_tlbi_insn
    cmp  x23, x24
    b.eq trap_tlbi
    adr  x24, el0_at_enabled_insn
    cmp  x23, x24
    b.eq undef_at_enabled
    b handler_fail

undef_at_denied:
    adr  x24, trap_flags
    ldr  x25, [x24]
    orr  x25, x25, #2
    str  x25, [x24]
    adr  x26, after_at_denied
    b return_to_el1

undef_at_enabled:
    adr  x24, trap_flags
    ldr  x25, [x24]
    orr  x25, x25, #16
    str  x25, [x24]
    adr  x26, after_at_enabled_undef
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
    .balign 64
zva_buf:
    .quad 0x1111111111111111
    .quad 0x2222222222222222
    .quad 0x3333333333333333
    .quad 0x4444444444444444
    .quad 0x5555555555555555
    .quad 0x6666666666666666
    .quad 0x7777777777777777
    .quad 0x8888888888888888
