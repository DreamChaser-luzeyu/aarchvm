    .text
    .global _start
_start:
    // Initialize SP in boot code; simulator does not force-reset SP.
    movz x9, #0xf000
    movk x9, #0x07ff, lsl #16
    mov sp, x9
    bl main
    brk #0
