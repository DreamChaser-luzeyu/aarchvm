    .text
    .global _start
_start:
    ldr x0, =0x47fff000
    mov sp, x0

    ldr x2, =0x0000000041770000
    mov x4, sp
    cmp x2, #0
    ccmp x4, x2, #0, ne
    b.cs 1f
    mov x0, #1
    b done
1:
    add x2, x2, #0x4000
    adds x4, sp, #1
    ccmp x2, x4, #0, cc
    b.cc 2f
    mov x0, #2
    b done
2:
    mov x0, #3

done:
    ldr x1, =0x09000000
    add x0, x0, #'0'
    str w0, [x1]
1:  wfi
    b 1b
