    .text
    .global _start
_start:
    movz x20, #0x0000
    movk x20, #0x0900, lsl #16
    movz x25, #0x0030, lsl #16
    msr  cpacr_el1, x25
    isb

    adr  x21, dst

    ldr  w0, =0x3fc00000 // 1.5f
    ldr  w1, =0xc0100000 // -2.25f
    fmov s0, w0
    fmov s1, w1
    fabd s2, s0, s1
    str  s2, [x21]
    mov  x0, x21
    adr  x1, exp_fabd_s
    bl   check4

    ldr  x0, =0x4014000000000000 // 5.0
    ldr  x1, =0x3ff8000000000000 // 1.5
    fmov d3, x0
    fmov d4, x1
    fabd d5, d3, d4
    str  d5, [x21, #8]
    add  x0, x21, #8
    adr  x1, exp_fabd_d
    bl   check8

    movi v6.16b, #0xaa
    ldr  w0, =0x3f800000 // 1.0f
    fmov s0, w0
    fmov s1, w0
    fcmeq s6, s0, s1
    str  q6, [x21, #16]
    add  x0, x21, #16
    adr  x1, exp_fcmeq_s_q
    bl   checkq

    ldr  w0, =0x7fc00000 // qNaN
    ldr  w1, =0x3f800000 // 1.0f
    fmov s0, w0
    fmov s1, w1
    fcmeq s7, s0, s1
    fmov w2, s7
    cbnz w2, fail

    ldr  w0, =0x40000000 // 2.0f
    ldr  w1, =0xc0400000 // -3.0f
    fmov s0, w0
    fmov s1, w1
    fcmge s8, s0, s1
    fmov w2, s8
    ldr  w3, =0xffffffff
    cmp  w2, w3
    b.ne fail

    ldr  x0, =0xbff0000000000000 // -1.0
    fmov d0, x0
    fmov d1, x0
    fcmgt d9, d0, d1
    fmov x2, d9
    cbnz x2, fail

    ldr  w0, =0xc0400000 // -3.0f
    ldr  w1, =0x40000000 // 2.0f
    fmov s0, w0
    fmov s1, w1
    facge s10, s0, s1
    fmov w2, s10
    ldr  w3, =0xffffffff
    cmp  w2, w3
    b.ne fail

    ldr  x0, =0x4000000000000000 // 2.0
    ldr  x1, =0xc000000000000000 // -2.0
    fmov d0, x0
    fmov d1, x1
    facgt d11, d0, d1
    fmov x2, d11
    cbnz x2, fail

    ldr  w0, =0x80000000 // -0.0f
    fmov s0, w0
    fcmeq s12, s0, #0.0
    fmov w2, s12
    ldr  w3, =0xffffffff
    cmp  w2, w3
    b.ne fail

    mov  x0, #0
    fmov d0, x0
    fcmge d13, d0, #0.0
    fmov x2, d13
    ldr  x3, =0xffffffffffffffff
    cmp  x2, x3
    b.ne fail

    ldr  w0, =0xc0000000 // -2.0f
    fmov s0, w0
    fcmgt s14, s0, #0.0
    fmov w2, s14
    cbnz w2, fail

    fcmle s15, s0, #0.0
    fmov w2, s15
    ldr  w3, =0xffffffff
    cmp  w2, w3
    b.ne fail

    ldr  x0, =0xbfd0000000000000 // -0.25
    fmov d0, x0
    fcmlt d16, d0, #0.0
    fmov x2, d16
    ldr  x3, =0xffffffffffffffff
    cmp  x2, x3
    b.ne fail

    ldr  w0, =0x7fc00000 // qNaN
    fmov s0, w0
    fcmgt s17, s0, #0.0
    fmov w2, s17
    cbnz w2, fail

    movz x0, #'J'
    str  x0, [x20]
    brk  #0

check4:
    ldr  w2, [x0]
    ldr  w3, [x1]
    cmp  w2, w3
    b.ne fail
    ret

check8:
    ldr  x2, [x0]
    ldr  x3, [x1]
    cmp  x2, x3
    b.ne fail
    ret

checkq:
    ldr  x2, [x0]
    ldr  x3, [x0, #8]
    ldr  x4, [x1]
    ldr  x5, [x1, #8]
    cmp  x2, x4
    b.ne fail
    cmp  x3, x5
    b.ne fail
    ret

fail:
    movz x0, #'F'
    str  x0, [x20]
    brk  #0

    .data
    .balign 16
exp_fabd_s:
    .word 0x40700000
exp_fabd_d:
    .quad 0x400c000000000000
exp_fcmeq_s_q:
    .quad 0x00000000ffffffff
    .quad 0x0000000000000000

dst:
    .space 32
