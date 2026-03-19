    .text
    .global _start
_start:
    movz x20, #0
    movk x20, #0x0900, lsl #16
    adr  x21, dst
    movz x25, #0x0030, lsl #16
    msr  cpacr_el1, x25
    isb

    msr  fpcr, xzr
    msr  fpsr, xzr
    adr  x22, src_widen
    ldr  q1, [x22]
    fcvtl v0.2d, v1.2s
    str  q0, [x21]
    mov  x0, x21
    adr  x1, exp_widen_low
    bl   check16_1
    mrs  x2, fpsr
    cbnz x2, fail1

    msr  fpsr, xzr
    fcvtl2 v2.2d, v1.4s
    str  q2, [x21, #16]
    add  x0, x21, #16
    adr  x1, exp_widen_high
    bl   check16_2
    mrs  x2, fpsr
    cbnz x2, fail2

    msr  fpcr, xzr
    msr  fpsr, xzr
    adr  x22, src_narrow_halfway
    ldr  q4, [x22]
    fcvtn v3.2s, v4.2d
    str  q3, [x21, #32]
    add  x0, x21, #32
    adr  x1, exp_narrow_even
    bl   check16_3
    mrs  x2, fpsr
    mov  x3, #(1 << 4)
    cmp  x2, x3
    b.ne fail3

    mov  x3, #(1 << 22)
    msr  fpcr, x3
    isb
    msr  fpsr, xzr
    adr  x22, init_narrow2
    ldr  q5, [x22]
    fcvtn2 v5.4s, v4.2d
    str  q5, [x21, #48]
    add  x0, x21, #48
    adr  x1, exp_narrow_posinf
    bl   check16_4
    mrs  x2, fpsr
    mov  x3, #(1 << 4)
    cmp  x2, x3
    b.ne fail4

    mov  x3, #(2 << 22)
    msr  fpcr, x3
    isb
    msr  fpsr, xzr
    fcvtn v6.2s, v4.2d
    str  q6, [x21, #64]
    add  x0, x21, #64
    adr  x1, exp_narrow_neginf
    bl   check16_5
    mrs  x2, fpsr
    mov  x3, #(1 << 4)
    cmp  x2, x3
    b.ne fail5

    msr  fpcr, xzr
    msr  fpsr, xzr
    adr  x22, src_widen_snan
    ldr  d7, [x22]
    fcvtl v8.2d, v7.2s
    str  q8, [x21, #80]
    ldr  x2, [x21, #80]
    ldr  x3, =0x7ff0000000000000
    and  x4, x2, x3
    cmp  x4, x3
    b.ne fail6
    ldr  x3, =0x0008000000000000
    tst  x2, x3
    b.eq fail6
    ldr  x2, [x21, #88]
    ldr  x3, =0x3ff0000000000000
    cmp  x2, x3
    b.ne fail6
    mrs  x2, fpsr
    cmp  x2, #1
    b.ne fail6

    msr  fpsr, xzr
    adr  x22, src_narrow_snan
    ldr  q9, [x22]
    fcvtn v10.2s, v9.2d
    str  q10, [x21, #96]
    ldr  w2, [x21, #96]
    ldr  w3, =0x7f800000
    and  w4, w2, w3
    cmp  w4, w3
    b.ne fail7
    tst  w2, #0x00400000
    b.eq fail7
    ldr  w2, [x21, #100]
    ldr  w3, =0x3f800000
    cmp  w2, w3
    b.ne fail7
    mrs  x2, fpsr
    cmp  x2, #1
    b.ne fail7

    movz x0, #'H'
    str  x0, [x20]
    brk  #0

check16_1:
    ldr  x2, [x0]
    ldr  x3, [x1]
    cmp  x2, x3
    b.ne fail1
    ldr  x2, [x0, #8]
    ldr  x3, [x1, #8]
    cmp  x2, x3
    b.ne fail1
    ret

check16_2:
    ldr  x2, [x0]
    ldr  x3, [x1]
    cmp  x2, x3
    b.ne fail2
    ldr  x2, [x0, #8]
    ldr  x3, [x1, #8]
    cmp  x2, x3
    b.ne fail2
    ret

check16_3:
    ldr  x2, [x0]
    ldr  x3, [x1]
    cmp  x2, x3
    b.ne fail3
    ldr  x2, [x0, #8]
    ldr  x3, [x1, #8]
    cmp  x2, x3
    b.ne fail3
    ret

check16_4:
    ldr  x2, [x0]
    ldr  x3, [x1]
    cmp  x2, x3
    b.ne fail4
    ldr  x2, [x0, #8]
    ldr  x3, [x1, #8]
    cmp  x2, x3
    b.ne fail4
    ret

check16_5:
    ldr  x2, [x0]
    ldr  x3, [x1]
    cmp  x2, x3
    b.ne fail5
    ldr  x2, [x0, #8]
    ldr  x3, [x1, #8]
    cmp  x2, x3
    b.ne fail5
    ret

fail1:
    movz x0, #'1'
    b done
fail2:
    movz x0, #'2'
    b done
fail3:
    movz x0, #'3'
    b done
fail4:
    movz x0, #'4'
    b done
fail5:
    movz x0, #'5'
    b done
fail6:
    movz x0, #'6'
    b done
fail7:
    movz x0, #'7'
done:
    str  x0, [x20]
    brk  #0

    .data
    .balign 16
src_widen:
    .word 0x3fc00000, 0xc0100000, 0x3f000000, 0x80000000
exp_widen_low:
    .quad 0x3ff8000000000000, 0xc002000000000000
exp_widen_high:
    .quad 0x3fe0000000000000, 0x8000000000000000

    .balign 16
src_narrow_halfway:
    .quad 0x3ff0000010000000, 0xbff0000010000000
exp_narrow_even:
    .word 0x3f800000, 0xbf800000, 0x00000000, 0x00000000
init_narrow2:
    .word 0x11223344, 0x55667788, 0x99aabbcc, 0xddeeff00
exp_narrow_posinf:
    .word 0x11223344, 0x55667788, 0x3f800001, 0xbf800000
exp_narrow_neginf:
    .word 0x3f800000, 0xbf800001, 0x00000000, 0x00000000

    .balign 16
src_widen_snan:
    .word 0x7f812345, 0x3f800000

    .balign 16
src_narrow_snan:
    .quad 0x7ff0123456789abc, 0x3ff0000000000000

dst:
    .space 112
