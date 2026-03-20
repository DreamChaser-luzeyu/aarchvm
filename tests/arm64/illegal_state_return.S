    .text
    .global _start
_start:
    movz x20, #0x0000
    movk x20, #0x0900, lsl #16      // UART

    adr  x10, vectors
    msr  vbar_el1, x10

    adr  x11, current_case
    str  xzr, [x11]
    adr  x11, executed1
    str  xzr, [x11]
    adr  x11, executed2
    str  xzr, [x11]

    // Case 1: illegal ERET from EL1 with reserved SPSR.M value must not
    // execute the target instruction. Instead it sets PSTATE.IL and the next
    // instruction attempt raises an Illegal State exception at the current EL.
    adr  x11, current_case
    movz x12, #1
    str  x12, [x11]
    adr  x0, illegal_target_el1
    msr  elr_el1, x0
    movz x0, #0x2                   // Reserved AArch64 SPSR.M value.
    msr  spsr_el1, x0
    eret

after_case1:
    adr  x11, executed1
    ldr  x12, [x11]
    cbnz x12, fail1

    // Case 2: legal return with SPSR.IL=1 must also trigger Illegal State on
    // the first target instruction, this time from lower EL.
    adr  x11, current_case
    movz x12, #2
    str  x12, [x11]
    adr  x0, illegal_target_el0
    msr  elr_el1, x0
    movz x0, #0x10, lsl #16         // SPSR_EL1.IL=1, EL0t.
    msr  spsr_el1, x0
    eret

after_case2:
    adr  x11, executed2
    ldr  x12, [x11]
    cbnz x12, fail2

pass:
    movz x0, #'I'
    str  x0, [x20]
    movz x0, #'\n'
    str  x0, [x20]
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
out:
    str  x0, [x20]
    brk  #0

illegal_target_el1:
    adr  x0, executed1
    movz x1, #1
    str  x1, [x0]
    svc  #0x41
    b    fail3

illegal_target_el0:
    adr  x0, executed2
    movz x1, #1
    str  x1, [x0]
    svc  #0x42
    b    fail4

    .balign 2048
vectors:
    b fail5
    .space 0x1fc
sync_spx:
    b sync_spx_handler
    .space 0x1fc
sync_lower:
    b sync_lower_handler

sync_spx_handler:
    adr  x9, current_case
    ldr  x9, [x9]
    cmp  x9, #1
    b.ne fail6

    mrs  x0, esr_el1
    tbz  x0, #25, fail6
    ubfx x1, x0, #26, #6
    cmp  x1, #0x0e
    b.ne fail6

    mrs  x2, elr_el1
    adr  x3, illegal_target_el1
    cmp  x2, x3
    b.ne fail6

    mrs  x4, spsr_el1
    tbz  x4, #20, fail6
    and  x5, x4, #0xf
    cmp  x5, #0x5
    b.ne fail6

    adr  x6, executed1
    ldr  x7, [x6]
    cbnz x7, fail6

    adr  x8, after_case1
    movz x9, #0x5
    msr  elr_el1, x8
    msr  spsr_el1, x9
    eret

sync_lower_handler:
    adr  x9, current_case
    ldr  x9, [x9]
    cmp  x9, #2
    b.ne fail6

    mrs  x0, esr_el1
    tbz  x0, #25, fail6
    ubfx x1, x0, #26, #6
    cmp  x1, #0x0e
    b.ne fail6

    mrs  x2, elr_el1
    adr  x3, illegal_target_el0
    cmp  x2, x3
    b.ne fail6

    mrs  x4, spsr_el1
    tbz  x4, #20, fail6
    and  x5, x4, #0xf
    cbnz x5, fail6

    adr  x6, executed2
    ldr  x7, [x6]
    cbnz x7, fail6

    adr  x8, after_case2
    movz x9, #0x5
    msr  elr_el1, x8
    msr  spsr_el1, x9
    eret

    .data
    .balign 8
current_case:
    .quad 0
executed1:
    .quad 0
executed2:
    .quad 0
