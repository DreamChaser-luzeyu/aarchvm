    .arch armv8.1-a
    .text
    .global _start
_start:
    movz x20, #0x0000
    movk x20, #0x0900, lsl #16      // UART
    adr  x21, shared
    add  x22, x21, #8               // flag
    add  x23, x21, #16              // ack
    add  x24, x21, #24              // done
    add  x25, x21, #32              // fail char

    movz x26, #0x7777
    movk x26, #0x6666, lsl #16
    movk x26, #0x5555, lsl #32
    movk x26, #0x4444, lsl #48

    mrs  x0, mpidr_el1
    and  x0, x0, #0xff
    cbz  x0, core0
    cmp  x0, #1
    b.ne secondary_bad_mpidr

core1_wait_flag:
    ldar x1, [x24]
    cbnz x1, secondary_exit
    movz x2, #1
    movz x3, #1
    casa x2, x3, [x22]
    cmp  x2, #1
    b.eq core1_observe
    wfe
    b    core1_wait_flag

core1_observe:
    ldr  x4, [x21]
    cmp  x4, x26
    b.ne secondary_bad_payload
    movz x5, #1
    stlr x5, [x23]
    sev

secondary_wait_done:
    ldar x6, [x24]
    cbnz x6, secondary_exit
    wfe
    b    secondary_wait_done

core0:
    str  x26, [x21]
    movz x7, #0
    movz x8, #1
    casal x7, x8, [x22]
    cbnz x7, primary_publish_failed
    sev

core0_wait_ack:
    ldar x9, [x24]
    cbnz x9, core0_report_or_verify
    ldar x10, [x23]
    cbnz x10, core0_success
    wfe
    b    core0_wait_ack

core0_success:
    movz x11, #1
    stlr x11, [x24]
    sev
    movz x0, #'C'
    b    print_and_halt

core0_report_or_verify:
    ldar x0, [x25]
    cbnz x0, print_and_halt
    ldar x10, [x23]
    cbnz x10, core0_success
    movz x0, #'c'
    b    print_and_halt

primary_publish_failed:
    movz x0, #'p'
    b    print_and_halt

secondary_bad_mpidr:
    movz x1, #'x'
    b    secondary_fail

secondary_bad_payload:
    movz x1, #'c'

secondary_fail:
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
    .quad 0                          // 8: flag
    .quad 0                          // 16: ack
    .quad 0                          // 24: done
    .quad 0                          // 32: fail char
