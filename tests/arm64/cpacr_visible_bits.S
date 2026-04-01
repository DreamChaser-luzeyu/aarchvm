    .text
    .global _start
_start:
    movz x20, #0
    movk x20, #0x0900, lsl #16

    mrs  x0, cpacr_el1
    cbnz x0, fail0

    movn x1, #0
    msr  cpacr_el1, x1
    isb
    mrs  x2, cpacr_el1
    movz x3, #0x0030, lsl #16
    cmp  x2, x3
    b.ne fail1

    movz x1, #0xffff
    movk x1, #0x1210, lsl #16
    movk x1, #0xabcd, lsl #32
    movk x1, #0x4321, lsl #48
    msr  cpacr_el1, x1
    isb
    mrs  x2, cpacr_el1
    movz x3, #0x0010, lsl #16
    cmp  x2, x3
    b.ne fail2

    movz x0, #'P'
    str  x0, [x20]
    movz x0, #'\n'
    str  x0, [x20]
    brk  #0

fail0:
    movz x0, #'0'
    b out
fail1:
    movz x0, #'1'
    b out
fail2:
    movz x0, #'2'
    b out

out:
    str  x0, [x20]
    movz x0, #'F'
    str  x0, [x20]
    brk  #0
