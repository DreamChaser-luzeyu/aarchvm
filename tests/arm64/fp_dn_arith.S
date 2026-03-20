    .text
    .global _start
_start:
    movz x20, #0
    movk x20, #0x0900, lsl #16
    movz x25, #0x0030, lsl #16
    msr  cpacr_el1, x25
    isb

    adr  x21, dst
    mov  x24, #(1 << 25)          // FPCR.DN
    msr  fpcr, x24
    isb

    movz x24, #'1'
    msr  fpsr, xzr
    adr  x22, qnan_s
    ldr  s0, [x22]
    adr  x22, one_s
    ldr  s1, [x22]
    fadd s2, s0, s1
    str  s2, [x21, #0]
    add  x0, x21, #0
    adr  x1, exp_defnan_s
    bl   check4
    mrs  x2, fpsr
    cbnz x2, fail1

    movz x24, #'2'
    msr  fpsr, xzr
    adr  x22, snan_s
    ldr  s3, [x22]
    adr  x22, one_s
    ldr  s4, [x22]
    fmov s5, s4
    fmadd s6, s3, s4, s5
    str  s6, [x21, #4]
    add  x0, x21, #4
    adr  x1, exp_defnan_s
    bl   check4
    mrs  x2, fpsr
    cmp  x2, #1
    b.ne fail2

    movz x24, #'3'
    msr  fpsr, xzr
    adr  x22, qnan_d
    ldr  d7, [x22]
    adr  x22, one_d
    ldr  d8, [x22]
    fdiv d9, d7, d8
    str  d9, [x21, #8]
    add  x0, x21, #8
    adr  x1, exp_defnan_d
    bl   check8
    mrs  x2, fpsr
    cbnz x2, fail3

    movz x24, #'4'
    msr  fpsr, xzr
    adr  x22, vec_add_a
    ldr  q10, [x22]
    adr  x22, vec_add_b
    ldr  q11, [x22]
    fadd v12.4s, v10.4s, v11.4s
    str  q12, [x21, #16]
    add  x0, x21, #16
    adr  x1, exp_vec_add
    bl   check16
    mrs  x2, fpsr
    cmp  x2, #1
    b.ne fail4

    movz x24, #'5'
    msr  fpsr, xzr
    adr  x22, vec_addp_a
    ldr  q13, [x22]
    adr  x22, vec_addp_b
    ldr  q14, [x22]
    faddp v15.4s, v13.4s, v14.4s
    str  q15, [x21, #32]
    add  x0, x21, #32
    adr  x1, exp_vec_addp
    bl   check16
    mrs  x2, fpsr
    cbnz x2, fail5

    movz x0, #'T'
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

fail1:
    movz x0, #'1'
    b out
fail2:
    movz x0, #'2'
    b out
fail3:
    movz x0, #'3'
    b out
fail4:
    movz x0, #'4'
    b out
fail5:
    movz x0, #'5'
    b out
fail:
    mov  x0, x24
out:
    str  x0, [x20]
    brk  #0

    .data
    .balign 16
qnan_s:
    .word 0x7fc12345
snan_s:
    .word 0x7f812345
one_s:
    .word 0x3f800000
exp_defnan_s:
    .word 0x7fc00000

    .balign 8
qnan_d:
    .quad 0x7ff8123456789abc
one_d:
    .quad 0x3ff0000000000000
exp_defnan_d:
    .quad 0x7ff8000000000000

    .balign 16
vec_add_a:
    .word 0x7fc12345, 0x3f800000, 0x40000000, 0x7f812345
vec_add_b:
    .word 0x3f800000, 0x40000000, 0x40400000, 0x40800000
exp_vec_add:
    .word 0x7fc00000, 0x40400000, 0x40a00000, 0x7fc00000

vec_addp_a:
    .word 0x7fc12345, 0x3f800000, 0x40000000, 0x40400000
vec_addp_b:
    .word 0x40800000, 0x40a00000, 0x7fc12345, 0x40e00000
exp_vec_addp:
    .word 0x7fc00000, 0x40a00000, 0x41100000, 0x7fc00000

dst:
    .space 48
