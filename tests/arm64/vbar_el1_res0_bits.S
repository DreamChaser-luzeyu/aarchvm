    .text
    .global _start
_start:
    movz x1, #0
    movk x1, #0x0900, lsl #16

    adr  x10, vectors
    add  x11, x10, #0x123
    msr  vbar_el1, x11
    isb

    mrs  x12, vbar_el1
    cmp  x12, x10
    b.ne fail1

    adr  x13, flag
    str  xzr, [x13]

    svc  #0

after_svc:
    ldr  x14, [x13]
    cbz  x14, fail2

    movz x0, #'V'
    str  x0, [x1]
    movz x0, #'\n'
    str  x0, [x1]
    brk  #0

fail1:
    movz x0, #'1'
    b out
fail2:
    movz x0, #'2'
    b out
out:
    str  x0, [x1]
    movz x0, #'F'
    str  x0, [x1]
    brk  #0

    .balign 2048
vectors:
    .space 0x200
sync_spx:
    b svc_handler

svc_handler:
    adr  x13, flag
    movz x14, #1
    str  x14, [x13]
    eret

    .data
    .balign 8
flag:
    .quad 0
