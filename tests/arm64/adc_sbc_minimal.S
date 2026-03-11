    .text
    .global _start
_start:
    movz x20, #0x0000
    movk x20, #0x0900, lsl #16

    mov  x1, #5
    mov  x2, #7
    cmp  xzr, xzr
    adc  x3, x1, x2
    cmp  x3, #13
    b.ne fail1

    movn x1, #0
    mov  x2, #0
    cmp  xzr, xzr
    adcs x4, x1, x2
    cbnz x4, fail2
    mrs  x5, nzcv
    movz x6, #0x6000, lsl #16
    cmp  x5, x6
    b.ne fail2

    mov  x1, #7
    mov  x2, #5
    cmp  xzr, xzr
    sbc  x7, x1, x2
    cmp  x7, #2
    b.ne fail3

    cmp  x2, x1
    ngc  x8, xzr
    movn x10, #0
    cmp  x8, x10
    b.ne fail4

    mov  w1, #1
    mov  w2, #2
    cmp  wzr, wzr
    adc  w9, w1, w2
    cmp  w9, #4
    b.ne fail5

    movz x0, #'W'
    str  x0, [x20]
    movz x0, #'
'
    str  x0, [x20]
    brk  #0

fail1:
    movz x0, #'1'
    str  x0, [x20]
    brk  #0
fail2:
    movz x0, #'2'
    str  x0, [x20]
    brk  #0
fail3:
    movz x0, #'3'
    str  x0, [x20]
    brk  #0
fail4:
    movz x0, #'4'
    str  x0, [x20]
    brk  #0
fail5:
    movz x0, #'5'
    str  x0, [x20]
    brk  #0
