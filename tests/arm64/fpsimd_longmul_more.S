    .text
    .global _start
_start:
    movz x20, #0x0000
    movk x20, #0x0900, lsl #16
    movz x25, #0x0030, lsl #16
    msr  cpacr_el1, x25
    isb

    adr  x21, scratch

    adr  x0, vec8b_a
    ldr  d1, [x0]
    adr  x0, vec8b_b
    ldr  d2, [x0]
    smull v0.8h, v1.8b, v2.8b
    str  q0, [x21]
    mov  x0, x21
    adr  x1, exp_smull_8h
    bl   check16

    adr  x0, vec8h_a
    ldr  q4, [x0]
    adr  x0, vec8h_b
    ldr  q5, [x0]
    umull2 v3.4s, v4.8h, v5.8h
    str  q3, [x21, #16]
    add  x0, x21, #16
    adr  x1, exp_umull2_4s
    bl   check16

    adr  x0, acc8h
    ldr  q6, [x0]
    adr  x0, vec8b_acc_a
    ldr  d7, [x0]
    adr  x0, vec8b_acc_b
    ldr  d8, [x0]
    umlal v6.8h, v7.8b, v8.8b
    str  q6, [x21, #32]
    add  x0, x21, #32
    adr  x1, exp_umlal_8h
    bl   check16

    adr  x0, acc4s
    ldr  q9, [x0]
    adr  x0, vec8h_sub_a
    ldr  q10, [x0]
    adr  x0, vec8h_sub_b
    ldr  q11, [x0]
    umlsl2 v9.4s, v10.8h, v11.8h
    str  q9, [x21, #48]
    add  x0, x21, #48
    adr  x1, exp_umlsl2_4s
    bl   check16

    adr  x0, acc2d_add
    ldr  q12, [x0]
    adr  x0, vec2s_add_a
    ldr  d13, [x0]
    adr  x0, vec2s_add_b
    ldr  d14, [x0]
    smlal v12.2d, v13.2s, v14.2s
    str  q12, [x21, #64]
    add  x0, x21, #64
    adr  x1, exp_smlal_2d
    bl   check16

    adr  x0, acc2d_sub
    ldr  q15, [x0]
    adr  x0, vec4s_sub_a
    ldr  q16, [x0]
    adr  x0, vec4s_sub_b
    ldr  q17, [x0]
    smlsl2 v15.2d, v16.4s, v17.4s
    str  q15, [x21, #80]
    add  x0, x21, #80
    adr  x1, exp_smlsl2_2d
    bl   check16

    movz x0, #'L'
    str  x0, [x20]
    movz x0, #'M'
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
vec8b_a:
    .byte 0x7f, 0x80, 0x02, 0xfe, 0x10, 0xf0, 0x55, 0xaa
vec8b_b:
    .byte 0x02, 0xff, 0x80, 0x7f, 0xf0, 0x10, 0x03, 0x03

    .balign 16
exp_smull_8h:
    .hword 0x00fe, 0x0080, 0xff00, 0xff02, 0xff00, 0xff00, 0x00ff, 0xfefe

    .balign 16
vec8h_a:
    .hword 0x0001, 0x0002, 0x0003, 0x0004, 0xff00, 0x8000, 0x1234, 0xffff
vec8h_b:
    .hword 0x0005, 0x0006, 0x0007, 0x0008, 0x0002, 0x0002, 0x0010, 0x0002
exp_umull2_4s:
    .word 0x0001fe00, 0x00010000, 0x00012340, 0x0001fffe

    .balign 16
acc8h:
    .hword 0x0001, 0x0002, 0x0003, 0x0004, 0x0005, 0x0006, 0x0007, 0x0008
vec8b_acc_a:
    .byte 10, 20, 30, 40, 250, 1, 2, 3
vec8b_acc_b:
    .byte 2, 3, 4, 5, 1, 255, 128, 64
exp_umlal_8h:
    .hword 0x0015, 0x003e, 0x007b, 0x00cc, 0x00ff, 0x0105, 0x0107, 0x00c8

    .balign 16
acc4s:
    .word 0x00000000, 0x00000001, 0x00000002, 0x00000003
vec8h_sub_a:
    .hword 0x0000, 0x0000, 0x0000, 0x0000, 0x1000, 0x8000, 0xffff, 0x1234
vec8h_sub_b:
    .hword 0x0000, 0x0000, 0x0000, 0x0000, 0x0010, 0x0002, 0x0002, 0x1000
exp_umlsl2_4s:
    .word 0xffff0000, 0xffff0001, 0xfffe0004, 0xfedcc003

    .balign 16
acc2d_add:
    .xword 0x0000000000000005, 0xfffffffffffffff9
vec2s_add_a:
    .word 0x7fffffff, 0x80000000
vec2s_add_b:
    .word 0x00000002, 0xffffffff
exp_smlal_2d:
    .xword 0x0000000100000003, 0x000000007ffffff9

    .balign 16
acc2d_sub:
    .xword 0x0000000000000000, 0x7fffffffffffffff
vec4s_sub_a:
    .word 0x00000000, 0x00000000, 0x40000000, 0x80000000
vec4s_sub_b:
    .word 0x00000000, 0x00000000, 0x40000000, 0xffffffff
exp_smlsl2_2d:
    .xword 0xf000000000000000, 0x7fffffff7fffffff

scratch:
    .space 96
