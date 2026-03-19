    .text
    .global _start
_start:
    movz x1, #0x0000
    movk x1, #0x0900, lsl #16      // UART

    adr  x10, vectors
    msr  vbar_el1, x10

    adr  x11, trap_count
    str  xzr, [x11]

    adr  x3, el0_mrs_uao
    msr  elr_el1, x3
    msr  spsr_el1, xzr             // EL0t
    eret

after_uao:
    adr  x11, trap_count
    ldr  x12, [x11]
    movz x13, #1
    cmp  x12, x13
    b.ne fail1

    adr  x3, el0_mrs_dit
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret

after_dit:
    adr  x11, trap_count
    ldr  x12, [x11]
    movz x13, #2
    cmp  x12, x13
    b.ne fail2

    adr  x3, el0_mrs_ssbs
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret

after_ssbs:
    adr  x11, trap_count
    ldr  x12, [x11]
    movz x13, #3
    cmp  x12, x13
    b.ne fail3

    adr  x3, el0_mrs_tco
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret

after_tco:
    adr  x11, trap_count
    ldr  x12, [x11]
    movz x13, #4
    cmp  x12, x13
    b.ne fail4

    movz x0, #'A'
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

out:
    movz x1, #0x0000
    movk x1, #0x0900, lsl #16
    str  x0, [x1]
    brk  #0

el0_mrs_uao:
    movz x0, #0x1111
    movk x0, #0x2222, lsl #16
    movk x0, #0x3333, lsl #32
    movk x0, #0x4444, lsl #48
el0_mrs_uao_insn:
    .inst 0xD5384280                  // mrs x0, uao
    svc  #0x11

el0_mrs_dit:
    movz x0, #0x5555
    movk x0, #0x6666, lsl #16
    movk x0, #0x7777, lsl #32
    movk x0, #0x8888, lsl #48
el0_mrs_dit_insn:
    .inst 0xD53B42A0                  // mrs x0, dit
    svc  #0x12

el0_mrs_ssbs:
    movz x0, #0x9999
    movk x0, #0xaaaa, lsl #16
    movk x0, #0xbbbb, lsl #32
    movk x0, #0xcccc, lsl #48
el0_mrs_ssbs_insn:
    .inst 0xD53B42C0                  // mrs x0, ssbs
    svc  #0x13

el0_mrs_tco:
    movz x0, #0xdddd
    movk x0, #0xeeee, lsl #16
    movk x0, #0x1357, lsl #32
    movk x0, #0x2468, lsl #48
el0_mrs_tco_insn:
    .inst 0xD53B42E0                  // mrs x0, tco
    svc  #0x14

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
    adr  x23, el0_mrs_uao_insn
    cmp  x22, x23
    b.eq undef_uao
    adr  x23, el0_mrs_dit_insn
    cmp  x22, x23
    b.eq undef_dit
    adr  x23, el0_mrs_ssbs_insn
    cmp  x22, x23
    b.eq undef_ssbs
    adr  x23, el0_mrs_tco_insn
    cmp  x22, x23
    b.eq undef_tco
    b handler_fail

undef_uao:
    movz x24, #0x1111
    movk x24, #0x2222, lsl #16
    movk x24, #0x3333, lsl #32
    movk x24, #0x4444, lsl #48
    cmp  x0, x24
    b.ne handler_fail
    adr  x25, trap_count
    movz x26, #1
    str  x26, [x25]
    adr  x27, after_uao
    b return_to_el1

undef_dit:
    movz x24, #0x5555
    movk x24, #0x6666, lsl #16
    movk x24, #0x7777, lsl #32
    movk x24, #0x8888, lsl #48
    cmp  x0, x24
    b.ne handler_fail
    adr  x25, trap_count
    movz x26, #2
    str  x26, [x25]
    adr  x27, after_dit
    b return_to_el1

undef_ssbs:
    movz x24, #0x9999
    movk x24, #0xaaaa, lsl #16
    movk x24, #0xbbbb, lsl #32
    movk x24, #0xcccc, lsl #48
    cmp  x0, x24
    b.ne handler_fail
    adr  x25, trap_count
    movz x26, #3
    str  x26, [x25]
    adr  x27, after_ssbs
    b return_to_el1

undef_tco:
    movz x24, #0xdddd
    movk x24, #0xeeee, lsl #16
    movk x24, #0x1357, lsl #32
    movk x24, #0x2468, lsl #48
    cmp  x0, x24
    b.ne handler_fail
    adr  x25, trap_count
    movz x26, #4
    str  x26, [x25]
    adr  x27, after_tco

return_to_el1:
    movz x28, #0x5                // EL1h
    msr  spsr_el1, x28
    msr  elr_el1, x27
    eret

handler_fail:
    movz x0, #'F'
    adr  x27, out
    movz x28, #0x5
    msr  spsr_el1, x28
    msr  elr_el1, x27
    eret

    .data
    .balign 8
trap_count:
    .quad 0
