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

    // Software step is inactive while the OS Lock or OS Double Lock is active.
    msr  oslar_el1, xzr
    msr  osdlr_el1, xzr
    msr  dbgprcr_el1, xzr
    isb

    adr  x3, handled
    str  xzr, [x3]

    // Case 1: enabling SS while debug exceptions are already enabled enters the
    // active-pending state, so the next instruction is not executed.
    movz x20, #0
    msr  daifset, #8
    movz x0, #0x2001               // KDE|SS
    msr  mdscr_el1, x0
    msr  daifclr, #8
case1_step_target:
    movz x20, #0x1234
after_case1:
    ldr  x4, [x3]
    movz x5, #1
    cmp  x4, x5
    b.ne fail1
    movz x5, #0x1234
    cmp  x20, x5
    b.ne fail2

    // Case 2: ERET to EL0 with SPSR_EL1.SS=1 executes one EL0 instruction,
    // then takes a Software Step exception before the next instruction.
    str  xzr, [x3]
    movz x5, #0
    movz x6, #0
    movz x0, #1                    // SS
    msr  mdscr_el1, x0
    msr  daifset, #8
    adr  x4, el0_case2_entry
    msr  elr_el1, x4
    movz x4, #0
    movk x4, #0x20, lsl #16        // SPSR_EL1.SS=1, EL0t
    msr  spsr_el1, x4
    eret
after_case2:
    ldr  x4, [x3]
    movz x7, #3
    cmp  x4, x7
    b.ne fail3
    movz x7, #0x55
    cmp  x5, x7
    b.ne fail4
    movz x7, #0x66
    cmp  x6, x7
    b.ne fail5

    // Case 3: stepping an SVC takes the SVC exception, and SPSR_EL1.SS must be
    // cleared because the preferred return is the instruction after SVC.
    str  xzr, [x3]
    movz x0, #1                    // SS
    msr  mdscr_el1, x0
    msr  daifset, #8
    adr  x4, el0_case3_entry
    msr  elr_el1, x4
    movz x4, #0
    movk x4, #0x20, lsl #16        // SPSR_EL1.SS=1, EL0t
    msr  spsr_el1, x4
    eret
after_case3:
    ldr  x4, [x3]
    movz x7, #4
    cmp  x4, x7
    b.ne fail6

    // Case 4: stepping a BRK takes the Breakpoint Instruction exception, and
    // SPSR_EL1.SS remains set because the preferred return address is the BRK.
    str  xzr, [x3]
    movz x0, #1                    // SS
    msr  mdscr_el1, x0
    msr  daifset, #8
    adr  x4, el0_case4_entry
    msr  elr_el1, x4
    movz x4, #0
    movk x4, #0x20, lsl #16        // SPSR_EL1.SS=1, EL0t
    msr  spsr_el1, x4
    eret
after_case4:
    ldr  x4, [x3]
    movz x7, #5
    cmp  x4, x7
    b.ne fail7

    movz x0, #'T'
    str  x0, [x1]
    movz x0, #'\n'
    str  x0, [x1]
1:
    b    1b

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
1:
    b    1b

el0_case2_entry:
    movz x5, #0x55
el0_case2_step_target:
    movz x6, #0x66
    svc  #0x11
el0_case2_after_svc:
    brk  #0

el0_case3_entry:
    svc  #0x123
el0_case3_after_svc:
    brk  #0

el0_case4_entry:
el0_case4_brk:
    brk  #0x456
    b    fail7

    .balign 8
handled:
    .quad 0

    .balign 2048
vectors:
    .space 0x200
sync_spx:
    b sync_spx_handler
    .space 0x1fc
sync_lower:
    b sync_lower_handler

sync_spx_handler:
    mrs  x24, esr_el1
    mrs  x25, elr_el1
    mrs  x26, spsr_el1

    adr  x27, case1_step_target
    cmp  x25, x27
    b.ne failF

    mov  x28, x24
    CHECK_EC x28, 0x33, fail7
    and  x28, x24, #0x1ffffff
    movz x29, #0x22
    cmp  x28, x29
    b.ne fail8
    tbnz x26, #21, fail9
    cbnz x20, failA

    adr  x27, handled
    movz x28, #1
    str  x28, [x27]
    msr  mdscr_el1, xzr
    eret

sync_lower_handler:
    mrs  x24, esr_el1
    mrs  x25, elr_el1
    mrs  x26, spsr_el1

    adr  x27, el0_case2_step_target
    cmp  x25, x27
    b.eq handle_case2_step

    adr  x27, el0_case2_after_svc
    cmp  x25, x27
    b.eq handle_case2_svc

    adr  x27, el0_case3_after_svc
    cmp  x25, x27
    b.eq handle_case3_svc

    adr  x27, el0_case4_brk
    cmp  x25, x27
    b.eq handle_case4_brk

    b failF

handle_case2_step:
    mov  x28, x24
    CHECK_EC x28, 0x32, failB
    tbz  x24, #24, failC
    tbnz x24, #6, failD
    and  x28, x24, #0x3f
    movz x29, #0x22
    cmp  x28, x29
    b.ne failE
    tbnz x26, #21, failF
    movz x28, #0x55
    cmp  x5, x28
    b.ne fail7
    cbnz x6, fail8

    adr  x27, handled
    movz x28, #2
    str  x28, [x27]
    msr  mdscr_el1, xzr
    eret

handle_case2_svc:
    mov  x28, x24
    CHECK_EC x28, 0x15, fail9
    and  x28, x24, #0xffff
    movz x29, #0x11
    cmp  x28, x29
    b.ne failA

    adr  x27, handled
    movz x28, #3
    str  x28, [x27]
    msr  mdscr_el1, xzr
    adr  x28, after_case2
    msr  elr_el1, x28
    movz x28, #0x5               // EL1h
    msr  spsr_el1, x28
    eret

handle_case3_svc:
    mov  x28, x24
    CHECK_EC x28, 0x15, failB
    and  x28, x24, #0xffff
    movz x29, #0x123
    cmp  x28, x29
    b.ne failC
    tbnz x26, #21, failD

    adr  x27, handled
    movz x28, #4
    str  x28, [x27]
    msr  mdscr_el1, xzr
    adr  x28, after_case3
    msr  elr_el1, x28
    movz x28, #0x5               // EL1h
    msr  spsr_el1, x28
    eret

handle_case4_brk:
    mov  x28, x24
    CHECK_EC x28, 0x3c, failE
    and  x28, x24, #0xffff
    movz x29, #0x456
    cmp  x28, x29
    b.ne failF
    tbz  x26, #21, failA

    adr  x27, handled
    movz x28, #5
    str  x28, [x27]
    msr  mdscr_el1, xzr
    adr  x28, after_case4
    msr  elr_el1, x28
    movz x28, #0x5               // EL1h
    msr  spsr_el1, x28
    eret
