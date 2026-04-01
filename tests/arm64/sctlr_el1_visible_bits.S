    .text
    .global _start
_start:
    movz x20, #0
    movk x20, #0x0900, lsl #16

    // Current model assumptions this test is locking in:
    // - EL0 is AArch64-only.
    // - FEAT_ExS, FEAT_CSV2_1p2/2/3, FEAT_LSMAOC, FEAT_BTI, FEAT_MTE,
    //   FEAT_PAuth, FEAT_MOPS, FEAT_SPECRES, FEAT_TIDCP1 are absent.
    mrs  x0, id_aa64pfr0_el1
    ubfx x1, x0, #0, #4
    movz x2, #1
    cmp  x1, x2
    b.ne fail0
    ubfx x1, x0, #56, #4
    cbnz x1, fail0

    mrs  x0, id_aa64pfr1_el1
    cbnz x0, fail0

    mrs  x0, id_aa64isar1_el1
    cbnz x0, fail0
    mrs  x0, id_aa64isar2_el1
    cbnz x0, fail0

    mrs  x0, id_aa64mmfr1_el1
    ubfx x1, x0, #52, #4
    cbnz x1, fail0
    ubfx x1, x0, #56, #4
    cbnz x1, fail0
    ubfx x1, x0, #32, #4
    cbnz x1, fail0

    mrs  x0, id_aa64mmfr2_el1
    ubfx x1, x0, #12, #4
    cbnz x1, fail0
    ubfx x1, x0, #8, #4
    cbnz x1, fail0
    ubfx x1, x0, #32, #4
    cbnz x1, fail0

    adr  x9, constants
    ldr  x0, [x9, #16]
    msr  sctlr_el1, x0
    isb
    mrs  x1, sctlr_el1
    ldr  x2, [x9]
    cmp  x1, x2
    b.ne fail1

    msr  sctlr_el1, xzr
    isb
    mrs  x1, sctlr_el1
    ldr  x2, [x9, #8]
    cmp  x1, x2
    b.ne fail2

    movz x0, #'S'
    str  x0, [x20]
    movz x0, #'\n'
    str  x0, [x20]
    brk  #0

fail0:
    movz x0, #'0'
    b out
fail1:
    movz x0, #'1'
    b out
fail2:
    movz x0, #'2'
    b out

out:
    str  x0, [x20]
    movz x0, #'F'
    str  x0, [x20]
    brk  #0

    .balign 8
constants:
    .quad 0x0000000034ddcb98
    .quad 0x0000000030500980
    .quad 0xffffffffffffeff8
