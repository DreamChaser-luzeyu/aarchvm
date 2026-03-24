    .text
    .global _start
_start:
    movz x1, #0x0000
    movk x1, #0x0900, lsl #16      // UART
    movz x2, #0x0000
    movk x2, #0x0902, lsl #16      // PERF mailbox

    adr  x10, vectors
    msr  vbar_el1, x10

    adr  x20, after_autiasppc
    .inst 0xF380001Fu              // AUTIASPPC <this PC>
    b    fail
after_autiasppc:
    adr  x20, after_autibsppc
    .inst 0xF3A0003Fu              // AUTIBSPPC <this PC>
    b    fail
after_autibsppc:
    adr  x20, after_paciasppc
    .inst 0xDAC1A3FEu              // PACIASPPC
    b    fail
after_paciasppc:
    adr  x20, after_pacibsppc
    .inst 0xDAC1A7FEu              // PACIBSPPC
    b    fail
after_pacibsppc:
    adr  x20, after_pacnbiasppc
    .inst 0xDAC183FEu              // PACNBIASPPC
    b    fail
after_pacnbiasppc:
    adr  x20, after_pacnbibsppc
    .inst 0xDAC187FEu              // PACNBIBSPPC
    b    fail
after_pacnbibsppc:
    adr  x20, after_autiasppcr
    .inst 0xDAC190BEu              // AUTIASPPCR X5
    b    fail
after_autiasppcr:
    adr  x20, after_autibsppcr
    .inst 0xDAC194DEu              // AUTIBSPPCR X6
    b    fail
after_autibsppcr:
    adr  x20, after_pacia171615
    .inst 0xDAC18BFEu              // PACIA171615
    b    fail
after_pacia171615:
    adr  x20, after_pacib171615
    .inst 0xDAC18FFEu              // PACIB171615
    b    fail
after_pacib171615:
    adr  x20, after_autia171615
    .inst 0xDAC1BBFEu              // AUTIA171615
    b    fail
after_autia171615:
    adr  x20, after_autib171615
    .inst 0xDAC1BFFEu              // AUTIB171615
    b    fail
after_autib171615:
    movz x0, #'L'
    str  x0, [x1]
    movz x0, #'\n'
    str  x0, [x1]
    b    exit_ok

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
    b    1b

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
    movz x25, #0x5                 // EL1h
    msr  spsr_el1, x25
    msr  elr_el1, x20
    eret

fail_from_handler:
    movz x25, #0x5
    msr  spsr_el1, x25
    adr  x26, fail
    msr  elr_el1, x26
    eret
