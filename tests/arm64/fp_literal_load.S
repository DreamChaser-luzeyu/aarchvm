    .text
    .global _start

    .macro CHECK_MEM128 base, expected, fail_label
    ldr  x0, [\base]
    ldr  x1, [\expected]
    cmp  x0, x1
    b.ne \fail_label
    ldr  x0, [\base, #8]
    ldr  x1, [\expected, #8]
    cmp  x0, x1
    b.ne \fail_label
    .endm

_start:
    movz x20, #0
    movk x20, #0x0900, lsl #16
    movz x25, #0x0030, lsl #16
    msr  cpacr_el1, x25
    isb

    adr  x21, scratch

    movi v0.16b, #0xff
    ldr  s0, lit_s
    str  q0, [x21]
    adr  x22, expect_s
    CHECK_MEM128 x21, x22, fail1

    b    1f
    .balign 8
lit_d_back:
    .quad 0x5566778899aabbcc
1:
    movi v1.16b, #0xee
    ldr  d1, lit_d_back
    add  x23, x21, #16
    str  q1, [x23]
    adr  x22, expect_d
    CHECK_MEM128 x23, x22, fail2

    movi v2.16b, #0xaa
    ldr  q2, lit_q
    add  x24, x21, #32
    str  q2, [x24]
    adr  x22, lit_q
    CHECK_MEM128 x24, x22, fail3

    movz x0, #'L'
    str  x0, [x20]
    movz x0, #'\n'
    str  x0, [x20]
    brk  #0

fail1:
    movz x0, #'1'
    b    out
fail2:
    movz x0, #'2'
    b    out
fail3:
    movz x0, #'3'
out:
    str  x0, [x20]
    brk  #0

    .balign 4
lit_s:
    .word 0x11223344
    .balign 16
lit_q:
    .quad 0x0123456789abcdef
    .quad 0xfedcba9876543210

    .data
    .balign 16
expect_s:
    .quad 0x0000000011223344
    .quad 0x0000000000000000
expect_d:
    .quad 0x5566778899aabbcc
    .quad 0x0000000000000000
scratch:
    .space 48
