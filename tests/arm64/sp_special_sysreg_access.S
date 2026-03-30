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

    movz x3, #0x9000
    mov  sp, x3

    movz x4, #0x1234
    movk x4, #0x5678, lsl #16
    movk x4, #0x9abc, lsl #32
    movk x4, #0xdef0, lsl #48

    movz x5, #0x1111
    movk x5, #0x2222, lsl #16
    movk x5, #0x3333, lsl #32
    movk x5, #0x4444, lsl #48

    movz x0, #1
    msr  spsel, x0                 // EL1h
    msr  sp_el0, x4
    mrs  x6, sp_el0
    cmp  x6, x4
    b.ne fail1
    msr  sp_el0, x5
    mrs  x6, sp_el0
    cmp  x6, x5
    b.ne fail2
    mov  x6, sp
    cmp  x6, x3
    b.ne fail3

    movz x0, #0
    msr  spsel, x0                 // EL1t
    mrs  x6, spsel
    cbnz x6, fail4

    movz x0, #0x1357
    movk x0, #0x2468, lsl #16
    movk x0, #0x369c, lsl #32
    movk x0, #0x48ad, lsl #48
el1t_mrs_sp_el0_insn:
    mrs  x0, sp_el0
    b    failF

after_el1t_mrs_sp_el0:
    movz x13, #1
    adr  x12, trap_count
    ldr  x11, [x12]
    cmp  x11, x13
    b.ne fail5
    movz x13, #0x1357
    movk x13, #0x2468, lsl #16
    movk x13, #0x369c, lsl #32
    movk x13, #0x48ad, lsl #48
    cmp  x0, x13
    b.ne fail6
    movz x0, #0
    msr  spsel, x0                 // back to EL1t for the SP_EL0 write-undef case
    mrs  x6, spsel
    cbnz x6, fail7

    movz x9, #0xabcd
    movk x9, #0xef01, lsl #16
    movk x9, #0x2345, lsl #32
    movk x9, #0x6789, lsl #48
el1t_msr_sp_el0_insn:
    msr  sp_el0, x9
    b    failF

after_el1t_msr_sp_el0:
    movz x13, #2
    adr  x12, trap_count
    ldr  x11, [x12]
    cmp  x11, x13
    b.ne fail7
    mrs  x6, sp_el0
    movz x13, #0x1111
    movk x13, #0x2222, lsl #16
    movk x13, #0x3333, lsl #32
    movk x13, #0x4444, lsl #48
    cmp  x6, x13
    b.ne fail8

    movz x11, #0xaaaa
    movk x11, #0xbbbb, lsl #16
    movk x11, #0xcccc, lsl #32
    movk x11, #0xdddd, lsl #48
el1h_mrs_sp_el1_insn:
    mrs  x11, sp_el1
    b    failF

after_el1h_mrs_sp_el1:
    movz x13, #3
    adr  x12, trap_count
    ldr  x10, [x12]
    cmp  x10, x13
    b.ne fail9
    movz x13, #0xaaaa
    movk x13, #0xbbbb, lsl #16
    movk x13, #0xcccc, lsl #32
    movk x13, #0xdddd, lsl #48
    cmp  x11, x13
    b.ne failA

    movz x12, #0x5555
    movk x12, #0x6666, lsl #16
    movk x12, #0x7777, lsl #32
    movk x12, #0x8888, lsl #48
el1h_msr_sp_el1_insn:
    msr  sp_el1, x12
    b    failF

after_el1h_msr_sp_el1:
    movz x13, #4
    adr  x10, trap_count
    ldr  x9, [x10]
    cmp  x9, x13
    b.ne failB

    movz x0, #'R'
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
fail7:
    movz x0, #'7'
    b out
fail8:
    movz x0, #'8'
    b out
fail9:
    movz x0, #'9'
    b out
failA:
    movz x0, #'A'
    b out
failB:
    movz x0, #'B'
    b out
failF:
    movz x0, #'F'
    b out

out:
    str  x0, [x1]
    brk  #0

    .balign 2048
vectors:
sync_sp0:
    b sync_handler
    .space 0x1fc
sync_spx:
    b sync_handler
    .space 0x1fc
sync_lower:
    b sync_handler

sync_handler:
    mrs  x20, esr_el1
    ubfx x21, x20, #26, #6
    cbnz x21, handler_fail

    mrs  x22, elr_el1
    adr  x23, el1t_mrs_sp_el0_insn
    cmp  x22, x23
    b.eq handle_el1t_mrs_sp_el0
    adr  x23, el1t_msr_sp_el0_insn
    cmp  x22, x23
    b.eq handle_el1t_msr_sp_el0
    adr  x23, el1h_mrs_sp_el1_insn
    cmp  x22, x23
    b.eq handle_el1h_mrs_sp_el1
    adr  x23, el1h_msr_sp_el1_insn
    cmp  x22, x23
    b.eq handle_el1h_msr_sp_el1
    b handler_fail

handle_el1t_mrs_sp_el0:
    adr  x24, trap_count
    movz x25, #1
    str  x25, [x24]
    adr  x26, after_el1t_mrs_sp_el0
    b return_el1h

handle_el1t_msr_sp_el0:
    adr  x24, trap_count
    movz x25, #2
    str  x25, [x24]
    adr  x26, after_el1t_msr_sp_el0
    b return_el1h

handle_el1h_mrs_sp_el1:
    adr  x24, trap_count
    movz x25, #3
    str  x25, [x24]
    adr  x26, after_el1h_mrs_sp_el1
    b return_el1h

handle_el1h_msr_sp_el1:
    adr  x24, trap_count
    movz x25, #4
    str  x25, [x24]
    adr  x26, after_el1h_msr_sp_el1

return_el1h:
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
trap_count:
    .quad 0
