    .text
    .global _start
_start:
    movz x20, #0
    movk x20, #0x0900, lsl #16
    adr  x21, src
    ld1  {v0.16b-v1.16b}, [x21]
    ext  v2.16b, v0.16b, v1.16b, #8
    adr  x22, dst
    str  q2, [x22]
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
    .byte 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15
    .byte 16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31
dst:
    .space 16
exp:
    .byte 8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23
