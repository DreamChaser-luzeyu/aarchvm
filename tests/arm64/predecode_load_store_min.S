    .text
    .global _start
_start:
    ldr x1, =0x09000000
    ldr x20, =scratch
    add x27, x20, #0x80
    mov sp, x27

    // Unsigned-offset X load/store with SP base.
    movz x2, #0x7788
    movk x2, #0x5566, lsl #16
    movk x2, #0x3344, lsl #32
    movk x2, #0x1122, lsl #48
    str  x2, [sp, #16]
    ldr  x3, [sp, #16]
    cmp  x3, x2
    b.ne fail_x_sp

    // Unsigned-offset W load/store.
    movz w4, #0xCDEF
    movk w4, #0x89AB, lsl #16
    str  w4, [x20, #8]
    ldr  w5, [x20, #8]
    cmp  w5, w4
    b.ne fail_w

    // Unsigned-offset byte forms.
    mov  w6, #0x80
    strb w6, [x20, #1]
    ldrb w7, [x20, #1]
    cmp  w7, #0x80
    b.ne fail_b
    ldrsb x8, [x20, #1]
    movn x9, #0x7f
    cmp  x8, x9
    b.ne fail_ldrsbx
    ldrsb w10, [x20, #1]
    movn w11, #0x7f
    cmp  w10, w11
    b.ne fail_ldrsbw

    // Unsigned-offset halfword forms.
    movz w12, #0x8001
    strh w12, [x20, #2]
    ldrh w13, [x20, #2]
    cmp  w13, w12
    b.ne fail_h
    ldrsh x14, [x20, #2]
    movn x15, #0x7ffe
    cmp  x14, x15
    b.ne fail_ldrshx
    ldrsh w16, [x20, #2]
    movn w17, #0x7ffe
    cmp  w16, w17
    b.ne fail_ldrshw

    // Unsigned-offset sign-extending word load.
    movz w18, #0x0001
    movk w18, #0x8000, lsl #16
    str  w18, [x20, #4]
    ldr  w19, [x20, #4]
    cmp  w19, w18
    b.ne fail_w_reload
    ldrsw x21, [x20, #4]
    sxtw x22, w18
    cmp  x21, x22
    b.ne fail_ldrsw

    // Post-index byte store/load.
    add  x23, x20, #0x20
    mov  w24, #0x55
    strb w24, [x23], #1
    add  x25, x20, #0x21
    cmp  x23, x25
    b.ne fail_post_store_wb
    sub  x23, x23, #1
    ldrb w24, [x23], #1
    cmp  w24, #0x55
    b.ne fail_post_load_val
    cmp  x23, x25
    b.ne fail_post_load_wb

    // Pre-index word store/load.
    add  x23, x20, #0x3c
    movz w24, #0x5678
    movk w24, #0x1234, lsl #16
    str  w24, [x23, #4]!
    add  x25, x20, #0x40
    cmp  x23, x25
    b.ne fail_pre_store_wb
    ldr  w26, [x23]
    cmp  w26, w24
    b.ne fail_pre_store_val
    sub  x23, x23, #4
    ldr  w26, [x23, #4]!
    cmp  x23, x25
    b.ne fail_pre_load_wb
    cmp  w26, w24
    b.ne fail_pre_load_val

pass:
    mov  w0, #'L'
    str  w0, [x1]
    mov  w0, #'\n'
    str  w0, [x1]
    brk  #0

fail_x_sp:
    mov w0, #'1'
    b report_fail
fail_w:
    mov w0, #'2'
    b report_fail
fail_b:
    mov w0, #'3'
    b report_fail
fail_ldrsbx:
    mov w0, #'4'
    b report_fail
fail_ldrsbw:
    mov w0, #'5'
    b report_fail
fail_h:
    mov w0, #'6'
    b report_fail
fail_ldrshx:
    mov w0, #'7'
    b report_fail
fail_ldrshw:
    mov w0, #'8'
    b report_fail
fail_w_reload:
    mov w0, #'9'
    b report_fail
fail_ldrsw:
    mov w0, #'A'
    b report_fail
fail_post_store_wb:
    mov w0, #'B'
    b report_fail
fail_post_load_val:
    mov w0, #'C'
    b report_fail
fail_post_load_wb:
    mov w0, #'D'
    b report_fail
fail_pre_store_wb:
    mov w0, #'E'
    b report_fail
fail_pre_store_val:
    mov w0, #'F'
    b report_fail
fail_pre_load_wb:
    mov w0, #'G'
    b report_fail
fail_pre_load_val:
    mov w0, #'H'

report_fail:
    str  w0, [x1]
    mov  w0, #'\n'
    str  w0, [x1]
    brk  #0

    .data
    .balign 16
scratch:
    .space 256
