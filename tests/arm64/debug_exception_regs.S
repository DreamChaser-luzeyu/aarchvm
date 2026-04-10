    .arch armv8.1-a
    .text
    .global _start

    .macro CHECK_EC reg, value, fail_label
    ubfx \reg, \reg, #26, #6
    movz x30, #\value
    cmp  \reg, x30
    b.ne \fail_label
    .endm

_start:
    movz x1, #0x0000
    movk x1, #0x0900, lsl #16      // UART

    adr  x2, vectors
    msr  vbar_el1, x2

    // Breakpoint/watchpoint exceptions require the OS Lock to be unlocked and
    // the OS Double Lock to be inactive.
    msr  oslar_el1, xzr
    msr  osdlr_el1, xzr
    msr  dbgprcr_el1, xzr
    isb

    adr  x3, handled
    str  xzr, [x3]

    // Case 0: At EL1, PSTATE.D masks hardware breakpoint exceptions even when
    // KDE|MDE are set, so the matching instruction must execute normally.
    movz x19, #0
    msr  daifclr, #0xf
    msr  daifset, #0x8             // D=1
    adr  x4, el1_break_masked_insn
    msr  dbgbvr0_el1, x4
    movz x5, #0x1e3               // BAS=0xf, execute, EL1, enabled
    msr  dbgbcr0_el1, x5
    movz x5, #0xa000              // KDE|MDE
    msr  mdscr_el1, x5
    isb
el1_break_masked_insn:
    movz x19, #0x1111
after_el1_break_masked:
    ldr  x4, [x3]
    cbnz x4, fail
    movz x5, #0x1111
    cmp  x19, x5
    b.ne fail
    msr  dbgbcr0_el1, xzr
    msr  mdscr_el1, xzr
    isb

    // Case 1: EL1 breakpoint with explicit SPSR_EL1/FAR_EL1/ELR_EL1 checks.
    movz x0, #0xa000, lsl #16      // NZCV=1010
    msr  nzcv, x0
    msr  daifclr, #0xf
    msr  daifset, #0x5             // A=1, F=1
    movz x0, #1
    msr  pan, x0
    movz x20, #0
    adr  x4, el1_break_insn
    msr  dbgbvr0_el1, x4
    movz x5, #0x1e3               // BAS=0xf, execute, EL1, enabled
    msr  dbgbcr0_el1, x5
    movz x5, #0xa000              // KDE|MDE
    msr  mdscr_el1, x5
    isb
el1_break_insn:
    movz x20, #0x1234
after_el1_break:
    ldr  x4, [x3]
    cmp  x4, #1
    b.ne fail
    cbnz x20, fail

    // Case 2: EL1 load watchpoint with a different saved PSTATE.
    movz x0, #0x3000, lsl #16      // NZCV=0011
    msr  nzcv, x0
    msr  daifclr, #0xf
    msr  daifset, #0x6             // A=1, I=1
    msr  pan, xzr
    movz x21, #0
    adr  x4, watch_load_data
    msr  dbgwvr0_el1, x4
    movz x5, #0x1feb              // BAS=0xff, load, EL1, enabled
    msr  dbgwcr0_el1, x5
    movz x5, #0xa000              // KDE|MDE
    msr  mdscr_el1, x5
    isb
    adr  x22, watch_load_data
el1_watch_load_insn:
    ldr  x21, [x22]
after_el1_watch_load:
    ldr  x4, [x3]
    cmp  x4, #2
    b.ne fail
    cbnz x21, fail

    // Case 3: EL0 breakpoint must save the programmed EL0 PSTATE.
    movz x26, #0
    adr  x4, el0_break_insn
    msr  dbgbvr0_el1, x4
    movz x5, #0x1e5               // BAS=0xf, execute, EL0, enabled
    msr  dbgbcr0_el1, x5
    movz x5, #0x8000              // MDE
    msr  mdscr_el1, x5
    adr  x4, el0_break_entry
    msr  elr_el1, x4
    movz x4, #0x01c0              // A=1, I=1, F=1
    movk x4, #0xc040, lsl #16     // NZCV=1100, PAN=1, EL0t
    msr  spsr_el1, x4
    eret
after_el0_break:
    ldr  x4, [x3]
    cmp  x4, #3
    b.ne fail
    cbnz x26, fail

    // Case 4: EL0 store watchpoint must also preserve the programmed PSTATE.
    adr  x4, watch_el0_data
    ldr  x27, [x4]
    msr  dbgwvr0_el1, x4
    movz x5, #0x1ff5              // BAS=0xff, store, EL0, enabled
    msr  dbgwcr0_el1, x5
    movz x5, #0x8000              // MDE
    msr  mdscr_el1, x5
    adr  x4, el0_watch_store_entry
    msr  elr_el1, x4
    movz x4, #0x0280              // D=1, I=1
    movk x4, #0x2040, lsl #16     // NZCV=0010, PAN=1, EL0t
    msr  spsr_el1, x4
    eret
after_el0_watch_store:
    ldr  x4, [x3]
    cmp  x4, #4
    b.ne fail
    adr  x4, watch_el0_data
    ldr  x5, [x4]
    cmp  x5, x27
    b.ne fail

    movz x0, #'G'
    str  x0, [x1]
    movz x0, #'\n'
    str  x0, [x1]
    brk  #0

fail:
    movz x0, #'F'
    str  x0, [x1]
    brk  #0

el0_break_entry:
el0_break_insn:
    movz x26, #0x9999
    brk  #0

el0_watch_store_entry:
    movz x28, #0x1111
    movk x28, #0x2222, lsl #16
    movk x28, #0x3333, lsl #32
    movk x28, #0x4444, lsl #48
    adr  x29, watch_el0_data
