    .text
    .global _start
_start:
    movz x1, #0
    movk x1, #0x0900, lsl #16      // UART

    movz x25, #0x0030, lsl #16
    msr  cpacr_el1, x25
    isb

    adr  x10, vectors
    msr  vbar_el1, x10

    adr  x11, saved_sctlr
    mrs  x12, sctlr_el1
    str  x12, [x11]

    // Enable strict alignment. Structured LD1/ST1 .16B still use 8-bit elements,
    // so misaligned base addresses must remain valid.
    movz x13, #2
    orr  x12, x12, x13
    msr  sctlr_el1, x12
    isb

    // One-register, no-writeback load/store.
    adr  x2, src_one
    add  x2, x2, #1
    ld1  {v0.16b}, [x2]

    adr  x3, dst_one
    add  x3, x3, #3
    st1  {v0.16b}, [x3]
    mov  x0, x3
    adr  x1, expected_one
    bl   check16

    // One-register, post-indexed load/store.
    adr  x4, src_post
    add  x4, x4, #1
    ld1  {v1.16b}, [x4], #16
    adr  x5, src_post
    add  x5, x5, #17
    cmp  x4, x5
    b.ne fail

    adr  x6, dst_post
    add  x6, x6, #5
    st1  {v1.16b}, [x6], #16
    adr  x7, dst_post
    add  x7, x7, #21
    cmp  x6, x7
    b.ne fail
    adr  x0, dst_post
    add  x0, x0, #5
    adr  x1, expected_post
    bl   check16

    // Two-register multiple-structure form must also remain byte-aligned.
    adr  x8, src_two
    add  x8, x8, #1
    ld1  {v2.16b, v3.16b}, [x8]

    adr  x9, dst_two
    add  x9, x9, #7
    st1  {v2.16b, v3.16b}, [x9]
    mov  x0, x9
    adr  x1, expected_two
    bl   check32

pass:
    adr  x11, saved_sctlr
    ldr  x12, [x11]
    msr  sctlr_el1, x12
    isb
    adr  x1, uart_base
    ldr  x1, [x1]
    movz x0, #'J'
    str  x0, [x1]
    movz x0, #'\n'
    str  x0, [x1]
    brk  #0

fail:
    adr  x11, saved_sctlr
    ldr  x12, [x11]
    msr  sctlr_el1, x12
    isb
    adr  x1, uart_base
    ldr  x1, [x1]
    movz x0, #'F'
    str  x0, [x1]
    brk  #0

check16:
    movz x2, #16
1:
    ldrb w4, [x0], #1
    ldrb w5, [x1], #1
    cmp  w4, w5
    b.ne fail
    subs x2, x2, #1
    b.ne 1b
    ret

check32:
    movz x2, #32
2:
    ldrb w4, [x0], #1
    ldrb w5, [x1], #1
    cmp  w4, w5
    b.ne fail
    subs x2, x2, #1
    b.ne 2b
    ret

    .balign 2048
vectors:
    .space 0x200
sync_spx:
    b fail_from_handler

fail_from_handler:
    adr  x11, fail
    msr  elr_el1, x11
    eret

    .data
    .balign 16
uart_base:
    .quad 0x09000000
saved_sctlr:
    .quad 0
src_one:
    .byte 0xf0
    .byte 0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07
    .byte 0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f
expected_one:
    .byte 0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07
    .byte 0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f
src_post:
    .byte 0xe0
    .byte 0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17
    .byte 0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x1e,0x1f
expected_post:
    .byte 0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17
    .byte 0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x1e,0x1f
src_two:
    .byte 0xd0
    .byte 0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27
    .byte 0x28,0x29,0x2a,0x2b,0x2c,0x2d,0x2e,0x2f
    .byte 0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37
    .byte 0x38,0x39,0x3a,0x3b,0x3c,0x3d,0x3e,0x3f
expected_two:
    .byte 0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27
    .byte 0x28,0x29,0x2a,0x2b,0x2c,0x2d,0x2e,0x2f
    .byte 0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37
    .byte 0x38,0x39,0x3a,0x3b,0x3c,0x3d,0x3e,0x3f
dst_one:
    .space 64
dst_post:
    .space 64
dst_two:
    .space 96
