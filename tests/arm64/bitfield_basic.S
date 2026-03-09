    .text
    .global _start
_start:
    movz x1, #0x0f0f
    movk x1, #0x0f0f, lsl #16
    movk x1, #0xf0f0, lsl #32
    movk x1, #0x00f0, lsl #48      // x1 = 0x00f0f0f00f0f0f0f

    // lsr x2, x1, #4 => 0x000f0f0f00f0f0f0
    lsr  x2, x1, #4
    movz x10, #0xf0f0
    movk x10, #0x00f0, lsl #16
    movk x10, #0x0f0f, lsl #32
    movk x10, #0x000f, lsl #48
    subs xzr, x2, x10
    b.ne fail1

    // lsl x3, x1, #8 => 0xf0f0f00f0f0f0f00
    lsl  x3, x1, #8
    movz x11, #0x0f00
    movk x11, #0x0f0f, lsl #16
    movk x11, #0xf00f, lsl #32
    movk x11, #0xf0f0, lsl #48
    subs xzr, x3, x11
    b.ne fail2

    // ubfx x4, x1, #8, #8 => 0x0f
    ubfx x4, x1, #8, #8
    movz x12, #0x0f
    subs xzr, x4, x12
    b.ne fail3

    // sbfx x6, x5, #8, #8 where x5=-1 => -1
    movn x5, #0
    sbfx x6, x5, #8, #8
    subs xzr, x6, x5
    b.ne fail4

    // bfi x7, x8, #8, #8 with x8=0xab => x7=0xab00
    movz x7, #0
    movz x8, #0xab
    bfi  x7, x8, #8, #8
    movz x9, #0xab00
    subs xzr, x7, x9
    b.ne fail5

    // cntpct_el0 should be readable (minimal alias to virtual counter in this model).
    mrs  x13, cntpct_el0
    mrs  x14, cntvct_el0
    subs x15, x14, x13
    b.lo fail6

    movz x1, #0x0000
    movk x1, #0x0900, lsl #16
    movz x0, #'U'
    str  x0, [x1]
    movz x0, #'\n'
    str  x0, [x1]
    brk  #0

fail:
    movz x1, #0x0000
    movk x1, #0x0900, lsl #16
    movz x0, #'F'
    str  x0, [x1]
    brk  #0

fail1:
    movz x1, #0x0000
    movk x1, #0x0900, lsl #16
    movz x0, #'1'
    str  x0, [x1]
    brk  #0
fail2:
    movz x1, #0x0000
    movk x1, #0x0900, lsl #16
    movz x0, #'2'
    str  x0, [x1]
    brk  #0
fail3:
    movz x1, #0x0000
    movk x1, #0x0900, lsl #16
    movz x0, #'3'
    str  x0, [x1]
    brk  #0
fail4:
    movz x1, #0x0000
    movk x1, #0x0900, lsl #16
    movz x0, #'4'
    str  x0, [x1]
    brk  #0
fail5:
    movz x1, #0x0000
    movk x1, #0x0900, lsl #16
    movz x0, #'5'
    str  x0, [x1]
    brk  #0
fail6:
    movz x1, #0x0000
    movk x1, #0x0900, lsl #16
    movz x0, #'6'
    str  x0, [x1]
    brk  #0
