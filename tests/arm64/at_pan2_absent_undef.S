    .text
    .global _start
_start:
    movz x1, #0x0000
    movk x1, #0x0900, lsl #16      // UART
    movz x2, #0x0000
    movk x2, #0x0902, lsl #16      // PERF mailbox

    adr  x10, vectors
    msr  vbar_el1, x10

    adr  x21, handled
    str  xzr, [x21]

    ldr  x17, par_seed
    msr  par_el1, x17

    ldr  x9, operand_a
el1_s1e1rp_insn:
    .inst 0xD5087909              // AT S1E1RP, X9
after_el1_s1e1rp:
    ldr  x18, [x21]
    movz x19, #1
    cmp  x18, x19
    b.ne fail
    mrs  x14, par_el1
    cmp  x14, x17
    b.ne fail
    ldr  x15, operand_a
    cmp  x9, x15
    b.ne fail

    ldr  x10, operand_b
el1_s1e1wp_insn:
    .inst 0xD508792A              // AT S1E1WP, X10
after_el1_s1e1wp:
    ldr  x18, [x21]
    movz x19, #2
    cmp  x18, x19
    b.ne fail
    mrs  x14, par_el1
    cmp  x14, x17
    b.ne fail
    ldr  x15, operand_b
    cmp  x10, x15
    b.ne fail

    adr  x3, el0_s1e1rp_entry
    msr  elr_el1, x3
    msr  spsr_el1, xzr            // EL0t
    eret

after_el0_s1e1rp:
    ldr  x18, [x21]
    movz x19, #3
    cmp  x18, x19
    b.ne fail
    mrs  x14, par_el1
    cmp  x14, x17
    b.ne fail
    ldr  x15, operand_c
    cmp  x11, x15
    b.ne fail

    adr  x3, el0_s1e1wp_entry
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret

after_el0_s1e1wp:
    ldr  x18, [x21]
    movz x19, #4
    cmp  x18, x19
    b.ne fail
    mrs  x14, par_el1
    cmp  x14, x17
    b.ne fail
    ldr  x15, operand_d
    cmp  x12, x15
    b.ne fail

    movz x0, #'N'
    str  x0, [x1]
    movz x0, #'\n'
    str  x0, [x1]
    b    exit_ok

el0_s1e1rp_entry:
    ldr  x11, operand_c
el0_s1e1rp_insn:
    .inst 0xD508790B              // AT S1E1RP, X11
    b    fail

el0_s1e1wp_entry:
    ldr  x12, operand_d
el0_s1e1wp_insn:
    .inst 0xD508792C              // AT S1E1WP, X12
    b    fail

fail:
    movz x0, #'F'
    str  x0, [x1]
    movz x0, #'\n'
    str  x0, [x1]

exit_ok:
    movz x0, #3
    str  x0, [x2, #0x08]
1:
    wfi
    b    1b

    .balign 2048
vectors:
    .space 0x200
sync_spx:
    b sync_handler
    .space 0x1fc
sync_lower:
    b sync_handler

sync_handler:
    mrs  x22, esr_el1
    ubfx x23, x22, #26, #6
    cbnz x23, fail_from_handler
    tbz  x22, #25, fail_from_handler
    and  x24, x22, #0x1ffffff
    cbnz x24, fail_from_handler

    mrs  x25, far_el1
    cbnz x25, fail_from_handler
    mrs  x26, elr_el1

    adr  x27, el1_s1e1rp_insn
    cmp  x26, x27
    b.eq handle_el1_s1e1rp
    adr  x27, el1_s1e1wp_insn
    cmp  x26, x27
    b.eq handle_el1_s1e1wp
    adr  x27, el0_s1e1rp_insn
    cmp  x26, x27
    b.eq handle_el0_s1e1rp
    adr  x27, el0_s1e1wp_insn
    cmp  x26, x27
    b.eq handle_el0_s1e1wp
    b fail_from_handler

handle_el1_s1e1rp:
    adr  x27, handled
    movz x28, #1
    str  x28, [x27]
    adr  x29, after_el1_s1e1rp
    msr  elr_el1, x29
    eret

handle_el1_s1e1wp:
    adr  x27, handled
    movz x28, #2
    str  x28, [x27]
    adr  x29, after_el1_s1e1wp
    msr  elr_el1, x29
    eret

handle_el0_s1e1rp:
    adr  x27, handled
    movz x28, #3
    str  x28, [x27]
    adr  x29, after_el0_s1e1rp
    movz x30, #0x5                // EL1h
    msr  spsr_el1, x30
    msr  elr_el1, x29
    eret

handle_el0_s1e1wp:
    adr  x27, handled
    movz x28, #4
    str  x28, [x27]
    adr  x29, after_el0_s1e1wp
    movz x30, #0x5                // EL1h
    msr  spsr_el1, x30
    msr  elr_el1, x29
    eret

fail_from_handler:
    movz x30, #0x5
    msr  spsr_el1, x30
    adr  x29, fail
    msr  elr_el1, x29
    eret

    .data
    .balign 8
handled:
    .quad 0
par_seed:
    .quad 0x123456789abc5800
operand_a:
    .quad 0x0000000040123456
operand_b:
    .quad 0xffff00004056789a
operand_c:
    .quad 0x0000000040abcdef
operand_d:
    .quad 0xffff00004013579b
