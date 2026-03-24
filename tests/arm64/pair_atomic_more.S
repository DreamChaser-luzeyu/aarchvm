    .arch armv8.1-a
    .text
    .global _start
_start:
    movz x1, #0x0000
    movk x1, #0x0900, lsl #16      // UART

    adr  x10, vectors
    msr  vbar_el1, x10

    adr  x20, pair32_slot
    adr  x21, pair64_slot
    add  x22, x20, #4              // misaligned for 32-bit pair total-size access
    add  x23, x21, #8              // misaligned for 64-bit pair total-size access
    adr  x24, handled
    str  xzr, [x24]
    adr  x2, phase
    movz x3, #'1'
    str  x3, [x2]

    // 32-bit pair exclusive success path.
    movz w2, #0x1111
    movz w3, #0x2222
    stp  w2, w3, [x20]

    ldxp w4, w5, [x20]
    cmp  w4, w2
    b.ne fail
    cmp  w5, w3
    b.ne fail

    movz w6, #0x3333
    movz w7, #0x4444
    stxp w8, w6, w7, [x20]
    cbnz w8, fail
    ldp  w9, w10, [x20]
    cmp  w9, w6
    b.ne fail
    cmp  w10, w7
    b.ne fail

    ldaxp w11, w12, [x20]
    cmp  w11, w6
    b.ne fail
    cmp  w12, w7
    b.ne fail

    movz w13, #0x5555
    movz w14, #0x6666
    stlxp w15, w13, w14, [x20]
    cbnz w15, fail
    ldp  w16, w17, [x20]
    cmp  w16, w13
    b.ne fail
    cmp  w17, w14
    b.ne fail

fault_ldxp32:
    ldxp w18, w19, [x22]
    b    fail
after_ldxp32:
    ldr  x0, [x24]
    cmp  x0, #1
    b.ne fail
    adr  x2, phase
    movz x3, #'2'
    str  x3, [x2]

    movz w25, #0x007a
    movz w26, #0x7777
    movz w27, #0x8888
fault_stxp32:
    stxp w25, w26, w27, [x22]
    b    fail
after_stxp32:
    ldr  x0, [x24]
    cmp  x0, #2
    b.ne fail
    cmp  w25, #0x007a
    b.ne fail
    adr  x2, phase
    movz x3, #'3'
    str  x3, [x2]

    movz x2, #0x1111
    movk x2, #0x2222, lsl #16
    movk x2, #0x3333, lsl #32
    movk x2, #0x4444, lsl #48
    movz x3, #0x5555
    movk x3, #0x6666, lsl #16
    movk x3, #0x7777, lsl #32
    movk x3, #0x8888, lsl #48
    stp  x2, x3, [x21]

fault_ldxp64:
    ldxp x4, x5, [x23]
    b    fail
after_ldxp64:
    ldr  x0, [x24]
    cmp  x0, #3
    b.ne fail
    adr  x2, phase
    movz x3, #'4'
    str  x3, [x2]

    movz w28, #0x005b
    movz x6, #0x9999
    movk x6, #0xaaaa, lsl #16
    movk x6, #0xbbbb, lsl #32
    movk x6, #0xcccc, lsl #48
    movz x7, #0xdddd
    movk x7, #0xeeee, lsl #16
    movk x7, #0xffff, lsl #32
    movk x7, #0x1111, lsl #48
fault_stxp64:
    stxp w28, x6, x7, [x23]
    b    fail
after_stxp64:
    ldr  x0, [x24]
    cmp  x0, #4
    b.ne fail
    cmp  w28, #0x005b
    b.ne fail
    adr  x2, phase
    movz x3, #'5'
    str  x3, [x2]

    movz w2, #0xaaaa
    movz w3, #0xbbbb
    movz w4, #0xcccc
    movz w5, #0xdddd
fault_casp32:
    casp w2, w3, w4, w5, [x22]
    b    fail
after_casp32:
    ldr  x0, [x24]
    cmp  x0, #5
    b.ne fail
    movz w29, #0xaaaa
    cmp  w2, w29
    b.ne fail
    movz w29, #0xbbbb
    cmp  w3, w29
    b.ne fail
    adr  x2, phase
    movz x3, #'6'
    str  x3, [x2]

    movz x8, #0x1010
    movk x8, #0x2020, lsl #16
    movk x8, #0x3030, lsl #32
    movk x8, #0x4040, lsl #48
    movz x9, #0x5050
    movk x9, #0x6060, lsl #16
    movk x9, #0x7070, lsl #32
    movk x9, #0x8080, lsl #48
    movz x10, #0x9090
    movk x10, #0xa0a0, lsl #16
    movk x10, #0xb0b0, lsl #32
    movk x10, #0xc0c0, lsl #48
    movz x11, #0xd0d0
    movk x11, #0xe0e0, lsl #16
    movk x11, #0xf0f0, lsl #32
    movk x11, #0x1111, lsl #48
fault_casp64:
    casp x8, x9, x10, x11, [x23]
    b    fail