el0_watch_store_insn:
    str  x28, [x29]
    brk  #0

    .balign 8
handled:
    .quad 0
watch_load_data:
    .quad 0x1122334455667788
watch_el0_data:
    .quad 0xdeadbeef24681357

    .balign 2048
vectors:
    .space 0x200
sync_spx:
    b sync_spx_handler
    .space 0x1fc
sync_lower:
    b sync_lower_handler

sync_spx_handler:
    mrs  x10, esr_el1
    mrs  x11, elr_el1
    mrs  x12, spsr_el1
    mrs  x13, far_el1

    adr  x14, el1_break_insn
    cmp  x11, x14
    b.eq handle_el1_break
    adr  x14, el1_watch_load_insn
    cmp  x11, x14
    b.eq handle_el1_watch_load
    b fail_from_handler

handle_el1_break:
    mov  x14, x10
    CHECK_EC x14, 0x31, fail_from_handler
    tbz  x10, #25, fail_from_handler
    and  x14, x10, #0x1ffffff
    movz x15, #0x22
    cmp  x14, x15
    b.ne fail_from_handler
    cbnz x13, fail_from_handler
    cbnz x20, fail_from_handler
    and  x14, x12, #0xf
    cmp  x14, #0x5
    b.ne fail_from_handler
    tbnz x12, #20, fail_from_handler
    ubfx x14, x12, #28, #4
    cmp  x14, #0xa
    b.ne fail_from_handler
    ubfx x14, x12, #6, #4
    cmp  x14, #0x5
    b.ne fail_from_handler
    tbz  x12, #22, fail_from_handler
    adr  x14, handled
    movz x15, #1
    str  x15, [x14]
    msr  dbgbcr0_el1, xzr
    msr  mdscr_el1, xzr
    adr  x14, after_el1_break
    msr  elr_el1, x14
    eret

handle_el1_watch_load:
    mov  x14, x10
    CHECK_EC x14, 0x35, fail_from_handler
    tbz  x10, #25, fail_from_handler
    and  x14, x10, #0x7f
    movz x15, #0x22
    cmp  x14, x15
    b.ne fail_from_handler
    adr  x14, watch_load_data
    cmp  x13, x14
    b.ne fail_from_handler
    cbnz x21, fail_from_handler
    and  x14, x12, #0xf
    cmp  x14, #0x5
    b.ne fail_from_handler
    tbnz x12, #20, fail_from_handler
    ubfx x14, x12, #28, #4
    cmp  x14, #0x3
    b.ne fail_from_handler
    ubfx x14, x12, #6, #4
    cmp  x14, #0x6
    b.ne fail_from_handler
    tbnz x12, #22, fail_from_handler
    adr  x14, handled
    movz x15, #2
    str  x15, [x14]
    msr  dbgwcr0_el1, xzr
    msr  mdscr_el1, xzr
    adr  x14, after_el1_watch_load
    msr  elr_el1, x14
    eret

sync_lower_handler:
    mrs  x10, esr_el1
    mrs  x11, elr_el1
    mrs  x12, spsr_el1
    mrs  x13, far_el1

    adr  x14, el0_break_insn
    cmp  x11, x14
    b.eq handle_el0_break
    adr  x14, el0_watch_store_insn
    cmp  x11, x14
    b.eq handle_el0_watch_store
    b fail_from_handler

handle_el0_break:
    mov  x14, x10
    CHECK_EC x14, 0x30, fail_from_handler
    tbz  x10, #25, fail_from_handler
    and  x14, x10, #0x1ffffff
    movz x15, #0x22
    cmp  x14, x15
    b.ne fail_from_handler
    cbnz x13, fail_from_handler
    cbnz x26, fail_from_handler
    and  x14, x12, #0xf
    cbnz x14, fail_from_handler
    tbnz x12, #20, fail_from_handler
    ubfx x14, x12, #28, #4
    cmp  x14, #0xc
    b.ne fail_from_handler
    ubfx x14, x12, #6, #4
    cmp  x14, #0x7
    b.ne fail_from_handler
    tbz  x12, #22, fail_from_handler
    adr  x14, handled
    movz x15, #3
    str  x15, [x14]
    msr  dbgbcr0_el1, xzr
    msr  mdscr_el1, xzr
    adr  x14, after_el0_break
    movz x15, #0x5
    msr  spsr_el1, x15
    msr  elr_el1, x14
    eret

handle_el0_watch_store:
    mov  x14, x10
    CHECK_EC x14, 0x34, fail_from_handler
    tbz  x10, #25, fail_from_handler
    and  x14, x10, #0x7f
    movz x15, #0x62
    cmp  x14, x15
    b.ne fail_from_handler
    adr  x14, watch_el0_data
    cmp  x13, x14
    b.ne fail_from_handler
    and  x14, x12, #0xf
    cbnz x14, fail_from_handler
    tbnz x12, #20, fail_from_handler
    ubfx x14, x12, #28, #4
    cmp  x14, #0x2
    b.ne fail_from_handler
    ubfx x14, x12, #6, #4
    cmp  x14, #0xa
    b.ne fail_from_handler
    tbz  x12, #22, fail_from_handler
    adr  x14, handled
    movz x15, #4
    str  x15, [x14]
    msr  dbgwcr0_el1, xzr
    msr  mdscr_el1, xzr
    adr  x14, after_el0_watch_store
    movz x15, #0x5
    msr  spsr_el1, x15
    msr  elr_el1, x14
    eret

fail_from_handler:
    adr  x14, fail
    movz x15, #0x5
    msr  spsr_el1, x15
    msr  elr_el1, x14
    eret
