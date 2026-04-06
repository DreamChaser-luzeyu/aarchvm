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

    movz x4, #0x0280               // D=1, I=1.
    movk x4, #0x9040, lsl #16      // NZCV=1001, PAN=1, EL0t.

    movz x0, #0
    msr  spsel, x0
    mrs  x2, spsel
    cbnz x2, fail1

    movz x0, #1
    msr  spsel, x0
    mrs  x2, spsel
    movz x3, #1
    cmp  x2, x3
    b.ne fail2

    movz x0, #1
    msr  pan, x0
    mrs  x2, pan
    movz x3, #1
    cmp  x2, x3
    b.ne fail3

    movz x0, #0
    msr  pan, x0
    mrs  x2, pan
    cbnz x2, fail4

    adr  x3, el0_msr_spsel_reg
    msr  elr_el1, x3
    msr  spsr_el1, x4
    eret

after_el0_spsel:
    adr  x11, trap_count
    ldr  x12, [x11]
    movz x13, #1
    cmp  x12, x13
    b.ne fail5

    adr  x3, el0_msr_pan_reg
    msr  elr_el1, x3
    msr  spsr_el1, x4
    eret

after_el0_pan:
    adr  x11, trap_count
    ldr  x12, [x11]
    movz x13, #2
    cmp  x12, x13
    b.ne fail6

    movz x0, #'Y'
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

out:
    str  x0, [x1]
    brk  #0

el0_msr_spsel_reg:
    movz x0, #0x1111
    movk x0, #0x2222, lsl #16
    movk x0, #0x3333, lsl #32
    movk x0, #0x4444, lsl #48
el0_msr_spsel_reg_insn:
    msr  spsel, x0
    svc  #0x11

el0_msr_pan_reg:
    movz x0, #0x5555
    movk x0, #0x6666, lsl #16
    movk x0, #0x7777, lsl #32
    movk x0, #0x8888, lsl #48
el0_msr_pan_reg_insn:
    msr  pan, x0
    svc  #0x12

    .balign 2048
vectors:
    .space 0x400
sync_lower:
    b lower_sync_handler

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
    adr  x23, el0_msr_spsel_reg_insn
    cmp  x22, x23
    b.eq undef_spsel_reg
    adr  x23, el0_msr_pan_reg_insn
    cmp  x22, x23
    b.eq undef_pan_reg
    b handler_fail

undef_spsel_reg:
    movz x24, #0x1111
    movk x24, #0x2222, lsl #16
    movk x24, #0x3333, lsl #32
    movk x24, #0x4444, lsl #48
    cmp  x0, x24
    b.ne handler_fail
    adr  x25, trap_count
    movz x26, #1
    str  x26, [x25]
    adr  x27, after_el0_spsel
    b return_to_el1

undef_pan_reg:
    movz x24, #0x5555
    movk x24, #0x6666, lsl #16
    movk x24, #0x7777, lsl #32
    movk x24, #0x8888, lsl #48
    cmp  x0, x24
    b.ne handler_fail
    adr  x25, trap_count
    movz x26, #2
    str  x26, [x25]
    adr  x27, after_el0_pan

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
