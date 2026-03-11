    .text
    .global _start
_start:
    movz x20, #0
    movk x20, #0x0900, lsl #16

    adr  x0, vectors
    msr  vbar_el1, x0

    mrs  x0, cntvct_el0
    add  x0, x0, #64
    msr  cntv_cval_el0, x0
    mov  x1, #1
    msr  cntv_ctl_el0, x1

    svc  #0

    adr  x0, wake_flag
    ldr  x1, [x0]
    cmp  x1, #1
    b.ne fail1
    adr  x0, irq_flag
    ldr  x1, [x0]
    cmp  x1, #1
    b.ne fail2

    movz x0, #'W'
    str  x0, [x20]
    movz x0, #'\n'
    str  x0, [x20]
    brk  #0

fail1:
    movz x0, #'1'
    str  x0, [x20]
    brk  #0
fail2:
    movz x0, #'2'
    str  x0, [x20]
    brk  #0

    .balign 2048
vectors:
    b .
    .space 0x80 - 4
    b .
    .space 0x80 - 4
    b sync_spx
    .space 0x80 - 4
    b irq_spx
    .space 0x80 - 4
    b .
    .space 0x80 - 4
    b .
    .space 0x80 - 4
    b .
    .space 0x80 - 4
    b .

sync_spx:
    wfi
    adr  x0, wake_flag
    mov  x1, #1
    str  x1, [x0]
    msr  daifclr, #2
    adr  x0, irq_flag
    ldr  x1, [x0]
    cmp  x1, #1
    b.ne fail2_sync
    eret

irq_spx:
    adr  x0, irq_flag
    mov  x1, #1
    str  x1, [x0]
    eret

fail2_sync:
    movz x0, #'2'
    str  x0, [x20]
    brk  #0

    .data
    .balign 8
wake_flag:
    .quad 0
irq_flag:
    .quad 0
