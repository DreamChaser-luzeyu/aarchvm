    .text
    .global _start
_start:
    movz x1, #0x0000
    movk x1, #0x0900, lsl #16      // UART

    movz x9, #3                    // table descriptor bits
    movz x20, #0x1000              // L0
    movz x21, #0x2000              // L1
    movz x22, #0x3000              // L2 low
    movz x23, #0x4000              // L3 low
    movz x24, #0x7000              // L2 high
    movz x25, #0x8000              // L3 high
    movz x15, #0x0403              // valid + AF + attridx0

    // Low VA identity map for code/data/page tables plus a high VA test page.
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

    movz x3, #0x0050, lsl #16      // VA 0x40000000 -> PA 0x500000
    orr  x2, x3, x15
    str  x2, [x25]

    movz x10, #0x0050, lsl #16
    str  wzr, [x10]

    msr  ttbr0_el1, x20
    movz x2, #16
    movk x2, #0x8010, lsl #16      // 4KB granule, TTBR0/1 T0SZ=T1SZ=16
    msr  tcr_el1, x2
    movz x2, #0
    msr  mair_el1, x2
    movz x2, #1
    msr  sctlr_el1, x2
    isb

    movz x30, #0x4000, lsl #16     // test VA
    adr  x26, shared
    add  x27, x26, #8              // phase
    add  x28, x26, #16             // core1 store done
    add  x24, x26, #24             // secondary failure marker
    add  x23, x26, #32             // failure char

    mrs  x0, mpidr_el1
    and  x0, x0, #0xff
    cbz  x0, core0
    cmp  x0, #1
    b.ne secondary_bad_cpu

core1_wait:
    ldar x12, [x27]
    cbnz x12, core1_store
    ldar x13, [x24]
    cbnz x13, secondary_exit
    wfe
    b    core1_wait

core1_store:
    movz w14, #1
    str  w14, [x30]
    movz x15, #1
    stlr x15, [x28]
    sev

secondary_exit:
    brk  #0

core0:
    ldxr w2, [x30]
    cbnz w2, primary_fail_a
    movz x3, #1
    stlr x3, [x27]
    sev

core0_wait_done:
    ldar x4, [x24]
    cbnz x4, fail_report
    ldar x5, [x28]
    cbnz x5, core0_try_stxr
    wfe
    b    core0_wait_done

core0_try_stxr:
    movz w6, #2
    stxr w7, w6, [x30]
    cbz  w7, primary_fail_b
    ldr  w8, [x30]
    movz w9, #1
    cmp  w8, w9
    b.ne primary_fail_c

success:
    movz x2, #0
    msr  sctlr_el1, x2
    isb
    movz x0, #'V'
    str  x0, [x1]
    movz x0, #'\n'
    str  x0, [x1]
    brk  #0

secondary_bad_cpu:
    movz x2, #'X'
    b    secondary_fail

primary_fail_a:
    movz x2, #'A'
    b    primary_fail

primary_fail_b:
    movz x2, #'B'
    b    primary_fail

primary_fail_c:
    movz x2, #'C'

primary_fail:
    movz x3, #0
    msr  sctlr_el1, x3
    isb
    mov  x0, x2
    str  x0, [x1]
    movz x0, #'\n'
    str  x0, [x1]
    brk  #0

secondary_fail:
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

    .data
    .balign 8
shared:
    .quad 0
    .quad 0    // 8: phase
    .quad 0    // 16: core1 store done
    .quad 0    // 24: secondary failure marker
    .quad 0    // 32: failure char
