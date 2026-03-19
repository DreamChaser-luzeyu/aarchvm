    .text
    .global _start
_start:
    movz x1, #0x0000
    movk x1, #0x0900, lsl #16

    adr  x10, vectors
    msr  vbar_el1, x10

    adr  x11, trap_count
    str  xzr, [x11]

    adr  x3, el0_tlbi_vmalle1
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret

after_vmalle1:
    adr  x11, trap_count
    ldr  x12, [x11]
    movz x13, #1
    cmp  x12, x13
    b.ne fail1

    adr  x3, el0_tlbi_vae1
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret

after_vae1:
    adr  x11, trap_count
    ldr  x12, [x11]
    movz x13, #2
    cmp  x12, x13
    b.ne fail2

    adr  x3, el0_tlbi_aside1
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret

after_aside1:
    adr  x11, trap_count
    ldr  x12, [x11]
    movz x13, #3
    cmp  x12, x13
    b.ne fail3

    adr  x3, el0_ic_iallu
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret

after_ic_iallu:
    adr  x11, trap_count
    ldr  x12, [x11]
    movz x13, #4
    cmp  x12, x13
    b.ne fail4

    adr  x3, el0_ic_ialluis
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret

after_ic_ialluis:
    adr  x11, trap_count
    ldr  x12, [x11]
    movz x13, #5
    cmp  x12, x13
    b.ne fail5

    adr  x3, el0_dc_isw
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret

after_dc_isw:
    adr  x11, trap_count
    ldr  x12, [x11]
    movz x13, #6
    cmp  x12, x13
    b.ne fail6

    adr  x3, el0_dc_cisw
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret

after_dc_cisw:
    adr  x11, trap_count
    ldr  x12, [x11]
    movz x13, #7
    cmp  x12, x13
    b.ne fail7

    movz x0, #'K'
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
fail3:
    movz x0, #'3'
    b out
fail4:
    movz x0, #'4'
    b out
fail5:
    movz x0, #'5'
    b out
fail6:
    movz x0, #'6'
    b out
fail7:
    movz x0, #'7'
    b out

out:
    str  x0, [x1]
    brk  #0

el0_tlbi_vmalle1:
el0_tlbi_vmalle1_insn:
    tlbi vmalle1
    svc  #0x11

el0_tlbi_vae1:
    adr  x0, trap_count
el0_tlbi_vae1_insn:
    tlbi vae1, x0
    svc  #0x12

el0_tlbi_aside1:
    movz x0, #0
el0_tlbi_aside1_insn:
    tlbi aside1, x0
    svc  #0x13

el0_ic_iallu:
el0_ic_iallu_insn:
    ic   iallu
    svc  #0x14

el0_ic_ialluis:
el0_ic_ialluis_insn:
    ic   ialluis
    svc  #0x15

el0_dc_isw:
    movz x0, #0
el0_dc_isw_insn:
    dc   isw, x0
    svc  #0x16

el0_dc_cisw:
    movz x0, #0
el0_dc_cisw_insn:
    dc   cisw, x0
    svc  #0x17

    .balign 2048
vectors:
    .space 0x400
sync_lower:
    b lower_sync_handler

lower_sync_handler:
    mrs  x20, esr_el1
    ubfx x21, x20, #26, #6
    cbnz x21, handler_fail

    mrs  x22, elr_el1
    adr  x23, el0_tlbi_vmalle1_insn
    cmp  x22, x23
    b.eq handle_vmalle1
    adr  x23, el0_tlbi_vae1_insn
    cmp  x22, x23
    b.eq handle_vae1
    adr  x23, el0_tlbi_aside1_insn
    cmp  x22, x23
    b.eq handle_aside1
    adr  x23, el0_ic_iallu_insn
    cmp  x22, x23
    b.eq handle_ic_iallu
    adr  x23, el0_ic_ialluis_insn
    cmp  x22, x23
    b.eq handle_ic_ialluis
    adr  x23, el0_dc_isw_insn
    cmp  x22, x23
    b.eq handle_dc_isw
    adr  x23, el0_dc_cisw_insn
    cmp  x22, x23
    b.eq handle_dc_cisw
    b handler_fail

handle_vmalle1:
    adr  x24, trap_count
    movz x25, #1
    str  x25, [x24]
    adr  x26, after_vmalle1
    b return_to_el1

handle_vae1:
    adr  x24, trap_count
    movz x25, #2
    str  x25, [x24]
    adr  x26, after_vae1
    b return_to_el1

handle_aside1:
    adr  x24, trap_count
    movz x25, #3
    str  x25, [x24]
    adr  x26, after_aside1
    b return_to_el1

handle_ic_iallu:
    adr  x24, trap_count
    movz x25, #4
    str  x25, [x24]
    adr  x26, after_ic_iallu
    b return_to_el1

handle_ic_ialluis:
    adr  x24, trap_count
    movz x25, #5
    str  x25, [x24]
    adr  x26, after_ic_ialluis
    b return_to_el1

handle_dc_isw:
    adr  x24, trap_count
    movz x25, #6
    str  x25, [x24]
    adr  x26, after_dc_isw
    b return_to_el1

handle_dc_cisw:
    adr  x24, trap_count
    movz x25, #7
    str  x25, [x24]
    adr  x26, after_dc_cisw
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
    .balign 8
trap_count:
    .quad 0
