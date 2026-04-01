    .text
    .global _start

    .macro LOAD_IMM64 reg, imm0, imm1, imm2, imm3
    movz \reg, #\imm0
    movk \reg, #\imm1, lsl #16
    movk \reg, #\imm2, lsl #32
    movk \reg, #\imm3, lsl #48
    .endm

    .macro CHECK_HANDLED value
    ldr  x13, [x21]
    movz x14, #\value
    cmp  x13, x14
    b.ne fail
    .endm

    .macro CHECK_REG_IMM reg, imm0, imm1, imm2, imm3
    LOAD_IMM64 x26, \imm0, \imm1, \imm2, \imm3
    cmp  \reg, x26
    b.ne fail_from_handler
    .endm

    .macro CHECK_EL0_SPSR nzcv, daif
    mrs  x29, spsr_el1
    and  x30, x29, #0xf
    cbnz x30, fail_from_handler
    tbnz x29, #20, fail_from_handler
    ubfx x30, x29, #28, #4
    movz x26, #\nzcv
    cmp  x30, x26
    b.ne fail_from_handler
    ubfx x30, x29, #6, #4
    movz x26, #\daif
    cmp  x30, x26
    b.ne fail_from_handler
    tbz  x29, #22, fail_from_handler
    .endm

_start:
    movz x1, #0x0000
    movk x1, #0x0900, lsl #16      // UART
    movz x2, #0x0000
    movk x2, #0x0902, lsl #16      // PERF mailbox

    adr  x10, vectors
    msr  vbar_el1, x10

    adr  x21, handled
    str  xzr, [x21]

    LOAD_IMM64 x9, 0x1111, 0x2222, 0x3333, 0x4444
el1_wfet_insn:
    .inst 0xD5031009               // WFET X9
    b    fail
after_el1_wfet:
    CHECK_HANDLED 1

    LOAD_IMM64 x10, 0xAAAA, 0xBBBB, 0xCCCC, 0xDDDD
el1_wfit_insn:
    .inst 0xD503102A               // WFIT X10
    b    fail
after_el1_wfit:
    CHECK_HANDLED 2

    adr  x3, el0_wfet_entry
    msr  elr_el1, x3
    movz x4, #0x0280               // D=1, I=1.
    movk x4, #0xA040, lsl #16      // NZCV=1010, PAN=1, EL0t.
    msr  spsr_el1, x4
    eret
after_el0_wfet:
    CHECK_HANDLED 3

    adr  x3, el0_wfit_entry
    msr  elr_el1, x3
    movz x4, #0x0140               // A=1, F=1.
    movk x4, #0x5040, lsl #16      // NZCV=0101, PAN=1, EL0t.
    msr  spsr_el1, x4
    eret
after_el0_wfit:
    CHECK_HANDLED 4

    movz x0, #'w'
    str  x0, [x1]
    movz x0, #'\n'
    str  x0, [x1]
    b    exit_ok

el0_wfet_entry:
    LOAD_IMM64 x11, 0x1357, 0x2468, 0x369C, 0x48AD
el0_wfet_insn:
    .inst 0xD503100B               // WFET X11
    b    fail

el0_wfit_entry:
    LOAD_IMM64 x12, 0x55AA, 0xAA55, 0x0F0F, 0xF0F0
el0_wfit_insn:
    .inst 0xD503102C               // WFIT X12
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
    ubfx x22, x20, #26, #6
    cbnz x22, fail_from_handler
    tbz  x20, #25, fail_from_handler
    and  x23, x20, #0x1ffffff
    cbnz x23, fail_from_handler

    mrs  x24, far_el1
    cbnz x24, fail_from_handler
    mrs  x25, elr_el1

    adr  x26, el1_wfet_insn
    cmp  x25, x26
    b.eq handle_el1_wfet
    adr  x26, el1_wfit_insn
    cmp  x25, x26
    b.eq handle_el1_wfit
    adr  x26, el0_wfet_insn
    cmp  x25, x26
    b.eq handle_el0_wfet
    adr  x26, el0_wfit_insn
    cmp  x25, x26
    b.eq handle_el0_wfit
    b    fail_from_handler

handle_el1_wfet:
    CHECK_REG_IMM x9, 0x1111, 0x2222, 0x3333, 0x4444
    adr  x27, handled
    movz x28, #1
    str  x28, [x27]
    adr  x30, after_el1_wfet
    msr  elr_el1, x30
    eret

handle_el1_wfit:
    CHECK_REG_IMM x10, 0xAAAA, 0xBBBB, 0xCCCC, 0xDDDD
    adr  x27, handled
    movz x28, #2
    str  x28, [x27]
    adr  x30, after_el1_wfit
    msr  elr_el1, x30
    eret

handle_el0_wfet:
    CHECK_REG_IMM x11, 0x1357, 0x2468, 0x369C, 0x48AD
    CHECK_EL0_SPSR 0xA, 0xA
    adr  x27, handled
    movz x28, #3
    str  x28, [x27]
    adr  x30, after_el0_wfet
    b    return_to_el1

handle_el0_wfit:
    CHECK_REG_IMM x12, 0x55AA, 0xAA55, 0x0F0F, 0xF0F0
    CHECK_EL0_SPSR 0x5, 0x5
    adr  x27, handled
    movz x28, #4
    str  x28, [x27]
    adr  x30, after_el0_wfit

return_to_el1:
    movz x29, #0x5                 // EL1h
    msr  spsr_el1, x29
    msr  elr_el1, x30
    eret

fail_from_handler:
    adr  x30, fail
    movz x29, #0x5
    msr  spsr_el1, x29
    msr  elr_el1, x30
    eret

    .data
    .balign 8
handled:
    .quad 0
