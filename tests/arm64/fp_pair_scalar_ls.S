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

    adr  x0, src_words
    ldr  s0, [x0]
    ldr  s1, [x0, #4]

    adr  x2, pair_buf
    sub  x3, x2, #8
    stp  s0, s1, [x3, #8]!
    subs xzr, x3, x2
    b.ne fail1

    ldr  x4, [x2]
    adr  x5, expect_pair_words
    ldr  x6, [x5]
    cmp  x4, x6
    b.ne fail2

    movi v0.16b, #0xff
    movi v1.16b, #0xee
    mov  x3, x2
    ldp  s0, s1, [x3], #8
    add  x4, x2, #8
    subs xzr, x3, x4
    b.ne fail3

    adr  x6, scratch
    str  q0, [x6]
    str  q1, [x6, #16]
    adr  x7, expect_q0
    CHECK_MEM128 x6, x7, fail4
    add  x6, x6, #16
    adr  x7, expect_q1
    CHECK_MEM128 x6, x7, fail4

    adr  x0, src_words
    ldr  s2, [x0, #8]
    ldr  s3, [x0, #12]
    adr  x8, pair_buf2
    stp  s2, s3, [x8, #8]
    ldr  x9, [x8, #8]
    adr  x10, expect_pair_words2
    ldr  x11, [x10]
    cmp  x9, x11
    b.ne fail5

    movi v2.16b, #0xaa
    movi v3.16b, #0xbb
    ldp  s2, s3, [x8, #8]
    adr  x6, scratch2
    str  q2, [x6]
    str  q3, [x6, #16]
    adr  x7, expect_q2
    CHECK_MEM128 x6, x7, fail6
    add  x6, x6, #16
    adr  x7, expect_q3
    CHECK_MEM128 x6, x7, fail6

    movz x0, #'P'
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
    b    out
fail4:
    movz x0, #'4'
    b    out
fail5:
    movz x0, #'5'
    b    out
fail6:
    movz x0, #'6'
out:
    str  x0, [x20]
    brk  #0

    .data
    .balign 16
src_words:
    .word 0x11223344
    .word 0x55667788
    .word 0x99aabbcc
    .word 0xddeeff00
expect_pair_words:
    .quad 0x5566778811223344
expect_pair_words2:
    .quad 0xddeeff0099aabbcc
expect_q0:
    .quad 0x0000000011223344
    .quad 0x0000000000000000
expect_q1:
    .quad 0x0000000055667788
    .quad 0x0000000000000000
expect_q2:
    .quad 0x0000000099aabbcc
    .quad 0x0000000000000000
expect_q3:
    .quad 0x00000000ddeeff00
    .quad 0x0000000000000000
pair_buf:
    .space 16
pair_buf2:
    .space 16
scratch:
    .space 32
scratch2:
    .space 32
