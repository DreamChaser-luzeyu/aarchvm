    .text
    .global _start
_start:
    movz x1, #0x0000
    movk x1, #0x0900, lsl #16
    movz x2, #0x0000
    movk x2, #0x0902, lsl #16

    adr  x10, vectors
    msr  vbar_el1, x10

    adr  x5, fail
    adr  x6, fail

    adr  x20, after_retaasppcr
    .inst 0xD65F0BE5u              // RETAASPPCR X5
    b    fail
after_retaasppcr:
    adr  x20, after_retabsppcr
    .inst 0xD65F0FE6u              // RETABSPPCR X6
    b    fail
after_retabsppcr:
    movz x0, #'R'
    str  x0, [x1]
    movz x0, #'\n'
    str  x0, [x1]
    b exit_ok

fail:
    movz x0, #'F'
    str  x0, [x1]
    movz x0, #'\n'
    str  x0, [x1]

exit_ok:
    movz x0, #3
    str  x0, [x2, #0x08]
1:
    wfi
    b 1b

    .balign 2048
vectors:
    .space 0x200
sync_spx:
    b sync_handler
    .space 0x1fc
sync_lower:
    b fail_from_handler

sync_handler:
    mrs  x21, esr_el1
    ubfx x22, x21, #26, #6
    cbnz x22, fail_from_handler
    tbz  x21, #25, fail_from_handler
    and  x23, x21, #0x1ffffff
    cbnz x23, fail_from_handler
    mrs  x24, far_el1
    cbnz x24, fail_from_handler
    movz x25, #0x5
    msr  spsr_el1, x25
    msr  elr_el1, x20
    eret

fail_from_handler:
    movz x25, #0x5
    msr  spsr_el1, x25
    adr  x26, fail
    msr  elr_el1, x26
    eret
