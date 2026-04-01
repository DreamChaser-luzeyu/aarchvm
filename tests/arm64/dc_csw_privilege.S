    .text
    .global _start
_start:
    movz x1, #0x0000
    movk x1, #0x0900, lsl #16      // UART

    adr  x10, vectors
    msr  vbar_el1, x10

    adr  x11, trap_count
    str  xzr, [x11]

    movz x0, #0
    dc   csw, x0

    adr  x3, el0_dc_csw
    msr  elr_el1, x3
    movz x4, #0x01c0               // A=1, I=1, F=1.
    movk x4, #0xA040, lsl #16      // NZCV=1010, PAN=1, EL0t.
    msr  spsr_el1, x4
    eret

after_undef:
    adr  x11, trap_count
    ldr  x12, [x11]
    movz x13, #1
    cmp  x12, x13
    b.ne fail

    movz x0, #'J'
    str  x0, [x1]
    movz x0, #'\n'
    str  x0, [x1]
    brk  #0

fail:
    movz x0, #'F'
    str  x0, [x1]
    brk  #0

el0_dc_csw:
el0_dc_csw_insn:
    .inst 0xD5087A41               // dc csw, x1
    svc  #0x66

    .balign 2048
vectors:
    .space 0x200
sync_spx:
    b sync_handler
    .space 0x1fc
    b sync_handler

sync_handler:
    mrs  x20, esr_el1
    ubfx x21, x20, #26, #6
    cbnz x21, handler_fail
    tbz  x20, #25, handler_fail
    and  x22, x20, #0x1ffffff
    cbnz x22, handler_fail

    mrs  x23, elr_el1
    adr  x24, el0_dc_csw_insn
    cmp  x23, x24
    b.ne handler_fail

    mrs  x25, far_el1
    cbnz x25, handler_fail

    mrs  x26, spsr_el1
    and  x27, x26, #0xf
    cbnz x27, handler_fail
    tbnz x26, #20, handler_fail
    ubfx x27, x26, #28, #4
    cmp  x27, #0xa
    b.ne handler_fail
    ubfx x27, x26, #6, #4
    cmp  x27, #0x7
    b.ne handler_fail
    tbz  x26, #22, handler_fail

    adr  x24, trap_count
    movz x25, #1
    str  x25, [x24]

    movz x26, #0x5                 // EL1h
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
