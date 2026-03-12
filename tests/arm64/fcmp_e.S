    .text
    .global _start
_start:
    movz x20, #0x0000
    movk x20, #0x0900, lsl #16

    ldr x0, =0x4000000000000000 // 2.0
    ldr x1, =0xc008000000000000 // -3.0
    fmov d0, x0
    fmov d1, x1

    fcmpe d0, d1
    mrs x2, nzcv
    ldr x3, =0x20000000
    cmp x2, x3
    b.ne fail

    fcmpe d1, #0.0
    mrs x2, nzcv
    ldr x3, =0x80000000
    cmp x2, x3
    b.ne fail

    fcmpe d0, #0.0
    mrs x2, nzcv
    ldr x3, =0x20000000
    cmp x2, x3
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
