    .text
    .global _start
_start:
    movz x1, #0
    movk x1, #0x0900, lsl #16      // UART

    movz x4, #0x3fe5               // bit5 + ALLINT + SSBS + BTYPE + DAIF + M[4:0]
    movk x4, #0xfff0, lsl #16      // NZCV + RES0[27:26] + TCO/DIT/UAO + PAN/SS/IL
    movk x4, #0x001f, lsl #32      // PM/PPEND/EXLOCK/PACM/UINJ

    msr  spsr_el1, x4
    isb
    mrs  x5, spsr_el1

    movz x6, #0x3c20               // bit5 + ALLINT + SSBS + BTYPE
    movk x6, #0x0f80, lsl #16      // RES0[27:26] + TCO/DIT/UAO
    movk x6, #0x001f, lsl #32      // PM/PPEND/EXLOCK/PACM/UINJ
    tst  x5, x6
    b.ne fail1

    movz x7, #0x03c5               // DAIF + M[4:0]
    movk x7, #0xf070, lsl #16      // NZCV + PAN/SS/IL
    subs xzr, x5, x7
    b.ne fail2

    movz x0, #'P'
    str  x0, [x1]
    movz x0, #'\n'
    str  x0, [x1]
    brk  #0

fail1:
    movz x0, #'1'
    b out
fail2:
    movz x0, #'2'
    b out

out:
    str  x0, [x1]
    movz x0, #'F'
    str  x0, [x1]
    brk  #0
