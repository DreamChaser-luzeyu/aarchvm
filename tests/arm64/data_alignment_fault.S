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

    // A=0: misaligned scalar load must still complete.
    movz x13, #2
    bic  x12, x12, x13
    msr  sctlr_el1, x12
    isb

    adr  x2, data_bytes
    add  x2, x2, #1
    ldr  x3, [x2]
    movz x4, #0x0302
    movk x4, #0x0504, lsl #16
    movk x4, #0x0706, lsl #32
    movk x4, #0x0908, lsl #48
    subs xzr, x3, x4
    b.ne fail

    // A=1: misaligned scalar and pair loads must fault.
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
    adr  x17, fault_ldr
    str  x17, [x16]
    adr  x18, resume_elr
    adr  x19, after_ldr_fault
    str  x19, [x18]
    adr  x20, expected_far
    adr  x21, data_bytes
    add  x21, x21, #1
    str  x21, [x20]
fault_ldr:
    ldr  x5, [x21]
    b    fail

after_ldr_fault:
    adr  x14, case_id
    movz x15, #2
    str  x15, [x14]
    adr  x16, expected_elr
    adr  x17, fault_ldp
    str  x17, [x16]
    adr  x18, resume_elr
    adr  x19, pass
    str  x19, [x18]
    adr  x20, expected_far
    adr  x21, data_bytes
    add  x21, x21, #4
    str  x21, [x20]
fault_ldp:
    ldp  x6, x7, [x21]
    b    fail

pass:
    adr  x11, saved_sctlr
    ldr  x12, [x11]
    msr  sctlr_el1, x12
    isb
    movz x0, #'A'
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
    tbnz x2, #8, fail_from_handler  // CM=0: alignment fault on normal access.
    tbnz x2, #7, fail_from_handler  // S1PTW=0 for alignment faults in this model.
    tbnz x2, #6, fail_from_handler
    and  x4, x2, #0x3f
    cmp  x4, #0x21
    b.ne fail_from_handler
    mrs  x5, far_el1
    adr  x6, expected_far
    ldr  x6, [x6]
    subs xzr, x5, x6
    b.ne fail_from_handler
    mrs  x7, elr_el1
    adr  x8, expected_elr
    ldr  x8, [x8]
    subs xzr, x7, x8
    b.ne fail_from_handler
    adr  x9, resume_elr
    ldr  x9, [x9]
    msr  elr_el1, x9
    eret

fail_from_handler:
    adr  x9, fail
    msr  elr_el1, x9
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
data_bytes:
    .byte 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16
