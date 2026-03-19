    .text
    .global _start
_start:
    movz x1, #0
    movk x1, #0x0900, lsl #16

    adr  x2, fp_src
    adr  x3, fp_dst

    adr  x10, vectors
    msr  vbar_el1, x10

    adr  x11, flags
    stp  xzr, xzr, [x11]
    stp  xzr, xzr, [x11, #16]

    msr  cpacr_el1, xzr
el1_structured_insn:
    ld3  {v0.16b, v1.16b, v2.16b}, [x2]

after_el1_trap:
    adr  x11, flags
    ldr  x12, [x11]
    cbz  x12, fail1

    movz x0, #0x0010, lsl #16
    msr  cpacr_el1, x0
    adr  x13, el0_structured_insn
    msr  elr_el1, x13
    msr  spsr_el1, xzr
    eret

after_el0_trap:
    adr  x11, flags
    ldr  x12, [x11, #8]
    cbz  x12, fail2

    movz x0, #0x0030, lsl #16
    msr  cpacr_el1, x0
    adr  x13, el0_structured_ok
    msr  elr_el1, x13
    msr  spsr_el1, xzr
    eret

after_el0_ok:
    adr  x11, flags
    ldr  x12, [x11, #16]
    cbz  x12, fail3

    movz x0, #'T'
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
    str  x0, [x1]
    brk  #0

el0_structured_insn:
    ld3  {v0.16b, v1.16b, v2.16b}, [x2]
    svc  #0x10

el0_structured_ok:
    ld3  {v0.16b, v1.16b, v2.16b}, [x2]
    str  q0, [x3, #0]
    str  q1, [x3, #16]
    str  q2, [x3, #32]
    mov  x0, x3
    adr  x4, fp_exp
    mov  x2, #48
    bl   check_mem
    svc  #0x33
el0_structured_fail:
    svc  #0x3f

check_mem:
    cbz  x2, check_ok
check_loop:
    ldrb w6, [x0], #1
    ldrb w7, [x4], #1
    cmp  w6, w7
    b.ne el0_structured_fail
    subs x2, x2, #1
    b.ne check_loop
check_ok:
    ret

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
    adr  x24, el1_structured_insn
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
    adr  x24, el0_structured_insn
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

fp_src:
    .byte 0x00,0x80,0xc0,0x01,0x81,0xc1,0x02,0x82,0xc2,0x03,0x83,0xc3,0x04,0x84,0xc4,0x05
    .byte 0x85,0xc5,0x06,0x86,0xc6,0x07,0x87,0xc7,0x08,0x88,0xc8,0x09,0x89,0xc9,0x0a,0x8a
    .byte 0xca,0x0b,0x8b,0xcb,0x0c,0x8c,0xcc,0x0d,0x8d,0xcd,0x0e,0x8e,0xce,0x0f,0x8f,0xcf
fp_exp:
    .byte 0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f
    .byte 0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x8a,0x8b,0x8c,0x8d,0x8e,0x8f
    .byte 0xc0,0xc1,0xc2,0xc3,0xc4,0xc5,0xc6,0xc7,0xc8,0xc9,0xca,0xcb,0xcc,0xcd,0xce,0xcf
fp_dst:
    .space 48
