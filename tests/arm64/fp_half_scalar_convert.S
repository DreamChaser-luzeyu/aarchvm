    .arch armv8.2-a+fp16
    .text
    .global _start
_start:
    movz x20, #0
    movk x20, #0x0900, lsl #16      // UART
    movz x25, #0x0030, lsl #16
    msr  cpacr_el1, x25
    isb

    adr  x21, data
    adr  x22, tmp

    ldr  h0, [x21]
    fcvt s1, h0
    fmov w2, s1
    ldr  w3, [x21, #4]
    cmp  w2, w3
    b.ne fail1

    fcvt d4, h0
    fmov x5, d4
    ldr  x6, [x21, #8]
    cmp  x5, x6
    b.ne fail2

    ldr  s7, [x21, #4]
    fcvt h8, s7
    str  h8, [x22]
    ldrh w9, [x22]
    ldrh w10, [x21, #16]
    cmp  w9, w10
    b.ne fail3

    ldr  d11, [x21, #8]
    fcvt h12, d11
    str  h12, [x22]
    ldrh w13, [x22]
    cmp  w13, w10
    b.ne fail4

    msr  fpcr, xzr
    isb
    msr  fpsr, xzr
    ldr  s14, [x21, #20]
    fcvt h15, s14
    str  h15, [x22]
    ldrh w16, [x22]
    ldrh w17, [x21, #18]
    cmp  w16, w17
    b.ne fail5
    mrs  x18, fpsr
    mov  x19, #0x10
    cmp  x18, x19
    b.ne fail6

    msr  fpsr, xzr
    ldr  s20, [x21, #24]
    fcvt h21, s20
    str  h21, [x22]
    ldrh w23, [x22]
    movz w24, #0x7c00
    cmp  w23, w24
    b.ne fail7
    mrs  x18, fpsr
    mov  x19, #0x14
    cmp  x18, x19
    b.ne fail8

    movz x18, #0x0400, lsl #16      // FPCR.AHP=1
    msr  fpcr, x18
    isb

    adr  x23, ahp_half_65536
    ldr  h0, [x23]
    fcvt s1, h0
    fmov w2, s1
    adr  x24, ahp_exp_s_65536
    ldr  w3, [x24]
    cmp  w2, w3
    b.ne fail9

    adr  x23, ahp_half_max
    ldr  h0, [x23]
    fcvt d4, h0
    fmov x5, d4
    adr  x24, ahp_exp_d_max
    ldr  x6, [x24]
    cmp  x5, x6
    b.ne failA

    msr  fpsr, xzr
    adr  x23, ahp_src_s_65536
    ldr  s7, [x23]
    fcvt h8, s7
    str  h8, [x22]
    ldrh w9, [x22]
    movz w10, #0x7c00
    cmp  w9, w10
    b.ne failB
    mrs  x18, fpsr
    cbnz x18, failC

    msr  fpsr, xzr
    adr  x23, ahp_src_s_inf
    ldr  s7, [x23]
    fcvt h8, s7
    str  h8, [x22]
    ldrh w9, [x22]
    movz w10, #0x7fff
    cmp  w9, w10
    b.ne failD
    mrs  x18, fpsr
    mov  x19, #0x1
    cmp  x18, x19
    b.ne failE

    msr  fpsr, xzr
    adr  x23, ahp_src_s_qnan
    ldr  s7, [x23]
    fcvt h8, s7
    str  h8, [x22]
    ldrh w9, [x22]
    cbnz w9, failF
    mrs  x18, fpsr
    mov  x19, #0x1
    cmp  x18, x19
    b.ne failG

    movz x0, #'K'
    b    done

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
    b    done
fail7:
    movz x0, #'7'
    b    done
fail8:
    movz x0, #'8'
    b    done
fail9:
    movz x0, #'9'
    b    done
failA:
    movz x0, #'A'
    b    done
failB:
    movz x0, #'B'
    b    done
failC:
    movz x0, #'C'
    b    done
failD:
    movz x0, #'D'
    b    done
failE:
    movz x0, #'E'
    b    done
failF:
    movz x0, #'F'
    b    done
failG:
    movz x0, #'G'

done:
    str  x0, [x20]
    brk  #0

    .data
    .balign 8
data:
    .hword 0x3e00                     // 1.5h
    .hword 0
    .word  0x3fc00000                 // 1.5f
    .quad  0x3ff8000000000000         // 1.5d
    .hword 0x3e00                     // expected half(1.5)
    .hword 0x3c00                     // expected half(1 + 2^-11) under RNE
    .word  0x3f801000                 // 1 + 2^-11
    .word  0x4788b800                 // 70000.0f
ahp_half_65536:
    .hword 0x7c00                     // alternative half 65536.0
ahp_half_max:
    .hword 0x7fff                     // alternative half max finite 131008.0
    .balign 4
ahp_exp_s_65536:
    .word  0x47800000
    .balign 8
ahp_exp_d_max:
    .quad  0x40fffc0000000000
    .balign 4
ahp_src_s_65536:
    .word  0x47800000
ahp_src_s_inf:
    .word  0x7f800000
ahp_src_s_qnan:
    .word  0x7fc00000
tmp:
    .hword 0
