    .text
    .global _start
_start:
    movz x20, #0x0000
    movk x20, #0x0900, lsl #16      // UART
    movz x25, #0x0030, lsl #16
    msr  cpacr_el1, x25
    isb

    // FPSR in this AArch64-only model should keep only QC/IDC/IXC/UFC/OFC/DZC/IOC.
    movz x0, #0xffff
    movk x0, #0xffff, lsl #16
    msr  fpsr, x0
    mrs  x1, fpsr
    movz x2, #0x009f
    movk x2, #0x0800, lsl #16
    cmp  x1, x2
    b.ne fail

    movz x9, #0x0800
    lsl  x9, x9, #16               // FPSR.QC

    // Saturating vector add must set QC.
    msr  fpsr, xzr
    movi v0.16b, #0x7f
    movi v1.16b, #0x01
    sqadd v2.16b, v0.16b, v1.16b
    umov w3, v2.b[0]
    cmp  w3, #0x7f
    b.ne fail
    mrs  x4, fpsr
    tst  x4, x9
    b.eq fail

    // Non-saturating add must leave QC clear after software clears FPSR.
    msr  fpsr, xzr
    movi v3.16b, #0x02
    movi v4.16b, #0x03
    sqadd v5.16b, v3.16b, v4.16b
    umov w6, v5.b[0]
    cmp  w6, #0x05
    b.ne fail
    mrs  x4, fpsr
    tst  x4, x9
    b.ne fail

    // Signed saturating narrow must also set QC.
    msr  fpsr, xzr
    adr  x21, signed_narrow_src
    ldr  q6, [x21]
    sqxtn v7.8b, v6.8h
    umov w8, v7.b[0]
    cmp  w8, #0x80
    b.ne fail
    mrs  x4, fpsr
    tst  x4, x9
    b.eq fail

    // Unsigned saturating narrow must set QC and clamp to the destination max.
    msr  fpsr, xzr
    adr  x22, unsigned_narrow_src
    ldr  q8, [x22]
    uqxtn v9.8b, v8.8h
    umov w10, v9.b[0]
    cmp  w10, #0xff
    b.ne fail
    mrs  x4, fpsr
    tst  x4, x9
    b.eq fail

    movz x0, #'Q'
    str  x0, [x20]
    movz x0, #'\n'
    str  x0, [x20]
    brk  #0

fail:
    movz x0, #'F'
    str  x0, [x20]
    brk  #0

    .data
    .balign 16
signed_narrow_src:
    .hword 0xff7f, 0x0080, 0x0001, 0x0002, 0x007f, 0x00ff, 0x0100, 0x7fff

    .balign 16
unsigned_narrow_src:
    .hword 0x0100, 0x00ff, 0x0001, 0x0002, 0x0003, 0x00fe, 0x1234, 0xffff
