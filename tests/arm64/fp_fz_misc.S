    .text
    .global _start
_start:
    movz x20, #0
    movk x20, #0x0900, lsl #16
    adr  x21, dst
    movz x25, #0x0030, lsl #16
    msr  cpacr_el1, x25
    isb

    mov  x24, #(1 << 24)         // FPCR.FZ
    msr  fpcr, x24
    isb

    movz x23, #'1'
    msr  fpsr, xzr
    ldr  w0, =0x00000001
    fmov s0, w0
    fsqrt s1, s0
    str  s1, [x21, #0]
    add  x0, x21, #0
    adr  x1, exp_zero_s
    bl   check4
    mrs  x2, fpsr
    cmp  x2, #0x80
    b.ne fail

    movz x23, #'2'
    msr  fpsr, xzr
    ldr  x0, =0x0000000000000001
    fmov d0, x0
    fsqrt d1, d0
    str  d1, [x21, #8]
    add  x0, x21, #8
    adr  x1, exp_zero_d
    bl   check8
    mrs  x2, fpsr
    cmp  x2, #0x80
    b.ne fail

    movz x23, #'3'
    msr  fpsr, xzr
    ldr  w0, =0x00000001
    fmov s0, w0
    frintn s1, s0
    str  s1, [x21, #16]
    add  x0, x21, #16
    adr  x1, exp_zero_s
    bl   check4
    mrs  x2, fpsr
    cmp  x2, #0x80
    b.ne fail

    movz x23, #'4'
    msr  fpsr, xzr
    ldr  x0, =0x0000000000000001
    fmov d0, x0
    frintn d1, d0
    str  d1, [x21, #24]
    add  x0, x21, #24
    adr  x1, exp_zero_d
    bl   check8
    mrs  x2, fpsr
    cmp  x2, #0x80
    b.ne fail

    movz x23, #'5'
    msr  fpsr, xzr
    ldr  w0, =0x00000001
    fmov s0, w0
    frecpx s1, s0
    str  s1, [x21, #32]
    add  x0, x21, #32
    adr  x1, exp_frecpx_s
    bl   check4
    mrs  x2, fpsr
    cmp  x2, #0x80
    b.ne fail

    movz x23, #'6'
    msr  fpsr, xzr
    ldr  x0, =0x0000000000000001
    fmov d0, x0
    frecpx d1, d0
    str  d1, [x21, #40]
    add  x0, x21, #40
    adr  x1, exp_frecpx_d
    bl   check8
    mrs  x2, fpsr
    cmp  x2, #0x80
    b.ne fail

    movz x23, #'7'
    msr  fpsr, xzr
    ldr  w0, =0x00000001
    fmov s0, w0
    frecpe s1, s0
    str  s1, [x21, #48]
    add  x0, x21, #48
    adr  x1, exp_inf_s
    bl   check4
    mrs  x2, fpsr
    cmp  x2, #0x82
    b.ne fail

    movz x23, #'8'
    msr  fpsr, xzr
    ldr  x0, =0x0000000000000001
    fmov d0, x0
    frecpe d1, d0
    str  d1, [x21, #56]
    add  x0, x21, #56
    adr  x1, exp_inf_d
    bl   check8
    mrs  x2, fpsr
    cmp  x2, #0x82
    b.ne fail

    movz x23, #'9'
    msr  fpsr, xzr
    ldr  w0, =0x00000001
    fmov s0, w0
    frsqrte s1, s0
    str  s1, [x21, #64]
    add  x0, x21, #64
    adr  x1, exp_inf_s
    bl   check4
    mrs  x2, fpsr
    cmp  x2, #0x82
    b.ne fail

    movz x23, #'A'
    msr  fpsr, xzr
    ldr  x0, =0x0000000000000001
    fmov d0, x0
    frsqrte d1, d0
    str  d1, [x21, #72]
    add  x0, x21, #72
    adr  x1, exp_inf_d
    bl   check8
    mrs  x2, fpsr
    cmp  x2, #0x82
    b.ne fail

    msr  fpcr, xzr
    isb

    movz x23, #'B'
    msr  fpsr, xzr
    fmov s0, wzr
    frecpe s1, s0
    str  s1, [x21, #80]
    add  x0, x21, #80
    adr  x1, exp_inf_s
    bl   check4
    mrs  x2, fpsr
    cmp  x2, #0x2
    b.ne fail

    movz x23, #'C'
    msr  fpsr, xzr
    fmov s0, wzr
    frsqrte s1, s0
    str  s1, [x21, #84]
    add  x0, x21, #84
    adr  x1, exp_inf_s
    bl   check4
    mrs  x2, fpsr
    cmp  x2, #0x2
    b.ne fail

    mov  x24, #(1 << 24)
    msr  fpcr, x24
    isb

    movz x23, #'D'
    msr  fpsr, xzr
    adr  x22, vec_sqrt_src
    ldr  q0, [x22]
    fsqrt v1.4s, v0.4s
    str  q1, [x21, #96]
    add  x0, x21, #96
    adr  x1, exp_vec_sqrt
    bl   check16
    mrs  x2, fpsr
    cmp  x2, #0x80
    b.ne fail

    movz x23, #'E'
    msr  fpsr, xzr
    adr  x22, vec_frintn_src
    ldr  q0, [x22]
    frintn v1.4s, v0.4s
    str  q1, [x21, #112]
    add  x0, x21, #112
    adr  x1, exp_vec_frintn
    bl   check16
    mrs  x2, fpsr
    cmp  x2, #0x80
    b.ne fail

    movz x23, #'F'
    msr  fpsr, xzr
    adr  x22, vec_frecpe_src
    ldr  q0, [x22]
    frecpe v1.4s, v0.4s
    str  q1, [x21, #128]
    add  x0, x21, #128
    adr  x1, exp_vec_frecpe
    bl   check16
    mrs  x2, fpsr
    cmp  x2, #0x82
    b.ne fail

    movz x23, #'G'
    msr  fpsr, xzr
    adr  x22, vec_frsqrte_src
    ldr  q0, [x22]
    frsqrte v1.4s, v0.4s
    str  q1, [x21, #144]
    add  x0, x21, #144
    adr  x1, exp_vec_frsqrte
    bl   check16
    mrs  x2, fpsr
    cmp  x2, #0x82
    b.ne fail

    movz x0, #'Z'
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

fail:
    str  x23, [x20]
    brk  #0

    .data
    .balign 16
dst:
    .space 160

exp_zero_s:
    .word 0x00000000
exp_frecpx_s:
    .word 0x7f000000
exp_inf_s:
    .word 0x7f800000

    .balign 8
exp_zero_d:
    .quad 0x0000000000000000
exp_frecpx_d:
    .quad 0x7fe0000000000000
exp_inf_d:
    .quad 0x7ff0000000000000

    .balign 16
vec_sqrt_src:
    .word 0x00000001, 0x3f800000, 0x40800000, 0x41100000
exp_vec_sqrt:
    .word 0x00000000, 0x3f800000, 0x40000000, 0x40400000

vec_frintn_src:
    .word 0x00000001, 0x3f800000, 0x40000000, 0xbf800000
exp_vec_frintn:
    .word 0x00000000, 0x3f800000, 0x40000000, 0xbf800000

vec_frecpe_src:
    .word 0x00000001, 0x3f800000, 0x40000000, 0x7f800000
exp_vec_frecpe:
    .word 0x7f800000, 0x3f7f8000, 0x3eff8000, 0x00000000

vec_frsqrte_src:
    .word 0x00000001, 0x3f800000, 0x40800000, 0x7f800000
exp_vec_frsqrte:
    .word 0x7f800000, 0x3f7f8000, 0x3eff8000, 0x00000000
