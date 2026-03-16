    .text
    .global _start
_start:
    movz x1, #0
    movk x1, #0x0900, lsl #16      // UART

    movz x9, #3                    // table descriptor bits
    movz x20, #0x1000              // L0
    movz x21, #0x2000              // L1
    movz x22, #0x3000              // L2 low
    movz x23, #0x4000              // L3 low
    movz x24, #0x7000              // L2 high
    movz x25, #0x8000              // L3 high
    movz x15, #0x0403              // valid + AF + attridx0

    // Build minimal page tables.
    orr  x2, x21, x9
    str  x2, [x20]
    orr  x2, x22, x9
    str  x2, [x21]
    orr  x2, x24, x9
    str  x2, [x21, #8]
    orr  x2, x23, x9
    str  x2, [x22]
    orr  x2, x25, x9
    str  x2, [x24]

    // Identity map low pages for code/data/page tables.
    movz x3, #0x0000
    orr  x2, x3, x15
    str  x2, [x23, #0]
    movz x3, #0x1000
    orr  x2, x3, x15
    str  x2, [x23, #8]
    movz x3, #0x2000
    orr  x2, x3, x15
    str  x2, [x23, #16]
    movz x3, #0x3000
    orr  x2, x3, x15
    str  x2, [x23, #24]
    movz x3, #0x4000
    orr  x2, x3, x15
    str  x2, [x23, #32]
    movz x3, #0x7000
    orr  x2, x3, x15
    str  x2, [x23, #56]
    movz x3, #0x8000
    orr  x2, x3, x15
    str  x2, [x23, #64]

    // Shared virtual page: VA 0x40000000 -> PA 0x500000 initially.
    movz x3, #0x0050, lsl #16
    orr  x2, x3, x15
    str  x2, [x25]

    // Backing pages contents.
    movz x10, #0x0050, lsl #16
    movz x11, #0x11
    str  x11, [x10]
    movz x10, #0x0060, lsl #16
    movz x11, #0x22
    str  x11, [x10]

    // Enable MMU identically on both CPUs.
    msr  ttbr0_el1, x20
    movz x2, #16
    movk x2, #0x8010, lsl #16      // T0SZ=16, T1SZ=16, TG1=4KB
    msr  tcr_el1, x2
    movz x2, #0
    msr  mair_el1, x2
    movz x2, #1
    msr  sctlr_el1, x2
    isb

    movz x30, #0x4000, lsl #16     // VA under test
    adr  x26, shared
    add  x27, x26, #8              // phase1_done
    add  x28, x26, #16             // phase2_go
    add  x24, x26, #24             // finished
    add  x23, x26, #32             // fail char

    mrs  x0, mpidr_el1
    and  x0, x0, #0xff
    cbz  x0, core0
    cmp  x0, #1
    b.ne secondary_bad_cpu

core1_prime:
    ldr  x12, [x30]
    subs xzr, x12, #0x11
    b.ne secondary_fail_a
    movz x13, #1
    stlr x13, [x27]
    sev
core1_wait_remap:
    ldar x14, [x28]
    cbnz x14, core1_check_new
    wfe
    b    core1_wait_remap

core1_check_new:
    ldr  x12, [x30]
    subs xzr, x12, #0x22
    b.ne secondary_fail_b
    movz x13, #1
    stlr x13, [x24]
    sev
    brk  #0

core0:
core0_wait_prime:
    ldar x13, [x27]
    cbnz x13, core0_remap
    wfe
    b    core0_wait_prime

core0_remap:
    movz x3, #0x0060, lsl #16
    orr  x2, x3, x15
    str  x2, [x25]
    dsb  sy
    tlbi vae1is, x30
    dsb  sy
    isb
    movz x13, #1
    stlr x13, [x28]
    sev

core0_wait_done:
    ldar x14, [x24]
    cbnz x14, success
    ldar x15, [x23]
    cbnz x15, fail_report
    wfe
    b    core0_wait_done

secondary_bad_cpu:
    movz x2, #'X'
    stlr x2, [x23]
    movz x3, #1
    stlr x3, [x24]
    sev
    brk  #0

secondary_fail_a:
    movz x2, #'A'
    stlr x2, [x23]
    movz x3, #1
    stlr x3, [x24]
    sev
    brk  #0

secondary_fail_b:
    movz x2, #'B'
    stlr x2, [x23]
    movz x3, #1
    stlr x3, [x24]
    sev
    brk  #0

fail_report:
    movz x2, #0
    msr  sctlr_el1, x2
    isb
    ldr  x0, [x23]
    str  x0, [x1]
    movz x0, #'\n'
    str  x0, [x1]
    brk  #0

success:
    movz x2, #0
    msr  sctlr_el1, x2
    isb
    movz x0, #'M'
    str  x0, [x1]
    movz x0, #'\n'
    str  x0, [x1]
    brk  #0

    .data
    .balign 8
shared:
    .quad 0
    .quad 0    // 8: phase1_done
    .quad 0    // 16: phase2_go
    .quad 0    // 24: finished
    .quad 0    // 32: fail char
