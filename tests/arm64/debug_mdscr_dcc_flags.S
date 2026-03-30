    .text
    .global _start

    .macro LOAD64 reg, imm0, imm1, imm2, imm3
    movz \reg, #\imm0
    movk \reg, #\imm1, lsl #16
    movk \reg, #\imm2, lsl #32
    movk \reg, #\imm3, lsl #48
    .endm

_start:
    movz x1, #0x0000
    movk x1, #0x0900, lsl #16

    mrs  x2, mdccsr_el0
    cbnz x2, fail1
    mrs  x2, mdscr_el1
    tbnz x2, #30, fail2
    tbnz x2, #29, fail2

    LOAD64 x0, 0x0000, 0x6000, 0x0000, 0x0000
    msr  mdscr_el1, x0
    mrs  x2, mdccsr_el0
    LOAD64 x3, 0x0000, 0x6000, 0x0000, 0x0000
    cmp  x2, x3
    b.ne fail3
    mrs  x2, mdscr_el1
    tbz  x2, #30, fail4
    tbz  x2, #29, fail4

    msr  mdscr_el1, xzr
    mrs  x2, mdccsr_el0
    cbnz x2, fail5
    mrs  x2, mdscr_el1
    tbnz x2, #30, fail6
    tbnz x2, #29, fail6

    msr  oslar_el1, xzr
    LOAD64 x0, 0x0d0c, 0x0b0a, 0x3333, 0x4444
    msr  dbgdtrtx_el0, x0
    mrs  x2, mdccsr_el0
    tbz  x2, #29, fail7
    tbnz x2, #30, fail7
    mrs  x2, mdscr_el1
    tbz  x2, #29, fail8
    tbnz x2, #30, fail8

    msr  mdscr_el1, xzr
    mrs  x2, mdccsr_el0
    tbz  x2, #29, fail9
    tbnz x2, #30, fail9
    mrs  x2, mdscr_el1
    tbz  x2, #29, failA
    tbnz x2, #30, failA

    movz x0, #1
    msr  oslar_el1, x0
    msr  mdscr_el1, xzr
    mrs  x2, mdccsr_el0
    cbnz x2, failB
    mrs  x2, mdscr_el1
    tbnz x2, #30, failC
    tbnz x2, #29, failC

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

out:
    str  x0, [x1]
    brk  #0
