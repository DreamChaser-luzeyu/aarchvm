    .text
    .global _start

_start:
    movz x20, #0x0000
    movk x20, #0x0900, lsl #16   // UART base 0x09000000
    movz x21, #0x0000
    movk x21, #0x0901, lsl #16   // KMI base  0x09010000

    ldr w0, [x21, #0xFE0]
    cmp w0, #0x50
    b.ne fail
    ldr w0, [x21, #0xFE4]
    cmp w0, #0x10
    b.ne fail
    ldr w0, [x21, #0xFE8]
    cmp w0, #0x04
    b.ne fail
    ldr w0, [x21, #0xFEC]
    cbnz w0, fail

    mov w0, #0x14               // EN | RXINTREN
    str w0, [x21, #0x00]

    mov w0, #0xFF               // reset
    str w0, [x21, #0x08]
    mov w1, #0xFA
    bl  recv_expect
    mov w1, #0xAA
    bl  recv_expect

    mov w0, #0xF2               // get ID
    str w0, [x21, #0x08]
    mov w1, #0xFA
    bl  recv_expect
    mov w1, #0xAB
    bl  recv_expect
    mov w1, #0x83
    bl  recv_expect

    mov w0, #0xF0               // get scancode set
    str w0, [x21, #0x08]
    mov w1, #0xFA
    bl  recv_expect
    mov w0, #0x00
    str w0, [x21, #0x08]
    mov w1, #0xFA
    bl  recv_expect
    mov w1, #0x02
    bl  recv_expect

    mov w0, #'K'
    bl  putc
    mov w0, #'M'
    bl  putc
    mov w0, #'I'
    bl  putc
    mov w0, #'\n'
    bl  putc
    brk #0

recv_expect:
1:  ldr w2, [x21, #0x10]
    tbz w2, #0, 1b
    ldr w2, [x21, #0x08]
    cmp w2, w1
    b.ne fail
    ret

putc:
    str x0, [x20]
    ret

fail:
    mov w0, #'F'
    bl  putc
    mov w0, #'A'
    bl  putc
    mov w0, #'I'
    bl  putc
    mov w0, #'L'
    bl  putc
    mov w0, #'\n'
    bl  putc
    brk #1
