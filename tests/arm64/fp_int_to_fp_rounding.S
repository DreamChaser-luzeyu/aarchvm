    .text
    .global _start
_start:
    movz x20, #0
    movk x20, #0x0900, lsl #16
    movz x25, #0x0030, lsl #16
    msr  cpacr_el1, x25
    isb

    adr  x21, dst
    adr  x22, data32
    adr  x23, data64

    movz x24, #'1'
    msr  fpcr, xzr
    isb
    msr  fpsr, xzr
    ldr  w0, [x22, #0]            // 42
    scvtf s0, w0
    str  s0, [x21, #0]
    add  x0, x21, #0
    adr  x1, exp_exact_s
    bl   check4
    mrs  x2, fpsr
    cbnz x2, fail1

    movz x24, #'2'
    msr  fpcr, xzr                // round to nearest even
    isb
    msr  fpsr, xzr
    ldr  w0, [x22, #4]            // 16777217
    scvtf s1, w0
    str  s1, [x21, #4]
    add  x0, x21, #4
    adr  x1, exp_rne_s
    bl   check4
    mrs  x2, fpsr
    mov  x3, #(1 << 4)
    cmp  x2, x3
    b.ne fail2

    movz x24, #'3'
    mov  x0, #(1 << 22)           // round toward +inf
    msr  fpcr, x0
    isb
    msr  fpsr, xzr
    ldr  w0, [x22, #4]            // 16777217
    scvtf s2, w0
    str  s2, [x21, #8]
    add  x0, x21, #8
    adr  x1, exp_posinf_s
    bl   check4
    mrs  x2, fpsr
    mov  x3, #(1 << 4)
    cmp  x2, x3
    b.ne fail3

    movz x24, #'4'
    mov  x0, #(2 << 22)           // round toward -inf
    msr  fpcr, x0
    isb
    msr  fpsr, xzr
    ldr  w0, [x22, #8]            // -16777217
    scvtf s3, w0
    str  s3, [x21, #12]
    add  x0, x21, #12
    adr  x1, exp_neginf_s
    bl   check4
    mrs  x2, fpsr
    mov  x3, #(1 << 4)
    cmp  x2, x3
    b.ne fail4

    movz x24, #'5'
    msr  fpcr, xzr                // round to nearest even
    isb
    msr  fpsr, xzr
    ldr  x0, [x23, #0]            // 2^53 + 1
    ucvtf d4, x0
    str  d4, [x21, #16]
    add  x0, x21, #16
    adr  x1, exp_rne_d
    bl   check8
    mrs  x2, fpsr
    mov  x3, #(1 << 4)
    cmp  x2, x3
    b.ne fail5

    movz x24, #'6'
    mov  x0, #(1 << 22)           // round toward +inf
    msr  fpcr, x0
    isb
    msr  fpsr, xzr
    ldr  x0, [x23, #0]            // 2^53 + 1
    ucvtf d5, x0
    str  d5, [x21, #24]
    add  x0, x21, #24
    adr  x1, exp_posinf_d
    bl   check8
    mrs  x2, fpsr
    mov  x3, #(1 << 4)
    cmp  x2, x3
    b.ne fail6

    movz x24, #'7'
    mov  x0, #(1 << 22)           // round toward +inf
    msr  fpcr, x0
    isb
    msr  fpsr, xzr
    ldr  s6, [x22, #4]            // 16777217 bit pattern in Sn
    scvtf s7, s6
    str  s7, [x21, #32]
    add  x0, x21, #32
    adr  x1, exp_posinf_s
    bl   check4
    mrs  x2, fpsr
    mov  x3, #(1 << 4)
    cmp  x2, x3
    b.ne fail7

    movz x24, #'8'
    msr  fpcr, xzr                // round to nearest even
    isb
    msr  fpsr, xzr
    ldr  d8, [x23, #0]            // 2^53 + 1 bit pattern in Dn
    ucvtf d9, d8
    str  d9, [x21, #40]
    add  x0, x21, #40
    adr  x1, exp_rne_d
    bl   check8
    mrs  x2, fpsr
    mov  x3, #(1 << 4)
    cmp  x2, x3
    b.ne fail8

    movz x0, #'I'
    movz x19, #0
    movk x19, #0x0900, lsl #16
    str  x0, [x19]
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
fail6:
    movz x0, #'6'
    b out
fail7:
    movz x0, #'7'
    b out
fail8:
    movz x0, #'8'
    b out
fail:
    mov  x0, x24
out:
    movz x19, #0
    movk x19, #0x0900, lsl #16
    str  x0, [x19]
    brk  #0

    .data
    .balign 16
data32:
    .word 42
    .word 0x01000001
    .word 0xfeffffff

    .balign 8
data64:
    .quad 0x0020000000000001

exp_exact_s:
    .word 0x42280000
exp_rne_s:
    .word 0x4b800000
exp_posinf_s:
    .word 0x4b800001
exp_neginf_s:
    .word 0xcb800001

exp_rne_d:
    .quad 0x4340000000000000
exp_posinf_d:
    .quad 0x4340000000000001

    .balign 16
dst:
    .space 48
