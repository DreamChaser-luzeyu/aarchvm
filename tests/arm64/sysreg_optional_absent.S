    .text
    .global _start
_start:
    movz x1, #0x0000
    movk x1, #0x0900, lsl #16      // UART

    adr  x10, vectors
    msr  vbar_el1, x10

    adr  x11, trap_flags
    str  xzr, [x11]

    movz x0, #0x7788
    movk x0, #0x5566, lsl #16
    movk x0, #0x3344, lsl #32
    movk x0, #0x1122, lsl #48
tpidr2_mrs_insn:
    .inst 0xD53BD0A0                  // mrs x0, tpidr2_el0

after_tpidr2_mrs:
    movz x2, #0x7788
    movk x2, #0x5566, lsl #16
    movk x2, #0x3344, lsl #32
    movk x2, #0x1122, lsl #48
    subs xzr, x0, x2
    b.ne fail1
    adr  x11, trap_flags
    ldr  x12, [x11]
    tbz  x12, #0, fail2

    movz x0, #0xbeef
    movk x0, #0xcafe, lsl #16
    movk x0, #0x5678, lsl #32
    movk x0, #0x1234, lsl #48
tpidr2_msr_insn:
    .inst 0xD51BD0A0                  // msr tpidr2_el0, x0

after_tpidr2_msr:
    adr  x11, trap_flags
    ldr  x12, [x11]
    tbz  x12, #1, fail3

    movz x0, #0x4321
    movk x0, #0x8765, lsl #16
    movk x0, #0xcba9, lsl #32
    movk x0, #0x0fed, lsl #48
gcspr_mrs_insn:
    .inst 0xD53B2520                  // mrs x0, gcspr_el0

after_gcspr_mrs:
    movz x2, #0x4321
    movk x2, #0x8765, lsl #16
    movk x2, #0xcba9, lsl #32
    movk x2, #0x0fed, lsl #48
    subs xzr, x0, x2
    b.ne fail4
    adr  x11, trap_flags
    ldr  x12, [x11]
    tbz  x12, #2, fail5

    movz x0, #0x55aa
    movk x0, #0xa55a, lsl #16
gcspr_msr_insn:
    .inst 0xD51B2520                  // msr gcspr_el0, x0

after_gcspr_msr:
    adr  x11, trap_flags
    ldr  x12, [x11]
    tbz  x12, #3, fail6

    movz x0, #'U'
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

out:
    str  x0, [x1]
    brk  #0

    .balign 2048
vectors:
    .space 0x200
sync_spx:
    b sync_handler

sync_handler:
    mrs  x20, esr_el1
    lsr  x21, x20, #26
    cbnz x21, handler_fail

    mrs  x22, elr_el1
    adr  x23, tpidr2_mrs_insn
    cmp  x22, x23
    b.eq trap_tpidr2_mrs
    adr  x23, tpidr2_msr_insn
    cmp  x22, x23
    b.eq trap_tpidr2_msr
    adr  x23, gcspr_mrs_insn
    cmp  x22, x23
    b.eq trap_gcspr_mrs
    adr  x23, gcspr_msr_insn
    cmp  x22, x23
    b.eq trap_gcspr_msr
    b handler_fail

trap_tpidr2_mrs:
    adr  x24, trap_flags
    ldr  x25, [x24]
    orr  x25, x25, #1
    str  x25, [x24]
    adr  x26, after_tpidr2_mrs
    b return_to_el1

trap_tpidr2_msr:
    adr  x24, trap_flags
    ldr  x25, [x24]
    orr  x25, x25, #2
    str  x25, [x24]
    adr  x26, after_tpidr2_msr
    b return_to_el1

trap_gcspr_mrs:
    adr  x24, trap_flags
    ldr  x25, [x24]
    orr  x25, x25, #4
    str  x25, [x24]
    adr  x26, after_gcspr_mrs
    b return_to_el1

trap_gcspr_msr:
    adr  x24, trap_flags
    ldr  x25, [x24]
    orr  x25, x25, #8
    str  x25, [x24]
    adr  x26, after_gcspr_msr
    b return_to_el1

return_to_el1:
    msr  elr_el1, x26
    eret

handler_fail:
    movz x0, #'F'
    adr  x26, out
    msr  elr_el1, x26
    eret

    .data
    .balign 8
trap_flags:
    .quad 0
