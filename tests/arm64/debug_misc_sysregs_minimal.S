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

    mrs  x2, oslsr_el1
    tbz  x2, #1, fail1

    LOAD64 x0, 0x1111, 0x2222, 0x0000, 0x0000
    msr  oseccr_el1, x0
    mrs  x2, oseccr_el1
    cmp  x2, x0
    b.ne fail2

    msr  oslar_el1, xzr
    mrs  x2, oslsr_el1
    tbnz x2, #1, fail3

    LOAD64 x0, 0xaaaa, 0xbbbb, 0x0000, 0x0000
    msr  oseccr_el1, x0
    movz x0, #1
    msr  oslar_el1, x0
    mrs  x2, oseccr_el1
    LOAD64 x3, 0x1111, 0x2222, 0x0000, 0x0000
    cmp  x2, x3
    b.ne fail4

    mrs  x2, dbgclaimclr_el1
    cbnz x2, fail5

    LOAD64 x0, 0x00a5, 0xffff, 0xffff, 0xffff
    msr  dbgclaimset_el1, x0
    mrs  x2, dbgclaimclr_el1
    movz x3, #0x00a5
    cmp  x2, x3
    b.ne fail6

    LOAD64 x0, 0x0024, 0xbeef, 0x1357, 0x2468
    msr  dbgclaimclr_el1, x0
    mrs  x2, dbgclaimset_el1
    movz x3, #0x0081
    cmp  x2, x3
    b.ne fail7

    mrs  x2, dbgauthstatus_el1
    movz x3, #0x000a
    cmp  x2, x3
    b.ne fail8

    mrs  x2, mdrar_el1
    cbnz x2, fail9

    adr  x3, el0_mrs_oslsr_entry
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret
after_el0_mrs_oslsr:
    CHECK_STATE 1, failA

    adr  x3, el0_mrs_dbgauthstatus_entry
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret
after_el0_mrs_dbgauthstatus:
    CHECK_STATE 2, failB

    adr  x3, el0_mrs_dbgclaimset_entry
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret
after_el0_mrs_dbgclaimset:
    CHECK_STATE 3, failC

    adr  x3, el0_msr_oseccr_entry
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret
after_el0_msr_oseccr:
    CHECK_STATE 4, failD

    mrs  x2, oseccr_el1
    LOAD64 x3, 0x1111, 0x2222, 0x0000, 0x0000
    cmp  x2, x3
    b.ne failE

    mrs  x2, dbgclaimset_el1
    movz x3, #0x0081
    cmp  x2, x3
    b.ne failF

    movz x0, #'O'
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

el0_mrs_oslsr_entry:
    LOAD64 x0, 0x1357, 0x2468, 0x369c, 0x48ad
el0_mrs_oslsr_insn:
    mrs  x0, oslsr_el1
    svc  #0x11

el0_mrs_dbgauthstatus_entry:
    LOAD64 x0, 0xaaaa, 0xbbbb, 0xcccc, 0xdddd
el0_mrs_dbgauthstatus_insn:
    mrs  x0, dbgauthstatus_el1
    svc  #0x12

el0_mrs_dbgclaimset_entry:
    LOAD64 x0, 0x0102, 0x0304, 0x0506, 0x0708
el0_mrs_dbgclaimset_insn:
    mrs  x0, dbgclaimset_el1
    svc  #0x13

el0_msr_oseccr_entry:
    LOAD64 x0, 0x9999, 0x8888, 0x0000, 0x0000
el0_msr_oseccr_insn:
    msr  oseccr_el1, x0
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
    adr  x23, el0_mrs_oslsr_insn
    cmp  x22, x23
    b.eq undef_mrs_oslsr
    adr  x23, el0_mrs_dbgauthstatus_insn
    cmp  x22, x23
    b.eq undef_mrs_dbgauthstatus
    adr  x23, el0_mrs_dbgclaimset_insn
    cmp  x22, x23
    b.eq undef_mrs_dbgclaimset
    adr  x23, el0_msr_oseccr_insn
    cmp  x22, x23
    b.eq undef_msr_oseccr
    b handler_fail

undef_mrs_oslsr:
    LOAD64 x24, 0x1357, 0x2468, 0x369c, 0x48ad
    cmp  x0, x24
    b.ne handler_fail
    adr  x25, handled
    movz x26, #1
    str  x26, [x25]
    adr  x27, after_el0_mrs_oslsr
    b return_to_el1

undef_mrs_dbgauthstatus:
    LOAD64 x24, 0xaaaa, 0xbbbb, 0xcccc, 0xdddd
    cmp  x0, x24
    b.ne handler_fail
    adr  x25, handled
    movz x26, #2
    str  x26, [x25]
    adr  x27, after_el0_mrs_dbgauthstatus
    b return_to_el1

undef_mrs_dbgclaimset:
    LOAD64 x24, 0x0102, 0x0304, 0x0506, 0x0708
    cmp  x0, x24
    b.ne handler_fail
    adr  x25, handled
    movz x26, #3
    str  x26, [x25]
    adr  x27, after_el0_mrs_dbgclaimset
    b return_to_el1

undef_msr_oseccr:
    adr  x25, handled
    movz x26, #4
    str  x26, [x25]
    adr  x27, after_el0_msr_oseccr

return_to_el1:
    movz x28, #0x5
    msr  spsr_el1, x28
    msr  elr_el1, x27
    eret

handler_fail:
    movz x0, #'X'
    adr  x27, out
    movz x28, #0x5
    msr  spsr_el1, x28
    msr  elr_el1, x27
    eret

    .data
    .balign 8
handled:
    .quad 0
