    .text
    .global _start
_start:
    movz x20, #0x0000
    movk x20, #0x0900, lsl #16      // UART

    adr  x10, vectors
    msr  vbar_el1, x10

    adr  x11, current_case
    stp  xzr, xzr, [x11]
    str  xzr, [x11, #16]

    // Case 1: EL1 BR to a misaligned target must take a same-EL PC alignment
    // fault before any target instruction executes.
    adr  x11, current_case
    movz x12, #1
    str  x12, [x11]
    adr  x0, case1_target
    add  x0, x0, #2
    br   x0

after_case1:
    adr  x11, executed1
    ldr  x12, [x11]
    cbnz x12, fail1

    // Case 2: if ERET returns to EL0 with both IL=1 and a misaligned PC, PC
    // alignment fault still has priority over Illegal Execution state.
    adr  x11, current_case
    movz x12, #2
    str  x12, [x11]
    adr  x0, case2_target
    add  x0, x0, #2
    msr  elr_el1, x0
    movz x1, #0x0280                // D=1, I=1.
    movk x1, #0x5050, lsl #16       // NZCV=0101, PAN=1, IL=1, EL0t.
    msr  spsr_el1, x1
    eret

after_case2:
    adr  x11, executed2
    ldr  x12, [x11]
    cbnz x12, fail2

    movz x0, #'P'
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
faila:
    movz x0, #'A'
    b out
failb:
    movz x0, #'B'
    b out

out:
    str  x0, [x20]
    brk  #0

case1_target:
    adr  x0, executed1
    movz x1, #1
    str  x1, [x0]
    svc  #0x11
    b    fail3

case2_target:
    adr  x0, executed2
    movz x1, #1
    str  x1, [x0]
    svc  #0x12
    b    fail4

    .balign 2048
vectors:
    b .
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
    b.ne fail5

    mrs  x0, esr_el1
    ubfx x1, x0, #26, #6
    cmp  x1, #0x22
    b.ne fail5

    mrs  x2, far_el1
    mrs  x3, elr_el1
    adr  x4, case1_target
    add  x4, x4, #2
    cmp  x2, x4
    b.ne fail6
    cmp  x3, x4
    b.ne fail7

    adr  x6, executed1
    ldr  x7, [x6]
    cbnz x7, fail8

    adr  x8, after_case1
    movz x9, #0x5                  // EL1h
    msr  elr_el1, x8
    msr  spsr_el1, x9
    eret

sync_lower_handler:
    adr  x9, current_case
    ldr  x9, [x9]
    cmp  x9, #2
    b.ne fail5

    mrs  x0, esr_el1
    ubfx x1, x0, #26, #6
    cmp  x1, #0x22
    b.ne fail9

    mrs  x2, far_el1
    mrs  x3, elr_el1
    adr  x4, case2_target
    add  x4, x4, #2
    cmp  x2, x4
    b.ne faila
    cmp  x3, x4
    b.ne failb

    mrs  x5, spsr_el1
    and  x6, x5, #0xf
    cbnz x6, fail9
    tbz  x5, #20, fail9
    tbz  x5, #22, fail9
    ubfx x6, x5, #28, #4
    cmp  x6, #0x5
    b.ne fail9
    ubfx x6, x5, #6, #4
    cmp  x6, #0xa
    b.ne fail9

    adr  x6, executed2
    ldr  x7, [x6]
    cbnz x7, fail9

    adr  x8, after_case2
    movz x9, #0x5                  // EL1h
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
