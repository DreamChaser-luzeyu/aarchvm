    .text
    .global _start
_start:
    movz x20, #0
    movk x20, #0x0900, lsl #16

    movn x0, #0
    msr  contextidr_el1, x0
    isb
    mrs  x1, contextidr_el1
    movz x2, #0xffff
    movk x2, #0xffff, lsl #16
    cmp  x1, x2
    b.ne fail1

    movz x0, #0x5678
    movk x0, #0x1234, lsl #16
    movk x0, #0xcdef, lsl #32
    movk x0, #0x89ab, lsl #48
    msr  contextidr_el1, x0
    isb
    mrs  x1, contextidr_el1
    movz x2, #0x5678
    movk x2, #0x1234, lsl #16
    cmp  x1, x2
    b.ne fail2

    movz x0, #'C'
    str  x0, [x20]
    movz x0, #'\n'
    str  x0, [x20]
    brk  #0

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
