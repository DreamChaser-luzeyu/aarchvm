    .text
    .global _start
_start:
    movz x20, #0x0000
    movk x20, #0x0900, lsl #16
    movz x25, #0x0030, lsl #16
    msr  cpacr_el1, x25
    isb

    adr  x21, scratch

    adr  x0, idx4h_a
    ldr  d1, [x0]
    adr  x0, idx_rhs_h
    ldr  q2, [x0]
    smull v0.4s, v1.4h, v2.h[0]
    str  q0, [x21]
    mov  x0, x21
    adr  x1, exp_smull_idx_4s
    bl   check16

    adr  x0, idx4s_a
    ldr  q4, [x0]
    adr  x0, idx_rhs_s
    ldr  q5, [x0]
    smull2 v3.2d, v4.4s, v5.s[1]
    str  q3, [x21, #16]
    add  x0, x21, #16
    adr  x1, exp_smull2_idx_2d
    bl   check16

    adr  x0, idx_acc4s_u
    ldr  q6, [x0]
    adr  x0, idx4h_u_a
    ldr  d7, [x0]
    adr  x0, idx_rhs_h
    ldr  q8, [x0]
    umlal v6.4s, v7.4h, v8.h[4]
    str  q6, [x21, #32]
    add  x0, x21, #32
    adr  x1, exp_umlal_idx_4s
    bl   check16

    adr  x0, idx4h_u_a
    ldr  d10, [x0]
    adr  x0, idx_rhs_h
    ldr  q11, [x0]
    umull v9.4s, v10.4h, v11.h[4]
    str  q9, [x21, #48]
    add  x0, x21, #48
    adr  x1, exp_umull_idx_4s
    bl   check16

    adr  x0, idx_acc2d_u
    ldr  q12, [x0]
    adr  x0, idx4s_u_a
    ldr  q13, [x0]
    adr  x0, idx_rhs_s_u
    ldr  q14, [x0]
    umlsl2 v12.2d, v13.4s, v14.s[1]
    str  q12, [x21, #64]
    add  x0, x21, #64
    adr  x1, exp_umlsl2_idx_2d
    bl   check16

    adr  x0, idx_acc4s_s
    ldr  q0, [x0]
    adr  x0, idx4h_s_a
    ldr  d1, [x0]
    adr  x0, idx_rhs_h_hi
    ldr  q2, [x0]
    smlal v0.4s, v1.4h, v2.h[7]
    str  q0, [x21, #80]
    add  x0, x21, #80
    adr  x1, exp_smlal_idx_4s
    bl   check16

    adr  x0, idx_acc2d_s
    ldr  q3, [x0]
    adr  x0, idx4s_s_a
    ldr  q4, [x0]
    adr  x0, idx_rhs_s_neg
    ldr  q5, [x0]
    smlsl2 v3.2d, v4.4s, v5.s[1]
    str  q3, [x21, #96]
    add  x0, x21, #96
    adr  x1, exp_smlsl2_idx_2d
    bl   check16

    movz x0, #'L'
    str  x0, [x20]
    movz x0, #'X'
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
    .balign 8
idx4h_a:
    .hword 0x1000, 0x8000, 0x7fff, 0xffff
idx_rhs_h:
    .hword 0xfffd, 0, 0, 0, 0x0010, 0, 0, 0

    .balign 16
exp_smull_idx_4s:
    .word 0xffffd000, 0x00018000, 0xfffe8003, 0x00000003

    .balign 16
idx4s_a:
    .word 0x00000000, 0x00000000, 0x40000000, 0xfffffffe
idx_rhs_s:
    .word 0, 0xfffffffd, 0, 0
exp_smull2_idx_2d:
    .xword 0xffffffff40000000, 0x0000000000000006

    .balign 16
idx_acc4s_u:
    .word 0x00000001, 0x00000002, 0x00000003, 0x00000004
idx4h_u_a:
    .hword 0xffff, 0x1234, 0x8000, 0x0001
exp_umlal_idx_4s:
    .word 0x000ffff1, 0x00012342, 0x00080003, 0x00000014
exp_umull_idx_4s:
    .word 0x000ffff0, 0x00012340, 0x00080000, 0x00000010

    .balign 16
idx_acc2d_u:
    .xword 0x0000000000000003, 0x0000000000000000
idx4s_u_a:
    .word 0x00000000, 0x00000000, 0xffffffff, 0x80000000
idx_rhs_s_u:
    .word 0, 0x00000002, 0, 0
exp_umlsl2_idx_2d:
    .xword 0xfffffffe00000005, 0xffffffff00000000

    .balign 16
idx_acc4s_s:
    .word 0x7fffffff, 0x00000000, 0x00000001, 0x00000002
idx4h_s_a:
    .hword 0x0001, 0xffff, 0x4000, 0x8000
idx_rhs_h_hi:
    .hword 0, 0, 0, 0, 0, 0, 0, 0x7fff
exp_smlal_idx_4s:
    .word 0x80007ffe, 0xffff8001, 0x1fffc001, 0xc0008002

    .balign 16
idx_acc2d_s:
    .xword 0x0000000000000000, 0x0000000000000005
idx4s_s_a:
    .word 0x00000000, 0x00000000, 0x7fffffff, 0x80000000
idx_rhs_s_neg:
    .word 0, 0xfffffffe, 0, 0
exp_smlsl2_idx_2d:
    .xword 0x00000000fffffffe, 0xffffffff00000005

scratch:
    .space 112
