    .text
    .global _start
_start:
    movz x1, #0x0000
    movk x1, #0x0900, lsl #16
    movz x2, #0x0000
    movk x2, #0x0902, lsl #16

    adr  x10, vectors
    msr  vbar_el1, x10

    adr  x30, fail
    adr  x13, fail
    adr  x14, fail
    adr  x15, fail
    adr  x16, fail
    adr  x17, fail
    adr  x18, fail
    adr  x19, fail
    adr  x21, fail
    adr  x22, fail
    adr  x23, fail
    adr  x24, fail
    adr  x25, fail

    adr  x20, after_braa
    .inst 0xD71F09AEu              // BRAA X13, X14
    b    fail
after_braa:
    adr  x20, after_brab
    .inst 0xD71F0DF0u              // BRAB X15, X16
    b    fail
after_brab:
    adr  x20, after_blraa
    .inst 0xD73F0A32u              // BLRAA X17, X18
    b    fail
after_blraa:
    adr  x20, after_blrab
    .inst 0xD73F0E74u              // BLRAB X19, X20
    b    fail
after_blrab:
    adr  x20, after_braaz
    .inst 0xD61F097Fu              // BRAAZ X11
    b    fail
after_braaz:
    adr  x20, after_brabz
    .inst 0xD61F0D9Fu              // BRABZ X12
    b    fail
after_brabz:
    adr  x20, after_blraaz
    .inst 0xD63F09BFu              // BLRAAZ X13
    b    fail
after_blraaz:
    adr  x20, after_blrabz
    .inst 0xD63F0DDFu              // BLRABZ X14
    b    fail
after_blrabz:
    adr  x20, after_retaa
    .inst 0xD65F0BFFu              // RETAA
    b    fail
after_retaa:
    adr  x20, after_retab
    .inst 0xD65F0FFFu              // RETAB
    b    fail
after_retab:
    adr  x20, after_eretaa
    .inst 0xD69F0BFFu              // ERETAA
    b    fail
after_eretaa:
    adr  x20, after_eretab
    .inst 0xD69F0FFFu              // ERETAB
    b    fail
after_eretab:
    movz x0, #'B'
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
    mrs  x26, esr_el1
    ubfx x27, x26, #26, #6
    cbnz x27, fail_from_handler
    tbz  x26, #25, fail_from_handler
    and  x28, x26, #0x1ffffff
    cbnz x28, fail_from_handler
    mrs  x29, far_el1
    cbnz x29, fail_from_handler
    movz x9, #0x5
    msr  spsr_el1, x9
    msr  elr_el1, x20
    eret

fail_from_handler:
    movz x9, #0x5
    msr  spsr_el1, x9
    adr  x26, fail
    msr  elr_el1, x26
    eret
