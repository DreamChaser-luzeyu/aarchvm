    .text
    .global _start
_start:
    movz x1, #0
    movk x1, #0x0900, lsl #16      // UART

    mrs  x20, sctlr_el1

    // Keep stage-1 disabled and force I=1 so the direct data-side AT result
    // cannot accidentally inherit the instruction-side Normal WT attributes.
    movz x21, #1
    bic  x22, x20, x21
    movz x21, #0x1000
    orr  x22, x22, x21
    msr  sctlr_el1, x22
    isb

    movz x29, #0x0050, lsl #16

    at   s1e1r, x29
    mrs  x4, par_el1
    bl   check_par

    at   s1e1w, x29
    mrs  x4, par_el1
    bl   check_par

    msr  sctlr_el1, x20
    isb
    movz x0, #'H'
    str  x0, [x1]
    movz x0, #'\n'
    str  x0, [x1]
    brk  #0

fail:
    msr  sctlr_el1, x20
    isb
    movz x0, #'F'
    str  x0, [x1]
    brk  #0

check_par:
    tbnz x4, #0, fail               // F == 0
    tbz  x4, #11, fail              // RES1 bit remains set

    ubfx x5, x4, #12, #44
    movz x6, #0x0500
    cmp  x5, x6
    b.ne fail

    lsr  x5, x4, #56                // ATTR == Device-nGnRnE
    cbnz x5, fail

    ubfx x5, x4, #7, #2             // SH == Outer Shareable
    movz x6, #2
    cmp  x5, x6
    b.ne fail
    ret
