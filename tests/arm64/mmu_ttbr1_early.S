    .text
    .global _start
_start:
    movz x1, #0
    movk x1, #0x0900, lsl #16      // UART
    adr  x29, print_target

    movz x9, #3                     // descriptor bits

    // Tables
    movz x20, #0x1000               // L0 for TTBR1
    movz x21, #0x2000               // L1 for TTBR1

    // L0[0] -> L1
    orr  x2, x21, x9
    str  x2, [x20]

    // L1[0] block -> PA 0x0 (1GB block)
    movz x3, #0
    movz x2, #0x0401                  // valid block + AF
    add  x2, x3, x2
    str  x2, [x21]

    // Put marker at physical 0x100
    movz x10, #0x100
    movz x11, #0x66
    str  x11, [x10]

    // TTBR0/TTBR1 + TCR (T0SZ=T1SZ=16, TG0=4KB, TG1=4KB(10b))
    msr  ttbr0_el1, x20
    msr  ttbr1_el1, x20
    movz x2, #16                     // T0SZ
    movk x2, #0x8010, lsl #16        // TG1=10 + T1SZ=16
    msr  tcr_el1, x2
    movz x2, #1
    msr  sctlr_el1, x2
    isb

    // Build high VA alias for 0x100 and read via TTBR1.
    movz x30, #0xffff, lsl #48
    movz x12, #0x100
    add  x12, x12, x30              // 0xFFFF000000000100
    ldr  x13, [x12]
    subs xzr, x13, #0x66
    b.ne fail

    // Fetch via high VA alias.
    adr  x14, high_target
    add  x14, x14, x30
    br   x14

high_target:
    br   x29

print_target:
    movz x2, #0
    msr  sctlr_el1, x2
    isb
    movz x0, #'K'
    str  x0, [x1]
    movz x0, #'\n'
    str  x0, [x1]
    brk  #0

fail:
    movz x2, #0
    msr  sctlr_el1, x2
    isb
    movz x0, #'F'
    str  x0, [x1]
    brk  #0
