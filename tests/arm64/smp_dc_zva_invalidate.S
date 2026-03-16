    .text
    .global _start
_start:
    movz x20, #0x0000
    movk x20, #0x0900, lsl #16      // UART
    adr  x21, shared
    add  x22, x21, #64              // stage0
    add  x23, x21, #72              // stage1
    add  x24, x21, #80              // fail char

    mrs  x0, mpidr_el1
    and  x0, x0, #0xff
    cbz  x0, core0
    cmp  x0, #1
    b.ne secondary_fail

secondary_wait:
    ldar x1, [x22]
    cbnz x1, secondary_zva
    wfe
    b    secondary_wait

secondary_zva:
    add  x2, x21, #24               // inside same 64B cache line
    dc   zva, x2
    movz x3, #1
    stlr x3, [x23]
    sev
secondary_exit:
    brk  #0

core0:
    movz w1, #0x12
    str  w1, [x21]
    ldxr w2, [x21]
    cmp  w2, w1
    b.ne primary_fail_a
    movz x3, #1
    stlr x3, [x22]
    sev

core0_wait:
    ldar x4, [x23]
    cbnz x4, core0_try_stxr
    wfe
    b    core0_wait

core0_try_stxr:
    movz w5, #0x34
    stxr w6, w5, [x21]
    cbz  w6, primary_fail_b         // must fail: other core zeroed line
    ldr  w7, [x21]
    cbnz w7, primary_fail_c         // line should have been zeroed by ZVA
    movz x0, #'D'
    b    print_and_halt

secondary_fail:
    movz x1, #'x'
    stlr x1, [x24]
    sev
    b    secondary_exit

primary_fail_a:
    movz x0, #'A'
    b    print_and_halt
primary_fail_b:
    movz x0, #'B'
    b    print_and_halt
primary_fail_c:
    movz x0, #'C'

print_and_halt:
    str  x0, [x20]
    movz x0, #'\n'
    str  x0, [x20]
    brk  #0

    .data
    .balign 64
shared:
    .space 64                       // zva block, word0 is monitored word
    .quad 0                         // 64: stage0
    .quad 0                         // 72: stage1
    .quad 0                         // 80: fail char
