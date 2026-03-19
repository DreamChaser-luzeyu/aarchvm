    .text
    .global _start
_start:
    movz x20, #0x0000
    movk x20, #0x0900, lsl #16
    movz x25, #0x0030, lsl #16
    mov  w9, #0
    msr  cpacr_el1, x25
    isb

    adr  x21, qnan_d
    ldr  d0, [x21]
    adr  x21, one_d
    ldr  d1, [x21]
    adr  x21, one_s
    ldr  s2, [x21]
    adr  x21, two_s
    ldr  s3, [x21]
    adr  x21, snan_s
    ldr  s4, [x21]

    cmp  wzr, wzr
    msr  fpsr, xzr
    fccmpe d0, d1, #0xc, eq
    bl   check_unordered
    mrs  x2, fpsr
    cmp  x2, #1
    b.ne fail1

    cmp  w9, #1
    msr  fpsr, xzr
    fccmpe d0, d1, #0xc, eq
    mrs  x2, nzcv
    ldr  x3, =0xc0000000
    cmp  x2, x3
    b.ne fail2
    mrs  x2, fpsr
    cbnz x2, fail3

    cmp  w9, #1
    msr  fpsr, xzr
    fccmp s2, s2, #0x5, mi
    mrs  x2, nzcv
    ldr  x3, =0x60000000
    cmp  x2, x3
    b.ne fail4
    mrs  x2, fpsr
    cbnz x2, fail5

    cmp  wzr, wzr
    msr  fpsr, xzr
    fccmp s4, s3, #0xa, ne
    mrs  x2, nzcv
    ldr  x3, =0xa0000000
    cmp  x2, x3
    b.ne fail6
    mrs  x2, fpsr
    cbnz x2, fail7

    cmp  wzr, wzr
    msr  fpsr, xzr
    fccmp s2, s3, #0x0, al
    mrs  x2, nzcv
    ldr  x3, =0x80000000
    cmp  x2, x3
    b.ne fail8
    mrs  x2, fpsr
    cbnz x2, fail9

    movz x0, #'C'
    str  x0, [x20]
    brk  #0

check_unordered:
    mrs  x3, nzcv
    ldr  x4, =0x30000000
    cmp  x3, x4
    b.ne failu
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
fail9:
    movz x0, #'9'
    b out
failu:
    movz x0, #'U'
out:
    str  x0, [x20]
    brk  #0

    .data
    .balign 8
qnan_d:
    .quad 0x7ff8000000000000
one_d:
    .quad 0x3ff0000000000000
one_s:
    .word 0x3f800000
two_s:
    .word 0x40000000
snan_s:
    .word 0x7f800001
