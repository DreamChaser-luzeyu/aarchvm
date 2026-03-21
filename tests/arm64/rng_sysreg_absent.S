    .text
    .global _start
_start:
    movz x15, #0x0000
    movk x15, #0x0900, lsl #16      // UART
    movz x16, #0x0000
    movk x16, #0x0902, lsl #16      // PERF mailbox

    adr  x10, vectors
    msr  vbar_el1, x10

    adr  x11, trap_flags
    str  xzr, [x11]

    movz x0, #0x1357
    movk x0, #0x2468, lsl #16
    movk x0, #0x369a, lsl #32
    movk x0, #0x48bc, lsl #48
el1_rndr_insn:
    .inst 0xD53B2400                  // mrs x0, rndr
    brk  #0

after_el1_rndr:
    movz x2, #0x1357
    movk x2, #0x2468, lsl #16
    movk x2, #0x369a, lsl #32
    movk x2, #0x48bc, lsl #48
    cmp  x0, x2
    b.ne fail1
    adr  x11, trap_flags
    ldr  x12, [x11]
    tbz  x12, #0, fail2

    movz x1, #0xaaaa
    movk x1, #0xbbbb, lsl #16
    movk x1, #0xcccc, lsl #32
    movk x1, #0xdddd, lsl #48
el1_rndrrs_insn:
    .inst 0xD53B2421                  // mrs x1, rndrrs
    brk  #0

after_el1_rndrrs:
    movz x2, #0xaaaa
    movk x2, #0xbbbb, lsl #16
    movk x2, #0xcccc, lsl #32
    movk x2, #0xdddd, lsl #48
    cmp  x1, x2
    b.ne fail3
    adr  x11, trap_flags
    ldr  x12, [x11]
    tbz  x12, #1, fail4

    adr  x3, el0_rndr
    msr  elr_el1, x3
    msr  spsr_el1, xzr             // EL0t
    eret

after_el0_rndr:
    movz x2, #0x1111
    movk x2, #0x2222, lsl #16
    movk x2, #0x3333, lsl #32
    movk x2, #0x4444, lsl #48
    cmp  x0, x2
    b.ne fail5
    adr  x11, trap_flags
    ldr  x12, [x11]
    tbz  x12, #2, fail6

    adr  x3, el0_rndrrs
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret

after_el0_rndrrs:
    movz x2, #0x5555
    movk x2, #0x6666, lsl #16
    movk x2, #0x7777, lsl #32
    movk x2, #0x8888, lsl #48
    cmp  x1, x2
    b.ne fail7
    adr  x11, trap_flags
    ldr  x12, [x11]
    tbz  x12, #3, fail8

    movz x0, #'N'
    str  x0, [x15]
    movz x0, #'\n'
    str  x0, [x15]
    b    exit_ok

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
out:
    str  x0, [x15]
    movz x0, #'\n'
    str  x0, [x15]
    b    exit_ok

el0_rndr:
    movz x0, #0x1111
    movk x0, #0x2222, lsl #16
    movk x0, #0x3333, lsl #32
    movk x0, #0x4444, lsl #48
el0_rndr_insn:
    .inst 0xD53B2400                  // mrs x0, rndr
    b    fail5

el0_rndrrs:
    movz x1, #0x5555
    movk x1, #0x6666, lsl #16
    movk x1, #0x7777, lsl #32
    movk x1, #0x8888, lsl #48
el0_rndrrs_insn:
    .inst 0xD53B2421                  // mrs x1, rndrrs
    b    fail7

    .balign 2048
vectors:
    .space 0x200
sync_spx:
    b sync_handler
    .space 0x1fc
sync_lower:
    b sync_handler

sync_handler:
    mrs  x20, esr_el1
    ubfx x21, x20, #26, #6
    cbnz x21, failH
    tbz  x20, #25, failH
    and  x22, x20, #0x1ffffff
    cbnz x22, failH
    mrs  x23, far_el1
    cbnz x23, failH
    mrs  x24, elr_el1

    adr  x25, el1_rndr_insn
    cmp  x24, x25
    b.eq handle_el1_rndr
    adr  x25, el1_rndrrs_insn
    cmp  x24, x25
    b.eq handle_el1_rndrrs
    adr  x25, el0_rndr_insn
    cmp  x24, x25
    b.eq handle_el0_rndr
    adr  x25, el0_rndrrs_insn
    cmp  x24, x25
    b.eq handle_el0_rndrrs
    b    failH

handle_el1_rndr:
    adr  x11, trap_flags
    movz x12, #1
    str  x12, [x11]
    adr  x13, after_el1_rndr
    msr  elr_el1, x13
    eret

handle_el1_rndrrs:
    adr  x11, trap_flags
    movz x12, #2
    str  x12, [x11]
    adr  x13, after_el1_rndrrs
    msr  elr_el1, x13
    eret

handle_el0_rndr:
    adr  x11, trap_flags
    movz x12, #4
    str  x12, [x11]
    adr  x13, after_el0_rndr
    b    return_to_el1

handle_el0_rndrrs:
    adr  x11, trap_flags
    movz x12, #8
    str  x12, [x11]
    adr  x13, after_el0_rndrrs

return_to_el1:
    movz x14, #0x5
    msr  spsr_el1, x14
    msr  elr_el1, x13
    eret

failH:
    movz x0, #'H'
    str  x0, [x15]
    movz x0, #'\n'
    str  x0, [x15]
    b    exit_ok

exit_ok:
    movz x0, #3
    str  x0, [x16, #0x08]
1:
    wfi
    b    1b

    .data
    .balign 8
trap_flags:
    .quad 0
