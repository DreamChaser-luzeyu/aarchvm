    .text
    .global _start
_start:
    movz x1, #0x0000
    movk x1, #0x0900, lsl #16      // UART

    adr  x10, vectors
    msr  vbar_el1, x10

    adr  x11, trap_count
    str  xzr, [x11]

    adr  x3, el0_mrs_midr
    msr  elr_el1, x3
    msr  spsr_el1, xzr             // EL0t
    eret

after_midr:
    adr  x11, trap_count
    ldr  x12, [x11]
    movz x13, #1
    cmp  x12, x13
    b.ne fail1

    adr  x3, el0_mrs_clidr
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret

after_clidr:
    adr  x11, trap_count
    ldr  x12, [x11]
    movz x13, #2
    cmp  x12, x13
    b.ne fail2

    adr  x3, el0_mrs_mmfr0
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret

after_mmfr0:
    adr  x11, trap_count
    ldr  x12, [x11]
    movz x13, #3
    cmp  x12, x13
    b.ne fail3

    movz x0, #'D'
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

out:
    movz x1, #0x0000
    movk x1, #0x0900, lsl #16
    str  x0, [x1]
    brk  #0

el0_mrs_midr:
    movz x0, #0x4321
    movk x0, #0x8765, lsl #16
    movk x0, #0xcba9, lsl #32
    movk x0, #0x0fed, lsl #48
el0_mrs_midr_insn:
    mrs  x0, midr_el1
    svc  #0x11

el0_mrs_clidr:
    movz x0, #0x1111
    movk x0, #0x2222, lsl #16
    movk x0, #0x3333, lsl #32
    movk x0, #0x4444, lsl #48
el0_mrs_clidr_insn:
    mrs  x0, clidr_el1
    svc  #0x12

el0_mrs_mmfr0:
    movz x0, #0xaaaa
    movk x0, #0xbbbb, lsl #16
    movk x0, #0xcccc, lsl #32
    movk x0, #0xdddd, lsl #48
el0_mrs_mmfr0_insn:
    mrs  x0, id_aa64mmfr0_el1
    svc  #0x13

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
    adr  x23, el0_mrs_midr_insn
    cmp  x22, x23
    b.eq undef_midr
    adr  x23, el0_mrs_clidr_insn
    cmp  x22, x23
    b.eq undef_clidr
    adr  x23, el0_mrs_mmfr0_insn
    cmp  x22, x23
    b.eq undef_mmfr0
    b handler_fail

undef_midr:
    movz x24, #0x4321
    movk x24, #0x8765, lsl #16
    movk x24, #0xcba9, lsl #32
    movk x24, #0x0fed, lsl #48
    cmp  x0, x24
    b.ne handler_fail
    adr  x25, trap_count
    movz x26, #1
    str  x26, [x25]
    adr  x27, after_midr
    b return_to_el1

undef_clidr:
    movz x24, #0x1111
    movk x24, #0x2222, lsl #16
    movk x24, #0x3333, lsl #32
    movk x24, #0x4444, lsl #48
    cmp  x0, x24
    b.ne handler_fail
    adr  x25, trap_count
    movz x26, #2
    str  x26, [x25]
    adr  x27, after_clidr
    b return_to_el1

undef_mmfr0:
    movz x24, #0xaaaa
    movk x24, #0xbbbb, lsl #16
    movk x24, #0xcccc, lsl #32
    movk x24, #0xdddd, lsl #48
    cmp  x0, x24
    b.ne handler_fail
    adr  x25, trap_count
    movz x26, #3
    str  x26, [x25]
    adr  x27, after_mmfr0

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
