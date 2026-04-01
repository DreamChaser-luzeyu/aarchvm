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

    movz x12, #0xCAFE
    movk x12, #0xBABE, lsl #16
    movk x12, #0x7654, lsl #32
    movk x12, #0x3210, lsl #48
    msr  par_el1, x12

    adr  x3, el0_at_s1e0r_entry
    msr  elr_el1, x3
    movz x4, #0x0280               // D=1, I=1.
    movk x4, #0xA040, lsl #16      // NZCV=1010, PAN=1, EL0t.
    msr  spsr_el1, x4
    eret

after_el0_at_s1e0r:
    ldr  x13, [x11]
    movz x14, #1
    cmp  x13, x14
    b.ne fail

    adr  x3, el0_at_s1e0w_entry
    msr  elr_el1, x3
    movz x4, #0x0140               // A=1, F=1.
    movk x4, #0x5040, lsl #16      // NZCV=0101, PAN=1, EL0t.
    msr  spsr_el1, x4
    eret

after_el0_at_s1e0w:
    ldr  x13, [x11]
    movz x14, #3
    cmp  x13, x14
    b.ne fail

    movz x0, #'U'
    str  x0, [x1]
    movz x0, #'\n'
    str  x0, [x1]
    b    exit_ok

el0_at_s1e0r_entry:
    movz x9, #0x1111
    movk x9, #0x2222, lsl #16
    movk x9, #0x3333, lsl #32
    movk x9, #0x4444, lsl #48
el0_at_s1e0r_insn:
    at   s1e0r, x9
    b    fail

el0_at_s1e0w_entry:
    movz x10, #0xAAAA
    movk x10, #0xBBBB, lsl #16
    movk x10, #0xCCCC, lsl #32
    movk x10, #0xDDDD, lsl #48
el0_at_s1e0w_insn:
    at   s1e0w, x10
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
    .space 0x400
sync_lower:
    b lower_sync_handler

lower_sync_handler:
    mrs  x20, esr_el1
    ubfx x21, x20, #26, #6
    cbnz x21, fail_from_handler
    tbz  x20, #25, fail_from_handler
    and  x22, x20, #0x1ffffff
    cbnz x22, fail_from_handler

    mrs  x23, far_el1
    cbnz x23, fail_from_handler
    mrs  x24, elr_el1

    adr  x25, el0_at_s1e0r_insn
    cmp  x24, x25
    b.eq handle_s1e0r
    adr  x25, el0_at_s1e0w_insn
    cmp  x24, x25
    b.eq handle_s1e0w
    b fail_from_handler

handle_s1e0r:
    movz x26, #0x1111
    movk x26, #0x2222, lsl #16
    movk x26, #0x3333, lsl #32
    movk x26, #0x4444, lsl #48
    cmp  x9, x26
    b.ne fail_from_handler
    movz x27, #0xCAFE
    movk x27, #0xBABE, lsl #16
    movk x27, #0x7654, lsl #32
    movk x27, #0x3210, lsl #48
    mrs  x28, par_el1
    cmp  x28, x27
    b.ne fail_from_handler
    mrs  x29, spsr_el1
    and  x30, x29, #0xf
    cbnz x30, fail_from_handler
    tbnz x29, #20, fail_from_handler
    ubfx x30, x29, #28, #4
    movz x26, #0xA
    cmp  x30, x26
    b.ne fail_from_handler
    ubfx x30, x29, #6, #4
    movz x26, #0xA
    cmp  x30, x26
    b.ne fail_from_handler
    tbz  x29, #22, fail_from_handler
    adr  x26, handled
    movz x27, #1
    str  x27, [x26]
    adr  x28, after_el0_at_s1e0r
    b return_to_el1

handle_s1e0w:
    movz x26, #0xAAAA
    movk x26, #0xBBBB, lsl #16
    movk x26, #0xCCCC, lsl #32
    movk x26, #0xDDDD, lsl #48
    cmp  x10, x26
    b.ne fail_from_handler
    movz x27, #0xCAFE
    movk x27, #0xBABE, lsl #16
    movk x27, #0x7654, lsl #32
    movk x27, #0x3210, lsl #48
    mrs  x28, par_el1
    cmp  x28, x27
    b.ne fail_from_handler
    mrs  x29, spsr_el1
    and  x30, x29, #0xf
    cbnz x30, fail_from_handler
    tbnz x29, #20, fail_from_handler
    ubfx x30, x29, #28, #4
    movz x26, #0x5
    cmp  x30, x26
    b.ne fail_from_handler
    ubfx x30, x29, #6, #4
    movz x26, #0x5
    cmp  x30, x26
    b.ne fail_from_handler
    tbz  x29, #22, fail_from_handler
    adr  x26, handled
    ldr  x27, [x26]
    orr  x27, x27, #2
    str  x27, [x26]
    adr  x28, after_el0_at_s1e0w

return_to_el1:
    movz x29, #0x5                // EL1h
    msr  spsr_el1, x29
    msr  elr_el1, x28
    eret

fail_from_handler:
    adr  x28, fail
    movz x29, #0x5
    msr  spsr_el1, x29
    msr  elr_el1, x28
    eret

    .data
    .balign 8
handled:
    .quad 0
