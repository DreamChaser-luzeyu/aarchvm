    .text
    .global _start

    .macro LOAD64 reg, imm0, imm1, imm2, imm3
    movz \reg, #\imm0
    movk \reg, #\imm1, lsl #16
    movk \reg, #\imm2, lsl #32
    movk \reg, #\imm3, lsl #48
    .endm

    .macro CHECK_HANDLED value, fail_label
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

    mrs  x2, mdccsr_el0
    tbnz x2, #30, fail1
    tbnz x2, #29, fail1

    LOAD64 x0, 0x1111, 0x2222, 0x3333, 0x4444
    msr  dbgdtrtx_el0, x0
    mrs  x2, mdccsr_el0
    tbz  x2, #29, fail2
    tbnz x2, #30, fail2

    LOAD64 x0, 0xaaaa, 0xbbbb, 0xcccc, 0xdddd
    msr  dbgdtr_el0, x0
    mrs  x2, mdccsr_el0
    tbz  x2, #29, fail3
    tbnz x2, #30, fail3
    mrs  x3, dbgdtr_el0
    mrs  x3, dbgdtrrx_el0

    adr  x3, el0_mdccsr_entry
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret
after_el0_mdccsr:
    CHECK_HANDLED 1, fail4

    adr  x3, el0_dbgdtrtx_entry
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret
after_el0_dbgdtrtx:
    CHECK_HANDLED 2, fail5

    mrs  x4, mdscr_el1
    movz x5, #0x1000
    orr  x4, x4, x5
    msr  mdscr_el1, x4

    adr  x3, el0_mdccsr_trap_entry
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret
after_el0_mdccsr_trap:
    CHECK_HANDLED 3, fail6

    adr  x3, el0_dbgdtrtx_trap_entry
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret
after_el0_dbgdtrtx_trap:
    CHECK_HANDLED 4, fail7

    movz x0, #'C'
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
fail:
    movz x0, #'F'
    b out

out:
    str  x0, [x1]
    brk  #0

el0_mdccsr_entry:
    mrs  x0, mdccsr_el0
el0_mdccsr_svc:
    svc  #0x11

el0_dbgdtrtx_entry:
    LOAD64 x0, 0x0102, 0x0304, 0x0506, 0x0708
    msr  dbgdtrtx_el0, x0
el0_dbgdtrtx_svc:
    svc  #0x12

el0_mdccsr_trap_entry:
el0_mdccsr_trap_insn:
    mrs  x0, mdccsr_el0
    b    fail

el0_dbgdtrtx_trap_entry:
    LOAD64 x0, 0x1357, 0x2468, 0x369c, 0x48ad
el0_dbgdtrtx_trap_insn:
    msr  dbgdtrtx_el0, x0
    b    fail

    .balign 2048
vectors:
    .space 0x200
sync_spx:
    b sync_handler
    .space 0x1fc
sync_lower:
    b lower_sync_handler

sync_handler:
    movz x0, #'X'
    b out

lower_sync_handler:
    mrs  x20, esr_el1
    ubfx x21, x20, #26, #6
    mrs  x22, elr_el1
    mrs  x23, far_el1
    cbnz x23, fail_from_handler

    cmp  x21, #0x15
    b.eq handle_el0_svc
    cmp  x21, #0x18
    b.eq handle_el0_trap
    b fail_from_handler

handle_el0_svc:
    and  x24, x20, #0xffff
    movz x25, #0x11
    cmp  x24, x25
    b.eq handle_el0_mdccsr
    movz x25, #0x12
    cmp  x24, x25
    b.eq handle_el0_dbgdtrtx
    b fail_from_handler

handle_el0_mdccsr:
    adr  x24, handled
    movz x25, #1
    str  x25, [x24]
    movz x26, #0x5
    adr  x27, after_el0_mdccsr
    msr  spsr_el1, x26
    msr  elr_el1, x27
    eret

handle_el0_dbgdtrtx:
    adr  x24, handled
    movz x25, #2
    str  x25, [x24]
    movz x26, #0x5
    adr  x27, after_el0_dbgdtrtx
    msr  spsr_el1, x26
    msr  elr_el1, x27
    eret

handle_el0_trap:
    adr  x24, el0_mdccsr_trap_insn
    cmp  x22, x24
    b.eq handle_el0_mdccsr_trap
    adr  x24, el0_dbgdtrtx_trap_insn
    cmp  x22, x24
    b.eq handle_el0_dbgdtrtx_trap
    b fail_from_handler

handle_el0_mdccsr_trap:
    adr  x24, handled
    movz x25, #3
    str  x25, [x24]
    movz x26, #0x5
    adr  x27, after_el0_mdccsr_trap
    msr  spsr_el1, x26
    msr  elr_el1, x27
    eret

handle_el0_dbgdtrtx_trap:
    adr  x24, handled
    movz x25, #4
    str  x25, [x24]
    movz x26, #0x5
    adr  x27, after_el0_dbgdtrtx_trap
    msr  spsr_el1, x26
    msr  elr_el1, x27
    eret

fail_from_handler:
    movz x0, #'Y'
    b out

    .data
    .balign 8
handled:
    .quad 0
