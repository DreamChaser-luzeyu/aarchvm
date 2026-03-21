    .text
    .global _start
_start:
    movz x1, #0x0000
    movk x1, #0x0900, lsl #16      // UART

    adr  x10, vectors
    msr  vbar_el1, x10

    adr  x11, handled
    str  xzr, [x11]

el1_at_s1e1rp_insn:
    .inst 0xD5087900               // at s1e1rp, x0
after_el1_at_s1e1rp:
    ldr  x12, [x11]
    movz x13, #1
    cmp  x12, x13
    b.ne fail1

el1_at_s1e1wp_insn:
    .inst 0xD5087921               // at s1e1wp, x1
after_el1_at_s1e1wp:
    ldr  x12, [x11]
    movz x13, #2
    cmp  x12, x13
    b.ne fail2

    adr  x3, el0_at_s1e1rp_entry
    msr  elr_el1, x3
    msr  spsr_el1, xzr             // EL0t
    eret

after_el0_at_s1e1rp:
    ldr  x12, [x11]
    movz x13, #3
    cmp  x12, x13
    b.ne fail3

    adr  x3, el0_at_s1e1wp_entry
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret

after_el0_at_s1e1wp:
    ldr  x12, [x11]
    movz x13, #4
    cmp  x12, x13
    b.ne fail4

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

out:
    str  x0, [x1]
    brk  #0

el0_at_s1e1rp_entry:
el0_at_s1e1rp_insn:
    .inst 0xD5087900               // at s1e1rp, x0
    b    fail3

el0_at_s1e1wp_entry:
el0_at_s1e1wp_insn:
    .inst 0xD5087921               // at s1e1wp, x1
    b    fail4

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
    cbnz x21, handler_fail
    tbz  x20, #25, handler_fail
    and  x22, x20, #0x1ffffff
    cbnz x22, handler_fail

    mrs  x23, far_el1
    cbnz x23, handler_fail

    mrs  x24, elr_el1
    adr  x25, el1_at_s1e1rp_insn
    cmp  x24, x25
    b.eq handle_el1_rp
    adr  x25, el1_at_s1e1wp_insn
    cmp  x24, x25
    b.eq handle_el1_wp
    adr  x25, el0_at_s1e1rp_insn
    cmp  x24, x25
    b.eq handle_el0_rp
    adr  x25, el0_at_s1e1wp_insn
    cmp  x24, x25
    b.eq handle_el0_wp
    b handler_fail

handle_el1_rp:
    adr  x26, handled
    movz x27, #1
    str  x27, [x26]
    adr  x28, after_el1_at_s1e1rp
    msr  elr_el1, x28
    eret

handle_el1_wp:
    adr  x26, handled
    movz x27, #2
    str  x27, [x26]
    adr  x28, after_el1_at_s1e1wp
    msr  elr_el1, x28
    eret

handle_el0_rp:
    adr  x26, handled
    movz x27, #3
    str  x27, [x26]
    adr  x28, after_el0_at_s1e1rp
    msr  elr_el1, x28
    movz x29, #0x5                 // return to EL1h
    msr  spsr_el1, x29
    eret

handle_el0_wp:
    adr  x26, handled
    movz x27, #4
    str  x27, [x26]
    adr  x28, after_el0_at_s1e1wp
    msr  elr_el1, x28
    movz x29, #0x5                 // return to EL1h
    msr  spsr_el1, x29
    eret

handler_fail:
    movz x0, #'F'
    str  x0, [x1]
    brk  #0

    .balign 8
handled:
    .quad 0
