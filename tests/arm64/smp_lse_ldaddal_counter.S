    .arch armv8.1-a
    .text
    .global _start
_start:
    movz x20, #0x0000
    movk x20, #0x0900, lsl #16      // UART
    adr  x21, shared
    add  x22, x21, #8               // done0
    add  x23, x21, #16              // done1
    add  x24, x21, #24              // exit
    add  x27, x21, #32              // fail char

    mrs  x0, mpidr_el1
    and  x0, x0, #0xff
    cbz  x0, core0
    cmp  x0, #1
    b.ne secondary_fail
    mov  x25, x23
    b    worker

core0:
    mov  x25, x22

worker:
    movz x26, #128
worker_loop:
    movz w1, #1
    ldaddal w1, w2, [x21]
    subs x26, x26, #1
    b.ne worker_loop

    dmb  sy
    movz x3, #1
    stlr x3, [x25]
    sev

    mrs  x0, mpidr_el1
    and  x0, x0, #0xff
    cbnz x0, secondary_wait_exit

core0_wait_secondary:
    ldar x4, [x23]
    cbnz x4, core0_verify
    ldar x5, [x24]
    cbnz x5, core0_report_fail
    wfe
    b    core0_wait_secondary

core0_verify:
    ldr  w6, [x21]
    movz w7, #256
    cmp  w6, w7
    b.ne primary_fail
    movz x8, #1
    stlr x8, [x24]
    sev
    movz x0, #'J'
    b    print_and_halt

core0_report_fail:
    ldar x0, [x27]
    cbnz x0, print_and_halt
    movz x0, #'j'
    b    print_and_halt

secondary_fail:
    movz x1, #'x'
    stlr x1, [x27]
    movz x2, #1
    stlr x2, [x24]
    sev

secondary_wait_exit:
    ldar x11, [x24]
    cbnz x11, secondary_exit
    wfe
    b    secondary_wait_exit

primary_fail:
    movz x1, #'P'
    stlr x1, [x27]
    movz x2, #1
    stlr x2, [x24]
    sev
    mov  x0, x1
    b    print_and_halt

secondary_exit:
    brk  #0

print_and_halt:
    str  x0, [x20]
    movz x0, #'\n'
    str  x0, [x20]
    brk  #0

    .data
    .balign 8
shared:
    .word 0                          // 0: counter
    .word 0
    .quad 0                          // 8: done0
    .quad 0                          // 16: done1
    .quad 0                          // 24: exit
    .quad 0                          // 32: fail char
