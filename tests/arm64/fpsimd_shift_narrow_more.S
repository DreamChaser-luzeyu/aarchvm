    .text
    .global _start
_start:
    movz x20, #0x0000
    movk x20, #0x0900, lsl #16
    movz x25, #0x0030, lsl #16
    msr  cpacr_el1, x25
    isb

    movz x9, #0x0800
    lsl  x9, x9, #16               // FPSR.QC

    adr  x21, tmp

    msr  fpsr, xzr
    adr  x0, rshrn_src
    ldr  q0, [x0]
    rshrn v1.8b, v0.8h, #8
    str  d1, [x21]
    mov  x0, x21
    adr  x1, rshrn_exp
    bl   check8
    mrs  x2, fpsr
    tst  x2, x9
    b.ne fail

    adr  x0, rshrn2_init
    ldr  q2, [x0]
    adr  x0, rshrn_src
    ldr  q3, [x0]
    rshrn2 v2.16b, v3.8h, #8
    str  q2, [x21]
    mov  x0, x21
    adr  x1, rshrn2_exp
    bl   check16

    msr  fpsr, xzr
    adr  x0, sqshrn_src
    ldr  q4, [x0]
    sqshrn v5.8b, v4.8h, #1
    str  d5, [x21]
    mov  x0, x21
    adr  x1, sqshrn_exp
    bl   check8
    mrs  x2, fpsr
    tst  x2, x9
    b.eq fail

    msr  fpsr, xzr
    adr  x0, uqshrn_src
    ldr  q6, [x0]
    uqshrn v7.8b, v6.8h, #1
    str  d7, [x21]
    mov  x0, x21
    adr  x1, uqshrn_exp
    bl   check8
    mrs  x2, fpsr
    tst  x2, x9
    b.eq fail

    msr  fpsr, xzr
    adr  x0, sqrshrn_src
    ldr  q8, [x0]
    sqrshrn v9.8b, v8.8h, #8
    str  d9, [x21]
    mov  x0, x21
    adr  x1, sqrshrn_exp
    bl   check8
    mrs  x2, fpsr
    tst  x2, x9
    b.ne fail

    msr  fpsr, xzr
    adr  x0, uqrshrn_src
    ldr  q10, [x0]
    uqrshrn v11.8b, v10.8h, #8
    str  d11, [x21]
    mov  x0, x21
    adr  x1, uqrshrn_exp
    bl   check8
    mrs  x2, fpsr
    tst  x2, x9
    b.ne fail

    msr  fpsr, xzr
    adr  x0, sqshrun_src
    ldr  q12, [x0]
    sqshrun v13.8b, v12.8h, #1
    str  d13, [x21]
    mov  x0, x21
    adr  x1, sqshrun_exp
    bl   check8
    mrs  x2, fpsr
    tst  x2, x9
    b.eq fail

    msr  fpsr, xzr
    adr  x0, sqrshrun_src
    ldr  q14, [x0]
    sqrshrun v15.8b, v14.8h, #1
    str  d15, [x21]
    mov  x0, x21
    adr  x1, sqrshrun_exp
    bl   check8
    mrs  x2, fpsr
    tst  x2, x9
    b.eq fail

    msr  fpsr, xzr
    adr  x0, scalar_sqshrn_src
    ldr  d16, [x0]
    sqshrn b17, h16, #1
    umov w0, v17.b[0]
    cmp  w0, #0x7f
    b.ne fail
    mrs  x2, fpsr
    tst  x2, x9
    b.eq fail

    msr  fpsr, xzr
    adr  x0, scalar_uqshrn_src
    ldr  d18, [x0]
    uqshrn b19, h18, #1
    umov w0, v19.b[0]
    cmp  w0, #0xff
    b.ne fail
    mrs  x2, fpsr
    tst  x2, x9
    b.eq fail

    msr  fpsr, xzr
    adr  x0, scalar_sqrshrn_src
    ldr  d20, [x0]
    sqrshrn b21, h20, #8
    umov w0, v21.b[0]
    cmp  w0, #0x00
    b.ne fail
    mrs  x2, fpsr
    tst  x2, x9
    b.ne fail

    msr  fpsr, xzr
    adr  x0, scalar_uqrshrn_src
    ldr  d22, [x0]
    uqrshrn b23, h22, #8
    umov w0, v23.b[0]
    cmp  w0, #0x02
    b.ne fail
    mrs  x2, fpsr
    tst  x2, x9
    b.ne fail

    msr  fpsr, xzr
    adr  x0, scalar_sqshrun_src
    ldr  d24, [x0]
    sqshrun b25, h24, #1
    umov w0, v25.b[0]
    cmp  w0, #0xff
    b.ne fail
    mrs  x2, fpsr
    tst  x2, x9
    b.eq fail

    msr  fpsr, xzr
    adr  x0, scalar_sqrshrun_src
    ldr  d26, [x0]
    sqrshrun b27, h26, #1
    umov w0, v27.b[0]
    cmp  w0, #0xff
    b.ne fail
    mrs  x2, fpsr
    tst  x2, x9
    b.eq fail

    movz x0, #'N'
    str  x0, [x20]
    movz x0, #'\n'
    str  x0, [x20]
    brk  #0

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
    movz x0, #'F'
    str  x0, [x20]
    brk  #0

    .data
    .balign 16
