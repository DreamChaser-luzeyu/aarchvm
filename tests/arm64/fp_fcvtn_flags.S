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
    adr  x22, src_fcvtn_overflow
    ldr  q1, [x22]
    fcvtn v2.2s, v1.2d
    str  s2, [x21, #0]
    add  x0, x21, #0
    adr  x1, exp_inf_s
    bl   check4
    mrs  x2, fpsr
    mov  x3, #0x14
    cmp  x2, x3
    b.ne fail

    movz x23, #'2'
    msr  fpsr, xzr
    adr  x22, src_fcvtn_underflow
    ldr  q1, [x22]
    fcvtn v2.2s, v1.2d
    str  s2, [x21, #4]
    add  x0, x21, #4
    adr  x1, exp_min_norm_s
    bl   check4
    mrs  x2, fpsr
    mov  x3, #0x18
    cmp  x2, x3
    b.ne fail

    movz x23, #'3'
    msr  fpsr, xzr
    adr  x22, src_fcvtn_exact_min_sub
    ldr  q1, [x22]
    fcvtn v2.2s, v1.2d
    str  s2, [x21, #8]
    add  x0, x21, #8
    adr  x1, exp_min_sub_s
    bl   check4
    mrs  x2, fpsr
    cbnz x2, fail

    movz x23, #'4'
    msr  fpsr, xzr
    adr  x22, src_fcvtxn_overflow
    ldr  d0, [x22]
    fcvtxn s1, d0
    str  s1, [x21, #12]
    add  x0, x21, #12
    adr  x1, exp_max_finite_s
    bl   check4
    mrs  x2, fpsr
    mov  x3, #0x14
    cmp  x2, x3
    b.ne fail

    movz x23, #'5'
    msr  fpsr, xzr
    adr  x22, src_fcvtxn_underflow
    ldr  d0, [x22]
    fcvtxn s1, d0
    str  s1, [x21, #16]
    add  x0, x21, #16
    adr  x1, exp_min_sub_s
    bl   check4
    mrs  x2, fpsr
    mov  x3, #0x18
    cmp  x2, x3
    b.ne fail

    movz x23, #'6'
    msr  fpsr, xzr
    adr  x22, src_fcvtxn_exact
    ldr  d0, [x22]
    fcvtxn s1, d0
    str  s1, [x21, #20]
    add  x0, x21, #20
    adr  x1, exp_roundodd_exact
    bl   check4
    mrs  x2, fpsr
    cbnz x2, fail

    movz x0, #'N'
    str  x0, [x20]
    brk  #0

check4:
    ldr  w2, [x0]
    ldr  w3, [x1]
    cmp  w2, w3
    b.ne fail
    ret

fail:
    str  x23, [x20]
    brk  #0

    .data
    .balign 16
dst:
    .space 24

src_fcvtn_overflow:
    .quad 0x47f0000000000000      // +2^128 -> +inf in FCVTN
    .quad 0x0000000000000000

src_fcvtn_underflow:
    .quad 0x380fffffffffffff      // nextafter(0x1p-126, 0.0)
    .quad 0x0000000000000000

src_fcvtn_exact_min_sub:
    .quad 0x36a0000000000000      // +0x1p-149, exact smallest float subnormal
    .quad 0x0000000000000000

src_fcvtxn_overflow:
    .quad 0x47f0000000000000      // +2^128 -> max finite in FCVTXN

src_fcvtxn_underflow:
    .quad 0x0000000000000001      // smallest double subnormal, rounds-to-odd to float min subnormal

src_fcvtxn_exact:
    .quad 0x3800000000000000      // exact finite value that stays representable after round-to-odd narrowing

exp_inf_s:
    .word 0x7f800000

exp_min_norm_s:
    .word 0x00800000

exp_min_sub_s:
    .word 0x00000001

exp_max_finite_s:
    .word 0x7f7fffff

exp_roundodd_exact:
    .word 0x00400000
