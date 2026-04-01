    .arch armv8.1-a
    .text
    .global _start
_start:
    movz x1, #0x0000
    movk x1, #0x0900, lsl #16      // UART

    movz x0, #1
    msr  spsel, x0                 // EL1h so SP_EL0 is directly accessible

    movz x20, #0x9000
    mov  sp, x20

    movz x2, #0x1357
    movk x2, #0x2468, lsl #16
    movk x2, #0x369c, lsl #32
    movk x2, #0x48ad, lsl #48
    msr  tpidr_el0, x2
    mrs  x3, tpidr_el0
    cmp  x3, x2
    b.ne fail1

    movz x4, #0xabcd
    movk x4, #0xef01, lsl #16
    movk x4, #0x2345, lsl #32
    movk x4, #0x6789, lsl #48
    msr  sp_el0, x4
    mrs  x5, sp_el0
    cmp  x5, x4
    b.ne fail2

    // MRS with Rt==31 must discard the result instead of writing SP.
    mrs  xzr, tpidr_el0
    mov  x6, sp
    cmp  x6, x20
    b.ne fail3

    mrs  xzr, sp_el0
    mov  x6, sp
    cmp  x6, x20
    b.ne fail4

    // MSR with Rt==31 must source zero, not the current SP value.
    msr  tpidr_el0, xzr
    mrs  x7, tpidr_el0
    cbnz x7, fail5
    mov  x6, sp
    cmp  x6, x20
    b.ne fail6

    msr  sp_el0, xzr
    mrs  x8, sp_el0
    cbnz x8, fail7
    mov  x6, sp
    cmp  x6, x20
    b.ne fail8

    movz x0, #'Z'
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

out:
    str  x0, [x1]
    brk  #0
