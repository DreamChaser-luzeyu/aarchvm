    .text
    .global _start
_start:
    movz x20, #0
    movk x20, #0x0900, lsl #16
    movz x25, #0x0030, lsl #16
    msr  cpacr_el1, x25
    isb

    adr  x21, dst

    // CMEQ (2D)
    adr  x22, cmp_eq_lhs
    ldr  q0, [x22]
    adr  x22, cmp_eq_rhs
    ldr  q1, [x22]
    cmeq v2.2d, v0.2d, v1.2d
    str  q2, [x21]
    mov  x0, x21
    adr  x1, cmp_eq_exp
    movz x19, #'1'
    bl   check16

    // CMTST (2D), must not alias CMEQ
    adr  x22, cmtst_lhs
    ldr  q0, [x22]
    adr  x22, cmtst_rhs
    ldr  q1, [x22]
    cmtst v2.2d, v0.2d, v1.2d
    str  q2, [x21, #16]
    add  x0, x21, #16
    adr  x1, cmtst_exp
    movz x19, #'2'
    bl   check16

    // Ordered compares (2D), matches the Debian systemd blocker family.
    adr  x22, cmp_ord_lhs
    ldr  q0, [x22]
    adr  x22, cmp_ord_rhs
    ldr  q1, [x22]

    cmhi v2.2d, v0.2d, v1.2d
    str  q2, [x21, #32]
    add  x0, x21, #32
    adr  x1, cmhi_exp
    movz x19, #'3'
    bl   check16

    cmgt v2.2d, v0.2d, v1.2d
    str  q2, [x21, #48]
    add  x0, x21, #48
    adr  x1, cmgt_exp
    movz x19, #'4'
    bl   check16

    cmhs v2.2d, v0.2d, v1.2d
    str  q2, [x21, #64]
    add  x0, x21, #64
    adr  x1, cmhs_exp
    movz x19, #'5'
    bl   check16

    cmge v2.2d, v0.2d, v1.2d
    str  q2, [x21, #80]
    add  x0, x21, #80
    adr  x1, cmge_exp
    movz x19, #'6'
    bl   check16

    movz x0, #'C'
    str  x0, [x20]
    brk  #0

check16:
    ldr  x2, [x0]
    ldr  x3, [x1]
    cmp  x2, x3
    b.ne fail
    ldr  x2, [x0, #8]
    ldr  x3, [x1, #8]
    cmp  x2, x3
    b.ne fail
    ret

fail:
    str  x19, [x20]
    brk  #0

    .data
    .balign 16
cmp_eq_lhs:
    .quad 0x1122334455667788,0x00000000deadbeef
cmp_eq_rhs:
    .quad 0x1122334455667788,0x00000000deadbeee
cmp_eq_exp:
    .quad 0xffffffffffffffff,0x0000000000000000

    .balign 16
cmtst_lhs:
    .quad 0x00000000000000f0,0x0000000000000000
cmtst_rhs:
    .quad 0x0000000000000030,0x0000000000000001
cmtst_exp:
    .quad 0xffffffffffffffff,0x0000000000000000

    .balign 16
cmp_ord_lhs:
    .quad 0x8000000000000000,0x0000000000000005
cmp_ord_rhs:
    .quad 0x7fffffffffffffff,0x0000000000000005
cmhi_exp:
    .quad 0xffffffffffffffff,0x0000000000000000
cmgt_exp:
    .quad 0x0000000000000000,0x0000000000000000
cmhs_exp:
    .quad 0xffffffffffffffff,0xffffffffffffffff
cmge_exp:
    .quad 0x0000000000000000,0xffffffffffffffff

dst:
    .space 96
