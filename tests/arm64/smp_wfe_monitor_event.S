    .text
    .global _start
_start:
    movz x20, #0x0000
    movk x20, #0x0900, lsl #16      // UART
    adr  x21, shared
    add  x22, x21, #8               // done flag

    mrs  x0, mpidr_el1
    and  x0, x0, #0xff
    cbz  x0, primary
    cmp  x0, #1
    b.ne fail

    sevl
secondary_wait:
    ldxr w1, [x21]
    cbz  w1, secondary_got_lock
    wfe
    b    secondary_wait

secondary_got_lock:
    movz x2, #1
    stlr x2, [x22]
    sev
    brk  #0

primary:
    movz w1, #1
    str  w1, [x21]                  // lock held
    movz x3, #64
primary_delay:
    subs x3, x3, #1
    b.ne primary_delay

    stlr wzr, [x21]                 // unlock without SEV

primary_wait:
    ldar x4, [x22]
    cbnz x4, success
    wfe
    b    primary_wait

success:
    movz x0, #'M'
    str  x0, [x20]
    movz x0, #'\n'
    str  x0, [x20]
    brk  #0

fail:
    movz x0, #'F'
    str  x0, [x20]
    brk  #0

    .data
    .balign 8
shared:
    .word 0
    .word 0
    .quad 0
