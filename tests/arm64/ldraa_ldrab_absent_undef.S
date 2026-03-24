    .arch armv8.3-a+pauth
    .text
    .global _start
_start:
    movz x24, #0x0000
    movk x24, #0x0900, lsl #16      // UART
    movz x28, #0x0000
    movk x28, #0x0902, lsl #16      // PERF mailbox

    adr  x25, vectors
    msr  vbar_el1, x25

    bl   load_test_regs

    adr  x20, after_ldraa_off
ldraa_off_insn:
    .inst 0xF8200441                // LDRAA X1, [X2]
    b    fail
after_ldraa_off:
    adr  x20, after_ldrab_off
ldrab_off_insn:
    .inst 0xF8A00483                // LDRAB X3, [X4]
    b    fail
after_ldrab_off:
    adr  x20, after_ldraa_pre
ldraa_pre_insn:
    .inst 0xF82014C5                // LDRAA X5, [X6, #8]!
    b    fail
after_ldraa_pre:
    adr  x20, after_ldrab_pre
ldrab_pre_insn:
    .inst 0xF8FFFD07                // LDRAB X7, [X8, #-8]!
    b    fail
after_ldrab_pre:
    bl   check_test_regs

    movz x0, #'A'
    str  x0, [x24]
    movz x0, #'\n'
    str  x0, [x24]
    b    exit_ok

load_test_regs:
    adr  x21, expected
    ldr  x1, [x21, #0]
    adr  x2, src0
    ldr  x3, [x21, #8]
    adr  x4, src1
    ldr  x5, [x21, #16]
    adr  x6, src2_base
    ldr  x7, [x21, #24]
    adr  x8, src3_after
    ret

check_test_regs:
    adr  x21, expected

    ldr  x22, [x21, #0]
    cmp  x1, x22
    b.ne fail
    adr  x22, src0
    cmp  x2, x22
    b.ne fail

    ldr  x22, [x21, #8]
    cmp  x3, x22
    b.ne fail
    adr  x22, src1
    cmp  x4, x22
    b.ne fail

    ldr  x22, [x21, #16]
    cmp  x5, x22
    b.ne fail
    adr  x22, src2_base
    cmp  x6, x22
    b.ne fail

    ldr  x22, [x21, #24]
    cmp  x7, x22
    b.ne fail
    adr  x22, src3_after
    cmp  x8, x22
    b.ne fail
    ret

fail:
    movz x0, #'F'
    str  x0, [x24]
    movz x0, #'\n'
    str  x0, [x24]

exit_ok:
    movz x0, #3
    str  x0, [x28, #0x08]
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
    b fail_from_handler

sync_handler:
    mrs  x26, esr_el1
    ubfx x27, x26, #26, #6
    cbnz x27, fail_from_handler
    tbz  x26, #25, fail_from_handler
    and  x27, x26, #0x1ffffff
    cbnz x27, fail_from_handler
    mrs  x27, far_el1
    cbnz x27, fail_from_handler
    movz x29, #0x5
    msr  spsr_el1, x29
    msr  elr_el1, x20
    eret

fail_from_handler:
    movz x29, #0x5
    msr  spsr_el1, x29
    adr  x27, fail
    msr  elr_el1, x27
    eret

    .data
    .balign 8
expected:
    .quad 0x1111111122222222
    .quad 0x3333333344444444
    .quad 0x5555555566666666
    .quad 0x7777777788888888
src0:
    .quad 0x0102030405060708
src1:
    .quad 0x1112131415161718
src2_base:
    .quad 0x2122232425262728
src2_data:
    .quad 0x3132333435363738
src3_data:
    .quad 0x4142434445464748
src3_after:
    .quad 0x5152535455565758
