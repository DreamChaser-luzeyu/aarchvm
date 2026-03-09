    .text
    .global _start
_start:
    movz x1, #0x0000
    movk x1, #0x0900, lsl #16
    movz x2, #0x0000
    movk x2, #0x0a00, lsl #16

    adr  x10, vectors
    msr  vbar_el1, x10

    adr  x3, irq_count
    movz x4, #0
    str  x4, [x3]

    // DAIF read/write checks.
    mrs  x11, daif
    tbnz x11, #7, fail3

    msr  daifset, #2
    mrs  x11, daif
    tbz  x11, #7, fail3

    movz x12, #0x180
    msr  daif, x12
    mrs  x11, daif
    tbz  x11, #8, fail3
    tbz  x11, #7, fail3

    movz x12, #0
    msr  daif, x12
    mrs  x11, daif
    tbnz x11, #7, fail3

    // Phase-1: DAIF mask behavior
    msr  daifset, #2
    dmb  sy
    dsb  sy
    isb
    clrex

    mrs  x5, cntvct_el0
    add  x5, x5, #30
    str  x5, [x2, #8]
    movz x6, #1
    str  x6, [x2, #16]

    movz x7, #120
masked_wait:
    subs x7, x7, #1
    b.ne masked_wait

    ldr  x8, [x3]
    cbnz x8, fail1

    // Unmask and ensure pending IRQ is taken.
    msr  daifclr, #2
wait_first_irq:
    ldr  x8, [x3]
    cbz  x8, wait_first_irq

    // Phase-2: WFI wakeup by next timer IRQ
    mrs  x5, cntvct_el0
    add  x5, x5, #30
    str  x5, [x2, #8]
    movz x6, #1
    str  x6, [x2, #16]

    wfi

    ldr  x8, [x3]
    subs x9, x8, #2
    b.ne fail2

    movz x0, #'W'
    str  x0, [x1]
    movz x0, #'\n'
    str  x0, [x1]
    brk  #0

fail1:
    movz x0, #'1'
    str  x0, [x1]
    brk  #0
fail2:
    movz x0, #'2'
    str  x0, [x1]
    brk  #0
fail3:
    movz x0, #'3'
    str  x0, [x1]
    brk  #0

    .balign 2048
vectors:
    .space 0x280
irq_vector:
    b irq_handler

irq_handler:
    ldr  x8, [x3]
    add  x8, x8, #1
    str  x8, [x3]

    movz x6, #0
    str  x6, [x2, #16]
    eret

    .data
    .balign 8
irq_count:
    .quad 0
