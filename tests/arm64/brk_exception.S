    .arch armv8.1-a
    .text
    .global _start
_start:
    movz x1, #0x0000
    movk x1, #0x0900, lsl #16      // UART
    movz x2, #0x0000
    movk x2, #0x0902, lsl #16      // PERF mailbox

    adr  x10, vectors
    msr  vbar_el1, x10

    adr  x11, handled
    str  xzr, [x11]

    movz x0, #0x9000, lsl #16
    msr  nzcv, x0
    msr  daifclr, #0xf
    msr  daifset, #0x6
    movz x0, #1
    msr  pan, x0

el1_brk_insn:
    brk  #0x1234

after_el1_brk:
    ldr  x12, [x11]
    movz x13, #1
    cmp  x12, x13
    b.ne fail

    adr  x3, el0_brk_entry
    msr  elr_el1, x3
    movz x4, #0x0280               // D=1, I=1.
    movk x4, #0x5040, lsl #16      // NZCV=0101, PAN=1, EL0t.
    msr  spsr_el1, x4
    eret

after_el0_brk:
    ldr  x12, [x11]
    movz x13, #3
    cmp  x12, x13
    b.ne fail

    movz x0, #'B'
    str  x0, [x1]
    movz x0, #'\n'
    str  x0, [x1]
    b    exit_ok

el0_brk_entry:
el0_brk_insn:
    brk  #0xabcd
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
    mrs  x20, esr_el1
    ubfx x21, x20, #26, #6
    movz x22, #0x3c
    cmp  x21, x22
    b.ne fail_from_handler
    tbz  x20, #25, fail_from_handler
    ubfx x23, x20, #0, #16

    mrs  x24, far_el1
    cbnz x24, fail_from_handler
    mrs  x25, elr_el1

    adr  x26, el1_brk_insn
    cmp  x25, x26
    b.eq handle_el1_brk
    adr  x26, el0_brk_insn
    cmp  x25, x26
    b.eq handle_el0_brk
    b fail_from_handler

handle_el1_brk:
    movz x26, #0x1234
    cmp  x23, x26
    b.ne fail_from_handler
    mrs  x26, spsr_el1
    and  x27, x26, #0xf
    cmp  x27, #0x5
    b.ne fail_from_handler
    tbnz x26, #20, fail_from_handler
    ubfx x27, x26, #28, #4
    cmp  x27, #0x9
    b.ne fail_from_handler
    ubfx x27, x26, #6, #4
    cmp  x27, #0x6
    b.ne fail_from_handler
    tbz  x26, #22, fail_from_handler
    adr  x27, handled
    movz x28, #1
    str  x28, [x27]
    adr  x29, after_el1_brk
    msr  elr_el1, x29
    eret

handle_el0_brk:
    movz x26, #0xabcd
    cmp  x23, x26
    b.ne fail_from_handler
    mrs  x26, spsr_el1
    and  x27, x26, #0xf
    cbnz x27, fail_from_handler
    tbnz x26, #20, fail_from_handler
    ubfx x27, x26, #28, #4
    cmp  x27, #0x5
    b.ne fail_from_handler
    ubfx x27, x26, #6, #4
    cmp  x27, #0xa
    b.ne fail_from_handler
    tbz  x26, #22, fail_from_handler
    adr  x27, handled
    ldr  x28, [x27]
    orr  x28, x28, #2
    str  x28, [x27]
    adr  x29, after_el0_brk
    movz x30, #0x5                // EL1h
    msr  spsr_el1, x30
    msr  elr_el1, x29
    eret

fail_from_handler:
    adr  x29, fail
    movz x30, #0x5
    msr  spsr_el1, x30
    msr  elr_el1, x29
    eret

    .data
    .balign 8
handled:
    .quad 0
