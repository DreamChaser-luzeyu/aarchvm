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

    mrs  x2, mdccint_el1
    cbnz x2, fail1

    LOAD64 x0, 0xffff, 0xffff, 0xffff, 0xffff
    msr  mdccint_el1, x0
    mrs  x2, mdccint_el1
    movz x3, #0x0000
    movk x3, #0x6000, lsl #16
    cmp  x2, x3
    b.ne fail2

    mrs  x2, mdccsr_el0
    cbnz x2, fail3

    LOAD64 x0, 0x1357, 0x2468, 0xaaaa, 0xbbbb
    msr  osdtrtx_el1, x0
    mrs  x2, osdtrtx_el1
    LOAD64 x3, 0x1357, 0x2468, 0x0000, 0x0000
    cmp  x2, x3
    b.ne fail4
    mrs  x2, mdccsr_el0
    cbnz x2, fail5

    LOAD64 x0, 0xabcd, 0xef01, 0x1111, 0x2222
    msr  osdtrrx_el1, x0
    mrs  x2, osdtrrx_el1
    LOAD64 x3, 0xabcd, 0xef01, 0x0000, 0x0000
    cmp  x2, x3
    b.ne fail6
    mrs  x2, mdccsr_el0
    cbnz x2, fail7

    LOAD64 x0, 0x0d0c, 0x0b0a, 0x3333, 0x4444
    msr  dbgdtrtx_el0, x0
    mrs  x2, mdccsr_el0
    tbz  x2, #29, fail8
    tbnz x2, #30, fail8

    mrs  x2, osdtrtx_el1
    LOAD64 x3, 0x0d0c, 0x0b0a, 0x0000, 0x0000
    cmp  x2, x3
    b.ne fail9
    mrs  x2, mdccsr_el0
    tbz  x2, #29, failA
    tbnz x2, #30, failA

    LOAD64 x0, 0x2222, 0x1111, 0x4444, 0x3333
    msr  osdtrtx_el1, x0
    mrs  x2, osdtrtx_el1
    LOAD64 x3, 0x2222, 0x1111, 0x0000, 0x0000
    cmp  x2, x3
    b.ne failB
    mrs  x2, mdccsr_el0
    tbz  x2, #29, failC
    tbnz x2, #30, failC

    adr  x3, el0_mrs_mdccint_entry
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret
after_el0_mrs_mdccint:
    CHECK_HANDLED 1, failD
    LOAD64 x2, 0x0102, 0x0304, 0x0506, 0x0708
    cmp  x0, x2
    b.ne failE
    mrs  x2, mdccint_el1
    movz x3, #0x0000
    movk x3, #0x6000, lsl #16
    cmp  x2, x3
    b.ne failF

    adr  x3, el0_mrs_osdtrrx_entry
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret
after_el0_mrs_osdtrrx:
    CHECK_HANDLED 2, failG
    LOAD64 x2, 0x9999, 0xaaaa, 0xbbbb, 0xcccc
    cmp  x0, x2
    b.ne failH
    mrs  x2, osdtrrx_el1
    LOAD64 x3, 0xabcd, 0xef01, 0x0000, 0x0000
    cmp  x2, x3
    b.ne failI

    adr  x3, el0_msr_osdtrtx_entry
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret
after_el0_msr_osdtrtx:
    CHECK_HANDLED 3, failJ
    LOAD64 x2, 0x2468, 0x1357, 0xcccc, 0xdddd
    cmp  x0, x2
    b.ne failK
    mrs  x2, osdtrtx_el1
    LOAD64 x3, 0x2222, 0x1111, 0x0000, 0x0000
    cmp  x2, x3
    b.ne failL
    mrs  x2, mdccsr_el0
    tbz  x2, #29, failM
    tbnz x2, #30, failM

    movz x0, #'Y'
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
failG:
    movz x0, #'G'
    b out
failH:
    movz x0, #'H'
    b out
failI:
    movz x0, #'I'
    b out
failJ:
    movz x0, #'J'
    b out
failK:
    movz x0, #'K'
    b out
failL:
    movz x0, #'L'
    b out
failM:
    movz x0, #'M'
    b out
fail:
    movz x0, #'F'
    b out

out:
    str  x0, [x1]
    brk  #0

el0_mrs_mdccint_entry:
    LOAD64 x0, 0x0102, 0x0304, 0x0506, 0x0708
el0_mrs_mdccint_insn:
    mrs  x0, mdccint_el1
    b    fail

el0_mrs_osdtrrx_entry:
    LOAD64 x0, 0x9999, 0xaaaa, 0xbbbb, 0xcccc
el0_mrs_osdtrrx_insn:
    mrs  x0, osdtrrx_el1
    b    fail

el0_msr_osdtrtx_entry:
    LOAD64 x0, 0x2468, 0x1357, 0xcccc, 0xdddd
el0_msr_osdtrtx_insn:
    msr  osdtrtx_el1, x0
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
    cbnz x21, fail_from_handler
    tbz  x20, #25, fail_from_handler
    and  x22, x20, #0x1ffffff
    cbnz x22, fail_from_handler
    mrs  x23, far_el1
    cbnz x23, fail_from_handler
    mrs  x24, elr_el1

    adr  x25, el0_mrs_mdccint_insn
    cmp  x24, x25
    b.eq handle_el0_mrs_mdccint
    adr  x25, el0_mrs_osdtrrx_insn
    cmp  x24, x25
    b.eq handle_el0_mrs_osdtrrx
    adr  x25, el0_msr_osdtrtx_insn
    cmp  x24, x25
    b.eq handle_el0_msr_osdtrtx
    b    fail_from_handler

handle_el0_mrs_mdccint:
    adr  x26, handled
    movz x27, #1
    str  x27, [x26]
    movz x28, #0x5
    adr  x29, after_el0_mrs_mdccint
    msr  spsr_el1, x28
    msr  elr_el1, x29
    eret

handle_el0_mrs_osdtrrx:
    adr  x26, handled
    movz x27, #2
    str  x27, [x26]
    movz x28, #0x5
    adr  x29, after_el0_mrs_osdtrrx
    msr  spsr_el1, x28
    msr  elr_el1, x29
    eret

handle_el0_msr_osdtrtx:
    adr  x26, handled
    movz x27, #3
    str  x27, [x26]
    movz x28, #0x5
    adr  x29, after_el0_msr_osdtrtx
    msr  spsr_el1, x28
    msr  elr_el1, x29
    eret

fail_from_handler:
    movz x0, #'Z'
    b out

    .data
    .balign 8
handled:
    .quad 0
