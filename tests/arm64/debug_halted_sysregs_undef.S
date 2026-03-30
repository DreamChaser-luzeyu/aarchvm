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

    LOAD64 x0, 0x1111, 0x2222, 0x3333, 0x4444
el1_mrs_dlr_insn:
    mrs  x0, dlr_el0
    b    fail
after_el1_mrs_dlr:
    LOAD64 x2, 0x1111, 0x2222, 0x3333, 0x4444
    cmp  x0, x2
    b.ne fail1
    CHECK_HANDLED 1, fail2

    LOAD64 x0, 0x5555, 0x6666, 0x7777, 0x8888
el1_msr_dspsr_insn:
    msr  dspsr_el0, x0
    b    fail
after_el1_msr_dspsr:
    LOAD64 x2, 0x5555, 0x6666, 0x7777, 0x8888
    cmp  x0, x2
    b.ne fail3
    CHECK_HANDLED 2, fail4

    adr  x3, el0_mrs_dlr_entry
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret
after_el0_mrs_dlr:
    LOAD64 x2, 0x9999, 0xaaaa, 0xbbbb, 0xcccc
    cmp  x0, x2
    b.ne fail5
    CHECK_HANDLED 3, fail6

    adr  x3, el0_msr_dspsr_entry
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret
after_el0_msr_dspsr:
    LOAD64 x2, 0xdddd, 0xeeee, 0x1234, 0x5678
    cmp  x0, x2
    b.ne fail7
    CHECK_HANDLED 4, fail8

    movz x0, #'D'
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
fail:
    movz x0, #'F'
    b out

out:
    str  x0, [x1]
    brk  #0

el0_mrs_dlr_entry:
    LOAD64 x0, 0x9999, 0xaaaa, 0xbbbb, 0xcccc
el0_mrs_dlr_insn:
    mrs  x0, dlr_el0
    b    fail

el0_msr_dspsr_entry:
    LOAD64 x0, 0xdddd, 0xeeee, 0x1234, 0x5678
el0_msr_dspsr_insn:
    msr  dspsr_el0, x0
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
    mrs  x20, esr_el1
    ubfx x21, x20, #26, #6
    cbnz x21, fail_from_el1_handler
    tbz  x20, #25, fail_from_el1_handler
    and  x22, x20, #0x1ffffff
    cbnz x22, fail_from_el1_handler
    mrs  x23, far_el1
    cbnz x23, fail_from_el1_handler
    mrs  x24, elr_el1

    adr  x25, el1_mrs_dlr_insn
    cmp  x24, x25
    b.eq handle_el1_mrs_dlr
    adr  x25, el1_msr_dspsr_insn
    cmp  x24, x25
    b.eq handle_el1_msr_dspsr
    b    fail_from_el1_handler

handle_el1_mrs_dlr:
    adr  x26, handled
    movz x27, #1
    str  x27, [x26]
    adr  x28, after_el1_mrs_dlr
    msr  elr_el1, x28
    eret

handle_el1_msr_dspsr:
    adr  x26, handled
    movz x27, #2
    str  x27, [x26]
    adr  x28, after_el1_msr_dspsr
    msr  elr_el1, x28
    eret

lower_sync_handler:
    mrs  x20, esr_el1
    ubfx x21, x20, #26, #6
    cbnz x21, fail_from_el0_handler
    tbz  x20, #25, fail_from_el0_handler
    and  x22, x20, #0x1ffffff
    cbnz x22, fail_from_el0_handler
    mrs  x23, far_el1
    cbnz x23, fail_from_el0_handler
    mrs  x24, elr_el1

    adr  x25, el0_mrs_dlr_insn
    cmp  x24, x25
    b.eq handle_el0_mrs_dlr
    adr  x25, el0_msr_dspsr_insn
    cmp  x24, x25
    b.eq handle_el0_msr_dspsr
    b    fail_from_el0_handler

handle_el0_mrs_dlr:
    adr  x26, handled
    movz x27, #3
    str  x27, [x26]
    movz x28, #0x5
    adr  x29, after_el0_mrs_dlr
    msr  spsr_el1, x28
    msr  elr_el1, x29
    eret

handle_el0_msr_dspsr:
    adr  x26, handled
    movz x27, #4
    str  x27, [x26]
    movz x28, #0x5
    adr  x29, after_el0_msr_dspsr
    msr  spsr_el1, x28
    msr  elr_el1, x29
    eret

fail_from_el1_handler:
    movz x0, #'X'
    b out
fail_from_el0_handler:
    movz x0, #'Y'
    b out

    .data
    .balign 8
handled:
    .quad 0
