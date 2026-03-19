    .text
    .global _start
_start:
    movz x20, #0
    movk x20, #0x0900, lsl #16
    movz x25, #0x0030, lsl #16
    msr  cpacr_el1, x25
    isb

    adr  x21, qnan_d
    ldr  d0, [x21]
    adr  x21, one_d
    ldr  d1, [x21]
    adr  x21, snan_d
    ldr  d2, [x21]
    adr  x21, qnan_s
    ldr  s3, [x21]
    adr  x21, one_s
    ldr  s4, [x21]

    msr  fpsr, xzr
    fcmp d0, #0.0
    bl   check_unordered
    mrs  x2, fpsr
    cbnz x2, fail1

    msr  fpsr, xzr
    fcmpe d0, #0.0
    bl   check_unordered
    mrs  x2, fpsr
    cmp  x2, #1
    b.ne fail2

    msr  fpsr, xzr
    fcmp d0, d1
    bl   check_unordered
    mrs  x2, fpsr
    cbnz x2, fail3

    msr  fpsr, xzr
    fcmpe d0, d1
    bl   check_unordered
    mrs  x2, fpsr
    cmp  x2, #1
    b.ne fail4

    msr  fpsr, xzr
    fcmp d2, #0.0
    bl   check_unordered
    mrs  x2, fpsr
    cmp  x2, #1
    b.ne fail5

    msr  fpsr, xzr
    fcmp s3, #0.0
    bl   check_unordered
    mrs  x2, fpsr
    cbnz x2, fail6

    msr  fpsr, xzr
    fcmpe s3, s4
    bl   check_unordered
    mrs  x2, fpsr
    cmp  x2, #1
    b.ne fail7

    movz x0, #'Q'
    str  x0, [x20]
    movz x0, #'\n'
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
failu:
    movz x0, #'U'
out:
    str  x0, [x20]
    brk  #0

    .data
    .balign 8
qnan_d:
    .quad 0x7ff8000000000000
snan_d:
    .quad 0x7ff0000000000001
one_d:
    .quad 0x3ff0000000000000
qnan_s:
    .word 0x7fc00000
one_s:
    .word 0x3f800000
