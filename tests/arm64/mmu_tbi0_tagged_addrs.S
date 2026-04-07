    .text
    .global _start
_start:
    movz x1, #0
    movk x1, #0x0900, lsl #16      // UART

    movz x9, #3
    movz x20, #0x1000              // L0
    movz x21, #0x2000              // L1
    movz x16, #0x0401              // L1 block attrs: AF + block

    // L0[0] -> L1, L1[0] -> low 1GB identity block.
    orr  x2, x21, x9
    str  x2, [x20]
    str  x16, [x21]

    msr  ttbr0_el1, x20
    movz x2, #16
    movk x2, #0x20, lsl #32        // TBI0=1
    msr  tcr_el1, x2
    movz x2, #0
    msr  mair_el1, x2
    movz x2, #1
    msr  sctlr_el1, x2
    isb

    // Tagged data address must still translate through TTBR0.
    adr  x20, data_slot
    movz x21, #0xab00, lsl #48
    orr  x20, x20, x21
    movz x22, #0x55
    str  x22, [x20]
    ldr  x23, [x20]
    cmp  x23, x22
    b.ne fail

    // AT should also ignore the top byte and report success in PAR_EL1.
    at   s1e1r, x20
    mrs  x24, par_el1
    tbnz x24, #0, fail
    tbz  x24, #11, fail

    // Tagged branch target must execute, and the stored PC must be canonical.
    adr  x19, tagged_target
    orr  x19, x19, x21
    blr  x19
    cmp  x0, #0x50                // 'P'
    b.ne fail

    movz x2, #0
    msr  sctlr_el1, x2
    isb
    movz x0, #'T'
    str  x0, [x1]
    movz x0, #'\n'
    str  x0, [x1]
    brk  #0

tagged_target:
tagged_here:
    adr  x12, tagged_here
    lsr  x13, x12, #56
    cbnz x13, fail
    movz x0, #'P'
    ret

fail:
    movz x2, #0
    msr  sctlr_el1, x2
    isb
    movz x0, #'F'
    str  x0, [x1]
    brk  #0

    .balign 8
data_slot:
    .quad 0
