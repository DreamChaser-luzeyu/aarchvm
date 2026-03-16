    .text
    .global _start
_start:
    movz x20, #0x0000
    movk x20, #0x0900, lsl #16      // UART
    adr  x21, shared
    add  x22, x21, #16
    add  x23, x21, #24
    add  x26, x21, #32
    add  x27, x21, #40

    mrs  x0, mpidr_el1
    and  x0, x0, #0xff
    cbz  x0, core0
    cmp  x0, #1
    b.ne secondary_fail
    mov  x24, x23                    // done1
    b    worker

core0:
    mov  x24, x22                    // done0

worker:
    movz x25, #64
worker_loop:
lock_try:
    ldaxr w1, [x21, #0]             // lock
    cbnz w1, lock_try
    movz w2, #1
    stlxr w3, w2, [x21, #0]
    cbnz w3, lock_try

    ldr  w4, [x21, #8]              // counter
    add  w4, w4, #1
    str  w4, [x21, #8]
    stlr wzr, [x21, #0]

    subs x25, x25, #1
    b.ne worker_loop

    movz x5, #1
    stlr x5, [x24]
    sev

    mrs  x0, mpidr_el1
    and  x0, x0, #0xff
    cbnz x0, secondary_wait_exit

core0_wait_secondary:
    ldar x6, [x26]                  // exit
    cbnz x6, core0_report_or_verify
    ldar x7, [x23]                  // done1
    cbnz x7, core0_verify
    wfe
    b    core0_wait_secondary

core0_verify:
    ldr  w8, [x21, #8]
    movz w9, #128
    cmp  w8, w9
    b.ne primary_fail
    movz x10, #1
    stlr x10, [x26]
    sev
    movz x0, #'L'
    b    print_and_halt

core0_report_or_verify:
    ldar x0, [x27]
    cbnz x0, print_and_halt
    b    core0_verify

secondary_wait_exit:
    ldar x11, [x26]
    cbnz x11, secondary_exit
    wfe
    b    secondary_wait_exit

secondary_fail:
    movz x1, #'l'
    stlr x1, [x27]
    movz x2, #1
    stlr x2, [x26]
    sev
secondary_exit:
    brk  #0

primary_fail:
    movz x0, #'P'

print_and_halt:
    str  x0, [x20]
    movz x0, #'\n'
    str  x0, [x20]
    brk  #0

    .data
    .balign 8
shared:
    .word 0                          // 0: lock
    .word 0
    .word 0                          // 8: counter
    .word 0
    .quad 0                          // 16: done0
    .quad 0                          // 24: done1
    .quad 0                          // 32: exit
    .quad 0                          // 40: fail_char
