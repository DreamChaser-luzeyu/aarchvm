    .text
    .global _start
_start:
    movz x20, #0
    movk x20, #0x0900, lsl #16
    movz x25, #0x0030, lsl #16
    msr  cpacr_el1, x25
    isb

    adr  x21, scratch

    adr  x22, src_basic
    ldr  q1, [x22]
    addp d0, v1.2d
    str  q0, [x21]
    mov  x0, x21
    adr  x1, exp_basic
    bl   check16

    adr  x22, dst_init
    ldr  q2, [x22]
    adr  x22, src_wrap
    ldr  q3, [x22]
    addp d2, v3.2d
    str  q2, [x21, #16]
    add  x0, x21, #16
    adr  x1, exp_wrap
    bl   check16

    movz x0, #'J'
    str  x0, [x20]
    movz x0, #'\n'
    str  x0, [x20]
    brk  #0

check16:
    ldr  x2, [x0]
    ldr  x3, [x1]
    cmp  x2, x3
    b.ne fail
    ldr  x2, [x0, #8]
    ldr  x3, [x1, #8]
    cmp  x2, x3
    b.ne fail
    ret

fail:
    movz x0, #'F'
    str  x0, [x20]
    brk  #0

    .data
    .balign 16
src_basic:
    .quad 1, 2
exp_basic:
    .quad 3, 0

dst_init:
    .quad 0xaaaaaaaaaaaaaaaa, 0xbbbbbbbbbbbbbbbb
src_wrap:
    .quad 0xffffffffffffffff, 2
exp_wrap:
    .quad 1, 0

scratch:
    .space 32
