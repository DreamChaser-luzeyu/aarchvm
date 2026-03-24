    .arch armv8.3-a+pauth
    .text
    .global _start
_start:
    movz x1, #0x0000
    movk x1, #0x0900, lsl #16      // UART
    movz x28, #0x0000
    movk x28, #0x0902, lsl #16     // PERF mailbox

    adr  x10, vectors
    msr  vbar_el1, x10

    adr  x29, el1_pauth_nops
    b    load_test_regs

el1_pauth_nops:
    xpaclri
    pacia1716
    pacib1716
    autia1716
    autib1716
    paciasp
    pacibsp
    autiasp
    autibsp
    paciaz
    pacibz
    autiaz
    autibz

    adr  x29, el1_checked
    b    check_test_regs

el1_checked:

    adr  x27, el0_entry
    msr  elr_el1, x27
    msr  spsr_el1, xzr             // EL0t
    eret

after_el0:
    adr  x29, after_el0_checked
    b    check_test_regs

after_el0_checked:
    movz x0, #'P'
    str  x0, [x1]
    movz x0, #'\n'
    str  x0, [x1]
    b    exit_ok

el0_entry:
    xpaclri
    autia1716
    paciasp
    autibsp
    svc  #0x44
    b    fail

load_test_regs:
    adr  x20, expected
    ldr  x2,  [x20, #0]
    ldr  x3,  [x20, #8]
    ldr  x4,  [x20, #16]
    ldr  x5,  [x20, #24]
    ldr  x6,  [x20, #32]
    ldr  x7,  [x20, #40]
    ldr  x8,  [x20, #48]
    ldr  x9,  [x20, #56]
    ldr  x10, [x20, #64]
    ldr  x11, [x20, #72]
    ldr  x12, [x20, #80]
    ldr  x13, [x20, #88]
    ldr  x14, [x20, #96]
    ldr  x15, [x20, #104]
    ldr  x16, [x20, #112]
    ldr  x17, [x20, #120]
    ldr  x18, [x20, #128]
    ldr  x19, [x20, #136]
    ldr  x30, [x20, #144]
    br   x29

check_test_regs:
    adr  x20, expected
    ldr  x21, [x20, #0]
    cmp  x2, x21
    b.ne fail
    ldr  x21, [x20, #8]
    cmp  x3, x21
    b.ne fail
    ldr  x21, [x20, #16]
    cmp  x4, x21
    b.ne fail
    ldr  x21, [x20, #24]
    cmp  x5, x21
    b.ne fail
    ldr  x21, [x20, #32]
    cmp  x6, x21
    b.ne fail
    ldr  x21, [x20, #40]
    cmp  x7, x21
    b.ne fail
    ldr  x21, [x20, #48]
    cmp  x8, x21
    b.ne fail
    ldr  x21, [x20, #56]
    cmp  x9, x21
    b.ne fail
    ldr  x21, [x20, #64]
    cmp  x10, x21
    b.ne fail
    ldr  x21, [x20, #72]
    cmp  x11, x21
    b.ne fail
    ldr  x21, [x20, #80]
    cmp  x12, x21
    b.ne fail
    ldr  x21, [x20, #88]
    cmp  x13, x21
    b.ne fail
    ldr  x21, [x20, #96]
    cmp  x14, x21
    b.ne fail
    ldr  x21, [x20, #104]
    cmp  x15, x21
    b.ne fail
    ldr  x21, [x20, #112]
    cmp  x16, x21
    b.ne fail
    ldr  x21, [x20, #120]
    cmp  x17, x21
    b.ne fail
    ldr  x21, [x20, #128]
    cmp  x18, x21
    b.ne fail
    ldr  x21, [x20, #136]
    cmp  x19, x21
    b.ne fail
    ldr  x21, [x20, #144]
    cmp  x30, x21
    b.ne fail
    br   x29

fail:
    movz x0, #'F'
    str  x0, [x1]
    movz x0, #'\n'
    str  x0, [x1]

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
    b sync_handler

sync_handler:
    mrs  x22, esr_el1
    ubfx x23, x22, #26, #6
    cmp  x23, #0x15               // SVC from EL0 only
    b.ne fail_from_handler
    and  x24, x22, #0xFFFF
    movz x25, #0x44
    cmp  x24, x25
    b.ne fail_from_handler
    movz x26, #0x5                // EL1h
    msr  spsr_el1, x26
    adr  x27, after_el0
    msr  elr_el1, x27
    eret

fail_from_handler:
    movz x26, #0x5
    msr  spsr_el1, x26
    adr  x27, fail
    msr  elr_el1, x27
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
    .quad 0x8877665544332211
