    .text
    .global _start

    .macro LOAD64 reg, imm0, imm1, imm2, imm3
    movz \reg, #\imm0
    movk \reg, #\imm1, lsl #16
    movk \reg, #\imm2, lsl #32
    movk \reg, #\imm3, lsl #48
    .endm

    .macro CHECK_STATE value, fail_label
    adr  x11, handled
    ldr  x12, [x11]
    movz x13, #\value
    cmp  x12, x13
    b.ne \fail_label
    .endm

_start:
    movz x1, #0x0000
    movk x1, #0x0900, lsl #16

    adr  x10, vectors
    msr  vbar_el1, x10

    adr  x11, handled
    str  xzr, [x11]

    mrs  x2, dbgprcr_el1
    cbnz x2, fail1

    LOAD64 x0, 0xffff, 0xffff, 0xffff, 0xffff
    msr  dbgprcr_el1, x0
    mrs  x2, dbgprcr_el1
    movz x3, #1
    cmp  x2, x3
    b.ne fail2

    msr  dbgprcr_el1, xzr
    mrs  x2, dbgprcr_el1
    cbnz x2, fail3

    LOAD64 x0, 0xf040, 0x80ef, 0x0009, 0x0004
    msr  mdscr_el1, x0
    mrs  x2, mdscr_el1
    LOAD64 x3, 0xf040, 0x00e0, 0x0000, 0x0000
    cmp  x2, x3
    b.ne fail4

    msr  oslar_el1, xzr
    msr  mdscr_el1, xzr
    mrs  x2, mdscr_el1
    LOAD64 x3, 0x4040, 0x00e0, 0x0000, 0x0000
    cmp  x2, x3
    b.ne fail5

    movz x0, #1
    msr  oslar_el1, x0

    adr  x3, el0_mrs_mdscr_entry
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret
after_el0_mrs_mdscr:
    CHECK_STATE 1, fail6

    adr  x3, el0_mrs_dbgprcr_entry
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret
after_el0_mrs_dbgprcr:
    CHECK_STATE 2, fail7

    adr  x3, el0_msr_mdscr_entry
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret
after_el0_msr_mdscr:
    CHECK_STATE 3, fail8

    adr  x3, el0_msr_dbgprcr_entry
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret
after_el0_msr_dbgprcr:
    CHECK_STATE 4, fail9
    mrs  x2, dbgprcr_el1
    cbnz x2, failA

    movz x0, #'P'
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

out:
    str  x0, [x1]
    brk  #0

el0_mrs_mdscr_entry:
    LOAD64 x0, 0x1111, 0x2222, 0x3333, 0x4444
el0_mrs_mdscr_insn:
    mrs  x0, mdscr_el1
    svc  #0x11

el0_mrs_dbgprcr_entry:
    LOAD64 x0, 0xaaaa, 0xbbbb, 0xcccc, 0xdddd
el0_mrs_dbgprcr_insn:
    mrs  x0, dbgprcr_el1
    svc  #0x12

el0_msr_mdscr_entry:
    LOAD64 x0, 0x0102, 0x0304, 0x0506, 0x0708
el0_msr_mdscr_insn:
    msr  mdscr_el1, x0
    svc  #0x13

el0_msr_dbgprcr_entry:
    LOAD64 x0, 0x1357, 0x2468, 0x369c, 0x48ad
el0_msr_dbgprcr_insn:
    msr  dbgprcr_el1, x0
    svc  #0x14

    .balign 2048
vectors:
    .space 0x400
sync_lower:
    b lower_sync_handler

lower_sync_handler:
    mrs  x20, esr_el1
    ubfx x21, x20, #26, #6
    cbnz x21, handler_fail

    mrs  x22, elr_el1
    adr  x23, el0_mrs_mdscr_insn
    cmp  x22, x23
    b.eq undef_mrs_mdscr
    adr  x23, el0_mrs_dbgprcr_insn
    cmp  x22, x23
    b.eq undef_mrs_dbgprcr
    adr  x23, el0_msr_mdscr_insn
    cmp  x22, x23
    b.eq undef_msr_mdscr
    adr  x23, el0_msr_dbgprcr_insn
    cmp  x22, x23
    b.eq undef_msr_dbgprcr
    b handler_fail

undef_mrs_mdscr:
    LOAD64 x24, 0x1111, 0x2222, 0x3333, 0x4444
    cmp  x0, x24
    b.ne handler_fail
    adr  x25, handled
    movz x26, #1
    str  x26, [x25]
    adr  x27, after_el0_mrs_mdscr
    b return_to_el1

undef_mrs_dbgprcr:
    LOAD64 x24, 0xaaaa, 0xbbbb, 0xcccc, 0xdddd
    cmp  x0, x24
    b.ne handler_fail
    adr  x25, handled
    movz x26, #2
    str  x26, [x25]
    adr  x27, after_el0_mrs_dbgprcr
    b return_to_el1

undef_msr_mdscr:
    LOAD64 x24, 0x0102, 0x0304, 0x0506, 0x0708
    cmp  x0, x24
    b.ne handler_fail
    adr  x25, handled
    movz x26, #3
    str  x26, [x25]
    adr  x27, after_el0_msr_mdscr
    b return_to_el1

undef_msr_dbgprcr:
    LOAD64 x24, 0x1357, 0x2468, 0x369c, 0x48ad
    cmp  x0, x24
    b.ne handler_fail
    adr  x25, handled
    movz x26, #4
    str  x26, [x25]
    adr  x27, after_el0_msr_dbgprcr

return_to_el1:
    movz x28, #0x5
    msr  spsr_el1, x28
    msr  elr_el1, x27
    eret

handler_fail:
    movz x0, #'F'
    b out

    .data
    .balign 8
handled:
    .quad 0
