    .text
    .global _start
_start:
    movz x1, #0x0000
    movk x1, #0x0900, lsl #16

    adr  x10, vectors
    msr  vbar_el1, x10

    adr  x0, data_buf
dc_cvap_insn:
    .inst 0xD50B7C20              // dc cvap, x0
    brk  #0

after_cvap:
    adr  x11, trap_flags
    ldr  x12, [x11]
    tbz  x12, #0, fail1

    adr  x0, data_buf
dc_cvadp_insn:
    .inst 0xD50B7D20              // dc cvadp, x0
    brk  #0

after_cvadp:
    adr  x11, trap_flags
    ldr  x12, [x11]
    tbz  x12, #1, fail2

    movz x0, #'P'
    str  x0, [x1]
    movz x0, #'\n'
    str  x0, [x1]
    brk  #0

fail1:
    movz x0, #'1'
    b out
fail2:
    movz x0, #'2'
    b out

out:
    str  x0, [x1]
    brk  #0

    .balign 2048
vectors:
    .space 0x200
sync_spx:
    b sync_handler
    .space 0x400 - 0x200 - 4
sync_lower:
    b lower_sync_handler

sync_handler:
    mrs  x20, esr_el1
    ubfx x21, x20, #26, #6
    cbnz x21, handler_fail

    mrs  x22, elr_el1
    adr  x23, dc_cvap_insn
    cmp  x22, x23
    b.eq trap_cvap
    adr  x23, dc_cvadp_insn
    cmp  x22, x23
    b.eq trap_cvadp
    b handler_fail

lower_sync_handler:
    b handler_fail

trap_cvap:
    adr  x24, trap_flags
    ldr  x25, [x24]
    orr  x25, x25, #1
    str  x25, [x24]
    adr  x26, after_cvap
    b return_to_el1

trap_cvadp:
    adr  x24, trap_flags
    ldr  x25, [x24]
    orr  x25, x25, #2
    str  x25, [x24]
    adr  x26, after_cvadp
    b return_to_el1

return_to_el1:
    movz x27, #0x5
    msr  spsr_el1, x27
    msr  elr_el1, x26
    eret

handler_fail:
    movz x0, #'F'
    str  x0, [x1]
    brk  #0

    .data
    .balign 64
trap_flags:
    .quad 0
data_buf:
    .quad 0x0123456789abcdef
