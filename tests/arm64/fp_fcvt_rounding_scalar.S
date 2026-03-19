    .text
    .global _start
_start:
    movz x20, #0
    movk x20, #0x0900, lsl #16
    movz x25, #0x0030, lsl #16
    msr  cpacr_el1, x25
    isb

    adr  x21, data_f
    adr  x23, data_d

    ldr  s0, [x21, #(0 * 4)]
    fcvtns w0, s0
    cmp  w0, #2
    b.ne fail1

    fcvtnu w1, s0
    cmp  w1, #2
    b.ne fail2

    fcvtas w2, s0
    cmp  w2, #3
    b.ne fail3

    fcvtau w3, s0
    cmp  w3, #3
    b.ne fail4

    ldr  s4, [x21, #(1 * 4)]
    fcvtps w4, s4
    cmp  w4, #3
    b.ne fail5

    ldr  s5, [x21, #(2 * 4)]
    fcvtms w5, s5
    ldr  w22, =0xfffffffd
    cmp  w5, w22
    b.ne fail6

    ldr  s6, [x21, #(3 * 4)]
    fcvtmu w6, s6
    cmp  w6, #2
    b.ne fail7

    ldr  d7, [x23, #(0 * 8)]
    fcvtns x7, d7
    ldr  x22, =0xfffffffffffffffe
    cmp  x7, x22
    b.ne fail8

    fcvtas x8, d7
    ldr  x22, =0xfffffffffffffffd
    cmp  x8, x22
    b.ne fail9

    ldr  d9, [x23, #(1 * 8)]
    fcvtpu x9, d9
    cmp  x9, #3
    b.ne fail10

    ldr  d10, [x23, #(2 * 8)]
    fcvtms x10, d10
    ldr  x22, =0xfffffffffffffffd
    cmp  x10, x22
    b.ne fail11

    ldr  d11, [x23, #(3 * 8)]
    fcvtnu x11, d11
    cmp  x11, #2
    b.ne fail12

    ldr  d12, [x23, #(4 * 8)]
    fcvtmu x12, d12
    cmp  x12, #2
    b.ne fail13

    ldr  d13, [x23, #(0 * 8)]
    fcvtns w13, d13
    ldr  w22, =0xfffffffe
    cmp  w13, w22
    b.ne fail14

    ldr  s14, [x21, #(0 * 4)]
    fcvtau x14, s14
    cmp  x14, #3
    b.ne fail15

    msr  fpsr, xzr
    ldr  s15, [x21, #(4 * 4)]
    fcvtns w15, s15
    cmp  w15, #1
    b.ne fail16
    mrs  x0, fpsr
    mov  x1, #(1 << 4)
    cmp  x0, x1
    b.ne fail17

    msr  fpsr, xzr
    ldr  s16, [x21, #(5 * 4)]
    fcvtpu w16, s16
    cbnz w16, fail18
    mrs  x0, fpsr
    mov  x1, #1
    cmp  x0, x1
    b.ne fail19

    msr  fpsr, xzr
    ldr  s17, [x21, #(6 * 4)]
    fcvtns w17, s17
    ldr  w22, =0x7fffffff
    cmp  w17, w22
    b.ne fail20
    mrs  x0, fpsr
    mov  x1, #1
    cmp  x0, x1
    b.ne fail21

    movz x0, #'R'
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
    b out
fail7:
    movz x0, #'7'
    b out
fail8:
    movz x0, #'8'
    b out
fail9:
    movz x0, #'9'
    b out
fail10:
    movz x0, #'A'
    b out
fail11:
    movz x0, #'B'
    b out
fail12:
    movz x0, #'C'
    b out
fail13:
    movz x0, #'D'
    b out
fail14:
    movz x0, #'E'
    b out
fail15:
    movz x0, #'F'
    b out
fail16:
    movz x0, #'G'
    b out
fail17:
    movz x0, #'H'
    b out
fail18:
    movz x0, #'I'
    b out
fail19:
    movz x0, #'J'
    b out
fail20:
    movz x0, #'K'
    b out
fail21:
    movz x0, #'L'
out:
    str  x0, [x20]
    brk  #0

    .data
    .balign 8
data_f:
    .word 0x40200000              // 2.5f
    .word 0x40066666              // 2.1f
    .word 0xc0066666              // -2.1f
    .word 0x4039999a              // 2.9f
    .word 0x3f8ccccd              // 1.1f
    .word 0xbf800000              // -1.0f
    .word 0x7f800000              // +inf
    .word 0x00000000
data_d:
    .quad 0xc004000000000000      // -2.5
    .quad 0x4000cccccccccccd      // 2.1
    .quad 0xc000cccccccccccd      // -2.1
    .quad 0x4004000000000000      // 2.5
    .quad 0x4007333333333333      // 2.9
