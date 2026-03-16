    .text
    .global _start
_start:
    movz x20, #0x0000
    movk x20, #0x0900, lsl #16      // UART
    adr  x21, shared
    add  x22, x21, #8
    add  x23, x21, #16
    add  x24, x21, #24
    add  x25, x21, #32

    mrs  x0, mpidr_el1
    and  x0, x0, #0xff
    cbz  x0, core0
    cmp  x0, #1
    b.ne secondary_fail

secondary_wait_ready:
    ldar x1, [x24]                  // done
    cbnz x1, secondary_exit
    ldar x2, [x22]                  // ready
    cbnz x2, secondary_check
    wfe
    b    secondary_wait_ready

secondary_check:
    ldr  x3, [x21, #0]              // payload
    movz x4, #0x5678
    movk x4, #0x1234, lsl #16
    cmp  x3, x4
    b.ne secondary_fail
    movz x5, #1
    stlr x5, [x23]                  // ack
    sev
secondary_wait_done:
    ldar x6, [x24]
    cbnz x6, secondary_exit
    wfe
    b    secondary_wait_done

core0:
    movz x1, #0x5678
    movk x1, #0x1234, lsl #16
    str  x1, [x21, #0]
    movz x2, #1
    stlr x2, [x22]                  // ready
    sev
core0_wait_ack:
    ldar x3, [x24]                  // done
    cbnz x3, core0_report_shared
    ldar x4, [x23]                  // ack
    cbnz x4, core0_success
    wfe
    b    core0_wait_ack

core0_success:
    movz x5, #1
    stlr x5, [x24]
    sev
    movz x0, #'E'
    b    print_and_halt

core0_report_shared:
    ldar x0, [x25]
    cbnz x0, print_and_halt
    movz x0, #'e'
    b    print_and_halt

secondary_fail:
    movz x1, #'v'
    stlr x1, [x25]
    movz x2, #1
    stlr x2, [x24]
    sev
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
    .quad 0                          // 0: payload
    .quad 0                          // 8: ready
    .quad 0                          // 16: ack
    .quad 0                          // 24: done
    .quad 0                          // 32: fail_char
