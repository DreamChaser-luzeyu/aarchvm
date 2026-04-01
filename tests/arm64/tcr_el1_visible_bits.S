    .text
    .global _start
_start:
    movz x20, #0
    movk x20, #0x0900, lsl #16

    adr  x9, constants
    ldr  x0, [x9]
    ldr  x1, [x9, #8]

    msr  tcr_el1, x0
    isb
    mrs  x2, tcr_el1
    cmp  x2, x1
    b.ne fail1

    // AS must remain RES0 when only 8-bit ASIDs are implemented.
    tbnz x2, #36, fail2
    // PAuth is absent in the current model, so TBID1/TBID0 stay RES0.
    tbnz x2, #52, fail3
    tbnz x2, #51, fail3
    // TBI1/TBI0 are architected and should remain writable.
    tbz  x2, #38, fail4
    tbz  x2, #37, fail4

    movz x0, #'R'
    str  x0, [x20]
    movz x0, #'\n'
    str  x0, [x20]
    brk  #0

fail1:
    movz x0, #'1'
    b out
fail2:
    movz x0, #'2'
    b out
fail3:
    movz x0, #'3'
    b out
fail4:
    movz x0, #'4'
    b out

out:
    str  x0, [x20]
    movz x0, #'F'
    str  x0, [x20]
    brk  #0

    .balign 8
constants:
    .quad 0xfffffff2b6d03990
    .quad 0x00000062b6d03990
