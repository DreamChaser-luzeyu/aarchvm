    .text
    .global _start
_start:
    // EXTR / ROR
    movz x1, #0x7788
    movk x1, #0x5566, lsl #16
    movk x1, #0x3344, lsl #32
    movk x1, #0x1122, lsl #48      // x1 = 0x1122334455667788
    movz x2, #0xff00
    movk x2, #0xddee, lsl #16
    movk x2, #0xbbcc, lsl #32
    movk x2, #0x99aa, lsl #48      // x2 = 0x99aabbccddeeff00

    extr x3, x1, x2, #8
    movz x4, #0xeeff
    movk x4, #0xccdd, lsl #16
    movk x4, #0xaabb, lsl #32
    movk x4, #0x8899, lsl #48
    subs xzr, x3, x4
    b.ne fail1

    ror  x5, x1, #16
    movz x6, #0x5566
    movk x6, #0x3344, lsl #16
    movk x6, #0x1122, lsl #32
    movk x6, #0x7788, lsl #48
    subs xzr, x5, x6
    b.ne fail2

    // CSINC / CSET
    movz x10, #5
    movz x11, #7
    subs xzr, x10, x10             // Z=1
    csinc x12, x10, x11, ne        // cond false -> Rm+1 = 8
    movz x13, #8
    subs xzr, x12, x13
    b.ne fail3

    cset x14, eq
    movz x15, #1
    subs xzr, x14, x15
    b.ne fail4

    // UDIV / SDIV
    movz x16, #100
    movz x17, #9
    udiv x18, x16, x17
    movz x19, #11
    subs xzr, x18, x19
    b.ne fail5

    movz x20, #100
    neg  x20, x20
    sdiv x21, x20, x17
    movz x22, #11
    neg  x22, x22
    subs xzr, x21, x22
    b.ne fail6

    // LDRSW family
    adr  x23, words
    movn w24, #0                  // -1
    movn w25, #1                  // -2
    movz w26, #0x1234
    str  w24, [x23, #0]
    str  w25, [x23, #4]
    str  w26, [x23, #8]

    ldrsw x0, [x23, #4]
    movn  x1, #1
    subs  xzr, x0, x1
    b.ne  fail7

    mov   x2, x23
    ldrsw x3, [x2, #4]!
    subs  xzr, x3, x1
    b.ne  fail8
    add   x4, x23, #4
    subs  xzr, x2, x4
    b.ne  fail8

    mov   x5, x23
    ldrsw x6, [x5], #8
    movn  x7, #0
    subs  xzr, x6, x7
    b.ne  fail9
    add   x8, x23, #8
    subs  xzr, x5, x8
    b.ne  fail9

    ldursw x9, [x23, #8]
    subs   xzr, x9, x26
    b.ne   fail10

pass:
    movz x1, #0x0000
    movk x1, #0x0900, lsl #16
    movz x0, #'V'
    str  x0, [x1]
    movz x0, #'\n'
    str  x0, [x1]
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

out:
    movz x1, #0x0000
    movk x1, #0x0900, lsl #16
    str  x0, [x1]
    brk  #0

    .data
    .balign 8
words:
    .quad 0
    .quad 0
