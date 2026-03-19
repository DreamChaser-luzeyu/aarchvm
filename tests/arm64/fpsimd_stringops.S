    .text
    .global _start

    .macro cmp16 base, exp, fail
    ldr  x26, [\base]
    ldr  x27, [\exp]
    cmp  x26, x27
    b.ne \fail
    ldr  x26, [\base, #8]
    ldr  x27, [\exp, #8]
    cmp  x26, x27
    b.ne \fail
    .endm

    .macro cmp32 base, exp, fail
    cmp16 \base, \exp, \fail
    add  x24, \base, #16
    add  x25, \exp, #16
    cmp16 x24, x25, \fail
    .endm

_start:
    movz x20, #0x0000
    movk x20, #0x0900, lsl #16
    movz x25, #0x0030, lsl #16
    msr  cpacr_el1, x25
    isb

    adr  x21, src
    adr  x22, dst

    ldr  q0, [x21]
    str  q0, [x22]
    adr  x23, src
    cmp16 x22, x23, fail1

    sub  x3, x21, #16
    ldr  q1, [x3, #16]!
    cmp  x3, x21
    b.ne fail2
    str  q1, [x22, #16]
    adr  x23, src
    add  x24, x22, #16
    cmp16 x24, x23, fail2

    mov  x3, x21
    ldr  q2, [x3], #32
    add  x4, x21, #32
    cmp  x3, x4
    b.ne fail3
    str  q2, [x22, #32]
    adr  x23, src
    add  x24, x22, #32
    cmp16 x24, x23, fail3

    mov  x4, #16
    ldr  q3, [x21, x4]
    str  q3, [x22, #48]
    add  x23, x21, #16
    add  x24, x22, #48
    cmp16 x24, x23, fail4

    add  x5, x21, #32
    ldur q4, [x5, #-16]
    str  q4, [x22, #64]
    add  x23, x21, #16
    add  x24, x22, #64
    cmp16 x24, x23, fail5

    ld1  {v5.16b}, [x21]
    str  q5, [x22, #80]
    adr  x23, src
    add  x24, x22, #80
    cmp16 x24, x23, fail6

    mov  x3, x21
    ld1  {v6.16b}, [x3], #16
    add  x4, x21, #16
    cmp  x3, x4
    b.ne fail7
    str  q6, [x22, #96]
    adr  x23, src
    add  x24, x22, #96
    cmp16 x24, x23, fail7

    ld1  {v7.16b-v8.16b}, [x21]
    stp  q7, q8, [x22, #112]
    adr  x23, src
    add  x24, x22, #112
    cmp32 x24, x23, fail8

    movi v9.16b, #0x33
    str  q9, [x22, #144]
    adr  x23, exp33
    add  x24, x22, #144
    cmp16 x24, x23, fail9

    mov  w10, #0x11
    dup  v10.16b, w10
    and  v11.16b, v9.16b, v10.16b
    str  q11, [x22, #160]
    adr  x23, exp11
    add  x24, x22, #160
    cmp16 x24, x23, failA

    movz w12, #0x3344
    movk w12, #0x1122, lsl #16
    dup  v12.4s, w12
    add  x3, x22, #176
    str  s12, [x3]
    mov  x4, #1
    str  s12, [x3, x4, lsl #2]
    add  x5, x3, #12
    stur s12, [x5, #-4]
    ldr  s13, [x3]
    fmov w14, s13
    cmp  w14, w12
    b.ne failB
    ldr  s13, [x3, x4, lsl #2]
    fmov w14, s13
    cmp  w14, w12
    b.ne failB
    ldur s13, [x5, #-4]
    fmov w14, s13
    cmp  w14, w12
    b.ne failB

    adr  x3, zero_bytes
    ld1  {v13.16b}, [x3]
    cmeq v14.16b, v13.16b, #0
    str  q14, [x22, #192]
    adr  x23, exp_cmeq_zero
    add  x24, x22, #192
    cmp16 x24, x23, failC

    shrn v15.8b, v14.8h, #4
    fmov x16, d15
    adr  x23, exp_shrn
    ldr  x24, [x23]
    cmp  x16, x24
    b.ne failD

    mov  w17, #0x33
    dup  v16.16b, w17
    cmeq v17.16b, v9.16b, v16.16b
    str  q17, [x22, #208]
    adr  x23, exp_ff
    add  x24, x22, #208
    cmp16 x24, x23, failE

    movi v22.16b, #0x33
    movi v22.4s, #0
    str  q22, [x22, #352]
    adr  x23, exp_zeroq
    add  x24, x22, #352
    cmp16 x24, x23, failL

    movi v18.16b, #1
    movi v19.16b, #2
    addp v20.16b, v18.16b, v19.16b
    addp v20.16b, v20.16b, v20.16b
    str  q20, [x22, #224]
    adr  x23, exp_addp
    add  x24, x22, #224
    cmp16 x24, x23, failF

    movz x18, #0x7788
    movk x18, #0x5566, lsl #16
    movk x18, #0x3344, lsl #32
    movk x18, #0x1122, lsl #48
    fmov d21, x18
    str  q21, [x22, #240]
    adr  x23, exp_fmovd
    add  x24, x22, #240
    cmp16 x24, x23, failG

    add  x3, x22, #272
    stur q9, [x3, #-16]
    adr  x23, exp33
    add  x24, x22, #256
    cmp16 x24, x23, failH

    add  x3, x22, #272
    str  q10, [x3], #32
    add  x4, x22, #304
    cmp  x3, x4
    b.ne failI
    adr  x23, exp11
    add  x24, x22, #272
    cmp16 x24, x23, failI

    add  x3, x22, #288
    str  q11, [x3, #16]!
    add  x4, x22, #304
    cmp  x3, x4
    b.ne failJ
    adr  x23, exp11
    add  x24, x22, #304
    cmp16 x24, x23, failJ

    add  x3, x22, #320
    mov  x4, #16
    str  q9, [x3, x4]
    adr  x23, exp33
    add  x24, x22, #336
    cmp16 x24, x23, failK

    movz x0, #'W'
    str  x0, [x20]
    movz x0, #'\n'
    str  x0, [x20]
    brk  #0

fail1:  movz x0, #'1'; str x0, [x20]; brk #0
fail2:  movz x0, #'2'; str x0, [x20]; brk #0
fail3:  movz x0, #'3'; str x0, [x20]; brk #0
fail4:  movz x0, #'4'; str x0, [x20]; brk #0
fail5:  movz x0, #'5'; str x0, [x20]; brk #0
fail6:  movz x0, #'6'; str x0, [x20]; brk #0
fail7:  movz x0, #'7'; str x0, [x20]; brk #0
fail8:  movz x0, #'8'; str x0, [x20]; brk #0
fail9:  movz x0, #'9'; str x0, [x20]; brk #0
failA:  movz x0, #'A'; str x0, [x20]; brk #0
failB:  movz x0, #'B'; str x0, [x20]; brk #0
failC:  movz x0, #'C'; str x0, [x20]; brk #0
failD:  movz x0, #'D'; str x0, [x20]; brk #0
failE:  movz x0, #'E'; str x0, [x20]; brk #0
failF:  movz x0, #'F'; str x0, [x20]; brk #0
failG:  movz x0, #'G'; str x0, [x20]; brk #0
failH:  movz x0, #'H'; str x0, [x20]; brk #0
failI:  movz x0, #'I'; str x0, [x20]; brk #0
failJ:  movz x0, #'J'; str x0, [x20]; brk #0
failK:  movz x0, #'K'; str x0, [x20]; brk #0
failL:  movz x0, #'L'; str x0, [x20]; brk #0

    .data
    .balign 16
src:
    .quad 0x1716151413121110
    .quad 0x1f1e1d1c1b1a1918
    .quad 0x2726252423222120
    .quad 0x2f2e2d2c2b2a2928
exp33:
    .quad 0x3333333333333333
    .quad 0x3333333333333333
exp11:
    .quad 0x1111111111111111
    .quad 0x1111111111111111
zero_bytes:
    .byte 0,1,0,2,0,3,0,4,0,5,0,6,0,7,0,8
exp_cmeq_zero:
    .quad 0x00ff00ff00ff00ff
    .quad 0x00ff00ff00ff00ff
exp_shrn:
    .quad 0x0f0f0f0f0f0f0f0f
exp_ff:
    .quad 0xffffffffffffffff
    .quad 0xffffffffffffffff
exp_zeroq:
    .quad 0x0000000000000000
    .quad 0x0000000000000000
exp_addp:
    .quad 0x0808080804040404
    .quad 0x0808080804040404
exp_fmovd:
    .quad 0x1122334455667788
    .quad 0x0000000000000000
    .balign 16
dst:
    .space 384
