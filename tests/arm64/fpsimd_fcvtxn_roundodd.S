    .text
    .global _start
_start:
    movz x20, #0
    movk x20, #0x0900, lsl #16
    adr  x21, dst
    movz x25, #0x0030, lsl #16
    msr  cpacr_el1, x25
    isb

    mov  x0, #(1 << 22)
    msr  fpcr, x0
    isb

    msr  fpsr, xzr
    adr  x22, src_vec
    ldr  q1, [x22]
    fcvtxn v0.2s, v1.2d
    str  q0, [x21]
    mov  x0, x21
    adr  x1, exp_vec
    bl   check16_1
    mrs  x2, fpsr
    mov  x3, #(1 << 4)
    cmp  x2, x3
    b.ne fail1

    mov  x0, #(2 << 22)
    msr  fpcr, x0
    isb
    msr  fpsr, xzr
    adr  x22, src_scalar_inexact
    ldr  d3, [x22]
    fcvtxn s2, d3
    str  s2, [x21, #16]
    ldr  w2, [x21, #16]
    ldr  w3, =0x3f800001
    cmp  w2, w3
    b.ne fail2
    mrs  x2, fpsr
    mov  x3, #(1 << 4)
    cmp  x2, x3
    b.ne fail2

    msr  fpcr, xzr
    isb
    msr  fpsr, xzr
    adr  x22, src_scalar_exact
    ldr  d5, [x22]
    fcvtxn s4, d5
    str  s4, [x21, #20]
    ldr  w2, [x21, #20]
    ldr  w3, =0x3fc00000
    cmp  w2, w3
    b.ne fail3
    mrs  x2, fpsr
    cbnz x2, fail3

    msr  fpsr, xzr
    adr  x22, src_scalar_overflow
    ldr  d7, [x22]
    fcvtxn s6, d7
    str  s6, [x21, #24]
    ldr  w2, [x21, #24]
    ldr  w3, =0x7f7fffff
    cmp  w2, w3
    b.ne fail4
    mrs  x2, fpsr
    tst  x2, #(1 << 4)
    b.eq fail4

    msr  fpsr, xzr
    adr  x22, src_scalar_snan
    ldr  d9, [x22]
    fcvtxn s8, d9
    str  s8, [x21, #28]
    ldr  w2, [x21, #28]
    ldr  w3, =0x7fc091a2
    cmp  w2, w3
    b.ne fail5
    mrs  x2, fpsr
    cmp  x2, #1
    b.ne fail5

    msr  fpsr, xzr
    adr  x22, init_vec2
    ldr  q10, [x22]
    adr  x23, src_vec2
    ldr  q11, [x23]
    fcvtxn2 v10.4s, v11.2d
    str  q10, [x21, #32]
    add  x0, x21, #32
    adr  x1, exp_vec2
    bl   check16_6
    mrs  x2, fpsr
    mov  x3, #(1 << 4)
    cmp  x2, x3
    b.ne fail6

    movz x0, #'X'
    str  x0, [x20]
    brk  #0

check16_1:
    ldr  x2, [x0]
    ldr  x3, [x1]
    cmp  x2, x3
    b.ne fail1
    ldr  x2, [x0, #8]
    ldr  x3, [x1, #8]
    cmp  x2, x3
    b.ne fail1
    ret

check16_6:
    ldr  x2, [x0]
    ldr  x3, [x1]
    cmp  x2, x3
    b.ne fail6
    ldr  x2, [x0, #8]
    ldr  x3, [x1, #8]
    cmp  x2, x3
    b.ne fail6
    ret

fail1:
    movz x0, #'1'
    b done
fail2:
    movz x0, #'2'
    b done
fail3:
    movz x0, #'3'
    b done
fail4:
    movz x0, #'4'
    b done
fail5:
    movz x0, #'5'
    b done
fail6:
    movz x0, #'6'
done:
    str  x0, [x20]
    brk  #0

    .data
    .balign 16
src_vec:
    .quad 0x3ff0000010000000, 0xbff0000010000000
exp_vec:
    .word 0x3f800001, 0xbf800001, 0x00000000, 0x00000000

src_scalar_inexact:
    .quad 0x3ff0000010000000
src_scalar_exact:
    .quad 0x3ff8000000000000
src_scalar_overflow:
    .quad 0x47f0000000000000
src_scalar_snan:
    .quad 0x7ff0123456789abc

    .balign 16
init_vec2:
    .word 0x11223344, 0x55667788, 0x99aabbcc, 0xddeeff00
src_vec2:
    .quad 0x3ff0000010000000, 0x3ff8000000000000
exp_vec2:
    .word 0x11223344, 0x55667788, 0x3f800001, 0x3fc00000

dst:
    .space 48
