    .text
    .global _start
_start:
    movz x1, #0
    movk x1, #0x0900, lsl #16

    adr  x10, vectors
    msr  vbar_el1, x10

    movz x2, #0x1111
    movk x2, #0x1111, lsl #16
    movz x3, #0x2222
    movk x3, #0x2222, lsl #16
    movz x4, #0x3333
    movk x4, #0x3333, lsl #16
    movz x5, #0x4444
    movk x5, #0x4444, lsl #16
    movz x6, #0x5555
    movk x6, #0x5555, lsl #16
    movz x7, #0x6666
    movk x7, #0x6666, lsl #16
    movz x8, #0x7777
    movk x8, #0x7777, lsl #16
    movz x9, #0x8888
    movk x9, #0x8888, lsl #16
    adr  x11, scratch
    mov  x20, xzr

    adr  x21, inst_addg
inst_addg:
    .inst 0x91800062              // ADDG X2, X3, #0, #0
    b    fail

    adr  x21, inst_subg
inst_subg:
    .inst 0xD18104A4              // SUBG X4, X5, #0x10, #1
    b    fail

    adr  x21, inst_irg
inst_irg:
    .inst 0x9ADF10E6              // IRG X6, X7
    b    fail

    adr  x21, inst_gmi
inst_gmi:
    .inst 0x9ACA1528              // GMI X8, X9, X10
    b    fail

    adr  x21, inst_stgp
inst_stgp:
    .inst 0x69000D62              // STGP X2, X3, [X11]
    b    fail

    adr  x21, inst_stg
inst_stg:
    .inst 0xD9200964              // STG X4, [X11]
    b    fail

    adr  x21, inst_ldg
inst_ldg:
    .inst 0xD960016C              // LDG X12, [X11]
    b    fail

    cmp  x20, #7
    b.ne fail

    movz x0, #0x3333
    movk x0, #0x3333, lsl #16
    cmp  x4, x0
    b.ne fail
    movz x0, #0x4444
    movk x0, #0x4444, lsl #16
    cmp  x5, x0
    b.ne fail
    movz x0, #0x5555
    movk x0, #0x5555, lsl #16
    cmp  x6, x0
    b.ne fail
    movz x0, #0x6666
    movk x0, #0x6666, lsl #16
    cmp  x7, x0
    b.ne fail
    movz x0, #0x7777
    movk x0, #0x7777, lsl #16
    cmp  x8, x0
    b.ne fail
    movz x0, #0x8888
    movk x0, #0x8888, lsl #16
    cmp  x9, x0
    b.ne fail
    ldr  x0, [x11]
    cbnz x0, fail
    ldr  x0, [x11, #8]
    cbnz x0, fail

    movz x0, #'M'
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
    mrs  x0, esr_el1
    ubfx x2, x0, #26, #6
    cbnz x2, fail_from_handler
    tbz  x0, #25, fail_from_handler
    and  x2, x0, #0x1ffffff
    cbnz x2, fail_from_handler
    mrs  x2, far_el1
    cbnz x2, fail_from_handler
    mrs  x2, elr_el1
    cmp  x2, x21
    b.ne fail_from_handler
    add  x20, x20, #1
    add  x2, x2, #8
    msr  elr_el1, x2
    eret

fail_from_handler:
    adr  x2, fail
    msr  elr_el1, x2
    eret

    .balign 16
scratch:
    .quad 0
    .quad 0
