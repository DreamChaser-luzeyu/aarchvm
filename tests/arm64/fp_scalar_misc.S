    .text
    .global _start
_start:
    movz x20, #0
    movk x20, #0x0900, lsl #16
    movz x25, #0x0030, lsl #16
    msr  cpacr_el1, x25
    isb

    adr  x21, dst

    adr  x22, sqrt_s_src
    ldr  s1, [x22]
    fsqrt s0, s1
    str  s0, [x21]
    mov  x0, x21
    adr  x1, exp_sqrt_s
    bl   check4

    adr  x22, sqrt_d_src
    ldr  d3, [x22]
    fsqrt d2, d3
    str  d2, [x21, #8]
    add  x0, x21, #8
    adr  x1, exp_sqrt_d
    bl   check8

    adr  x22, frintn_s_src
    ldr  s5, [x22]
    frintn s4, s5
    str  s4, [x21, #16]
    add  x0, x21, #16
    adr  x1, exp_frintn_s
    bl   check4

    adr  x22, frintp_d_src
    ldr  d7, [x22]
    frintp d6, d7
    str  d6, [x21, #24]
    add  x0, x21, #24
    adr  x1, exp_frintp_d
    bl   check8

    adr  x22, frintm_s_src
    ldr  s9, [x22]
    frintm s8, s9
    str  s8, [x21, #32]
    add  x0, x21, #32
    adr  x1, exp_frintm_s
    bl   check4

    adr  x22, frintz_d_src
    ldr  d11, [x22]
    frintz d10, d11
    str  d10, [x21, #40]
    add  x0, x21, #40
    adr  x1, exp_frintz_d
    bl   check8

    adr  x22, frinta_s_src
    ldr  s13, [x22]
    frinta s12, s13
    str  s12, [x21, #48]
    add  x0, x21, #48
    adr  x1, exp_frinta_s
    bl   check4

    mov  x0, #(3 << 22)
    msr  fpcr, x0
    adr  x22, frintx_s_src
    ldr  s15, [x22]
    frintx s14, s15
    str  s14, [x21, #56]
    add  x0, x21, #56
    adr  x1, exp_frintx_s
    bl   check4

    mov  x0, #(2 << 22)
    msr  fpcr, x0
    adr  x22, frinti_d_src
    ldr  d17, [x22]
    frinti d16, d17
    str  d16, [x21, #64]
    add  x0, x21, #64
    adr  x1, exp_frinti_d
    bl   check8

    movz x0, #'P'
    str  x0, [x20]
    movz x0, #'\n'
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
    movz x0, #'F'
    str  x0, [x20]
    brk  #0

    .data
    .balign 8
sqrt_s_src:
    .word 0x40100000
exp_sqrt_s:
    .word 0x3fc00000
sqrt_d_src:
    .quad 0x4030000000000000
exp_sqrt_d:
    .quad 0x4010000000000000
frintn_s_src:
    .word 0x40200000
exp_frintn_s:
    .word 0x40000000
frintp_d_src:
    .quad 0xbff3333333333333
exp_frintp_d:
    .quad 0xbff0000000000000
frintm_s_src:
    .word 0xbf99999a
exp_frintm_s:
    .word 0xc0000000
frintz_d_src:
    .quad 0xbffc000000000000
exp_frintz_d:
    .quad 0xbff0000000000000
frinta_s_src:
    .word 0x40200000
exp_frinta_s:
    .word 0x40400000
frintx_s_src:
    .word 0x3fe00000
exp_frintx_s:
    .word 0x3f800000
frinti_d_src:
    .quad 0xbff4000000000000
exp_frinti_d:
    .quad 0xc000000000000000

dst:
    .space 72
