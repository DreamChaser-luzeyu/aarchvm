    .text
    .global _start
_start:
    movz x20, #0x0000
    movk x20, #0x0900, lsl #16      // UART
    adr  x21, shared
    add  x22, x21, #8
    add  x23, x21, #16
    add  x24, x21, #24

    mrs  x0, mpidr_el1
    and  x0, x0, #0xff
    cbz  x0, core0
    cmp  x0, #1
    b.ne secondary_fail

    movz x1, #1
    stlr x1, [x22]                  // secondary_ready
    sev
secondary_wait_done:
    ldar x2, [x23]                  // done
    cbnz x2, secondary_exit
    wfe
    b    secondary_wait_done

core0:
    movz x1, #1
    stlr x1, [x21, #0]              // primary_ready
    sev
core0_wait_secondary:
    ldar x2, [x23]                  // done
    cbnz x2, core0_report_shared
    ldar x3, [x22]
    cbnz x3, core0_verify
    wfe
    b    core0_wait_secondary

core0_verify:
    ldar x4, [x21, #0]
    cmp  x4, #1
    b.ne primary_fail
    ldar x5, [x22]
    cmp  x5, #1
    b.ne primary_fail
    movz x6, #1
    stlr x6, [x23]
    sev
    movz x0, #'S'
    b    print_and_halt

core0_report_shared:
    ldar x0, [x24]
    cbnz x0, print_and_halt
    movz x0, #'s'
    b    print_and_halt

secondary_fail:
    movz x1, #'m'
    stlr x1, [x24]
    movz x2, #1
    stlr x2, [x23]
    sev
secondary_exit:
    brk  #0

primary_fail:
    movz x0, #'M'

print_and_halt:
    str  x0, [x20]
    movz x0, #'\n'
    str  x0, [x20]
    brk  #0

    .data
    .balign 8
shared:
    .quad 0                          // 0: primary_ready
    .quad 0                          // 8: secondary_ready
    .quad 0                          // 16: done
    .quad 0                          // 24: fail_char
