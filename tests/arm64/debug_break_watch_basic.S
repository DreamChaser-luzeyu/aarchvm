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

    // Case 1: EL1 breakpoint exception must be taken before executing the
    // matching instruction.
    movz x20, #0
    adr  x4, el1_break_insn
    msr  dbgbvr0_el1, x4
    movz x5, #0x1e3               // BAS=0xf, execute, EL1, enabled
    msr  dbgbcr0_el1, x5
    movz x5, #0xa000              // KDE|MDE
    msr  mdscr_el1, x5
el1_break_insn:
    movz x20, #0x1234
after_el1_break:
    ldr  x4, [x3]
    movz x5, #1
    cmp  x4, x5
    b.ne fail1
    cbnz x20, fail2

    // Case 2: EL1 load watchpoint must fault before the load updates Rt.
    movz x21, #0
    adr  x4, watch_load_data
    msr  dbgwvr0_el1, x4
    movz x5, #0x1feb              // BAS=0xff, load, EL1, enabled
    msr  dbgwcr0_el1, x5
    movz x5, #0xa000              // KDE|MDE
    msr  mdscr_el1, x5
    adr  x22, watch_load_data
el1_watch_load_insn:
    ldr  x21, [x22]
after_el1_watch_load:
    ldr  x4, [x3]
    movz x5, #2
    cmp  x4, x5
    b.ne fail3
    cbnz x21, fail4

    // Case 3: EL1 store watchpoint must fault before memory is updated.
    adr  x4, watch_store_data
    ldr  x23, [x4]
    msr  dbgwvr0_el1, x4
    movz x5, #0x1ff3              // BAS=0xff, store, EL1, enabled
    msr  dbgwcr0_el1, x5
    movz x5, #0xa000              // KDE|MDE
    msr  mdscr_el1, x5
    movz x24, #0xabcd
    movk x24, #0xef01, lsl #16
    movk x24, #0x2345, lsl #32
    movk x24, #0x6789, lsl #48
    adr  x25, watch_store_data
el1_watch_store_insn:
    str  x24, [x25]
after_el1_watch_store:
    ldr  x4, [x3]
    movz x5, #3
    cmp  x4, x5
    b.ne fail5
    adr  x4, watch_store_data
    ldr  x5, [x4]
    cmp  x5, x23
    b.ne fail6

    // Case 4: EL0 breakpoint must be reported with EC=0x30 and leave the
    // trapped instruction unexecuted.
    movz x26, #0
    adr  x4, el0_break_insn
    msr  dbgbvr0_el1, x4
    movz x5, #0x1e5               // BAS=0xf, execute, EL0, enabled
    msr  dbgbcr0_el1, x5
    movz x5, #0x8000              // MDE
    msr  mdscr_el1, x5
    adr  x4, el0_break_entry
    msr  elr_el1, x4
    msr  spsr_el1, xzr            // EL0t
    eret
after_el0_break:
    ldr  x4, [x3]
    movz x5, #4
    cmp  x4, x5
    b.ne fail7
    cbnz x26, fail8

    // Case 5: EL0 store watchpoint must be reported with EC=0x34 and prevent
    // the store from committing.
    adr  x4, watch_el0_data
    ldr  x27, [x4]
    msr  dbgwvr0_el1, x4
    movz x5, #0x1ff5              // BAS=0xff, store, EL0, enabled
    msr  dbgwcr0_el1, x5
    movz x5, #0x8000              // MDE
    msr  mdscr_el1, x5
    adr  x4, el0_watch_store_entry
    msr  elr_el1, x4
    msr  spsr_el1, xzr            // EL0t
    eret
after_el0_watch_store:
    ldr  x4, [x3]
    movz x5, #5
    cmp  x4, x5
    b.ne fail9
    adr  x4, watch_el0_data
    ldr  x5, [x4]
    cmp  x5, x27
    b.ne failA

    movz x0, #'Q'
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
failC:
    movz x0, #'C'
    b out
failD:
    movz x0, #'D'
    b out
failE:
    movz x0, #'E'
    b out
failF:
    movz x0, #'F'
    b out

out:
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
watch_store_data:
    .quad 0x8877665544332211
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
    mrs  x12, far_el1

    adr  x13, el1_break_insn
    cmp  x11, x13
    b.eq handle_el1_break
    adr  x13, el1_watch_load_insn
    cmp  x11, x13
    b.eq handle_el1_watch_load
    adr  x13, el1_watch_store_insn
    cmp  x11, x13
    b.eq handle_el1_watch_store
    b failF

handle_el1_break:
    mov  x14, x10
    CHECK_EC x14, 0x31, failB
    and  x14, x10, #0x1ffffff
    movz x15, #0x22
    cmp  x14, x15
    b.ne failC
    cbnz x20, failD
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
    CHECK_EC x14, 0x35, failB
    and  x14, x10, #0x7f
    movz x15, #0x22
    cmp  x14, x15
    b.ne failC
    adr  x14, watch_load_data
    cmp  x12, x14
    b.ne failD
    cbnz x21, failE
    adr  x14, handled
    movz x15, #2
    str  x15, [x14]
    msr  dbgwcr0_el1, xzr
    msr  mdscr_el1, xzr
    adr  x14, after_el1_watch_load
    msr  elr_el1, x14
    eret

handle_el1_watch_store:
    mov  x14, x10
    CHECK_EC x14, 0x35, failB
    and  x14, x10, #0x7f
    movz x15, #0x62
    cmp  x14, x15
    b.ne failC
    adr  x14, watch_store_data
    cmp  x12, x14
    b.ne failD
    adr  x14, handled
    movz x15, #3
    str  x15, [x14]
    msr  dbgwcr0_el1, xzr
    msr  mdscr_el1, xzr
    adr  x14, after_el1_watch_store
    msr  elr_el1, x14
    eret

sync_lower_handler:
    mrs  x10, esr_el1
    mrs  x11, elr_el1
    mrs  x12, far_el1

    adr  x13, el0_break_insn
    cmp  x11, x13
    b.eq handle_el0_break
    adr  x13, el0_watch_store_insn
    cmp  x11, x13
    b.eq handle_el0_watch_store
    b failF

handle_el0_break:
    mov  x14, x10
    CHECK_EC x14, 0x30, failB
    and  x14, x10, #0x1ffffff
    movz x15, #0x22
    cmp  x14, x15
    b.ne failC
    cbnz x26, failD
    adr  x14, handled
    movz x15, #4
    str  x15, [x14]
    msr  dbgbcr0_el1, xzr
    msr  mdscr_el1, xzr
    adr  x14, after_el0_break
    msr  elr_el1, x14
    movz x14, #0x5               // EL1h
    msr  spsr_el1, x14
    eret

handle_el0_watch_store:
    mov  x14, x10
    CHECK_EC x14, 0x34, failB
    and  x14, x10, #0x7f
    movz x15, #0x62
    cmp  x14, x15
    b.ne failC
    adr  x14, watch_el0_data
    cmp  x12, x14
    b.ne failD
    adr  x14, handled
    movz x15, #5
    str  x15, [x14]
    msr  dbgwcr0_el1, xzr
    msr  mdscr_el1, xzr
    adr  x14, after_el0_watch_store
    msr  elr_el1, x14
    movz x14, #0x5               // EL1h
    msr  spsr_el1, x14
    eret
