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
    movz x13, #0x18                // SA | SA0
    orr  x12, x12, x13
    msr  sctlr_el1, x12
    isb

    adr  x20, handler_stack
    add  x20, x20, #0x80
    adr  x21, saved_handler_sp
    str  x20, [x21]
    mov  sp, x20

    // Case 1: EL1t uses SP_EL0, SA must fault before the load executes.
    adr  x14, case_id
    movz x15, #1
    str  x15, [x14]
    adr  x16, expected_elr
    adr  x17, fault_el1t
    str  x17, [x16]
    adr  x18, resume_elr
    adr  x19, after_el1t_fault
    str  x19, [x18]

    adr  x20, stack_area
    add  x20, x20, #8
    msr  sp_el0, x20
    msr  spsel, #0

fault_el1t:
    ldr  x0, [sp]
    b    fail

after_el1t_fault:
    msr  spsel, #1
    adr  x21, saved_handler_sp
    ldr  x22, [x21]
    subs xzr, sp, x22
    b.ne fail

    // Case 2: EL0 uses SP_EL0, SA0 must fault before the load executes.
    adr  x14, case_id
    movz x15, #2
    str  x15, [x14]
    adr  x16, expected_elr
    adr  x17, fault_el0
    str  x17, [x16]
    adr  x18, resume_elr
    adr  x19, pass
    str  x19, [x18]

    adr  x20, stack_area
    add  x20, x20, #24
    msr  sp_el0, x20
    adr  x21, fault_el0
    msr  elr_el1, x21
    movz x22, #0                   // EL0t
    msr  spsr_el1, x22
    eret

fault_el0:
    ldr  x0, [sp]
    b    fail

pass:
    adr  x21, saved_handler_sp
    ldr  x22, [x21]
    mov  sp, x22
    adr  x11, saved_sctlr
    ldr  x12, [x11]
    msr  sctlr_el1, x12
    isb
    movz x0, #'S'
    str  x0, [x1]
    movz x0, #'\n'
    str  x0, [x1]
    brk  #0

fail:
    adr  x21, saved_handler_sp
    ldr  x22, [x21]
    mov  sp, x22
    adr  x11, saved_sctlr
    ldr  x12, [x11]
    msr  sctlr_el1, x12
    isb
    movz x0, #'F'
    str  x0, [x1]
    brk  #0

    .macro vector_slot target
    b \target
    .space 0x80 - 4
    .endm

    .balign 2048
vectors:
    vector_slot sync_handler       // Current EL, SP0, Sync
    vector_slot fail_from_handler  // Current EL, SP0, IRQ
    vector_slot fail_from_handler  // Current EL, SP0, FIQ
    vector_slot fail_from_handler  // Current EL, SP0, SError
    vector_slot sync_handler       // Current EL, SPx, Sync
    vector_slot fail_from_handler  // Current EL, SPx, IRQ
    vector_slot fail_from_handler  // Current EL, SPx, FIQ
    vector_slot fail_from_handler  // Current EL, SPx, SError
    vector_slot sync_handler       // Lower EL, AArch64, Sync
    vector_slot fail_from_handler  // Lower EL, AArch64, IRQ
    vector_slot fail_from_handler  // Lower EL, AArch64, FIQ
    vector_slot fail_from_handler  // Lower EL, AArch64, SError
    vector_slot fail_from_handler  // Lower EL, AArch32, Sync
    vector_slot fail_from_handler  // Lower EL, AArch32, IRQ
    vector_slot fail_from_handler  // Lower EL, AArch32, FIQ
    vector_slot fail_from_handler  // Lower EL, AArch32, SError

sync_handler:
    mrs  x2, esr_el1
    ubfx x3, x2, #26, #6
    cmp  x3, #0x26
    b.ne fail_from_handler
    and  x4, x2, #0x1ffffff
    cbnz x4, fail_from_handler
    mrs  x5, far_el1
    cbnz x5, fail_from_handler
    mrs  x6, elr_el1
    adr  x7, expected_elr
    ldr  x7, [x7]
    subs xzr, x6, x7
    b.ne fail_from_handler

    adr  x8, resume_elr
    ldr  x8, [x8]
    msr  elr_el1, x8
    movz x9, #5                    // EL1h
    msr  spsr_el1, x9
    eret

fail_from_handler:
    adr  x8, fail
    msr  elr_el1, x8
    movz x9, #5
    msr  spsr_el1, x9
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
saved_handler_sp:
    .quad 0
handler_stack:
    .space 0x80
    .balign 16
stack_area:
    .space 0x40
