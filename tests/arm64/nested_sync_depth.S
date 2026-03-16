    .text
    .global _start

_start:
    movz x1, #0x0000
    movk x1, #0x0900, lsl #16      // UART base

    adr  x10, vectors
    msr  vbar_el1, x10

    adr  x11, depth
    str  xzr, [x11]
    adr  x12, max_seen
    str  xzr, [x12]

    svc  #0

after_nested:
    adr  x11, depth
    adr  x12, max_seen
    ldr  x13, [x11]
    cbnz x13, fail
    ldr  x14, [x12]
    cmp  x14, #16
    b.ne fail

    movz x0, #'P'
    str  x0, [x1]
    movz x0, #'\n'
    str  x0, [x1]
    brk  #0

fail:
    movz x0, #'F'
    str  x0, [x1]
    brk  #0

    .balign 2048
vectors:
    .space 0x200
sync_spx:
    b sync_handler

sync_handler:
    adr  x11, depth
    ldr  x12, [x11]

    adr  x13, saved_elr
    add  x14, x13, x12, lsl #3
    mrs  x15, elr_el1
    str  x15, [x14]

    adr  x13, saved_spsr
    add  x14, x13, x12, lsl #3
    mrs  x15, spsr_el1
    str  x15, [x14]

    add  x12, x12, #1
    str  x12, [x11]

    adr  x13, max_seen
    ldr  x14, [x13]
    cmp  x12, x14
    csel x14, x12, x14, hi
    str  x14, [x13]

    cmp  x12, #16
    b.hs unwind

    svc  #0

unwind:
    adr  x11, depth
    ldr  x12, [x11]
    sub  x12, x12, #1
    str  x12, [x11]

    adr  x13, saved_elr
    add  x14, x13, x12, lsl #3
    ldr  x15, [x14]
    msr  elr_el1, x15

    adr  x13, saved_spsr
    add  x14, x13, x12, lsl #3
    ldr  x15, [x14]
    msr  spsr_el1, x15
    eret

    .data
    .balign 8
depth:
    .quad 0
max_seen:
    .quad 0
saved_elr:
    .zero 16 * 8
saved_spsr:
    .zero 16 * 8