rshrn_src:
    .hword 0x0001, 0x00ff, 0x0100, 0x017f, 0x0180, 0x0181, 0xfe7f, 0xfe80
rshrn_exp:
    .byte  0x00, 0x01, 0x01, 0x01, 0x02, 0x02, 0xfe, 0xff
rshrn2_init:
    .byte  0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7
    .byte  0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7
rshrn2_exp:
    .byte  0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7
    .byte  0x00, 0x01, 0x01, 0x01, 0x02, 0x02, 0xfe, 0xff

    .balign 16
sqshrn_src:
    .hword 0xfe00, 0xfeff, 0xff00, 0xff01, 0x00fe, 0x00ff, 0x0100, 0x01ff
sqshrn_exp:
    .byte  0x80, 0x80, 0x80, 0x80, 0x7f, 0x7f, 0x7f, 0x7f

    .balign 16
uqshrn_src:
    .hword 0x0000, 0x0001, 0x0002, 0x00fe, 0x00ff, 0x0100, 0x01fe, 0x03ff
uqshrn_exp:
    .byte  0x00, 0x00, 0x01, 0x7f, 0x7f, 0x80, 0xff, 0xff

    .balign 16
sqrshrn_src:
    .hword 0xfe7f, 0xfe80, 0xfeff, 0xff00, 0xff7f, 0xff80, 0x007f, 0x0080
sqrshrn_exp:
    .byte  0xfe, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x01

    .balign 16
uqrshrn_src:
    .hword 0x0001, 0x007f, 0x0080, 0x0081, 0x00ff, 0x0100, 0x017f, 0xff00
uqrshrn_exp:
    .byte  0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0xff

    .balign 16
sqshrun_src:
    .hword 0xfffc, 0xffff, 0x0000, 0x0001, 0x01fe, 0x01ff, 0x0200, 0x03ff
sqshrun_exp:
    .byte  0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff

    .balign 16
sqrshrun_src:
    .hword 0xfffb, 0xfffc, 0xfffd, 0xfffe, 0x01fd, 0x01fe, 0x01ff, 0x0200
sqrshrun_exp:
    .byte  0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff

    .balign 8
scalar_sqshrn_src:
    .quad 0x0000000000000100
scalar_uqshrn_src:
    .quad 0x00000000000003ff
scalar_sqrshrn_src:
    .quad 0x000000000000ff80
scalar_uqrshrn_src:
    .quad 0x0000000000000180
scalar_sqshrun_src:
    .quad 0x0000000000000200
scalar_sqrshrun_src:
    .quad 0x00000000000001ff

    .bss
    .balign 16
tmp:
    .skip 16
