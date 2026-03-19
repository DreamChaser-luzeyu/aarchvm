    .text
    .global _start
_start:
    movz x20, #0x0000
    movk x20, #0x0900, lsl #16
    movz x25, #0x0030, lsl #16
    msr  cpacr_el1, x25
    isb

    mov x0, #42
    scvtf d0, x0
    fmov x1, d0
    ldr x2, =0x4045000000000000 // 42.0
    cmp x1, x2
    b.ne fail

    mov w3, #7
    ucvtf d1, w3
    fmov x4, d1
    ldr x5, =0x401c000000000000 // 7.0
    cmp x4, x5
    b.ne fail

    ldr x6, =0x4012800000000000 // 4.625
    fmov d2, x6
    fcvtzs d3, d2
    scvtf d4, d3
    fmov x7, d4
    ldr x8, =0x4010000000000000 // 4.0
    cmp x7, x8
    b.ne fail

    mov x9, #9
    fmov d5, x9
    ucvtf d6, d5
    fmov x10, d6
    ldr x11, =0x4022000000000000 // 9.0
    cmp x10, x11
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
