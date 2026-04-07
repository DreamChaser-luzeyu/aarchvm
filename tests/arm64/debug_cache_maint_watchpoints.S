    .text
    .global _start
_start:
    movz x1, #0
    movk x1, #0x0900, lsl #16      // UART

    adr  x10, vectors
    msr  vbar_el1, x10

    adr  x11, handled
    str  xzr, [x11]

    // Breakpoint/watchpoint exceptions require the OS Lock to be unlocked and
    // the OS Double Lock to be inactive.
    msr  oslar_el1, xzr
    msr  osdlr_el1, xzr
    msr  dbgprcr_el1, xzr
    isb

    movz x9, #3
    movz x15, #0x0403
    movz x20, #0x1000              // L0
    movz x21, #0x2000              // L1
    movz x22, #0x3000              // L2 low
    movz x23, #0x4000              // L3 low
    movz x24, #0x7000              // L2 high
    movz x25, #0x8000              // L3 high

    // Low VA identity map + high half page tables.
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

    // VA 0x40000000 -> PA 0x500000, AF=1, AP[2]=0 => read/write at EL1.
    movz x3, #0x0050, lsl #16
    movz x2, #0x0403
    orr  x26, x3, x2
    str  x26, [x25]

    // Initialize the watched cache line before enabling the watchpoint.
    movz x26, #0x0050, lsl #16
    movz x27, #0x7788
    movk x27, #0x5566, lsl #16
    movk x27, #0x3344, lsl #32
    movk x27, #0x1122, lsl #48
    str  x27, [x26]
    movz x28, #0xff00
    movk x28, #0xddee, lsl #16
    movk x28, #0xbbcc, lsl #32
    movk x28, #0x99aa, lsl #48
    str  x28, [x26, #8]

    msr  ttbr0_el1, x20
    movz x2, #16
    msr  tcr_el1, x2
    movz x2, #0xff
    msr  mair_el1, x2
    movz x2, #1
    msr  sctlr_el1, x2
    isb

    movz x30, #0x4000, lsl #16
    msr  dbgwvr0_el1, x30
    movz x5, #0x1ff3              // BAS=0xff, store, EL1, enabled
    msr  dbgwcr0_el1, x5
    movz x5, #0xa000              // KDE|MDE
    msr  mdscr_el1, x5
    isb

fault_ivac:
    dc   ivac, x30
    b    fail_i
after_ivac:
    adr  x11, handled
    ldr  x12, [x11]
    cmp  x12, #1
    b.ne fail_a

fault_zva:
    dc   zva, x30
    b    fail_z
after_zva:
    adr  x11, handled
    ldr  x12, [x11]
    cmp  x12, #2
    b.ne fail_b

    ldr  x13, [x30]
    movz x14, #0x7788
    movk x14, #0x5566, lsl #16
    movk x14, #0x3344, lsl #32
    movk x14, #0x1122, lsl #48
    cmp  x13, x14
    b.ne fail_c

    ldr  x13, [x30, #8]
    movz x14, #0xff00
    movk x14, #0xddee, lsl #16
    movk x14, #0xbbcc, lsl #32
    movk x14, #0x99aa, lsl #48
    cmp  x13, x14
    b.ne fail_d

    movz x2, #0
    msr  sctlr_el1, x2
    isb
    movz x0, #'W'
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

fail_i:
    movz x0, #'I'
    b out_fail
fail_a:
    movz x0, #'A'
    b out_fail
fail_z:
    movz x0, #'Z'
    b out_fail
fail_b:
    movz x0, #'B'
    b out_fail
fail_c:
    movz x0, #'C'
    b out_fail
fail_d:
    movz x0, #'D'
    b out_fail
fail_h:
    movz x0, #'H'
    b out_fail
fail_1:
    movz x0, #'1'
    b out_fail
fail_2:
    movz x0, #'2'
    b out_fail
fail_3:
    movz x0, #'3'
    b out_fail
fail_4:
    movz x0, #'4'
    b out_fail
fail_5:
    movz x0, #'5'
    b out_fail
fail_6:
    movz x0, #'6'
    b out_fail
fail_7:
    movz x0, #'7'
    b out_fail
fail_8:
    movz x0, #'8'
    b out_fail
fail_9:
    movz x0, #'9'
    b out_fail
fail_j:
    movz x0, #'J'
    b out_fail

out_fail:
    movz x2, #0
    msr  sctlr_el1, x2
    isb
    str  x0, [x1]
    brk  #0

    .balign 2048
vectors:
    .space 0x200
sync_spx:
    b sync_handler

sync_handler:
    mrs  x2, esr_el1
    ubfx x3, x2, #26, #6
    cmp  x3, #0x35
    b.ne fail_h1
    and  x4, x2, #0x3f
    cmp  x4, #0x22
    b.ne fail_h2
    tbz  x2, #6, fail_h3

    adr  x11, handled
    ldr  x12, [x11]
    cbz  x12, handle_ivac
    cmp  x12, #1
    b.eq handle_zva
    b    fail_h4

handle_ivac:
    tbz  x2, #8, fail_h5
    mrs  x5, far_el1
    subs xzr, x5, x30
    b.ne fail_h6
    mrs  x6, elr_el1
    adr  x7, fault_ivac
    subs xzr, x6, x7
    b.ne fail_h7
    movz x12, #1
    str  x12, [x11]
    adr  x8, after_ivac
    msr  elr_el1, x8
    eret

handle_zva:
    tbnz x2, #8, fail_h8
    mrs  x5, far_el1
    subs xzr, x5, x30
    b.ne fail_h9
    mrs  x6, elr_el1
    adr  x7, fault_zva
    subs xzr, x6, x7
    b.ne fail_hj
    movz x12, #2
    str  x12, [x11]
    adr  x8, after_zva
    msr  elr_el1, x8
    eret

fail_from_handler:
    adr  x8, fail_h
    msr  elr_el1, x8
    eret

fail_h1:
    adr  x8, fail_1
    msr  elr_el1, x8
    eret
fail_h2:
    adr  x8, fail_2
    msr  elr_el1, x8
    eret
fail_h3:
    adr  x8, fail_3
    msr  elr_el1, x8
    eret
fail_h4:
    adr  x8, fail_4
    msr  elr_el1, x8
    eret
fail_h5:
    adr  x8, fail_5
    msr  elr_el1, x8
    eret
fail_h6:
    adr  x8, fail_6
    msr  elr_el1, x8
    eret
fail_h7:
    adr  x8, fail_7
    msr  elr_el1, x8
    eret
fail_h8:
    adr  x8, fail_8
    msr  elr_el1, x8
    eret
fail_h9:
    adr  x8, fail_9
    msr  elr_el1, x8
    eret
fail_hj:
    adr  x8, fail_j
    msr  elr_el1, x8
    eret

    .data
    .balign 8
handled:
    .quad 0
