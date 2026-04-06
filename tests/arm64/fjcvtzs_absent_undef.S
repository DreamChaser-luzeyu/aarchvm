    .text
    .global _start
_start:
    movz x20, #0
    movk x20, #0x0900, lsl #16      // UART

    adr  x22, vectors
    msr  vbar_el1, x22
    isb

    movz x25, #0x0030, lsl #16      // CPACR_EL1.FPEN=0b11
    msr  cpacr_el1, x25
    isb

    mov  x0, #0x1111
    .inst 0x1E7E0020                // FJCVTZS W0, D1
    mov  x0, #'B'
    str  x0, [x20]
    brk  #0

after_undef:
    mrs  x2, esr_el1
    lsr  x3, x2, #26
    cmp  x3, #0
    b.ne fail1
    ubfx x4, x2, #25, #1
    cmp  x4, #1
    b.ne fail2
    ubfx x5, x2, #0, #25
    cbnz x5, fail3
    mrs  x6, far_el1
    cbnz x6, fail4
    movz x7, #0x1111
    cmp  x0, x7
    b.ne fail5
    mov  x0, #'J'
    str  x0, [x20]
    brk  #0

fail1:
    mov  x0, #'1'
    b    done
fail2:
    mov  x0, #'2'
    b    done
fail3:
    mov  x0, #'3'
    b    done
fail4:
    mov  x0, #'4'
    b    done
fail5:
    mov  x0, #'5'
    b    done
done:
    str  x0, [x20]
    brk  #0

    .balign 2048
vectors:
    b .
    .space 0x200 - 4, 0
    adr  x24, after_undef
    msr  elr_el1, x24
    eret
