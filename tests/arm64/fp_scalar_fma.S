    .text
    .global _start
_start:
    movz x20, #0x0000
    movk x20, #0x0900, lsl #16
    movz x25, #0x0030, lsl #16
    msr  cpacr_el1, x25
    isb

    ldr x0, =0x3ff8000000000000 // 1.5
    ldr x1, =0x4000000000000000 // 2.0
    ldr x2, =0x3fe0000000000000 // 0.5
    fmov d0, x0
    fmov d1, x1
    fmov d2, x2

    fmadd d3, d0, d1, d2
    fmov x3, d3
    ldr x4, =0x400c000000000000 // 3.5
    cmp x3, x4
    b.ne fail

    fmsub d4, d0, d1, d2
    fmov x3, d4
    ldr x4, =0xc004000000000000 // -2.5
    cmp x3, x4
    b.ne fail

    fnmadd d5, d0, d1, d2
    fmov x3, d5
    ldr x4, =0xc00c000000000000 // -3.5
    cmp x3, x4
    b.ne fail

    fnmsub d6, d0, d1, d2
    fmov x3, d6
    ldr x4, =0x4004000000000000 // 2.5
    cmp x3, x4
    b.ne fail

    mov  w0, #0x3fc00000 // 1.5f
    mov  w1, #0x40000000 // 2.0f
    mov  w2, #0x3f000000 // 0.5f
    fmov s16, w0
    fmov s17, w1
    fmov s18, w2

    fmadd s19, s16, s17, s18
    fmov w3, s19
    mov  w4, #0x40600000 // 3.5f
    cmp  w3, w4
    b.ne fail

    fmsub s20, s16, s17, s18
    fmov w3, s20
    mov  w4, #0xc0200000 // -2.5f
    cmp  w3, w4
    b.ne fail

    fnmadd s21, s16, s17, s18
    fmov w3, s21
    mov  w4, #0xc0600000 // -3.5f
    cmp  w3, w4
    b.ne fail

    fnmsub s22, s16, s17, s18
    fmov w3, s22
    mov  w4, #0x40200000 // 2.5f
    cmp  w3, w4
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
