    .text
    .global _start
_start:
    ldr x1, =0x09000000

    movz x2, #0xcdef
    movk x2, #0x89ab, lsl #16
    movk x2, #0x4567, lsl #32
    movk x2, #0x0123, lsl #48      // x2 = 0x0123456789abcdef

    movz x3, #0x00ff
    movk x3, #0x00ff, lsl #16
    movk x3, #0x00ff, lsl #32
    movk x3, #0x00ff, lsl #48      // x3 = 0x00ff00ff00ff00ff

    // logical shifted register: AND
    and  x4, x2, x3
    movz x20, #0x00ef
    movk x20, #0x00ab, lsl #16
    movk x20, #0x0067, lsl #32
    movk x20, #0x0023, lsl #48     // 0x0023006700ab00ef
    cmp  x4, x20
    b.ne fail1

    // logical shifted register with invert: ORN
    orn  x5, xzr, x3
    movz x21, #0xff00
    movk x21, #0xff00, lsl #16
    movk x21, #0xff00, lsl #32
    movk x21, #0xff00, lsl #48     // 0xff00ff00ff00ff00
    cmp  x5, x21
    b.ne fail2

    // logical shifted register 32-bit with flags: BICS
    movz w6, #0x00ff
    movk w6, #0x00ff, lsl #16
    mov  w7, w6
    bics w8, w6, w7
    b.ne fail3
    cmp  w8, #0
    b.ne fail4

    // logical immediate: ORR / AND / EOR / ANDS
    orr  x9, xzr, #0x00ff00ff00ff00ff
    cmp  x9, x3
    b.ne fail5

    and  x10, x2, #0x0000ffff0000ffff
    movz x22, #0xcdef
    movk x22, #0x0000, lsl #16
    movk x22, #0x4567, lsl #32
    movk x22, #0x0000, lsl #48     // 0x000045670000cdef
    cmp  x10, x22
    b.ne fail6

    eor  w11, w7, #0x00ff00ff
    cmp  w11, #0
    b.ne fail7

    ands x12, x3, #0xff00ff00ff00ff00
    b.ne fail8

    // bitfield aliases: LSR / LSL / UBFX / SBFX / BFI
    lsr  x13, x2, #8
    movz x23, #0xabcd
    movk x23, #0x6789, lsl #16
    movk x23, #0x2345, lsl #32
    movk x23, #0x0001, lsl #48     // 0x000123456789abcd
    cmp  x13, x23
    b.ne fail9

    lsl  w14, w2, #8
    movz w24, #0xef00
    movk w24, #0xabcd, lsl #16     // 0xabcdef00
    cmp  w14, w24
    b.ne failA

    ubfx x15, x2, #16, #8
    cmp  x15, #0xab
    b.ne failB

    movn x16, #0
    sbfx x17, x16, #8, #8
    cmp  x17, x16
    b.ne failC

    movz w18, #0
    movz w19, #0xab
    bfi  w18, w19, #8, #8
    movz w26, #0xab00
    cmp  w18, w26
    b.ne failD

pass:
    mov  w0, #'P'
    str  w0, [x1]
    mov  w0, #'\n'
    str  w0, [x1]
    brk  #0

fail1:
    mov w0, #'1'
    b report_fail
fail2:
    mov w0, #'2'
    b report_fail
fail3:
    mov w0, #'3'
    b report_fail
fail4:
    mov w0, #'4'
    b report_fail
fail5:
    mov w0, #'5'
    b report_fail
fail6:
    mov w0, #'6'
    b report_fail
fail7:
    mov w0, #'7'
    b report_fail
fail8:
    mov w0, #'8'
    b report_fail
fail9:
    mov w0, #'9'
    b report_fail
failA:
    mov w0, #'A'
    b report_fail
failB:
    mov w0, #'B'
    b report_fail
failC:
    mov w0, #'C'
    b report_fail
failD:
    mov w0, #'D'

report_fail:
    str  w0, [x1]
    mov  w0, #'\n'
    str  w0, [x1]
    brk  #0
