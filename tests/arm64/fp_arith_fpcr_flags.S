    .text
    .global _start
_start:
    movz x20, #0
    movk x20, #0x0900, lsl #16
    movz x25, #0x0030, lsl #16
    msr  cpacr_el1, x25
    isb

    movz x0, #0x40, lsl #16 // FPCR.RMode = +Inf
    msr  fpcr, x0
    isb
    msr  fpsr, xzr
    adr  x21, add_a_s
    ldr  s0, [x21]
    adr  x21, add_b_s
    ldr  s1, [x21]
    fadd s2, s0, s1
    fmov w2, s2
    ldr  w3, =0x3f800001
    cmp  w2, w3
    b.ne fail1
    mrs  x2, fpsr
    mov  x3, #0x10
    cmp  x2, x3
    b.ne fail2

    msr  fpcr, xzr
    isb
    msr  fpsr, xzr
    adr  x21, div_a_d
    ldr  d4, [x21]
    adr  x21, div_b_d
    ldr  d5, [x21]
    fdiv d6, d4, d5
    fmov x2, d6
    ldr  x3, =0x7ff0000000000000
    cmp  x2, x3
    b.ne fail3
    mrs  x2, fpsr
    mov  x3, #0x2
    cmp  x2, x3
    b.ne fail4

    msr  fpsr, xzr
    adr  x21, fma_a_s
    ldr  s8, [x21]
    adr  x21, fma_b_s
    ldr  s9, [x21]
    adr  x21, fma_c_s
    ldr  s10, [x21]
    fmadd s11, s8, s9, s10
    fmov w2, s11
    ldr  w3, =0xb3800000
    cmp  w2, w3
    b.ne fail5
    mrs  x2, fpsr
    cbnz x2, fail6

    movz x0, #'A'
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
fail3:
    movz x0, #'3'
    b out
fail4:
    movz x0, #'4'
    b out
fail5:
    movz x0, #'5'
    b out
fail6:
    movz x0, #'6'
out:
    str  x0, [x20]
    brk  #0

    .data
    .balign 8
add_a_s:
    .word 0x3f800000
add_b_s:
    .word 0x33800000
div_a_d:
    .quad 0x3ff0000000000000
div_b_d:
    .quad 0x0000000000000000
fma_a_s:
    .word 0x3f800800
fma_b_s:
    .word 0x3f7ff000
fma_c_s:
    .word 0xbf800000
