    .text
    .global _start
_start:
    movz x20, #0
    movk x20, #0x0900, lsl #16
    adr  x21, dst
    movz x25, #0x0030, lsl #16
    msr  cpacr_el1, x25
    isb

    msr  fpsr, xzr
    adr  x22, src_signed_s
    ldr  q1, [x22]
    fcvtns v0.4s, v1.4s
    str  q0, [x21]
    mov  x0, x21
    adr  x1, exp_fcvtns_s
    bl   check16
    mrs  x2, fpsr
    mov  x3, #(1 << 4)
    cmp  x2, x3
    b.ne fail

    msr  fpsr, xzr
    fcvtps v2.4s, v1.4s
    str  q2, [x21, #16]
    add  x0, x21, #16
    adr  x1, exp_fcvtps_s
    bl   check16
    mrs  x2, fpsr
    mov  x3, #(1 << 4)
    cmp  x2, x3
    b.ne fail

    msr  fpsr, xzr
    fcvtms v3.4s, v1.4s
    str  q3, [x21, #32]
    add  x0, x21, #32
    adr  x1, exp_fcvtms_s
    bl   check16
    mrs  x2, fpsr
    mov  x3, #(1 << 4)
    cmp  x2, x3
    b.ne fail

    msr  fpsr, xzr
    fcvtas v4.4s, v1.4s
    str  q4, [x21, #48]
    add  x0, x21, #48
    adr  x1, exp_fcvtas_s
    bl   check16
    mrs  x2, fpsr
    mov  x3, #(1 << 4)
    cmp  x2, x3
    b.ne fail

    msr  fpsr, xzr
    adr  x22, src_unsigned_s
    ldr  q5, [x22]
    fcvtnu v6.4s, v5.4s
    str  q6, [x21, #64]
    add  x0, x21, #64
    adr  x1, exp_fcvtnu_s
    bl   check16
    mrs  x2, fpsr
    mov  x3, #0x11
    cmp  x2, x3
    b.ne fail

    msr  fpsr, xzr
    fcvtpu v7.4s, v5.4s
    str  q7, [x21, #80]
    add  x0, x21, #80
    adr  x1, exp_fcvtpu_s
    bl   check16
    mrs  x2, fpsr
    mov  x3, #0x11
    cmp  x2, x3
    b.ne fail

    msr  fpsr, xzr
    fcvtmu v8.4s, v5.4s
    str  q8, [x21, #96]
    add  x0, x21, #96
    adr  x1, exp_fcvtmu_s
    bl   check16
    mrs  x2, fpsr
    mov  x3, #0x11
    cmp  x2, x3
    b.ne fail

    msr  fpsr, xzr
    fcvtau v9.4s, v5.4s
    str  q9, [x21, #112]
    add  x0, x21, #112
    adr  x1, exp_fcvtau_s
    bl   check16
    mrs  x2, fpsr
    mov  x3, #0x11
    cmp  x2, x3
    b.ne fail

    msr  fpsr, xzr
    adr  x22, src_2s
    ldr  q10, [x22]
    fcvtps v11.2s, v10.2s
    str  q11, [x21, #128]
    add  x0, x21, #128
    adr  x1, exp_fcvtps_2s
    bl   check16
    mrs  x2, fpsr
    mov  x3, #(1 << 4)
    cmp  x2, x3
    b.ne fail

    msr  fpsr, xzr
    adr  x22, src_signed_d
    ldr  q12, [x22]
    fcvtns v13.2d, v12.2d
    str  q13, [x21, #144]
    add  x0, x21, #144
    adr  x1, exp_fcvtns_d
    bl   check16
    mrs  x2, fpsr
    mov  x3, #(1 << 4)
    cmp  x2, x3
    b.ne fail

    msr  fpsr, xzr
    adr  x22, src_unsigned_d
    ldr  q14, [x22]
    fcvtpu v15.2d, v14.2d
    str  q15, [x21, #160]
    add  x0, x21, #160
    adr  x1, exp_fcvtpu_d
    bl   check16
    mrs  x2, fpsr
    mov  x3, #0x11
    cmp  x2, x3
    b.ne fail

    movz x0, #'O'
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
    movz x0, #'F'
    str  x0, [x20]
    brk  #0

    .data
    .balign 16
src_signed_s:
    .word 0x40200000, 0xc0200000, 0x40066666, 0xc039999a
exp_fcvtns_s:
    .word 0x00000002, 0xfffffffe, 0x00000002, 0xfffffffd
exp_fcvtps_s:
    .word 0x00000003, 0xfffffffe, 0x00000003, 0xfffffffe
exp_fcvtms_s:
    .word 0x00000002, 0xfffffffd, 0x00000002, 0xfffffffd
exp_fcvtas_s:
    .word 0x00000003, 0xfffffffd, 0x00000002, 0xfffffffd

src_unsigned_s:
    .word 0x40200000, 0x40066666, 0x4039999a, 0xbf800000
exp_fcvtnu_s:
    .word 0x00000002, 0x00000002, 0x00000003, 0x00000000
exp_fcvtpu_s:
    .word 0x00000003, 0x00000003, 0x00000003, 0x00000000
exp_fcvtmu_s:
    .word 0x00000002, 0x00000002, 0x00000002, 0x00000000
exp_fcvtau_s:
    .word 0x00000003, 0x00000002, 0x00000003, 0x00000000

src_2s:
    .word 0x40066666, 0x4039999a, 0x00000000, 0x00000000
exp_fcvtps_2s:
    .word 0x00000003, 0x00000003, 0x00000000, 0x00000000

src_signed_d:
    .quad 0x4004000000000000, 0xc007333333333333
exp_fcvtns_d:
    .quad 0x0000000000000002, 0xfffffffffffffffd

src_unsigned_d:
    .quad 0x4004000000000000, 0x7ff0000000000000
exp_fcvtpu_d:
    .quad 0x0000000000000003, 0xffffffffffffffff

dst:
    .space 176
