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

secondary_wait_phase1:
    ldar x1, [x22]                  // phase
    cmp  x1, #1
    b.eq secondary_write
    ldar x2, [x23]                  // done
    cbnz x2, secondary_exit
    wfe
    b    secondary_wait_phase1

secondary_write:
    movz w3, #1
    str  w3, [x21, #0]              // ordinary store must clear core0 monitor
    movz x4, #2
    stlr x4, [x22]
    sev
secondary_wait_done:
    ldar x5, [x23]
    cbnz x5, secondary_exit
    wfe
    b    secondary_wait_done

core0:
    ldxr w1, [x21, #0]
    cbnz w1, primary_fail_a
    movz x2, #1
    stlr x2, [x22]
    sev
core0_wait_phase2:
    ldar x3, [x23]
    cbnz x3, core0_report_shared
    ldar x4, [x22]
    cmp  x4, #2
    b.eq core0_try_stxr
    wfe
    b    core0_wait_phase2

core0_try_stxr:
    movz w5, #2
    stxr w6, w5, [x21, #0]
    cbz  w6, primary_fail_b
    ldr  w7, [x21, #0]
    cmp  w7, #1
    b.ne primary_fail_c
    movz x8, #1
    stlr x8, [x23]
    sev
    movz x0, #'I'
    b    print_and_halt

core0_report_shared:
    ldar x0, [x24]
    cbnz x0, print_and_halt
    movz x0, #'i'
    b    print_and_halt

secondary_fail:
    movz x1, #'x'
    stlr x1, [x24]
    movz x2, #1
    stlr x2, [x23]
    sev
secondary_exit:
    brk  #0

primary_fail_a:
    movz x0, #'A'
    b    print_and_halt
primary_fail_b:
    movz x0, #'B'
    b    print_and_halt
primary_fail_c:
    movz x0, #'C'

print_and_halt:
    str  x0, [x20]
    movz x0, #'\n'
    str  x0, [x20]
    brk  #0

    .data
    .balign 8
shared:
    .word 0                          // 0: monitored word
    .word 0
    .quad 0                          // 8: phase
    .quad 0                          // 16: done
    .quad 0                          // 24: fail_char
