    .text
    .global _start
_start:
    // 64-bit ADDS with LSR
    mov x1, #0x20
    mov x2, #0x18
    adds x3, x1, x2, lsr #1
    mov x9, #0x2c
    cmp x3, x9
    b.ne fail1

    // 64-bit ADD with ASR
    mov x4, #-16
    mov x5, #1
    add x6, x5, x4, asr #2
    mov x9, #-3
    cmp x6, x9
    b.ne fail2

    // 32-bit SUBS with LSR
    mov w7, #0x80
    mov w8, #0x30
    subs w10, w7, w8, lsr #2
    mov w9, #0x74
    cmp w10, w9
    b.ne fail3

    // 32-bit ADD with ASR
    mov w11, #-8
    mov w12, #2
    add w13, w12, w11, asr #1
    mov w9, #-2
    cmp w13, w9
    b.ne fail4

    // 64-bit CMP alias of SUBS (shifted register) with LSR
    mov x14, #0x11
    mov x15, #0x22
    cmp x14, x15, lsr #1
    b.ne fail5

    // 64-bit CMP alias of SUBS (shifted register) with ASR
    mov x16, #-2
    mov x17, #-8
    cmp x16, x17, asr #2
    b.ne fail6

    mov w0, #'Q'
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
