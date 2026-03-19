    .text
    .global _start
_start:
    movz x20, #0x0000
    movk x20, #0x0900, lsl #16

    adr  x21, slot
    movz w0, #1
    str  w0, [x21]

    adr  x10, vectors
    msr  vbar_el1, x10

    ldxr w1, [x21]
    cmp  w1, #1
    b.ne fail
    svc  #0

after_svc:
    movz w2, #2
    stxr w3, w2, [x21]
    cbz  w3, fail
    ldr  w4, [x21]
    cmp  w4, #1
    b.ne fail

    movz x0, #'E'
    str  x0, [x20]
    movz x0, #'\n'
    str  x0, [x20]
    brk  #0

fail:
    movz x0, #'F'
    str  x0, [x20]
    brk  #0

    .balign 2048
vectors:
    .space 0x200
sync_spx:
    b sync_handler
    .space 0x5fc

sync_handler:
    mrs  x5, esr_el1
    ubfx x5, x5, #26, #6
    cmp  x5, #0x15
    b.ne fail_from_handler
    adr  x8, after_svc
    msr  elr_el1, x8
    eret

fail_from_handler:
    adr  x8, fail
    msr  elr_el1, x8
    eret

    .data
    .balign 8
slot:
    .word 0
    .word 0
