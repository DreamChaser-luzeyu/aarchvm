    .text
    .global _start
_start:
    movz x20, #0
    movk x20, #0x0900, lsl #16
    adr  x21, dst
    movz x25, #0x0030, lsl #16
    msr  cpacr_el1, x25
    isb

    adr  x22, src_addp_s
    ldr  d1, [x22]
    faddp s0, v1.2s
    str  s0, [x21]
    mov  x0, x21
    adr  x1, exp_addp_s
    bl   check4_1

    adr  x22, src_addp_d
    ldr  q2, [x22]
    faddp d3, v2.2d
    str  d3, [x21, #8]
    add  x0, x21, #8
    adr  x1, exp_addp_d
    bl   check8_2

    msr  fpsr, xzr
    adr  x22, src_maxp_s
    ldr  d4, [x22]
    fmaxp s5, v4.2s
    str  s5, [x21, #16]
    add  x0, x21, #16
    adr  x1, exp_maxp_s
    bl   check4_3
    mrs  x2, fpsr
    cbnz x2, fail3

    msr  fpsr, xzr
    adr  x22, src_minp_d
    ldr  q6, [x22]
    fminp d7, v6.2d
    str  d7, [x21, #24]
    add  x0, x21, #24
    adr  x1, exp_minp_d
    bl   check8_4
    mrs  x2, fpsr
    cbnz x2, fail4

    msr  fpsr, xzr
    adr  x22, src_maxnmp_s
    ldr  d8, [x22]
    fmaxnmp s9, v8.2s
    str  s9, [x21, #32]
    add  x0, x21, #32
    adr  x1, exp_maxnmp_s
    bl   check4_5
    mrs  x2, fpsr
    cbnz x2, fail5

    msr  fpsr, xzr
    adr  x22, src_minnmp_d
    ldr  q10, [x22]
    fminnmp d11, v10.2d
    str  d11, [x21, #40]
    add  x0, x21, #40
    adr  x1, exp_minnmp_d
    bl   check8_6
    mrs  x2, fpsr
    cmp  x2, #1
    b.ne fail6

    movz x0, #'Y'
    str  x0, [x20]
    brk  #0

check4_1:
    ldr  w2, [x0]
    ldr  w3, [x1]
    cmp  w2, w3
    b.ne fail1
    ret

check8_2:
    ldr  x2, [x0]
    ldr  x3, [x1]
    cmp  x2, x3
    b.ne fail2
    ret

check4_3:
    ldr  w2, [x0]
    ldr  w3, [x1]
    cmp  w2, w3
    b.ne fail3
    ret

check8_4:
    ldr  x2, [x0]
    ldr  x3, [x1]
    cmp  x2, x3
    b.ne fail4
    ret

check4_5:
    ldr  w2, [x0]
    ldr  w3, [x1]
    cmp  w2, w3
    b.ne fail5
    ret

check8_6:
    ldr  x2, [x0]
    ldr  x3, [x1]
    cmp  x2, x3
    b.ne fail6
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
done:
    str  x0, [x20]
    brk  #0

    .data
    .balign 16
src_addp_s:
    .word 0x3fc00000, 0x40100000
exp_addp_s:
    .word 0x40700000

    .balign 16
src_addp_d:
    .quad 0x3ff8000000000000, 0xc002000000000000
exp_addp_d:
    .quad 0xbfe8000000000000

    .balign 16
src_maxp_s:
    .word 0x7fc12345, 0x3f800000
exp_maxp_s:
    .word 0x7fc12345

    .balign 16
src_minp_d:
    .quad 0x4014000000000000, 0xc000000000000000
exp_minp_d:
    .quad 0xc000000000000000

    .balign 16
src_maxnmp_s:
    .word 0x7fc12345, 0x3f800000
exp_maxnmp_s:
    .word 0x3f800000

    .balign 16
src_minnmp_d:
    .quad 0x7ff0123456789abc, 0x3ff0000000000000
exp_minnmp_d:
    .quad 0x7ff8123456789abc

dst:
    .space 48
