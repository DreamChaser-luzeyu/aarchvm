    .arch armv8.1-a
    .text
    .global _start
_start:
    movz x1, #0x0000
    movk x1, #0x0900, lsl #16      // UART
    adr  x20, pair32_slot
    adr  x21, pair64_slot

    // CASP (32-bit pair) success path.
    movz w2, #11
    movz w3, #22
    stp  w2, w3, [x20]
    movz w2, #11
    movz w3, #22
    movz w4, #33
    movz w5, #44
    casp w2, w3, w4, w5, [x20]
    movz w6, #11
    subs wzr, w2, w6
    b.ne fail_a
    movz w6, #22
    subs wzr, w3, w6
    b.ne fail_a
    ldp  w6, w7, [x20]
    movz w8, #33
    subs wzr, w6, w8
    b.ne fail_a
    movz w8, #44
    subs wzr, w7, w8
    b.ne fail_a

    // CASPA (64-bit pair) mismatch path.
    movz x10, #0x1111
    movz x11, #0x2222
    stp  x10, x11, [x21]
    movz x12, #0xaaaa
    movz x13, #0xbbbb
    movz x14, #0x3333
    movz x15, #0x4444
    caspa x12, x13, x14, x15, [x21]
    subs xzr, x12, x10
    b.ne fail_b
    subs xzr, x13, x11
    b.ne fail_b
    ldp  x16, x17, [x21]
    subs xzr, x16, x10
    b.ne fail_b
    subs xzr, x17, x11
    b.ne fail_b

    // CASPL (64-bit pair) success path.
    orr  x12, x10, xzr
    orr  x13, x11, xzr
    movz x14, #0x5555
    movz x15, #0x6666
    caspl x12, x13, x14, x15, [x21]
    subs xzr, x12, x10
    b.ne fail_c
    subs xzr, x13, x11
    b.ne fail_c
    ldp  x16, x17, [x21]
    subs xzr, x16, x14
    b.ne fail_c
    subs xzr, x17, x15
    b.ne fail_c

    // CASPAL (64-bit pair) success path with a second update.
    orr  x12, x14, xzr
    orr  x13, x15, xzr
    movz x14, #0x7777
    movz x15, #0x8888
    caspal x12, x13, x14, x15, [x21]
    movz x18, #0x5555
    subs xzr, x12, x18
    b.ne fail_d
    movz x18, #0x6666
    subs xzr, x13, x18
    b.ne fail_d
    ldp  x16, x17, [x21]
    movz x18, #0x7777
    subs xzr, x16, x18
    b.ne fail_d
    movz x18, #0x8888
    subs xzr, x17, x18
    b.ne fail_d

    movz x0, #'C'
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

out:
    str  x0, [x1]
    brk  #0

    .data
    .balign 16
pair32_slot:
    .word 0
    .word 0
    .balign 16
pair64_slot:
    .quad 0
    .quad 0
