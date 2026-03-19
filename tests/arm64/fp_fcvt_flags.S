    .text
    .global _start
_start:
    movz x20, #0
    movk x20, #0x0900, lsl #16
    movz x25, #0x0030, lsl #16
    msr  cpacr_el1, x25
    isb

    msr fpsr, xzr
    adr  x1, val_exact
    ldr  d0, [x1]
    fcvtzu x2, d0
    cmp  x2, #7
    b.ne fail
    mrs  x3, fpsr
    cbnz x3, fail

    msr fpsr, xzr
    adr  x1, val_frac
    ldr  d0, [x1]
    fcvtzu x2, d0
    cmp  x2, #3
    b.ne fail
    mrs  x3, fpsr
    mov  x4, #(1 << 4)
    cmp  x3, x4
    b.ne fail

    msr fpsr, xzr
    adr  x1, val_neg
    ldr  d0, [x1]
    fcvtzu x2, d0
    cbnz x2, fail
    mrs  x3, fpsr
    mov  x4, #1
    cmp  x3, x4
    b.ne fail

    msr fpsr, xzr
    adr  x1, val_inf
    ldr  d0, [x1]
    fcvtzu x2, d0
    cmp  x2, -1
    b.ne fail
    mrs  x3, fpsr
    mov  x4, #1
    cmp  x3, x4
    b.ne fail

    msr fpsr, xzr
    adr  x1, val_sfrac
    ldr  d0, [x1]
    fcvtzs x2, d0
    mov  x4, #-3
    cmp  x2, x4
    b.ne fail
    mrs  x3, fpsr
    mov  x4, #(1 << 4)
    cmp  x3, x4
    b.ne fail

    movz x0, #'Z'
    str  x0, [x20]
    movz x0, #'\n'
    str  x0, [x20]
    brk  #0

fail:
    movz x0, #'F'
    str  x0, [x20]
    brk  #0

    .data
    .balign 8
val_exact:
    .quad 0x401c000000000000  // 7.0
val_frac:
    .quad 0x400e000000000000  // 3.75
val_neg:
    .quad 0xbff0000000000000  // -1.0
val_inf:
    .quad 0x7ff0000000000000  // +inf
val_sfrac:
    .quad 0xc00e000000000000  // -3.75
