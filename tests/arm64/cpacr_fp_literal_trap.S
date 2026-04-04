    .text
    .global _start
_start:
    movz x1, #0
    movk x1, #0x0900, lsl #16

    adr  x10, vectors
    msr  vbar_el1, x10

    adr  x11, flags
    stp  xzr, xzr, [x11]
    str  xzr, [x11, #16]

    msr  cpacr_el1, xzr
el1_fp_literal_insn:
    ldr  d0, lit_d

after_el1_trap:
    adr  x11, flags
    ldr  x12, [x11]
    cbz  x12, fail1

    movz x0, #0x0010, lsl #16
    msr  cpacr_el1, x0
    adr  x13, el0_fp_literal_insn
    msr  elr_el1, x13
    msr  spsr_el1, xzr
    eret

after_el0_trap:
    adr  x11, flags
    ldr  x12, [x11, #8]
    cbz  x12, fail2

    movz x0, #0x0030, lsl #16
    msr  cpacr_el1, x0
    adr  x13, el0_fp_literal_ok
    msr  elr_el1, x13
    msr  spsr_el1, xzr
    eret

after_el0_ok:
    adr  x11, flags
    ldr  x12, [x11, #16]
    cbz  x12, fail3

    movz x0, #'V'
    str  x0, [x1]
    movz x0, #'\n'
    str  x0, [x1]
    brk  #0

fail1:
    movz x0, #'1'
    b    out
fail2:
    movz x0, #'2'
    b    out
fail3:
    movz x0, #'3'
    b    out
failh:
    movz x0, #'H'
    b    out
out:
    str  x0, [x1]
    brk  #0

el0_fp_literal_insn:
    ldr  s1, lit_s
    svc  #0x10

el0_fp_literal_ok:
    ldr  q2, lit_q
    adr  x2, scratch
    str  q2, [x2]
    svc  #0x33
    svc  #0x3f

    .balign 2048
vectors:
    .space 0x200
sync_spx:
    b current_sync_handler
    .space 0x1fc
sync_lower:
    b lower_sync_handler

current_sync_handler:
    mrs  x20, esr_el1
    lsr  x21, x20, #26
    movz x22, #0x07
    cmp  x21, x22
    b.ne failh_from_handler
    mrs  x23, elr_el1
    adr  x24, el1_fp_literal_insn
    cmp  x23, x24
    b.ne failh_from_handler
    adr  x24, flags
    movz x25, #1
    str  x25, [x24]
    adr  x26, after_el1_trap
    msr  elr_el1, x26
    eret

lower_sync_handler:
    mrs  x20, esr_el1
    lsr  x21, x20, #26
    movz x22, #0x07
    cmp  x21, x22
    b.eq lower_fp_trap
    movz x22, #0x15
    cmp  x21, x22
    b.eq lower_svc
    b failh_from_handler

lower_fp_trap:
    mrs  x23, elr_el1
    adr  x24, el0_fp_literal_insn
    cmp  x23, x24
    b.ne failh_from_handler
    adr  x24, flags
    movz x25, #1
    str  x25, [x24, #8]
    adr  x26, after_el0_trap
    b return_el1h

lower_svc:
    and  x23, x20, #0xffff
    movz x24, #0x33
    cmp  x23, x24
    b.ne failh_from_handler

    adr  x24, scratch
    ldr  x25, [x24]
    adr  x26, lit_q
    ldr  x27, [x26]
    cmp  x25, x27
    b.ne failh_from_handler
    ldr  x25, [x24, #8]
    ldr  x27, [x26, #8]
    cmp  x25, x27
    b.ne failh_from_handler

    adr  x24, flags
    movz x25, #1
    str  x25, [x24, #16]
    adr  x26, after_el0_ok

return_el1h:
    msr  elr_el1, x26
    movz x27, #0x5
    msr  spsr_el1, x27
    eret

failh_from_handler:
    adr  x26, failh
    msr  elr_el1, x26
    movz x27, #0x5
    msr  spsr_el1, x27
    eret

    .balign 4
lit_s:
    .word 0x11223344
    .balign 8
lit_d:
    .quad 0x5566778899aabbcc
    .balign 16
lit_q:
    .quad 0x0123456789abcdef
    .quad 0xfedcba9876543210

    .data
    .balign 16
flags:
    .quad 0
    .quad 0
    .quad 0
scratch:
    .space 16
