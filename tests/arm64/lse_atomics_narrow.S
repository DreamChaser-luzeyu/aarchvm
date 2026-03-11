    .arch armv8.1-a
    .text
    .global _start
_start:
    movz x1, #0x0000
    movk x1, #0x0900, lsl #16      // UART
    adr  x20, byte_slot
    adr  x21, half_slot

    // CASB
    movz w2, #5
    strb w2, [x20]
    movz w2, #5
    movz w3, #9
    casb w2, w3, [x20]
    movz w4, #5
    subs wzr, w2, w4
    b.ne fail_a
    ldrb w5, [x20]
    movz w6, #9
    subs wzr, w5, w6
    b.ne fail_a

    // CASAH
    movz w7, #0x1234
    strh w7, [x21]
    movz w7, #0x1234
    movz w8, #0x5678
    casah w7, w8, [x21]
    movz w9, #0x1234
    subs wzr, w7, w9
    b.ne fail_b
    ldrh w10, [x21]
    movz w11, #0x5678
    subs wzr, w10, w11
    b.ne fail_b

    // SWPB
    movz w12, #0x12
    swpb w12, w13, [x20]
    movz w14, #9
    subs wzr, w13, w14
    b.ne fail_c
    ldrb w15, [x20]
    subs wzr, w15, w12
    b.ne fail_c

    // LDADDALH
    movz w16, #20
    strh w16, [x21]
    movz w16, #3
    ldaddalh w16, w17, [x21]
    movz w18, #20
    subs wzr, w17, w18
    b.ne fail_d
    ldrh w19, [x21]
    movz w22, #23
    subs wzr, w19, w22
    b.ne fail_d

    // LDCLRB
    movz w23, #0xff
    strb w23, [x20]
    movz w23, #0x0f
    ldclrb w23, w24, [x20]
    movz w25, #0xff
    subs wzr, w24, w25
    b.ne fail_e
    ldrb w26, [x20]
    movz w27, #0xf0
    subs wzr, w26, w27
    b.ne fail_e

    // LDEORH
    movz w2, #0x00aa
    strh w2, [x21]
    movz w2, #0x00f0
    ldeorh w2, w3, [x21]
    movz w4, #0x00aa
    subs wzr, w3, w4
    b.ne fail_f
    ldrh w5, [x21]
    movz w6, #0x005a
    subs wzr, w5, w6
    b.ne fail_f

    // LDSETALB
    movz w7, #0x30
    strb w7, [x20]
    movz w7, #0x0c
    ldsetalb w7, w8, [x20]
    movz w9, #0x30
    subs wzr, w8, w9
    b.ne fail_g
    ldrb w10, [x20]
    movz w11, #0x3c
    subs wzr, w10, w11
    b.ne fail_g

    // STADDLH
    movz w12, #4
    strh w12, [x21]
    movz w12, #3
    staddlh w12, [x21]
    ldrh w13, [x21]
    movz w14, #7
    subs wzr, w13, w14
    b.ne fail_h

    // STSETB
    movz w15, #0x11
    strb w15, [x20]
    movz w15, #0x24
    stsetb w15, [x20]
    ldrb w16, [x20]
    movz w17, #0x35
    subs wzr, w16, w17
    b.ne fail_i

    movz x0, #'N'
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
    b out
fail_i:
    movz x0, #'i'

out:
    str  x0, [x1]
    brk  #0

    .data
    .balign 8
byte_slot:
    .byte 0
    .byte 0
half_slot:
    .hword 0
    .hword 0
