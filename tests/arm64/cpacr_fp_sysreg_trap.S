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
el1_fp_sysreg_read:
el1_fp_sysreg_read_insn:
    mrs  x2, fpcr

after_el1_trap:
    adr  x11, flags
    ldr  x12, [x11]
    cbz  x12, fail1

    movz x0, #0x0010, lsl #16
    msr  cpacr_el1, x0
    adr  x3, el0_fp_sysreg_write
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret

after_el0_trap:
    adr  x11, flags
    ldr  x12, [x11, #8]
    cbz  x12, fail2

    movz x0, #0x0030, lsl #16
    msr  cpacr_el1, x0
    adr  x3, el0_fp_sysreg_ok
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret

after_el0_ok:
    adr  x11, flags
    ldr  x12, [x11, #16]
    cbz  x12, fail3

    movz x0, #0x0020, lsl #16
    msr  cpacr_el1, x0
el1_fp_sysreg_read_fpen10_insn:
    mrs  x2, fpcr

after_el1_fpen10_trap:
    adr  x11, flags
    ldr  x12, [x11, #24]
    cbz  x12, fail4

    adr  x3, el0_fp_sysreg_write_fpen10
    msr  elr_el1, x3
    msr  spsr_el1, xzr
    eret

after_el0_fpen10_trap:
    adr  x11, flags
    ldr  x12, [x11, #32]
    cbz  x12, fail5

    movz x0, #'R'
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
failh:
    movz x0, #'H'
    b out
out:
    str  x0, [x1]
    brk  #0

el0_fp_sysreg_write:
    movz x5, #0x009f
el0_fp_sysreg_write_insn:
    msr  fpsr, x5
    svc  #0x3f

el0_fp_sysreg_write_fpen10:
    movz x5, #0x009f
el0_fp_sysreg_write_fpen10_insn:
    msr  fpsr, x5
    svc  #0x3d

el0_fp_sysreg_ok:
    movz x5, #0x009f
    msr  fpsr, x5
    mrs  x6, fpsr
    cmp  x6, x5
    b.ne el0_fail
    movz x7, #0x00c0, lsl #16
    msr  fpcr, x7
    mrs  x8, fpcr
    cmp  x8, x7
    b.ne el0_fail
    svc  #0x33
el0_fail:
    svc  #0x3e

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
    adr  x24, el1_fp_sysreg_read_insn
    cmp  x23, x24
    b.eq current_el1_fp_trap_00
    adr  x24, el1_fp_sysreg_read_fpen10_insn
    cmp  x23, x24
    b.eq current_el1_fp_trap_10
    b failh_from_handler

current_el1_fp_trap_00:
    adr  x24, flags
    movz x25, #1
    str  x25, [x24]
    adr  x26, after_el1_trap
    msr  elr_el1, x26
    eret

current_el1_fp_trap_10:
    adr  x24, flags
    movz x25, #1
    str  x25, [x24, #24]
    adr  x26, after_el1_fpen10_trap
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
    adr  x24, el0_fp_sysreg_write_insn
    cmp  x23, x24
    b.eq lower_el0_fp_trap_01
    adr  x24, el0_fp_sysreg_write_fpen10_insn
    cmp  x23, x24
    b.eq lower_el0_fp_trap_10
    b failh_from_handler

lower_el0_fp_trap_01:
    adr  x24, flags
    movz x25, #1
    str  x25, [x24, #8]
    adr  x26, after_el0_trap
    b return_el1h

lower_el0_fp_trap_10:
    adr  x24, flags
    movz x25, #1
    str  x25, [x24, #32]
    adr  x26, after_el0_fpen10_trap
    b return_el1h

lower_svc:
    and  x23, x20, #0xffff
    movz x24, #0x33
    cmp  x23, x24
    b.eq lower_ok
    b failh_from_handler

lower_ok:
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

    .data
    .balign 8
flags:
    .quad 0
    .quad 0
    .quad 0
    .quad 0
    .quad 0
