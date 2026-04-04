    .text
    .global _start

    .macro CHECK_MEM128 base, expected, fail_label
    ldr  x0, [\base]
    ldr  x1, [\expected]
    cmp  x0, x1
    b.ne \fail_label
    ldr  x0, [\base, #8]
    ldr  x1, [\expected, #8]
    cmp  x0, x1
    b.ne \fail_label
    .endm

_start:
    movz x1, #0
    movk x1, #0x0900, lsl #16

    movz x25, #0x0030, lsl #16
    msr  cpacr_el1, x25
    isb

    adr  x2, src_words
    ldr  s0, [x2]
    ldr  s1, [x2, #4]
    adr  x3, pair_dst
    stp  xzr, xzr, [x3]

    adr  x10, vectors
    msr  vbar_el1, x10

    adr  x11, flags
    stp  xzr, xzr, [x11]
    stp  xzr, xzr, [x11, #16]

    msr  cpacr_el1, xzr
el1_fp_pair_insn:
    stnp s0, s1, [x3]

after_el1_trap:
    adr  x11, flags
    ldr  x12, [x11]
    cbz  x12, fail1
    ldr  x13, [x3]
    cbnz x13, fail1

    movz x0, #0x0010, lsl #16
    msr  cpacr_el1, x0
    adr  x13, el0_fp_pair_insn
    msr  elr_el1, x13
    msr  spsr_el1, xzr
    eret

after_el0_trap:
    adr  x11, flags
    ldr  x12, [x11, #8]
    cbz  x12, fail2
    adr  x13, saved_el0_base
    ldr  x14, [x13]
    subs xzr, x3, x14
    b.ne fail2

    movz x0, #0x0030, lsl #16
    msr  cpacr_el1, x0
    adr  x13, el0_fp_pair_ok
    msr  elr_el1, x13
    msr  spsr_el1, xzr
    eret

after_el0_ok:
    adr  x11, flags
    ldr  x12, [x11, #16]
    cbz  x12, fail3

    movz x1, #0
    movk x1, #0x0900, lsl #16
    movz x0, #'Y'
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
failh:
    movz x0, #'H'
    b out
out:
    movz x1, #0
    movk x1, #0x0900, lsl #16
    str  x0, [x1]
    brk  #0

el0_fp_pair_insn:
    adr  x3, el0_pair_buf
    add  x3, x3, #8
    adr  x4, saved_el0_base
    str  x3, [x4]
el0_fp_pair_trap_insn:
    ldp  s2, s3, [x3], #8
    svc  #0x10

el0_fp_pair_ok:
    adr  x4, src_words
    movi v4.16b, #0xff
    movi v5.16b, #0xee
    ldnp s4, s5, [x4]
    adr  x6, scratch
    str  q4, [x6]
    str  q5, [x6, #16]
    adr  x7, expect_q4
    CHECK_MEM128 x6, x7, fail3
    add  x6, x6, #16
    adr  x7, expect_q5
    CHECK_MEM128 x6, x7, fail3
    svc  #0x33

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
    adr  x24, el1_fp_pair_insn
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
    adr  x24, el0_fp_pair_trap_insn
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
    .balign 16
flags:
    .quad 0
    .quad 0
    .quad 0
    .quad 0
saved_el0_base:
    .quad 0
src_words:
    .word 0x11223344
    .word 0x55667788
pair_dst:
    .space 16
el0_pair_buf:
    .space 32
expect_q4:
    .quad 0x0000000011223344
    .quad 0x0000000000000000
expect_q5:
    .quad 0x0000000055667788
    .quad 0x0000000000000000
scratch:
    .space 32
