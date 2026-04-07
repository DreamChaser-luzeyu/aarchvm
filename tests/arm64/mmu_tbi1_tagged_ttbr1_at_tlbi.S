    .text
    .global _start
_start:
    movz x1, #0
    movk x1, #0x0900, lsl #16      // UART

    movz x9, #3
    movz x15, #0x0403              // L3 page attrs: valid + page + AF
    movz x16, #0x0401              // L1 block attrs: valid + block + AF

    // TTBR0: low 1GiB identity block for boot code/data.
    movz x20, #0x1000              // L0 low
    movz x21, #0x2000              // L1 low
    orr  x2, x21, x9
    str  x2, [x20]
    str  x16, [x21]

    // TTBR1 tables:
    //   high VA 0xABFF000000000000 -> low PA 0x0 page (code alias)
    //   high VA 0xABFF000040000000 -> PA 0x500000 / 0x520000 page (data alias)
    movz x22, #0x5000              // L0 high
    movz x23, #0x6000              // L1 high
    movz x24, #0x7000              // L2 high code
    movz x25, #0x8000              // L3 high code
    movz x26, #0x9000              // L2 high data
    movz x27, #0xA000              // L3 high data

    orr  x2, x23, x9
    str  x2, [x22]                 // L0[0] -> L1 high

    orr  x2, x24, x9
    str  x2, [x23]                 // L1[0] -> code subtree
    orr  x2, x26, x9
    str  x2, [x23, #8]             // L1[1] -> data subtree (VA 0x40000000)

    orr  x2, x25, x9
    str  x2, [x24]                 // L2[0] -> L3 code
    orr  x2, x27, x9
    str  x2, [x26]                 // L2[0] -> L3 data

    // Code alias page at PA 0x0.
    movz x3, #0
    orr  x2, x3, x15
    str  x2, [x25]

    // Data alias page at PA 0x500000 initially, then remap to 0x520000.
    movz x3, #0x0050, lsl #16
    orr  x2, x3, x15
    str  x2, [x27]
    movz x3, #0x0052, lsl #16
    orr  x28, x3, x15

    movz x10, #0x0050, lsl #16
    movz x11, #0x19
    str  x11, [x10]
    movz x10, #0x0052, lsl #16
    movz x11, #0x2A
    str  x11, [x10]

    msr  ttbr0_el1, x20
    msr  ttbr1_el1, x22
    movz x2, #16
    movk x2, #0x8010, lsl #16      // TG1=4KB (10b), T1SZ=16
    movk x2, #0x40, lsl #32        // TBI1=1
    msr  tcr_el1, x2
    movz x2, #0
    msr  mair_el1, x2
    movz x2, #1
    msr  sctlr_el1, x2
    isb

    // Tagged upper data VA: top byte is ignored, bits[55:48] still select TTBR1.
    movz x30, #0x4000, lsl #16
    movk x30, #0xABFF, lsl #48

    // Fill TLB via tagged TTBR1 access.
    ldr  x12, [x30]
    subs xzr, x12, #0x19
    b.ne fail

    // Change the PTE but keep the stale cached translation.
    str  x28, [x27]

    // AT must perform a fresh walk and ignore the tag byte.
    at   s1e1r, x30
    mrs  x13, par_el1
    tbnz x13, #0, fail
    tbz  x13, #11, fail
    ubfx x14, x13, #12, #44
    movz x15, #0x0520
    subs xzr, x14, x15
    b.ne fail
    lsr  x14, x13, #56
    cbnz x14, fail
    ubfx x14, x13, #7, #2
    movz x15, #2
    subs xzr, x14, x15
    b.ne fail

    // Ordinary data access must still observe the stale TLB entry before TLBI.
    ldr  x12, [x30]
    subs xzr, x12, #0x19
    b.ne fail

    // TLBI VAE1 must consume VA[55:12], so the tag byte is ignored here too.
    lsr  x14, x30, #12
    tlbi vae1, x14
    dsb  sy
    isb

    ldr  x12, [x30]
    subs xzr, x12, #0x2A
    b.ne fail

    // Tagged upper fetch should execute, and the guest-visible PC should be canonical.
    adr  x19, high_target
    movk x19, #0xABFF, lsl #48
    blr  x19
    cmp  x0, #0x50                 // 'P'
    b.ne fail

    movz x2, #0
    msr  sctlr_el1, x2
    isb
    movz x0, #'J'
    str  x0, [x1]
    movz x0, #'\n'
    str  x0, [x1]
    brk  #0

high_target:
high_here:
    adr  x2, high_here
    lsr  x3, x2, #56
    movz x4, #0xFF
    subs xzr, x3, x4
    b.ne fail
    ubfx x3, x2, #48, #8
    subs xzr, x3, x4
    b.ne fail
    movz x0, #'P'
    ret

fail:
    movz x2, #0
    msr  sctlr_el1, x2
    isb
    movz x0, #'F'
    str  x0, [x1]
    brk  #0
