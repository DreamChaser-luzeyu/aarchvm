    .text
    .global _start
_start:
    movz x1, #0x0000
    movk x1, #0x0900, lsl #16      // UART

    adr  x10, vectors
    msr  vbar_el1, x10

    adr  x11, handled
    str  xzr, [x11]

svc_here:
    svc  #0x1234

after_svc:
    ldr  x12, [x11]
    cbz  x12, fail

    mrs  x13, currentel
    movz x17, #4
    subs xzr, x13, x17
    b.ne fail_c

    movz x14, #0xbeef
    movk x14, #0xcafe, lsl #16
    movk x14, #0x5678, lsl #32
    movk x14, #0x1234, lsl #48
    msr  tpidr_el0, x14
    mrs  x15, tpidr_el0
    subs xzr, x15, x14
    b.ne fail_t

    mrs  x16, id_aa64isar0_el1
    lsr  x16, x16, #20
    movz x17, #0xf
    and  x16, x16, x17
    movz x18, #2
    subs xzr, x16, x18
    b.ne fail_i

    movz x0, #'S'
    str  x0, [x1]
    movz x0, #'\n'
    str  x0, [x1]
    brk  #0

fail:
    movz x0, #'F'
    b out
fail_c:
    movz x0, #'C'
    b out
fail_t:
    movz x0, #'T'
    b out
fail_i:
    movz x0, #'I'

out:
    str  x0, [x1]
    brk  #0

    .balign 2048
vectors:
    .space 0x200
sync_spx:
    b sync_handler

sync_handler:
    mrs  x2, esr_el1
    lsr  x3, x2, #26
    movz x4, #0x15
    subs xzr, x3, x4
    b.ne fail_from_handler

    movz x5, #0xffff
    and  x6, x2, x5
    movz x7, #0x1234
    subs xzr, x6, x7
    b.ne fail_from_handler

    mrs  x8, elr_el1
    adr  x9, after_svc
    subs xzr, x8, x9
    b.ne fail_from_handler

    adr  x10, handled
    movz x11, #1
    str  x11, [x10]

    adr  x12, after_svc
    msr  elr_el1, x12
    eret

fail_from_handler:
    adr  x10, handled
    str  xzr, [x10]
    adr  x12, fail
    msr  elr_el1, x12
    eret

    .data
    .balign 8
handled:
    .quad 0
