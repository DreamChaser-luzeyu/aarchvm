    .arch armv8.2-a+fp16
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
    adr  x22, src_half
    ldr  q1, [x22]
    fcvtl v0.4s, v1.4h
    str  q0, [x21]
    mov  x0, x21
    adr  x1, exp_widen_low
    bl   check16_1
    mrs  x2, fpsr
    cbnz x2, fail1

    msr  fpsr, xzr
    fcvtl2 v2.4s, v1.8h
    str  q2, [x21, #16]
    add  x0, x21, #16
    adr  x1, exp_widen_high
    bl   check16_2
    mrs  x2, fpsr
    cbnz x2, fail2

    msr  fpcr, xzr
    msr  fpsr, xzr
    adr  x22, src_single
    ldr  q4, [x22]
    fcvtn v3.4h, v4.4s
    str  q3, [x21, #32]
    add  x0, x21, #32
    adr  x1, exp_narrow_low
    bl   check16_3
    mrs  x2, fpsr
    mov  x3, #0x14
    cmp  x2, x3
    b.ne fail3

    msr  fpsr, xzr
    adr  x22, init_narrow2
    ldr  q5, [x22]
    adr  x23, src_single
    ldr  q4, [x23]
    fcvtn2 v5.8h, v4.4s
    str  q5, [x21, #48]
    add  x0, x21, #48
    adr  x1, exp_narrow_high
    bl   check16_4
    mrs  x2, fpsr
    mov  x3, #0x14
    cmp  x2, x3
    b.ne fail4

    movz x10, #0x0400, lsl #16      // FPCR.AHP=1
    msr  fpcr, x10
    isb
    msr  fpsr, xzr
    adr  x22, src_half_ahp
    ldr  q1, [x22]
    fcvtl v0.4s, v1.4h
    str  q0, [x21, #64]
    add  x0, x21, #64
    adr  x1, exp_widen_ahp
    bl   check16_5
    mrs  x2, fpsr
    cbnz x2, fail5

    msr  fpsr, xzr
    adr  x22, src_single_ahp
    ldr  q4, [x22]
    fcvtn v3.4h, v4.4s
    str  q3, [x21, #80]
    add  x0, x21, #80
    adr  x1, exp_narrow_ahp
    bl   check16_6
    mrs  x2, fpsr
    cbnz x2, fail6

    movz x0, #'v'
    b    done

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

check16_6:
    ldr  x2, [x0]
    ldr  x3, [x1]
    cmp  x2, x3
    b.ne fail6
    ldr  x2, [x0, #8]
    ldr  x3, [x1, #8]
    cmp  x2, x3
    b.ne fail6
    ret

fail1:
    movz x0, #'1'
    b    done
fail2:
    movz x0, #'2'
    b    done
fail3:
    movz x0, #'3'
    b    done
fail4:
    movz x0, #'4'
    b    done
fail5:
    movz x0, #'5'
    b    done
fail6:
    movz x0, #'6'

done:
    str  x0, [x20]
    brk  #0

    .data
    .balign 16
src_half:
    .hword 0x3e00, 0xc000, 0x3800, 0x0000
    .hword 0x3c00, 0xbc00, 0x4000, 0x4200
exp_widen_low:
    .word 0x3fc00000, 0xc0000000, 0x3f000000, 0x00000000
exp_widen_high:
    .word 0x3f800000, 0xbf800000, 0x40000000, 0x40400000

    .balign 16
src_single:
    .word 0x3fc00000, 0x3f801000, 0x4788b800, 0xc0000000
exp_narrow_low:
    .hword 0x3e00, 0x3c00, 0x7c00, 0xc000
    .hword 0x0000, 0x0000, 0x0000, 0x0000
init_narrow2:
    .hword 0x1111, 0x2222, 0x3333, 0x4444
    .hword 0xaaaa, 0xbbbb, 0xcccc, 0xdddd
exp_narrow_high:
    .hword 0x1111, 0x2222, 0x3333, 0x4444
    .hword 0x3e00, 0x3c00, 0x7c00, 0xc000

    .balign 16
src_half_ahp:
    .hword 0x7c00, 0x7fff, 0xfc00, 0x0000
    .hword 0x0000, 0x0000, 0x0000, 0x0000
exp_widen_ahp:
    .word 0x47800000, 0x47ffe000, 0xc7800000, 0x00000000

    .balign 16
src_single_ahp:
    .word 0x47800000, 0xc7800000, 0x00000000, 0xbf800000
exp_narrow_ahp:
    .hword 0x7c00, 0xfc00, 0x0000, 0xbc00
    .hword 0x0000, 0x0000, 0x0000, 0x0000

    .balign 16
dst:
    .space 96
