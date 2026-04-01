    .text
    .global _start
_start:
    movz x1, #0x0000
    movk x1, #0x0900, lsl #16

    adr  x10, vectors
    msr  vbar_el1, x10

    adr  x11, trap_count
    str  xzr, [x11]

    adr  x3, el0_hvc
    msr  elr_el1, x3
    movz x4, #0x0240               // D=1, F=1.
    movk x4, #0x7040, lsl #16      // NZCV=0111, PAN=1, EL0t.
    msr  spsr_el1, x4
    eret

after_hvc:
    adr  x11, trap_count
    ldr  x12, [x11]
    movz x13, #1
    cmp  x12, x13
    b.ne fail1

    adr  x3, el0_smc
    msr  elr_el1, x3
    movz x4, #0x0180               // A=1, I=1.
    movk x4, #0x2040, lsl #16      // NZCV=0010, PAN=1, EL0t.
    msr  spsr_el1, x4
    eret

after_smc:
    adr  x11, trap_count
    ldr  x12, [x11]
    movz x13, #2
    cmp  x12, x13
    b.ne fail2

    movz x0, #'H'
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

out:
    str  x0, [x1]
    brk  #0

el0_hvc:
el0_hvc_insn:
    hvc  #0x11
    svc  #0x71

el0_smc:
el0_smc_insn:
    smc  #0x22
    svc  #0x72

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

    mrs  x22, elr_el1
    adr  x23, el0_hvc_insn
    cmp  x22, x23
    b.eq handle_hvc
    adr  x23, el0_smc_insn
    cmp  x22, x23
    b.eq handle_smc
    b handler_fail

handle_hvc:
    mrs  x24, spsr_el1
    and  x25, x24, #0xf
    cbnz x25, handler_fail
    tbnz x24, #20, handler_fail
    ubfx x25, x24, #28, #4
    cmp  x25, #0x7
    b.ne handler_fail
    ubfx x25, x24, #6, #4
    cmp  x25, #0x9
    b.ne handler_fail
    tbz  x24, #22, handler_fail
    adr  x24, trap_count
    movz x25, #1
    str  x25, [x24]
    movz x26, #0x5
    adr  x27, after_hvc
    msr  spsr_el1, x26
    msr  elr_el1, x27
    eret

handle_smc:
    mrs  x24, spsr_el1
    and  x25, x24, #0xf
    cbnz x25, handler_fail
    tbnz x24, #20, handler_fail
    ubfx x25, x24, #28, #4
    cmp  x25, #0x2
    b.ne handler_fail
    ubfx x25, x24, #6, #4
    cmp  x25, #0x6
    b.ne handler_fail
    tbz  x24, #22, handler_fail
    adr  x24, trap_count
    movz x25, #2
    str  x25, [x24]
    movz x26, #0x5
    adr  x27, after_smc
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
