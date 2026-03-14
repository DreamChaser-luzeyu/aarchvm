    .text
    .global _start

_start:
    movz x20, #0x0000
    movk x20, #0x0900, lsl #16   // UART base 0x09000000
    movz x21, #0x0000
    movk x21, #0x0901, lsl #16   // KMI base  0x09010000

    mov w0, #0x14               // EN | RXINTREN
    str w0, [x21, #0x00]

    mov w19, #3
1:  ldr w0, [x21, #0x10]
    tbz w0, #0, 1b
    ldr w0, [x21, #0x08]
    str x0, [x20]
    subs w19, w19, #1
    b.ne 1b

    mov w0, #'P'
    str x0, [x20]
    brk #0
