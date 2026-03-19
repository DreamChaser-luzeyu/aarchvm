    .arch armv8.1-a
    .text
    .global _start
_start:
    movz x1, #0x0000
    movk x1, #0x0900, lsl #16      // UART

    adr  x10, vectors
    msr  vbar_el1, x10

    adr  x11, trap_count
    str  xzr, [x11]

    adr  x3, el0_mrs_spsel
    msr  elr_el1, x3
    msr  spsr_el1, xzr             // EL0t
    eret

after_mrs_spsel:
    adr  x11, trap_count
    ldr  x12, [x11]
    movz x13, #1
    cmp  x12, x13
    b.ne fail1

    adr  x3, el0_mrs_pan
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret

after_mrs_pan:
    adr  x11, trap_count
    ldr  x12, [x11]
    movz x13, #2
    cmp  x12, x13
    b.ne fail2

    adr  x3, el0_msr_spsel_imm
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret

after_msr_spsel_imm:
    adr  x11, trap_count
    ldr  x12, [x11]
    movz x13, #3
    cmp  x12, x13
    b.ne fail3

    adr  x3, el0_msr_pan_imm
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret

after_msr_pan_imm:
    adr  x11, trap_count
    ldr  x12, [x11]
    movz x13, #4
    cmp  x12, x13
    b.ne fail4

    movz x0, #'S'
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

el0_mrs_spsel:
    movz x0, #0x1357
    movk x0, #0x2468, lsl #16
    movk x0, #0x369c, lsl #32
    movk x0, #0x48ad, lsl #48
el0_mrs_spsel_insn:
    mrs  x0, spsel
    svc  #0x11

el0_mrs_pan:
    movz x0, #0x5555
    movk x0, #0xaaaa, lsl #16
    movk x0, #0x1234, lsl #32
    movk x0, #0xdead, lsl #48
el0_mrs_pan_insn:
    mrs  x0, pan
    svc  #0x12

el0_msr_spsel_imm:
el0_msr_spsel_imm_insn:
    msr  spsel, #1
    svc  #0x13

el0_msr_pan_imm:
el0_msr_pan_imm_insn:
    msr  pan, #1
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
    adr  x23, el0_mrs_spsel_insn
    cmp  x22, x23
    b.eq undef_mrs_spsel
    adr  x23, el0_mrs_pan_insn
    cmp  x22, x23
    b.eq undef_mrs_pan
    adr  x23, el0_msr_spsel_imm_insn
    cmp  x22, x23
    b.eq undef_msr_spsel_imm
    adr  x23, el0_msr_pan_imm_insn
    cmp  x22, x23
    b.eq undef_msr_pan_imm
    b handler_fail

undef_mrs_spsel:
    movz x24, #0x1357
    movk x24, #0x2468, lsl #16
    movk x24, #0x369c, lsl #32
    movk x24, #0x48ad, lsl #48
    cmp  x0, x24
    b.ne handler_fail
    adr  x25, trap_count
    movz x26, #1
    str  x26, [x25]
    adr  x27, after_mrs_spsel
    b return_to_el1

undef_mrs_pan:
    movz x24, #0x5555
    movk x24, #0xaaaa, lsl #16
    movk x24, #0x1234, lsl #32
    movk x24, #0xdead, lsl #48
    cmp  x0, x24
    b.ne handler_fail
    adr  x25, trap_count
    movz x26, #2
    str  x26, [x25]
    adr  x27, after_mrs_pan
    b return_to_el1

undef_msr_spsel_imm:
    adr  x25, trap_count
    movz x26, #3
    str  x26, [x25]
    adr  x27, after_msr_spsel_imm
    b return_to_el1

undef_msr_pan_imm:
    adr  x25, trap_count
    movz x26, #4
    str  x26, [x25]
    adr  x27, after_msr_pan_imm

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
