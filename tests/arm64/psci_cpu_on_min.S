    .text
    .global _start
_start:
    movz x20, #0x0000
    movk x20, #0x0900, lsl #16      // UART
    adr  x21, shared

    mrs  x0, mpidr_el1
    and  x0, x0, #0xff
    cbz  x0, primary

unexpected_secondary:
    movz x0, #'U'
    str  x0, [x20]
    brk  #0

primary:
    movz x0, #0
    movk x0, #0x8400, lsl #16       // PSCI_VERSION
    smc  #0
    cmp  x0, #2
    b.ne fail_v

    movz x0, #4
    movk x0, #0xC400, lsl #16       // PSCI_AFFINITY_INFO_64
    movz x1, #1
    movk x1, #0x8000, lsl #16       // target MPIDR = 0x80000001
    movz x2, #0
    smc  #0
    cmp  x0, #1                     // OFF
    b.ne fail_a

    movz x0, #3
    movk x0, #0xC400, lsl #16       // PSCI_CPU_ON_64
    movz x1, #1
    movk x1, #0x8000, lsl #16
    adr  x2, secondary_entry
    movz x3, #0x55aa
    smc  #0
    cbnz x0, fail_o

wait_flag:
    ldar x4, [x21]
    cbnz x4, verify_affinity
    wfe
    b    wait_flag

verify_affinity:
    movz x0, #4
    movk x0, #0xC400, lsl #16
    movz x1, #1
    movk x1, #0x8000, lsl #16
    movz x2, #0
    smc  #0
    cbnz x0, fail_b                 // ON expected

    movz x0, #'P'
    str  x0, [x20]
    movz x0, #'\n'
    str  x0, [x20]
    brk  #0

secondary_entry:
    adr  x21, shared
    movz x4, #0x55aa
    cmp  x0, x4
    b.ne secondary_fail
    movz x5, #1
    stlr x5, [x21]
    sev
    brk  #0

secondary_fail:
    movz x5, #'S'
    add  x6, x21, #8
    stlr x5, [x6]
    sev
    brk  #0

fail_v:
    movz x0, #'V'
    b fail
fail_a:
    movz x0, #'A'
    b fail
fail_o:
    movz x0, #'O'
    b fail
fail_b:
    movz x0, #'B'
    b fail
fail:
    str  x0, [x20]
    brk  #0

    .data
    .balign 8
shared:
    .quad 0
    .quad 0
