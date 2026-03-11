    .text
    .global _start
_start:
    movz x20, #0
    movk x20, #0x0900, lsl #16
    adr  x21, src
    ld1  {v1.16b-v2.16b}, [x21]
    uminp v0.16b, v1.16b, v2.16b
    adr  x22, dst
    str  q0, [x22]
    adr  x23, exp
    ldr  x0, [x22]
    ldr  x1, [x23]
    cmp  x0, x1
    b.ne fail
    ldr  x0, [x22, #8]
    ldr  x1, [x23, #8]
    cmp  x0, x1
    b.ne fail
    movz x0, #'W'
    str  x0, [x20]
    movz x0, #'
'
    str  x0, [x20]
    brk  #0
fail:
    movz x0, #'F'
    str  x0, [x20]
    brk  #0
    .data
    .balign 16
src:
    .byte 8,1,7,2,6,3,5,4,16,9,15,10,14,11,13,12
    .byte 4,8,3,7,2,6,1,5,12,16,11,15,10,14,9,13
dst:
    .space 16
exp:
    .byte 1,2,3,4,9,10,11,12,4,3,2,1,12,11,10,9
