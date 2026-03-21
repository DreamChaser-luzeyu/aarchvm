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
    movk x1, #0x0900, lsl #16      // UART

    adr  x10, vectors
    msr  vbar_el1, x10

    adr  x11, handled
    str  xzr, [x11]

    // ID_AA64DFR0_EL1 advertises two breakpoint and two watchpoint resources.
    // Index 1 must remain accessible.
    LOAD64 x0, 0x1111, 0x2222, 0x3333, 0x4444
    msr  dbgbvr1_el1, x0
    mrs  x2, dbgbvr1_el1
    cmp  x2, x0
    b.ne fail1

    LOAD64 x0, 0x5555, 0x6666, 0x7777, 0x8888
    msr  dbgbcr1_el1, x0
    mrs  x2, dbgbcr1_el1
    cmp  x2, x0
    b.ne fail2

    LOAD64 x0, 0x9999, 0xaaaa, 0xbbbb, 0xcccc
    msr  dbgwvr1_el1, x0
    mrs  x2, dbgwvr1_el1
    cmp  x2, x0
    b.ne fail3

    LOAD64 x0, 0xdddd, 0xeeee, 0x1234, 0x5678
    msr  dbgwcr1_el1, x0
    mrs  x2, dbgwcr1_el1
    cmp  x2, x0
    b.ne fail4

    // EL1: index 2 is outside the implemented debug resource range and must be
    // UNDEFINED, not silently accepted.
    LOAD64 x0, 0xa101, 0xa102, 0xa103, 0xa104
el1_mrs_dbgbvr2_insn:
    mrs  x0, dbgbvr2_el1
    b    fail
after_el1_mrs_dbgbvr2:
    LOAD64 x2, 0xa101, 0xa102, 0xa103, 0xa104
    cmp  x0, x2
    b.ne fail5
    CHECK_STATE 1, fail6

    LOAD64 x0, 0xb101, 0xb102, 0xb103, 0xb104
el1_msr_dbgwcr2_insn:
    msr  dbgwcr2_el1, x0
    b    fail
after_el1_msr_dbgwcr2:
    LOAD64 x2, 0xb101, 0xb102, 0xb103, 0xb104
    cmp  x0, x2
    b.ne fail7
    CHECK_STATE 2, fail8

    // EL0: implemented debug registers stay present and privileged, so access
    // must trap as an EL0 system instruction, not become UNDEFINED.
    adr  x3, el0_mrs_dbgbvr1_entry
    msr  elr_el1, x3
    msr  spsr_el1, xzr             // EL0t
    eret
after_el0_mrs_dbgbvr1:
    LOAD64 x2, 0xc101, 0xc102, 0xc103, 0xc104
    cmp  x0, x2
    b.ne fail9
    CHECK_STATE 3, failA

    // EL0: unimplemented debug resource indexes must be UNDEFINED rather than
    // trapping as if they were present.
    adr  x3, el0_mrs_dbgbvr2_entry
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret
after_el0_mrs_dbgbvr2:
    LOAD64 x2, 0xd101, 0xd102, 0xd103, 0xd104
    cmp  x0, x2
    b.ne failB
    CHECK_STATE 4, failC

    adr  x3, el0_msr_dbgwcr2_entry
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret
after_el0_msr_dbgwcr2:
    LOAD64 x2, 0xe101, 0xe102, 0xe103, 0xe104
    cmp  x0, x2
    b.ne failD
    CHECK_STATE 5, failE

    movz x0, #'K'
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
fail:
    movz x0, #'F'
    b out

out:
    movz x1, #0x0000
    movk x1, #0x0900, lsl #16
    str  x0, [x1]
    brk  #0

el0_mrs_dbgbvr1_entry:
    LOAD64 x0, 0xc101, 0xc102, 0xc103, 0xc104
el0_mrs_dbgbvr1_insn:
    mrs  x0, dbgbvr1_el1
    b    fail

el0_mrs_dbgbvr2_entry:
    LOAD64 x0, 0xd101, 0xd102, 0xd103, 0xd104
el0_mrs_dbgbvr2_insn:
    mrs  x0, dbgbvr2_el1
    b    fail

