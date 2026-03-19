    .text
    .global _start
_start:
    movz x20, #0
    movk x20, #0x0900, lsl #16      // UART
    adr  x21, shared
    add  x22, x21, #8               // woke
    add  x23, x21, #16              // unrelated store target

    mrs  x0, mpidr_el1
    and  x0, x0, #0xff
    cbz  x0, primary
    cmp  x0, #1
    b.ne fail_bad_cpu

    sevl
    wfe
    movz x1, #1
    stlr x1, [x21]                  // started
secondary_wait:
    wfe
    movz x1, #1
    stlr x1, [x22]
    sev
    brk  #0

primary:
primary_wait_started:
    ldar x1, [x21]
    cbnz x1, primary_probe
    b    primary_wait_started

primary_probe:
    movz x2, #0x1234
    str  x2, [x23]                  // unrelated write must not wake WFE
    movz x3, #0x2000
primary_delay:
    subs x3, x3, #1
    b.ne primary_delay

    ldar x4, [x22]
    cbnz x4, fail_spurious

    sev
primary_wait_woke:
    ldar x5, [x22]
    cbnz x5, success
    wfe
    b    primary_wait_woke

fail_bad_cpu:
    movz x0, #'B'
    b    out
fail_spurious:
    movz x0, #'S'
    b    out
success:
    movz x0, #'N'
out:
    str  x0, [x20]
    movz x0, #'\n'
    str  x0, [x20]
    brk  #0

    .data
    .balign 8
shared:
    .quad 0
    .quad 0
    .quad 0
