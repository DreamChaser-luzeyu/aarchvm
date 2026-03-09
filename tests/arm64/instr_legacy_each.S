    .text
    .global _start
_start:
    movz x1, #0
    movk x1, #0x0900, lsl #16      // UART
    movz x28, #'0'

    // NOP
    nop

    // B
    b test_bl
    b fail

test_bl:
    movz x28, #'1'
    // BL + RET
    bl sub_bl
    subs xzr, x3, #1
    b.ne fail

    // BR
    adr x4, after_br
    br  x4
    b fail
after_br:

    // BLR + RET
    adr x5, sub_blr
    blr x5
    subs xzr, x6, #1
    b.ne fail

    // B.cond
    subs xzr, x6, #1
    b.eq cond_ok
    b fail
cond_ok:

    // CBZ/CBNZ
    movz x7, #0
    cbz  x7, cbz_ok
    b fail
cbz_ok:
    movz x7, #1
    cbnz x7, cbnz_ok
    b fail
cbnz_ok:

    // TBZ/TBNZ
    movn w8, #0
    tbz  x8, #0, fail
    tbnz x8, #31, tb_ok
    b fail
tb_ok:
    movz x28, #'2'

    // ADR/ADRP
    adr  x9, adrp_base
    adrp x10, adrp_base
    subs xzr, x9, x10
    b.lt fail

adrp_base:
    movz x28, #'3'
    // MOVZ/MOVK/MOVN
    movz x11, #0x1234
    movk x11, #0x5678, lsl #16
    movz x12, #0x5678
    movk x12, #0x1234
    subs xzr, x11, x12
    b.eq fail

    movn w13, #0
    tbnz x13, #31, movn_ok
    b fail
movn_ok:
    movz x28, #'4'

    // ADD/SUB immediate
    movz x28, #'a'
    movz x14, #10
    add  x14, x14, #5
    subs xzr, x14, #15
    b.ne fail
    sub  x14, x14, #3
    subs xzr, x14, #12
    b.ne fail

    // ADDS/SUBS immediate
    movz x28, #'b'
    adds x15, x14, #1
    subs xzr, x15, #13
    b.ne fail
    subs x15, x15, #13
    b.ne fail

    // ADD/SUB shifted register
    movz x28, #'c'
    movz x16, #2
    add  x17, x16, x16, lsl #1
    subs xzr, x17, #6
    b.ne fail
    sub  x17, x17, x16
    subs xzr, x17, #4
    b.ne fail

    // CSEL
    movz x28, #'5'
    subs xzr, x17, #4
    movz x18, #0x11
    movz x19, #0x22
    csel x20, x18, x19, eq
    subs xzr, x20, #0x11
    b.ne fail

    // AND/BIC/ORR/ORN/EOR/EON/ANDS/BICS
    movz x21, #0x0f0
    movz x22, #0x00f
    and  x23, x21, x22
    subs xzr, x23, #0
    b.ne fail
    bic  x23, x21, x22
    subs xzr, x23, #0x0f0
    b.ne fail
    orr  x23, x21, x22
    subs xzr, x23, #0x0ff
    b.ne fail
    orn  x23, x21, x22
    tbz  x23, #8, fail
    eor  x23, x21, x22
    subs xzr, x23, #0x0ff
    b.ne fail
    eon  x23, x21, x22
    tbz  x23, #8, fail
    ands x23, x21, x22
    b.ne fail
    bics x23, x21, x22
    b.eq fail

    // Load/store family
    movz x28, #'6'
    adr  x24, mem
    movz x25, #0x88
    str  x25, [x24]
    ldr  x26, [x24]
    subs xzr, x26, #0x88
    b.ne fail

    movz w25, #0x44
    str  w25, [x24, #4]
    ldr  w26, [x24, #4]
    subs wzr, w26, #0x44
    b.ne fail

    movz w25, #0x33
    strb w25, [x24, #8]
    ldrb w26, [x24, #8]
    subs wzr, w26, #0x33
    b.ne fail

    movz w25, #0x55
    strh w25, [x24, #10]
    ldrh w26, [x24, #10]
    subs wzr, w26, #0x55
    b.ne fail

    movz x25, #0x123
    stur x25, [x24, #16]
    ldur x26, [x24, #16]
    subs xzr, x26, #0x123
    b.ne fail

    movz w25, #0x321
    stur w25, [x24, #20]
    ldur w26, [x24, #20]
    subs wzr, w26, #0x321
    b.ne fail

    add  x27, x24, #64
    movz x28, #0x1
    movz x29, #0x2
    stp  x28, x29, [x27, #-16]!
    ldp  x30, x20, [x27], #16
    subs xzr, x30, #1
    b.ne fail
    subs xzr, x20, #2
    b.ne fail

    // MRS/MSR system register subset
    movz x28, #'7'
    mrs  x2, currentel
    subs xzr, x2, #4
    b.ne fail

    subs xzr, x2, x2
    mrs  x2, nzcv
    tbz  x2, #30, fail

    movz x2, #0
    msr  nzcv, x2
    mrs  x2, nzcv
    tbnz x2, #30, fail

    // DAIF set/clr + MRS/MSR
    movz x28, #'8'
    mrs  x2, daif
    tbnz x2, #7, fail
    msr  daifset, #2
    mrs  x2, daif
    tbz  x2, #7, fail
    msr  daifclr, #2
    mrs  x2, daif
    tbnz x2, #7, fail

    // SPSel read/write
    mrs  x2, spsel
    msr  spsel, #0
    mrs  x3, spsel
    cbnz x3, fail
    msr  spsel, #1
    mrs  x3, spsel
    subs xzr, x3, #1
    b.ne fail
    msr  spsel, x2

    // Barrier / hint / event instructions
    movz x28, #'9'
    dmb sy
    dsb sy
    isb
    clrex
    sevl
    wfe
    sev
    wfe

    movz x0, #'E'
    str  x0, [x1]
    movz x0, #'\n'
    str  x0, [x1]
    brk  #0

sub_bl:
    movz x3, #1
    ret

sub_blr:
    movz x6, #1
    ret

fail:
    orr  x0, x28, xzr
    str  x0, [x1]
    brk  #0

    .data
    .balign 16
mem:
    .quad 0
    .quad 0
    .quad 0
    .quad 0
