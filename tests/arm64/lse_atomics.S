    .arch armv8.1-a
    .text
    .global _start
_start:
    movz x1, #0x0000
    movk x1, #0x0900, lsl #16      // UART
    adr  x20, word_slot
    adr  x21, quad_slot

    // CAS (32-bit)
    movz w2, #5
    str  w2, [x20]
    movz w2, #5
    movz w3, #9
    cas  w2, w3, [x20]
    movz w4, #5
    subs wzr, w2, w4
    b.ne fail_a
    ldr  w5, [x20]
    movz w6, #9
    subs wzr, w5, w6
    b.ne fail_a

    // CASAL (64-bit): mismatch then success
    movz x7, #0x1001
    str  x7, [x21]
    movz x8, #0x2002
    movz x9, #0x3003
    casal x8, x9, [x21]
    subs xzr, x8, x7
    b.ne fail_b
    ldr  x10, [x21]
    subs xzr, x10, x7
    b.ne fail_b

    orr  x8, x7, xzr
    movz x9, #0x4004
    casal x8, x9, [x21]
    subs xzr, x8, x7
    b.ne fail_b
    ldr  x10, [x21]
    subs xzr, x10, x9
    b.ne fail_b

    // SWPA (32-bit)
    movz w11, #0x12
    swpa w11, w12, [x20]
    movz w13, #9
    subs wzr, w12, w13
    b.ne fail_c
    ldr  w14, [x20]
    subs wzr, w14, w11
    b.ne fail_c

    // SWPAL (64-bit)
    movz x15, #0x55
    swpal x15, x16, [x21]
    subs xzr, x16, x9
    b.ne fail_d
    ldr  x17, [x21]
    subs xzr, x17, x15
    b.ne fail_d

    // LDADDA (32-bit)
    movz w18, #20
    str  w18, [x20]
    movz w18, #3
    ldadda w18, w19, [x20]
    movz w22, #20
    subs wzr, w19, w22
    b.ne fail_e
    ldr  w23, [x20]
    movz w24, #23
    subs wzr, w23, w24
    b.ne fail_e

    // LDCLRL (64-bit)
    movz x25, #0xff
    str  x25, [x21]
    movz x25, #0x0f
    ldclrl x25, x26, [x21]
    movz x27, #0xff
    subs xzr, x26, x27
    b.ne fail_f
    ldr  x28, [x21]
    movz x0, #0xf0
    subs xzr, x28, x0
    b.ne fail_f

    // LDEORAL (32-bit)
    movz w2, #0x00aa
    str  w2, [x20]
    movz w2, #0x00f0
    ldeoral w2, w3, [x20]
    movz w4, #0x00aa
    subs wzr, w3, w4
    b.ne fail_g
    ldr  w5, [x20]
    movz w6, #0x005a
    subs wzr, w5, w6
    b.ne fail_g

    // LDSETA (64-bit)
    movz x7, #0x0030
    str  x7, [x21]
    movz x7, #0x000c
    ldseta x7, x8, [x21]
    movz x9, #0x0030
    subs xzr, x8, x9
    b.ne fail_h
    ldr  x10, [x21]
    movz x11, #0x003c
    subs xzr, x10, x11
    b.ne fail_h

    movz x0, #'L'
    str  x0, [x1]
    movz x0, #'\n'
    str  x0, [x1]
    brk  #0

fail_a:
    movz x0, #'a'
    b out
fail_b:
    movz x0, #'b'
    b out
fail_c:
    movz x0, #'c'
    b out
fail_d:
    movz x0, #'d'
    b out
fail_e:
    movz x0, #'e'
    b out
fail_f:
    movz x0, #'f'
    b out
fail_g:
    movz x0, #'g'
    b out
fail_h:
    movz x0, #'h'

out:
    str  x0, [x1]
    brk  #0

    .data
    .balign 8
word_slot:
    .word 0
    .word 0
quad_slot:
    .quad 0
