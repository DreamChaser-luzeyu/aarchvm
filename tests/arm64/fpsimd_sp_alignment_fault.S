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
    movz x13, #0x18                // SA | SA0
    orr  x12, x12, x13
    msr  sctlr_el1, x12
    isb

    adr  x20, handler_stack
    add  x20, x20, #0x80
    adr  x21, saved_handler_sp
    str  x20, [x21]
    msr  sp_el1, x20
    mov  sp, x20

    adr  x0, store_source
    ldr  q2, [x0]

    // Case 1: whole-register LDR Q with SP base must take SP alignment fault.
    adr  x16, expected_elr
    adr  x17, fault_ldr_q
    str  x17, [x16]
    adr  x18, resume_elr
    adr  x19, after_ldr_q
    str  x19, [x18]
    adr  x20, stack_area
    add  x20, x20, #8
    msr  sp_el0, x20
    msr  spsel, #0

fault_ldr_q:
    ldr  q0, [sp]
    b    fail

after_ldr_q:
    msr  spsel, #1
    bl   check_handler_sp

    // Case 2: single-structure lane LD1 with SP base must also fault first.
    adr  x16, expected_elr
    adr  x17, fault_ld1_lane
    str  x17, [x16]
    adr  x18, resume_elr
    adr  x19, after_ld1_lane
    str  x19, [x18]
    adr  x20, stack_area
    add  x20, x20, #24
    msr  sp_el0, x20
    msr  spsel, #0

fault_ld1_lane:
    ld1  {v1.b}[0], [sp]
    b    fail

after_ld1_lane:
    msr  spsel, #1
    bl   check_handler_sp

    // Case 3: whole-register structured ST1 post-index must fault before
    // any writeback or memory update.
    adr  x16, expected_elr
    adr  x17, fault_st1_whole_post
    str  x17, [x16]
    adr  x18, resume_elr
    adr  x19, after_st1_whole_post
    str  x19, [x18]
    adr  x20, expected_sp
    adr  x21, store_target
    add  x22, x21, #8
    str  x22, [x20]
    msr  sp_el0, x22
    msr  spsel, #0

fault_st1_whole_post:
    st1  {v2.16b}, [sp], #16
    b    fail

after_st1_whole_post:
    msr  spsel, #1
    bl   check_handler_sp

    msr  spsel, #0
    mov  x3, sp
    msr  spsel, #1
    adr  x4, expected_sp
    ldr  x4, [x4]
    subs xzr, x3, x4
    b.ne fail

    adr  x0, store_target
    add  x0, x0, #8
    adr  x1, zero16
    mov  x2, #16
    bl   check_mem

    adr  x11, saved_sctlr
    ldr  x12, [x11]
    msr  sctlr_el1, x12
    isb
    movz x0, #'Z'
    movz x1, #0
    movk x1, #0x0900, lsl #16
    str  x0, [x1]
    movz x0, #'\n'
    str  x0, [x1]
    brk  #0

fail:
    adr  x21, saved_handler_sp
    ldr  x22, [x21]
    mov  sp, x22
    msr  spsel, #1
    adr  x11, saved_sctlr
    ldr  x12, [x11]
    msr  sctlr_el1, x12
    isb
    movz x1, #0
    movk x1, #0x0900, lsl #16
    movz x0, #'F'
    str  x0, [x1]
    brk  #0

check_handler_sp:
    adr  x21, saved_handler_sp
    ldr  x22, [x21]
    subs xzr, sp, x22
    b.ne fail
    ret

check_mem:
    cbz  x2, check_mem_ok
check_mem_loop:
    ldrb w3, [x0], #1
    ldrb w4, [x1], #1
    cmp  w3, w4
    b.ne fail
    subs x2, x2, #1
    b.ne check_mem_loop
check_mem_ok:
    ret

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
    vector_slot fail_from_handler  // Lower EL, AArch64, Sync
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
expected_elr:
    .quad 0
resume_elr:
    .quad 0
expected_sp:
    .quad 0
saved_handler_sp:
    .quad 0
handler_stack:
    .space 0x80
    .balign 16
stack_area:
    .space 0x40
    .balign 16
store_source:
    .byte 0xa0,0xa1,0xa2,0xa3,0xa4,0xa5,0xa6,0xa7
    .byte 0xa8,0xa9,0xaa,0xab,0xac,0xad,0xae,0xaf
store_target:
    .space 64
zero16:
    .space 16