el0_msr_dbgwcr2_entry:
    LOAD64 x0, 0xe101, 0xe102, 0xe103, 0xe104
el0_msr_dbgwcr2_insn:
    msr  dbgwcr2_el1, x0
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

    adr  x25, el1_mrs_dbgbvr2_insn
    cmp  x24, x25
    b.eq handle_el1_mrs_dbgbvr2

    adr  x25, el1_msr_dbgwcr2_insn
    cmp  x24, x25
    b.eq handle_el1_msr_dbgwcr2

    b    fail_from_el1_handler

handle_el1_mrs_dbgbvr2:
    LOAD64 x26, 0xa101, 0xa102, 0xa103, 0xa104
    cmp  x0, x26
    b.ne fail_from_el1_handler
    adr  x27, handled
    movz x28, #1
    str  x28, [x27]
    adr  x29, after_el1_mrs_dbgbvr2
    msr  elr_el1, x29
    eret

handle_el1_msr_dbgwcr2:
    LOAD64 x26, 0xb101, 0xb102, 0xb103, 0xb104
    cmp  x0, x26
    b.ne fail_from_el1_handler
    adr  x27, handled
    movz x28, #2
    str  x28, [x27]
    adr  x29, after_el1_msr_dbgwcr2
    msr  elr_el1, x29
    eret

lower_sync_handler:
    mrs  x20, esr_el1
    mrs  x22, elr_el1

    adr  x23, el0_mrs_dbgbvr1_insn
    cmp  x22, x23
    b.eq handle_el0_mrs_dbgbvr1

    adr  x23, el0_mrs_dbgbvr2_insn
    cmp  x22, x23
    b.eq handle_el0_mrs_dbgbvr2

    adr  x23, el0_msr_dbgwcr2_insn
    cmp  x22, x23
    b.eq handle_el0_msr_dbgwcr2

    b    fail_from_el0_handler

handle_el0_mrs_dbgbvr1:
    ubfx x21, x20, #26, #6
    movz x24, #0x18
    cmp  x21, x24
    b.ne fail_from_el0_handler
    tbz  x20, #25, fail_from_el0_handler
    LOAD64 x25, 0xc101, 0xc102, 0xc103, 0xc104
    cmp  x0, x25
    b.ne fail_from_el0_handler
    adr  x26, handled
    movz x27, #3
    str  x27, [x26]
    adr  x30, after_el0_mrs_dbgbvr1
    b    return_to_el1

handle_el0_mrs_dbgbvr2:
    ubfx x21, x20, #26, #6
    cbnz x21, fail_from_el0_handler
    tbz  x20, #25, fail_from_el0_handler
    and  x24, x20, #0x1ffffff
    cbnz x24, fail_from_el0_handler
    mrs  x24, far_el1
    cbnz x24, fail_from_el0_handler
    LOAD64 x25, 0xd101, 0xd102, 0xd103, 0xd104
    cmp  x0, x25
    b.ne fail_from_el0_handler
    adr  x26, handled
    movz x27, #4
    str  x27, [x26]
    adr  x30, after_el0_mrs_dbgbvr2
    b    return_to_el1

handle_el0_msr_dbgwcr2:
    ubfx x21, x20, #26, #6
    cbnz x21, fail_from_el0_handler
    tbz  x20, #25, fail_from_el0_handler
    and  x24, x20, #0x1ffffff
    cbnz x24, fail_from_el0_handler
    mrs  x24, far_el1
    cbnz x24, fail_from_el0_handler
    LOAD64 x25, 0xe101, 0xe102, 0xe103, 0xe104
    cmp  x0, x25
    b.ne fail_from_el0_handler
    adr  x26, handled
    movz x27, #5
    str  x27, [x26]
    adr  x30, after_el0_msr_dbgwcr2

return_to_el1:
    movz x29, #0x5                  // EL1h
    msr  spsr_el1, x29
    msr  elr_el1, x30
    eret

fail_from_el1_handler:
    adr  x29, out
    msr  elr_el1, x29
    eret

fail_from_el0_handler:
    adr  x30, out
    b    return_to_el1

    .data
    .balign 8
handled:
    .quad 0
