    .text
    .global _start
_start:
    movz x20, #0x0000
    movk x20, #0x0900, lsl #16

    ldr x0, =0x3ff0000000000000 // 1.0
    ldr x1, =0x4000000000000000 // 2.0
    fmov d0, x0
    fmov d1, x1

    cmp xzr, xzr
    fcsel d2, d0, d1, eq
    fmov x2, d2
    cmp x2, x0
    b.ne fail

    mov x4, #0
    mov x5, #1
    cmp x4, x5
    fcsel d3, d0, d1, eq
    fmov x3, d3
    cmp x3, x1
    b.ne fail

    movz x0, #'W'
    str  x0, [x20]
    movz x0, #'\n'
    str  x0, [x20]
    brk  #0

fail:
    movz x0, #'F'
    str  x0, [x20]
    brk  #0
