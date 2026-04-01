    .text
    .global _start
_start:
    movz x20, #0
    movk x20, #0x0900, lsl #16
    adr  x21, dst
    movz x25, #0x0030, lsl #16
    msr  cpacr_el1, x25
    isb

    movz x23, #'1'
    msr  fpsr, xzr
    adr  x22, src_smallest_sub_s
    ldr  s1, [x22]
    frecpe s0, s1
    str  s0, [x21, #0]
    add  x0, x21, #0
    adr  x1, exp_pos_inf_s
    bl   check4
    mrs  x2, fpsr
    mov  x3, #0x14
    cmp  x2, x3
    b.ne fail

    movz x23, #'2'
    msr  fpsr, xzr
    adr  x22, src_smallest_sub_d
    ldr  d1, [x22]
    frecpe d0, d1
    str  d0, [x21, #8]
    add  x0, x21, #8
    adr  x1, exp_pos_inf_d
    bl   check8
    mrs  x2, fpsr
    mov  x3, #0x14
    cmp  x2, x3
    b.ne fail

    movz x23, #'3'
    msr  fpsr, xzr
    adr  x22, src_smallest_sub_neg_s
    ldr  s1, [x22]
    frecpe s0, s1
    str  s0, [x21, #16]
    add  x0, x21, #16
    adr  x1, exp_neg_inf_s
    bl   check4
    mrs  x2, fpsr
    mov  x3, #0x14
    cmp  x2, x3
    b.ne fail

    movz x0, #'Y'
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

fail:
    str  x23, [x20]
    brk  #0

    .data
    .balign 16
dst:
    .space 24

src_smallest_sub_s:
    .word 0x00000001

src_smallest_sub_d:
    .quad 0x0000000000000001

src_smallest_sub_neg_s:
    .word 0x80000001

exp_pos_inf_s:
    .word 0x7f800000

exp_pos_inf_d:
    .quad 0x7ff0000000000000

exp_neg_inf_s:
    .word 0xff800000
