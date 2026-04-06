    .text
    .global _start
_start:
    movz x20, #0x0000
    movk x20, #0x0900, lsl #16      // UART
    adr  x21, shared
    add  x22, x21, #8               // ready
    add  x23, x21, #16              // ack
    add  x24, x21, #24              // done
    add  x25, x21, #32              // fail char

    movz x26, #0xcdef
    movk x26, #0x89ab, lsl #16
    movk x26, #0x4567, lsl #32
    movk x26, #0x0123, lsl #48

    mrs  x0, mpidr_el1
    and  x0, x0, #0xff
    cbz  x0, core0
    cmp  x0, #1
    b.ne secondary_bad_mpidr

core1_wait_ready:
    ldar x1, [x24]
    cbnz x1, secondary_exit
    ldr  x2, [x22]
    cbnz x2, core1_observe
    wfe
    b    core1_wait_ready

core1_observe:
    dmb  ish
    ldr  x3, [x21]
    cmp  x3, x26
    b.ne secondary_bad_payload
    movz x4, #1
    stlr x4, [x23]
    sev

secondary_wait_done:
    ldar x5, [x24]
    cbnz x5, secondary_exit
    wfe
    b    secondary_wait_done

core0:
    str  x26, [x21]
    dmb  ish
    movz x6, #1
    str  x6, [x22]
    dsb  ish
    sev

core0_wait_ack:
    ldar x7, [x24]
    cbnz x7, core0_report_or_verify
    ldar x8, [x23]
    cbnz x8, core0_success
    wfe
    b    core0_wait_ack

core0_success:
    movz x9, #1
    stlr x9, [x24]
    sev
    movz x0, #'B'
    b    print_and_halt

core0_report_or_verify:
    ldar x0, [x25]
    cbnz x0, print_and_halt
    ldar x8, [x23]
    cbnz x8, core0_success
    movz x0, #'b'
    b    print_and_halt

secondary_bad_mpidr:
    movz x1, #'x'
    b    secondary_fail

secondary_bad_payload:
    movz x1, #'b'

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
    .quad 0                          // 8: ready
    .quad 0                          // 16: ack
    .quad 0                          // 24: done
    .quad 0                          // 32: fail char
