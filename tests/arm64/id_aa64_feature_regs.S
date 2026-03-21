    .text
    .global _start
_start:
    movz x1, #0
    movk x1, #0x0900, lsl #16      // UART

    mrs  x2, id_aa64pfr0_el1
    movz x3, #0x0011
    movk x3, #0x0100, lsl #16
    subs xzr, x2, x3
    b.ne fail1

    mrs  x2, id_aa64isar0_el1
    movz x3, #0x0000
    movk x3, #0x0021, lsl #16
    subs xzr, x2, x3
    b.ne fail2

    mrs  x2, id_aa64mmfr0_el1
    movz x3, #0x0005
    movk x3, #0x0f00, lsl #16
    subs xzr, x2, x3
    b.ne fail3

    mrs  x2, id_aa64mmfr1_el1
    movz x3, #0x0010, lsl #16
    subs xzr, x2, x3
    b.ne fail4

    mrs  x2, id_aa64dfr0_el1
    movz x3, #0x1006
    movk x3, #0x0010, lsl #16
    subs xzr, x2, x3
    b.ne fail5

    mrs  x2, id_aa64dfr1_el1
    cbnz x2, fail6

    movz x0, #'I'
    str  x0, [x1]
    movz x0, #'\n'
    str  x0, [x1]
    brk  #0

fail1:
    movz x0, #'1'
    str  x0, [x1]
    b fail
fail2:
    movz x0, #'2'
    str  x0, [x1]
    b fail
fail3:
    movz x0, #'3'
    str  x0, [x1]
    b fail
fail4:
    movz x0, #'4'
    str  x0, [x1]
    b fail
fail5:
    movz x0, #'5'
    str  x0, [x1]
    b fail
fail6:
    movz x0, #'6'
    str  x0, [x1]
fail:
    movz x0, #'F'
    str  x0, [x1]
    brk  #0
