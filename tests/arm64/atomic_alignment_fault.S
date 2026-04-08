    .arch armv8.1-a+lse
    .text
    .global _start
_start:
    movz x1, #0
    movk x1, #0x0900, lsl #16      // UART

    adr  x10, vectors
    msr  vbar_el1, x10

    adr  x11, saved_sctlr
    mrs  x12, sctlr_el1
    str  x12, [x11]
    movz x13, #2
    bic  x12, x12, x13             // A=0: ordered/atomic paths must still fault
    msr  sctlr_el1, x12
    isb

    adr  x20, atomic_buf
    add  x20, x20, #1
    movz x21, #0x55aa

    adr  x14, case_id
    movz x15, #1
    str  x15, [x14]
    adr  x16, expected_elr
    adr  x17, fault_ldar
    str  x17, [x16]
    adr  x18, resume_elr
    adr  x19, after_ldar
    str  x19, [x18]
    adr  x22, expected_far
    str  x20, [x22]
    adr  x23, expected_wnr
    str  xzr, [x23]
fault_ldar:
    ldar x4, [x20]
    b    fail

after_ldar:
    adr  x14, case_id
    movz x15, #2
    str  x15, [x14]
    adr  x16, expected_elr
    adr  x17, fault_stlr
    str  x17, [x16]
    adr  x18, resume_elr
    adr  x19, after_stlr
    str  x19, [x18]
    adr  x23, expected_wnr
    movz x24, #1
    str  x24, [x23]
fault_stlr:
    stlr x21, [x20]
    b    fail

after_stlr:
    adr  x14, case_id
    movz x15, #3
    str  x15, [x14]
    adr  x16, expected_elr
    adr  x17, fault_ldxr
    str  x17, [x16]
    adr  x18, resume_elr
    adr  x19, after_ldxr
    str  x19, [x18]
    adr  x23, expected_wnr
    str  xzr, [x23]
fault_ldxr:
    ldxr x5, [x20]
    b    fail

after_ldxr:
    adr  x14, case_id
    movz x15, #4
    str  x15, [x14]
    adr  x16, expected_elr
    adr  x17, fault_ldadd
    str  x17, [x16]
    adr  x18, resume_elr
    adr  x19, pass
    str  x19, [x18]
    adr  x23, expected_wnr
    str  xzr, [x23]                // Atomic RMW alignment faults report WnR=0.
fault_ldadd:
    ldadd x21, x6, [x20]
    b    fail

pass:
    adr  x11, saved_sctlr
    ldr  x12, [x11]
    msr  sctlr_el1, x12
    isb
    movz x0, #'O'
    str  x0, [x1]
    movz x0, #'\n'
    str  x0, [x1]
    brk  #0

fail:
    adr  x11, saved_sctlr
    ldr  x12, [x11]
    msr  sctlr_el1, x12
    isb
    movz x0, #'F'
    str  x0, [x1]
    brk  #0

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
    tbnz x2, #10, fail_from_handler // FnV=0: FAR_EL1 is valid in this model.
    tbnz x2, #8, fail_from_handler  // CM=0: atomic alignment faults are not cache maintenance/AT.
    tbnz x2, #7, fail_from_handler  // S1PTW=0 in this model.
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
atomic_buf:
    .byte 0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88
    .byte 0x99,0xaa,0xbb,0xcc,0xdd,0xee,0xff,0x10
