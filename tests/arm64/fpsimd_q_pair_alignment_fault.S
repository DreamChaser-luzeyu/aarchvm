    .text
    .global _start
_start:
    movz x1, #0
    movk x1, #0x0900, lsl #16      // UART

    movz x25, #0x0030, lsl #16
    msr  cpacr_el1, x25
    isb

    adr  x10, vectors
    msr  vbar_el1, x10

    adr  x11, saved_sctlr
    mrs  x12, sctlr_el1
    str  x12, [x11]

    // A=0: misaligned Q-pair load/store must still complete.
    movz x13, #2
    bic  x12, x12, x13
    msr  sctlr_el1, x12
    isb

    adr  x2, pair_src
    add  x2, x2, #8
    ldp  q0, q1, [x2]

    adr  x3, pair_result
    stp  q0, q1, [x3]
    mov  x0, x3
    adr  x1, pair_expected
    bl   check32

    adr  x4, store_target
    add  x4, x4, #8
    stp  q0, q1, [x4]
    mov  x0, x4
    adr  x1, pair_expected
    bl   check32

    adr  x1, uart_base
    ldr  x1, [x1]

    // A=1: the same addresses must now fault because each element is 16 bytes.
    adr  x11, saved_sctlr
    ldr  x12, [x11]
    movz x13, #2
    orr  x12, x12, x13
    msr  sctlr_el1, x12
    isb

    adr  x14, expected_elr
    adr  x15, fault_ldp_q
    str  x15, [x14]
    adr  x16, resume_elr
    adr  x17, after_ldp_q
    str  x17, [x16]
    adr  x18, expected_far
    adr  x19, pair_src
    add  x19, x19, #8
    str  x19, [x18]
    adr  x20, expected_wnr
    str  xzr, [x20]
fault_ldp_q:
    ldp  q4, q5, [x19]
    b    fail

after_ldp_q:
    adr  x2, store_source
    ldp  q2, q3, [x2]

    adr  x14, expected_elr
    adr  x15, fault_stp_q
    str  x15, [x14]
    adr  x16, resume_elr
    adr  x17, pass
    str  x17, [x16]
    adr  x18, expected_far
    adr  x19, store_target
    add  x19, x19, #8
    str  x19, [x18]
    adr  x20, expected_wnr
    movz x21, #1
    str  x21, [x20]
fault_stp_q:
    stp  q2, q3, [x19]
    b    fail

pass:
    adr  x11, saved_sctlr
    ldr  x12, [x11]
    msr  sctlr_el1, x12
    isb
    movz x0, #'Y'
    str  x0, [x1]
    movz x0, #'\n'
    str  x0, [x1]
    brk  #0

fail:
    adr  x11, saved_sctlr
    ldr  x12, [x11]
    msr  sctlr_el1, x12
    isb
    adr  x1, uart_base
    ldr  x1, [x1]
    movz x0, #'F'
    str  x0, [x1]
    brk  #0

check32:
    ldr  x4, [x0]
    ldr  x5, [x1]
    cmp  x4, x5
    b.ne fail
    ldr  x4, [x0, #8]
    ldr  x5, [x1, #8]
    cmp  x4, x5
    b.ne fail
    ldr  x4, [x0, #16]
    ldr  x5, [x1, #16]
    cmp  x4, x5
    b.ne fail
    ldr  x4, [x0, #24]
    ldr  x5, [x1, #24]
    cmp  x4, x5
    b.ne fail
    ret

    .balign 2048
vectors:
    .space 0x200
sync_spx:
    b sync_handler

sync_handler:
    mrs  x2, esr_el1
    ubfx x3, x2, #26, #6
    cmp  x3, #0x25
    b.ne fail_from_handler
    and  x4, x2, #0x3f
    cmp  x4, #0x21
    b.ne fail_from_handler
    adr  x5, expected_wnr
    ldr  x5, [x5]
    tst  x2, #(1 << 6)
    cset x6, ne
    subs xzr, x6, x5
    b.ne fail_from_handler
    mrs  x7, far_el1
    adr  x8, expected_far
    ldr  x8, [x8]
    subs xzr, x7, x8
    b.ne fail_from_handler
    mrs  x9, elr_el1
    adr  x10, expected_elr
    ldr  x10, [x10]
    subs xzr, x9, x10
    b.ne fail_from_handler
    adr  x11, resume_elr
    ldr  x11, [x11]
    msr  elr_el1, x11
    eret

fail_from_handler:
    adr  x11, fail
    msr  elr_el1, x11
    eret

    .data
    .balign 16
uart_base:
    .quad 0x09000000
saved_sctlr:
    .quad 0
expected_elr:
    .quad 0
resume_elr:
    .quad 0
expected_far:
    .quad 0
expected_wnr:
    .quad 0
pair_src:
    .byte 0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07
    .byte 0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17
    .byte 0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x1e,0x1f
    .byte 0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27
    .byte 0x28,0x29,0x2a,0x2b,0x2c,0x2d,0x2e,0x2f
pair_expected:
    .byte 0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17
    .byte 0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x1e,0x1f
    .byte 0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27
    .byte 0x28,0x29,0x2a,0x2b,0x2c,0x2d,0x2e,0x2f
    .balign 16
store_source:
    .byte 0xa0,0xa1,0xa2,0xa3,0xa4,0xa5,0xa6,0xa7
    .byte 0xa8,0xa9,0xaa,0xab,0xac,0xad,0xae,0xaf
    .byte 0xb0,0xb1,0xb2,0xb3,0xb4,0xb5,0xb6,0xb7
    .byte 0xb8,0xb9,0xba,0xbb,0xbc,0xbd,0xbe,0xbf
pair_result:
    .space 32
store_target:
    .space 64
