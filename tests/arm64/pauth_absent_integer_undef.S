    .arch armv8.3-a+pauth
    .text
    .global _start
_start:
    movz x24, #0x0000
    movk x24, #0x0900, lsl #16     // UART
    movz x28, #0x0000
    movk x28, #0x0902, lsl #16     // PERF mailbox

    adr  x25, vectors
    msr  vbar_el1, x25

    bl   load_test_regs

    adr  x20, after_xpaci
    xpaci x2
    b    fail
after_xpaci:
    adr  x20, after_xpacd
    xpacd x3
    b    fail
after_xpacd:
    adr  x20, after_paciza
    paciza x4
    b    fail
after_paciza:
    adr  x20, after_pacizb
    pacizb x5
    b    fail
after_pacizb:
    adr  x20, after_pacia
    pacia x6, x7
    b    fail
after_pacia:
    adr  x20, after_pacib
    pacib x8, x9
    b    fail
after_pacib:
    adr  x20, after_pacdza
    pacdza x10
    b    fail
after_pacdza:
    adr  x20, after_pacdzb
    pacdzb x11
    b    fail
after_pacdzb:
    adr  x20, after_pacda
    pacda x12, x13
    b    fail
after_pacda:
    adr  x20, after_pacdb
    pacdb x14, x15
    b    fail
after_pacdb:
    adr  x20, after_autiza
    autiza x16
    b    fail
after_autiza:
    adr  x20, after_autizb
    autizb x17
    b    fail
after_autizb:
    adr  x20, after_autia
    autia x18, x19
    b    fail
after_autia:
    adr  x20, after_autib
    autib x2, x3
    b    fail
after_autib:
    adr  x20, after_autdza
    autdza x4
    b    fail
after_autdza:
    adr  x20, after_autdzb
    autdzb x5
    b    fail
after_autdzb:
    adr  x20, after_autda
    autda x6, x7
    b    fail
after_autda:
    adr  x20, after_autdb
    autdb x8, x9
    b    fail
after_autdb:
    adr  x20, after_pacga
    pacga x10, x11, x12
    b    fail
after_pacga:
    bl   check_test_regs
    movz x0, #'U'
    str  x0, [x24]
    movz x0, #'\n'
    str  x0, [x24]
    b    exit_ok

load_test_regs:
    adr  x21, expected
    ldr  x2,  [x21, #0]
    ldr  x3,  [x21, #8]
    ldr  x4,  [x21, #16]
    ldr  x5,  [x21, #24]
    ldr  x6,  [x21, #32]
    ldr  x7,  [x21, #40]
    ldr  x8,  [x21, #48]
    ldr  x9,  [x21, #56]
    ldr  x10, [x21, #64]
    ldr  x11, [x21, #72]
    ldr  x12, [x21, #80]
    ldr  x13, [x21, #88]
    ldr  x14, [x21, #96]
    ldr  x15, [x21, #104]
    ldr  x16, [x21, #112]
    ldr  x17, [x21, #120]
    ldr  x18, [x21, #128]
    ldr  x19, [x21, #136]
    ret

check_test_regs:
    adr  x21, expected
    ldr  x22, [x21, #0]
    cmp  x2, x22
    b.ne fail
    ldr  x22, [x21, #8]
    cmp  x3, x22
    b.ne fail
    ldr  x22, [x21, #16]
    cmp  x4, x22
    b.ne fail
    ldr  x22, [x21, #24]
    cmp  x5, x22
    b.ne fail
    ldr  x22, [x21, #32]
    cmp  x6, x22
    b.ne fail
    ldr  x22, [x21, #40]
    cmp  x7, x22
    b.ne fail
    ldr  x22, [x21, #48]
    cmp  x8, x22
    b.ne fail
    ldr  x22, [x21, #56]
    cmp  x9, x22
    b.ne fail
    ldr  x22, [x21, #64]
    cmp  x10, x22
    b.ne fail
    ldr  x22, [x21, #72]
    cmp  x11, x22
    b.ne fail
    ldr  x22, [x21, #80]
    cmp  x12, x22
    b.ne fail
    ldr  x22, [x21, #88]
    cmp  x13, x22
    b.ne fail
    ldr  x22, [x21, #96]
    cmp  x14, x22
    b.ne fail
    ldr  x22, [x21, #104]
    cmp  x15, x22
    b.ne fail
    ldr  x22, [x21, #112]
    cmp  x16, x22
    b.ne fail
    ldr  x22, [x21, #120]
    cmp  x17, x22
    b.ne fail
    ldr  x22, [x21, #128]
    cmp  x18, x22
    b.ne fail
    ldr  x22, [x21, #136]
    cmp  x19, x22
    b.ne fail
    ret

fail:
    movz x0, #'F'
    str  x0, [x24]
    movz x0, #'\n'
    str  x0, [x24]

exit_ok:
    movz x0, #3
    str  x0, [x28, #0x08]
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
    mrs  x26, esr_el1
    ubfx x27, x26, #26, #6
    cbnz x27, fail_from_handler
    movz x23, #0x5                // EL1h
    msr  spsr_el1, x23
    msr  elr_el1, x20
    eret

fail_from_handler:
    movz x23, #0x5
    msr  spsr_el1, x23
    adr  x22, fail
    msr  elr_el1, x22
    eret

    .data
    .balign 8
expected:
    .quad 0x1111111122222222
    .quad 0x3333333344444444
    .quad 0x5555555566666666
    .quad 0x7777777788888888
    .quad 0x99999999aaaabbbb
    .quad 0xccccddddeeeeffff
    .quad 0x0123456789abcdef
    .quad 0xfedcba9876543210
    .quad 0x0f0f0f0f10101010
    .quad 0x2020202030303030
    .quad 0x4040404050505050
    .quad 0x6060606070707070
    .quad 0x8080808090909090
    .quad 0xa0a0a0a0b0b0b0b0
    .quad 0xc0c0c0c0d0d0d0d0
    .quad 0xe0e0e0e0f0f0f0f0
    .quad 0x13579bdf2468ace0
    .quad 0x1122334455667788
