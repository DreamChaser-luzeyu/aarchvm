    .text
    .global _start
_start:
    movz x1, #0x0000
    movk x1, #0x0900, lsl #16

    adr  x10, vectors
    msr  vbar_el1, x10

    adr  x11, trap_count
    str  xzr, [x11]

    movz x0, #0xffff
    movk x0, #0x00ff, lsl #16
el1_hvc_insn:
    hvc  #0x1234

after_hvc:
    adr  x11, trap_count
    ldr  x12, [x11]
    movz x13, #1
    cmp  x12, x13
    b.ne fail1

    movz x0, #0xfffe
    movk x0, #0x00ff, lsl #16
el1_smc_insn:
    smc  #0x4321

after_smc:
    adr  x11, trap_count
    ldr  x12, [x11]
    movz x13, #2
    cmp  x12, x13
    b.ne fail2

    movz x0, #'J'
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

    .balign 2048
vectors:
    .space 0x200
sync_spx:
    b sync_handler

sync_handler:
    mrs  x20, esr_el1
    ubfx x21, x20, #26, #6
    cbnz x21, handler_fail

    and  x22, x20, #0x1ffffff
    cbnz x22, handler_fail

    mrs  x23, elr_el1
    adr  x24, el1_hvc_insn
    cmp  x23, x24
    b.eq handle_hvc
    adr  x24, el1_smc_insn
    cmp  x23, x24
    b.eq handle_smc
    b handler_fail

handle_hvc:
    adr  x24, trap_count
    movz x25, #1
    str  x25, [x24]
    adr  x26, after_hvc
    msr  elr_el1, x26
    eret

handle_smc:
    adr  x24, trap_count
    movz x25, #2
    str  x25, [x24]
    adr  x26, after_smc
    msr  elr_el1, x26
    eret

handler_fail:
    movz x0, #'X'
    str  x0, [x1]
    brk  #0

    .data
    .balign 8
trap_count:
    .quad 0
