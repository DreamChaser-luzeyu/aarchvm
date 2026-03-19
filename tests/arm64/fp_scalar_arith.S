    .text
    .global _start
_start:
    movz x20, #0x0000
    movk x20, #0x0900, lsl #16
    movz x25, #0x0030, lsl #16
    msr  cpacr_el1, x25
    isb

    ldr x0, =0x4000000000000000 // 2.0
    ldr x1, =0xc008000000000000 // -3.0
    ldr x2, =0x401c000000000000 // 7.0
    ldr x3, =0x4000000000000000 // 2.0
    ldr x4, =0x4014000000000000 // 5.0
    ldr x5, =0x3ff8000000000000 // 1.5
    fmov d0, x0
    fmov d1, x1
    fmul d2, d0, d1
    fmov x6, d2
    ldr x7, =0xc018000000000000 // -6.0
    cmp x6, x7
    b.ne fail

    fmov d3, x4
    fmov d4, x5
    fadd d5, d3, d4
    fmov x6, d5
    ldr x7, =0x401a000000000000 // 6.5
    cmp x6, x7
    b.ne fail

    fsub d6, d3, d4
    fmov x6, d6
    ldr x7, =0x400c000000000000 // 3.5
    cmp x6, x7
    b.ne fail

    fmov d7, x2
    fmov d8, x3
    fdiv d9, d7, d8
    fmov x6, d9
    ldr x7, =0x400c000000000000 // 3.5
    cmp x6, x7
    b.ne fail

    fabs d10, d1
    fmov x6, d10
    ldr x7, =0x4008000000000000 // 3.0
    cmp x6, x7
    b.ne fail

    fneg d11, d0
    fmov x6, d11
    ldr x7, =0xc000000000000000 // -2.0
    cmp x6, x7
    b.ne fail

    fcmp d0, d1
    mrs x6, nzcv
    ldr x7, =0x20000000
    cmp x6, x7
    b.ne fail

    fcmp d1, d0
    mrs x6, nzcv
    ldr x7, =0x80000000
    cmp x6, x7
    b.ne fail

    fcmp d0, d0
    mrs x6, nzcv
    ldr x7, =0x60000000
    cmp x6, x7
    b.ne fail

    fcmp d1, #0.0
    mrs x6, nzcv
    ldr x7, =0x80000000
    cmp x6, x7
    b.ne fail

    ldr x0, =0x400e000000000000 // 3.75
    fmov d12, x0
    fcvtzs w13, d12
    mov w14, #3
    cmp w13, w14
    b.ne fail

    ldr x0, =0xc00e000000000000 // -3.75
    fmov d13, x0
    fcvtzs x15, d13
    mov x16, #-3
    cmp x15, x16
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
