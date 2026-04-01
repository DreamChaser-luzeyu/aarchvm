    .text
    .global _start
_start:
    movz x20, #0
    movk x20, #0x0900, lsl #16

    mrs  x9, id_aa64mmfr0_el1
    ubfx x9, x9, #4, #4
    mrs  x10, id_aa64mmfr2_el1
    ubfx x10, x10, #12, #4

    movz x0, #0x5001
    movk x0, #0x1234, lsl #16
    movk x0, #0x0000, lsl #32
    movk x0, #0xabcd, lsl #48
    mov  x1, x0
    cmp  x9, #2
    b.eq 1f
    movz x2, #0xffff
    movk x2, #0xffff, lsl #16
    movk x2, #0xffff, lsl #32
    movk x2, #0x00ff, lsl #48
    and  x1, x1, x2
1:
    cmp  x10, #1
    b.eq 2f
    bic  x1, x1, #1
2:
    msr  ttbr0_el1, x0
    isb
    mrs  x3, ttbr0_el1
    cmp  x3, x1
    b.ne fail1

    movz x4, #0x9001
    movk x4, #0x5678, lsl #16
    movk x4, #0x0000, lsl #32
    movk x4, #0x1357, lsl #48
    mov  x5, x4
    cmp  x9, #2
    b.eq 3f
    movz x2, #0xffff
    movk x2, #0xffff, lsl #16
    movk x2, #0xffff, lsl #32
    movk x2, #0x00ff, lsl #48
    and  x5, x5, x2
3:
    cmp  x10, #1
    b.eq 4f
    bic  x5, x5, #1
4:
    msr  ttbr1_el1, x4
    isb
    mrs  x6, ttbr1_el1
    cmp  x6, x5
    b.ne fail2

    movz x0, #'T'
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
