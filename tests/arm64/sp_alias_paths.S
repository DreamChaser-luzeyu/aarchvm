    .text
    .global _start
_start:
    mov x0, #0x1000
    mov sp, x0

    mov x9, #0x1000
    cmp sp, x9
    b.ne fail1

    adds x2, sp, #1
    mov x9, #0x1001
    cmp x2, x9
    b.ne fail2

    subs xzr, sp, #0x1000
    b.ne fail3

    mov w3, #0x20
    add x4, sp, w3, uxtw
    mov x9, #0x1020
    cmp x4, x9
    b.ne fail4

    mov w5, #0x1000
    cmp sp, w5, uxtw
    b.ne fail5

    mov w6, #0x10
    add w7, wsp, w6, uxtw
    mov w9, #0x1010
    cmp w7, w9
    b.ne fail6

    mov w0, #'P'
    b done
fail1:
    mov w0, #'1'
    b done
fail2:
    mov w0, #'2'
    b done
fail3:
    mov w0, #'3'
    b done
fail4:
    mov w0, #'4'
    b done
fail5:
    mov w0, #'5'
    b done
fail6:
    mov w0, #'6'

done:
    ldr x1, =0x09000000
    str w0, [x1]
1:  wfi
    b 1b
