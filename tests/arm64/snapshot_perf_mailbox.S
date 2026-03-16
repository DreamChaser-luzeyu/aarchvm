    .text
    .global _start
_start:
    movz x20, #0
    movk x20, #0x0900, lsl #16
    movz x21, #0
    movk x21, #0x0902, lsl #16

    movz x0, #0x1234
    str  x0, [x21, #0x10]
    movz x0, #0x56
    str  x0, [x21, #0x18]
    movz x0, #0x78
    str  x0, [x21, #0x20]
    movz x0, #1
    str  x0, [x21, #0x08]

wait_uart:
    ldr  x0, [x20, #0x18]
    tst  x0, #0x10
    b.ne wait_uart
    ldr  x0, [x20]
    cmp  w0, #'Z'
    b.ne wait_uart

    movz x0, #2
    str  x0, [x21, #0x08]

    ldr  x0, [x21, #0x10]
    movz x1, #0x1234
    cmp  x0, x1
    b.ne fail

    ldr  x0, [x21, #0x18]
    movz x1, #0x56
    cmp  x0, x1
    b.ne fail

    ldr  x0, [x21, #0x20]
    movz x1, #0x78
    cmp  x0, x1
    b.ne fail

    ldr  x0, [x21, #0x28]
    movz x1, #2
    cmp  x0, x1
    b.ne fail

    ldr  x0, [x21, #0x38]
    movz x1, #1000
    cmp  x0, x1
    b.ls fail

pass:
    movz x0, #'P'
    str  x0, [x20]
    movz x0, #'\n'
    str  x0, [x20]
    brk  #0

fail:
    movz x0, #'F'
    str  x0, [x20]
    movz x0, #'\n'
    str  x0, [x20]
    brk  #0