after_casp64:
    ldr  x0, [x24]
    cmp  x0, #6
    b.ne fail
    movz x13, #0x1010
    movk x13, #0x2020, lsl #16
    movk x13, #0x3030, lsl #32
    movk x13, #0x4040, lsl #48
    cmp  x8, x13
    b.ne fail
    movz x13, #0x5050
    movk x13, #0x6060, lsl #16
    movk x13, #0x7070, lsl #32
    movk x13, #0x8080, lsl #48
    cmp  x9, x13
    b.ne fail

    movz x0, #'Q'
    str  x0, [x1]
    movz x0, #'\n'
    str  x0, [x1]
    brk  #0

fail:
    adr  x2, phase
    ldr  x0, [x2]
    str  x0, [x1]
    brk  #0

    .balign 2048
vectors:
    .space 0x200
sync_spx:
    b sync_handler

sync_handler:
    adr  x29, saved_regs
    str  x2, [x29]
    str  x3, [x29, #8]
    str  x8, [x29, #16]
    str  x9, [x29, #24]
    mrs  x2, esr_el1
    ubfx x3, x2, #26, #6
    cmp  x3, #0x25
    b.ne fail_from_handler
    and  x3, x2, #0x3f
    cmp  x3, #0x21
    b.ne fail_from_handler
    mrs  x4, elr_el1

    adr  x5, fault_ldxp32
    cmp  x4, x5
    b.eq handle_ldxp32
    adr  x5, fault_stxp32
    cmp  x4, x5
    b.eq handle_stxp32
    adr  x5, fault_ldxp64
    cmp  x4, x5
    b.eq handle_ldxp64
    adr  x5, fault_stxp64
    cmp  x4, x5
    b.eq handle_stxp64
    adr  x5, fault_casp32
    cmp  x4, x5
    b.eq handle_casp32
    adr  x5, fault_casp64
    cmp  x4, x5
    b.eq handle_casp64
    b    fail_from_handler

handle_ldxp32:
    tbnz x2, #6, fail_from_handler
    mrs  x6, far_el1
    cmp  x6, x22
    b.ne fail_from_handler
    movz x7, #1
    str  x7, [x24]
    adr  x30, saved_regs
    ldr  x2, [x30]
    ldr  x3, [x30, #8]
    ldr  x8, [x30, #16]
    ldr  x9, [x30, #24]
    adr  x30, after_ldxp32
    msr  elr_el1, x30
    eret

handle_stxp32:
    tbz  x2, #6, fail_from_handler
    mrs  x6, far_el1
    cmp  x6, x22
    b.ne fail_from_handler
    movz x7, #2
    str  x7, [x24]
    adr  x30, saved_regs
    ldr  x2, [x30]
    ldr  x3, [x30, #8]
    ldr  x8, [x30, #16]
    ldr  x9, [x30, #24]
    adr  x30, after_stxp32
    msr  elr_el1, x30
    eret

handle_ldxp64:
    tbnz x2, #6, fail_from_handler
    mrs  x6, far_el1
    cmp  x6, x23
    b.ne fail_from_handler
    movz x7, #3
    str  x7, [x24]
    adr  x30, saved_regs
    ldr  x2, [x30]
    ldr  x3, [x30, #8]
    ldr  x8, [x30, #16]
    ldr  x9, [x30, #24]
    adr  x30, after_ldxp64
    msr  elr_el1, x30
    eret

handle_stxp64:
    tbz  x2, #6, fail_from_handler
    mrs  x6, far_el1
    cmp  x6, x23
    b.ne fail_from_handler
    movz x7, #4
    str  x7, [x24]
    adr  x30, saved_regs
    ldr  x2, [x30]
    ldr  x3, [x30, #8]
    ldr  x8, [x30, #16]
    ldr  x9, [x30, #24]
    adr  x30, after_stxp64
    msr  elr_el1, x30
    eret

handle_casp32:
    tbnz x2, #6, fail_from_handler
    mrs  x6, far_el1
    cmp  x6, x22
    b.ne fail_from_handler
    movz x7, #5
    str  x7, [x24]
    adr  x30, saved_regs
    ldr  x2, [x30]
    ldr  x3, [x30, #8]
    ldr  x8, [x30, #16]
    ldr  x9, [x30, #24]
    adr  x30, after_casp32
    msr  elr_el1, x30
    eret

handle_casp64:
    tbnz x2, #6, fail_from_handler
    mrs  x6, far_el1
    cmp  x6, x23
    b.ne fail_from_handler
    movz x7, #6
    str  x7, [x24]
    adr  x30, saved_regs
    ldr  x2, [x30]
    ldr  x3, [x30, #8]
    ldr  x8, [x30, #16]
    ldr  x9, [x30, #24]
    adr  x30, after_casp64
    msr  elr_el1, x30
    eret

fail_from_handler:
    adr  x8, fail
    msr  elr_el1, x8
    eret

    .data
    .balign 16
pair32_slot:
    .word 0
    .word 0
    .word 0
    .word 0
pair64_slot:
    .quad 0
    .quad 0
    .quad 0
handled:
    .quad 0
phase:
    .quad '0'
saved_regs:
    .quad 0
    .quad 0
    .quad 0
    .quad 0
