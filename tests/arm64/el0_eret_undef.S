    .text
    .global _start
_start:
    movz x1, #0x0000
    movk x1, #0x0900, lsl #16

    adr  x10, vectors
    msr  vbar_el1, x10

    adr  x11, trap_count
    str  xzr, [x11]

    adr  x3, el0_eret
    msr  elr_el1, x3
    movz x4, #0x0280              // D=1, I=1.
    movk x4, #0x9040, lsl #16     // NZCV=1001, PAN=1, EL0t.
    msr  spsr_el1, x4
    eret

after_undef:
    adr  x11, trap_count
    ldr  x12, [x11]
    movz x13, #1
    cmp  x12, x13
    b.ne fail

    movz x0, #'E'
    str  x0, [x1]
    movz x0, #'\n'
    str  x0, [x1]
    brk  #0

fail:
    movz x0, #'F'
    str  x0, [x1]
    brk  #0

el0_eret:
el0_eret_insn:
    eret
    svc  #0x66

    .balign 2048
vectors:
    .space 0x400
sync_lower:
    b lower_sync_handler

lower_sync_handler:
    mrs  x20, esr_el1
    ubfx x21, x20, #26, #6
    cbnz x21, handler_fail
    tbz  x20, #25, handler_fail
    and  x28, x20, #0x1ffffff
    cbnz x28, handler_fail

    mrs  x22, elr_el1
    adr  x23, el0_eret_insn
    cmp  x22, x23
    b.ne handler_fail

    mrs  x24, far_el1
    cbnz x24, handler_fail

    mrs  x25, spsr_el1
    and  x26, x25, #0xf
    cbnz x26, handler_fail
    tbnz x25, #20, handler_fail
    ubfx x26, x25, #28, #4
    cmp  x26, #0x9
    b.ne handler_fail
    ubfx x26, x25, #6, #4
    cmp  x26, #0xa
    b.ne handler_fail
    tbz  x25, #22, handler_fail

    adr  x24, trap_count
    movz x25, #1
    str  x25, [x24]

    movz x26, #0x5
    adr  x27, after_undef
    msr  spsr_el1, x26
    msr  elr_el1, x27
    eret

handler_fail:
    movz x0, #'X'
    str  x0, [x1]
    brk  #0

    .data
    .balign 8
trap_count:
    .quad 0
