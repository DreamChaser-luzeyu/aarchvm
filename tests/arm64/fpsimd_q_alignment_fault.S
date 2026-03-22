    .text
    .global _start
_start:
    movz x1, #0
    movk x1, #0x0900, lsl #16      // UART

    movz x25, #0x0030, lsl #16     // Enable FP/AdvSIMD at EL1.
    msr  cpacr_el1, x25
    isb

    adr  x10, vectors
    msr  vbar_el1, x10

    adr  x11, saved_sctlr
    mrs  x12, sctlr_el1
    str  x12, [x11]

    // A=0: misaligned Q load must still complete.
    movz x13, #2
    bic  x12, x12, x13
    msr  sctlr_el1, x12
    isb

    adr  x2, src_bytes
    ldr  q0, [x2]
    adr  x3, load_result
    str  q0, [x3]
    mov  x0, x3
    adr  x1, load_expected
    bl   check16

    adr  x1, uart_base
    ldr  x1, [x1]

    // Prepare aligned source vector for the store-fault case.
    adr  x4, store_source
    ldr  q1, [x4]

    // A=1: misaligned Q load/store must fault, with WnR reflecting load/store.
    adr  x11, saved_sctlr
    ldr  x12, [x11]
    movz x13, #2
    orr  x12, x12, x13
    msr  sctlr_el1, x12
    isb

    adr  x14, case_id
    movz x15, #1
    str  x15, [x14]
    adr  x16, expected_elr
    adr  x17, fault_ldr_q
    str  x17, [x16]
    adr  x18, resume_elr
    adr  x19, after_ldr_q
    str  x19, [x18]
    adr  x20, expected_far
    adr  x21, src_bytes
    str  x21, [x20]
    adr  x22, expected_wnr
    str  xzr, [x22]
fault_ldr_q:
    ldr  q2, [x21]
    b    fail

after_ldr_q:
    adr  x14, case_id
    movz x15, #2
    str  x15, [x14]
    adr  x16, expected_elr
    adr  x17, fault_str_q
    str  x17, [x16]
    adr  x18, resume_elr
    adr  x19, pass
    str  x19, [x18]
    adr  x20, expected_far
    adr  x21, store_target
    add  x24, x21, #8
    str  x24, [x20]
    adr  x22, expected_wnr
    movz x23, #1
    str  x23, [x22]
    movz x23, #8
fault_str_q:
    str  q1, [x21, x23]
    b    fail

pass:
    adr  x11, saved_sctlr
    ldr  x12, [x11]
    msr  sctlr_el1, x12
    isb
    movz x0, #'Q'
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

check16:
    ldr  x4, [x0]
    ldr  x5, [x1]
    cmp  x4, x5
    b.ne fail
    ldr  x4, [x0, #8]
    ldr  x5, [x1, #8]
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
case_id:
    .quad 0
expected_elr:
    .quad 0
resume_elr:
    .quad 0
expected_far:
    .quad 0
expected_wnr:
    .quad 0
src_bytes:
    .byte 0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07
    .byte 0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f
    .byte 0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17
load_expected:
    .byte 0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07
    .byte 0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f
store_source:
    .byte 0xa0,0xa1,0xa2,0xa3,0xa4,0xa5,0xa6,0xa7
    .byte 0xa8,0xa9,0xaa,0xab,0xac,0xad,0xae,0xaf
store_target:
    .space 64
load_result:
    .space 16
