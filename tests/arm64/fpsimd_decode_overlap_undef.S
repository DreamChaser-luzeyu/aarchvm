    .text
    .global _start
_start:
    movz x1, #0x0000
    movk x1, #0x0900, lsl #16
    movz x25, #0x0030, lsl #16
    msr  cpacr_el1, x25
    isb

    adr  x10, vectors
    msr  vbar_el1, x10

    adr  x11, handled
    str  xzr, [x11]

case_dup_vs_ins:
    .inst 0x4e000400
after_dup_vs_ins:
    ldr  x12, [x11]
    movz x13, #1
    cmp  x12, x13
    b.ne fail

case_dup_vs_sqadd:
    .inst 0x0e000c00
after_dup_vs_sqadd:
    ldr  x12, [x11]
    movz x13, #3
    cmp  x12, x13
    b.ne fail

case_umov_invalid_esize:
    .inst 0x0e103c00
after_umov_invalid_esize:
    ldr  x12, [x11]
    movz x13, #7
    cmp  x12, x13
    b.ne fail

case_dup_element_invalid_esize:
    .inst 0x4e100400
after_dup_element_invalid_esize:
    ldr  x12, [x11]
    movz x13, #15
    cmp  x12, x13
    b.ne fail

case_dup_general_invalid_esize:
    .inst 0x0e100c00
after_dup_general_invalid_esize:
    ldr  x12, [x11]
    movz x13, #31
    cmp  x12, x13
    b.ne fail

case_ins_general_invalid_esize:
    .inst 0x4e101c00
after_ins_general_invalid_esize:
    ldr  x12, [x11]
    movz x13, #63
    cmp  x12, x13
    b.ne fail

case_sqxtn_decode_overlap_undef:
    .inst 0x0e004800
after_sqxtn_decode_overlap_undef:
    ldr  x12, [x11]
    movz x13, #127
    cmp  x12, x13
    b.ne fail

case_uqxtn_decode_overlap_undef:
    .inst 0x2e004800
after_uqxtn_decode_overlap_undef:
    ldr  x12, [x11]
    movz x13, #255
    cmp  x12, x13
    b.ne fail

case_luti_decode_overlap_undef:
    .inst 0x4e401000
after_luti_decode_overlap_undef:
    ldr  x12, [x11]
    movz x13, #511
    cmp  x12, x13
    b.ne fail

case_ext_decode_overlap_undef:
    .inst 0x2e400000
after_ext_decode_overlap_undef:
    ldr  x12, [x11]
    movz x13, #1023
    cmp  x12, x13
    b.ne fail

case_cnt_decode_overlap_undef:
    .inst 0x0e605800
after_cnt_decode_overlap_undef:
    ldr  x12, [x11]
    movz x13, #2047
    cmp  x12, x13
    b.ne fail

case_rev16_decode_overlap_undef:
    .inst 0x0e211800
after_rev16_decode_overlap_undef:
    ldr  x12, [x11]
    movz x13, #4095
    cmp  x12, x13
    b.ne fail

case_rev32_decode_overlap_undef:
    .inst 0x0e210800
after_rev32_decode_overlap_undef:
    ldr  x12, [x11]
    movz x13, #8191
    cmp  x12, x13
    b.ne fail

case_cls_decode_overlap_undef:
    .inst 0x0e224800
after_cls_decode_overlap_undef:
    ldr  x12, [x11]
    movz x13, #16383
    cmp  x12, x13
    b.ne fail

case_addv_reserved_decode_overlap_undef:
    .inst 0x0eb1b820
after_addv_reserved_decode_overlap_undef:
    ldr  x12, [x11]
    movz x13, #0x7fff
    cmp  x12, x13
    b.ne fail

case_sadalp_reserved_decode_overlap_undef:
    .inst 0x0ee068a4
after_sadalp_reserved_decode_overlap_undef:
    ldr  x12, [x11]
    movz x13, #0xffff
    cmp  x12, x13
    b.ne fail

case_pmul_absent_decode_overlap_undef:
    .inst 0x2e229c20
after_pmul_absent_decode_overlap_undef:
    ldr  x12, [x11]
    movz x13, #0xffff
    movk x13, #0x1, lsl #16
    cmp  x12, x13
    b.ne fail

case_pmull_absent_decode_overlap_undef:
    .inst 0x0e22e020
after_pmull_absent_decode_overlap_undef:
    ldr  x12, [x11]
    movz x13, #0xffff
    movk x13, #0x3, lsl #16
    cmp  x12, x13
    b.ne fail

case_mul_by_element_invalid_size_decode_overlap_undef:
    .inst 0x0f328020
after_mul_by_element_invalid_size_decode_overlap_undef:
    ldr  x12, [x11]
    movz x13, #0xffff
    movk x13, #0x7, lsl #16
    cmp  x12, x13
    b.ne fail

case_mla_vector_invalid_size_decode_overlap_undef:
    .inst 0x0ee29420
after_mla_vector_invalid_size_decode_overlap_undef:
    ldr  x12, [x11]
    movz x13, #0xffff
    movk x13, #0xf, lsl #16
    cmp  x12, x13
    b.ne fail

case_fcadd_absent_decode_overlap_undef:
    .inst 0x6e82e420
after_fcadd_absent_decode_overlap_undef:
    ldr  x12, [x11]
    movz x13, #0xffff
    movk x13, #0x1f, lsl #16
    cmp  x12, x13
    b.ne fail

case_fcmla_absent_decode_overlap_undef:
    .inst 0x6e82cc20
after_fcmla_absent_decode_overlap_undef:
    ldr  x12, [x11]
    movz x13, #0xffff
    movk x13, #0x3f, lsl #16
    cmp  x12, x13
    b.ne fail

case_sqrdmlah_absent_decode_overlap_undef:
    .inst 0x6e828420
after_sqrdmlah_absent_decode_overlap_undef:
    ldr  x12, [x11]
    movz x13, #0xffff
    movk x13, #0x7f, lsl #16
    cmp  x12, x13
    b.ne fail

case_sqrdmlsh_absent_decode_overlap_undef:
    .inst 0x6e828c20
after_sqrdmlsh_absent_decode_overlap_undef:
    ldr  x12, [x11]
    movz x13, #0xffff
    movk x13, #0xff, lsl #16
    cmp  x12, x13
    b.ne fail

case_udot_absent_decode_overlap_undef:
    .inst 0x6e829420
after_udot_absent_decode_overlap_undef:
    ldr  x12, [x11]
    movz x13, #0xffff
    movk x13, #0x1ff, lsl #16
    cmp  x12, x13
    b.ne fail

case_sdot_absent_decode_overlap_undef:
    .inst 0x4e829420
after_sdot_absent_decode_overlap_undef:
    ldr  x12, [x11]
    movz x13, #0xffff
    movk x13, #0x3ff, lsl #16
    cmp  x12, x13
    b.ne fail

case_aese_absent_decode_overlap_undef:
    .inst 0x4e284820
after_aese_absent_decode_overlap_undef:
    ldr  x12, [x11]
    movz x13, #0xffff
    movk x13, #0x7ff, lsl #16
    cmp  x12, x13
    b.ne fail

case_aesd_absent_decode_overlap_undef:
    .inst 0x4e285820
after_aesd_absent_decode_overlap_undef:
    ldr  x12, [x11]
    movz x13, #0xffff
    movk x13, #0xfff, lsl #16
    cmp  x12, x13
    b.ne fail

case_aesmc_absent_decode_overlap_undef:
    .inst 0x4e286820
after_aesmc_absent_decode_overlap_undef:
    ldr  x12, [x11]
    movz x13, #0xffff
    movk x13, #0x1fff, lsl #16
    cmp  x12, x13
    b.ne fail

case_aesimc_absent_decode_overlap_undef:
    .inst 0x4e287820
after_aesimc_absent_decode_overlap_undef:
    ldr  x12, [x11]
    movz x13, #0xffff
    movk x13, #0x3fff, lsl #16
    cmp  x12, x13
    b.ne fail

case_sha1c_absent_decode_overlap_undef:
    .inst 0x5e020020
after_sha1c_absent_decode_overlap_undef:
    ldr  x12, [x11]
    movz x13, #0xffff
    movk x13, #0x7fff, lsl #16
    cmp  x12, x13
    b.ne fail

case_sha256h_absent_decode_overlap_undef:
    .inst 0x5e024020
after_sha256h_absent_decode_overlap_undef:
    ldr  x12, [x11]
    movz x13, #0xffff
    movk x13, #0xffff, lsl #16
    cmp  x12, x13
    b.ne fail

case_sha1h_absent_decode_overlap_undef:
    .inst 0x5e280820
after_sha1h_absent_decode_overlap_undef:
    ldr  x12, [x11]
    movz x13, #0xffff
    movk x13, #0xffff, lsl #16
    movk x13, #0x1, lsl #32
    cmp  x12, x13
    b.ne fail

case_sha1su1_absent_decode_overlap_undef:
    .inst 0x5e281820
after_sha1su1_absent_decode_overlap_undef:
    ldr  x12, [x11]
    movz x13, #0xffff
    movk x13, #0xffff, lsl #16
    movk x13, #0x3, lsl #32
    cmp  x12, x13
    b.ne fail

case_sha1p_absent_decode_overlap_undef:
    .inst 0x5e021020
after_sha1p_absent_decode_overlap_undef:
    ldr  x12, [x11]
    movz x13, #0xffff
    movk x13, #0xffff, lsl #16
    movk x13, #0x7, lsl #32
    cmp  x12, x13
    b.ne fail

case_sha1m_absent_decode_overlap_undef:
    .inst 0x5e022020
after_sha1m_absent_decode_overlap_undef:
    ldr  x12, [x11]
    movz x13, #0xffff
    movk x13, #0xffff, lsl #16
    movk x13, #0xf, lsl #32
    cmp  x12, x13
    b.ne fail

case_sha1su0_absent_decode_overlap_undef:
    .inst 0x5e023020
after_sha1su0_absent_decode_overlap_undef:
    ldr  x12, [x11]
    movz x13, #0xffff
    movk x13, #0xffff, lsl #16
    movk x13, #0x1f, lsl #32
    cmp  x12, x13
    b.ne fail

case_sha256h2_absent_decode_overlap_undef:
    .inst 0x5e025020
after_sha256h2_absent_decode_overlap_undef:
    ldr  x12, [x11]
    movz x13, #0xffff
    movk x13, #0xffff, lsl #16
    movk x13, #0x3f, lsl #32
    cmp  x12, x13
    b.ne fail

case_sha256su0_absent_decode_overlap_undef:
    .inst 0x5e282820
after_sha256su0_absent_decode_overlap_undef:
    ldr  x12, [x11]
    movz x13, #0xffff
    movk x13, #0xffff, lsl #16
    movk x13, #0x7f, lsl #32
    cmp  x12, x13
    b.ne fail

case_sha256su1_absent_decode_overlap_undef:
    .inst 0x5e026020
after_sha256su1_absent_decode_overlap_undef:
    ldr  x12, [x11]
    movz x13, #0xffff
    movk x13, #0xffff, lsl #16
    movk x13, #0xff, lsl #32
    cmp  x12, x13
    b.ne fail

    movz x0, #'U'
    str  x0, [x1]
    movz x0, #'\n'
    str  x0, [x1]
    brk  #0

fail:
    movz x0, #'F'
    str  x0, [x1]
    brk  #0

    .balign 2048
vectors:
    .space 0x200
sync_spx:
    b sync_handler
    .space 0x1fc
sync_lower:
    b sync_handler

sync_handler:
    mrs  x20, esr_el1
    ubfx x21, x20, #26, #6
    cbnz x21, fail_from_handler
    tbz  x20, #25, fail_from_handler
    and  x22, x20, #0x1ffffff
    cbnz x22, fail_from_handler

    mrs  x23, far_el1
    cbnz x23, fail_from_handler
    mrs  x24, elr_el1

    adr  x25, case_dup_vs_ins
    cmp  x24, x25
    b.eq handle_dup_vs_ins
    adr  x25, case_dup_vs_sqadd
    cmp  x24, x25
    b.eq handle_dup_vs_sqadd
    adr  x25, case_umov_invalid_esize
    cmp  x24, x25
    b.eq handle_umov_invalid_esize
    adr  x25, case_dup_element_invalid_esize
    cmp  x24, x25
    b.eq handle_dup_element_invalid_esize
    adr  x25, case_dup_general_invalid_esize
    cmp  x24, x25
    b.eq handle_dup_general_invalid_esize
    adr  x25, case_ins_general_invalid_esize
    cmp  x24, x25
    b.eq handle_ins_general_invalid_esize
    adr  x25, case_sqxtn_decode_overlap_undef
    cmp  x24, x25
    b.eq handle_sqxtn_decode_overlap_undef
    adr  x25, case_uqxtn_decode_overlap_undef
    cmp  x24, x25
    b.eq handle_uqxtn_decode_overlap_undef
    adr  x25, case_luti_decode_overlap_undef
    cmp  x24, x25
    b.eq handle_luti_decode_overlap_undef
    adr  x25, case_ext_decode_overlap_undef
    cmp  x24, x25
    b.eq handle_ext_decode_overlap_undef
    adr  x25, case_cnt_decode_overlap_undef
    cmp  x24, x25
    b.eq handle_cnt_decode_overlap_undef
    adr  x25, case_rev16_decode_overlap_undef
    cmp  x24, x25
    b.eq handle_rev16_decode_overlap_undef
    adr  x25, case_rev32_decode_overlap_undef
    cmp  x24, x25
    b.eq handle_rev32_decode_overlap_undef
    adr  x25, case_cls_decode_overlap_undef
    cmp  x24, x25
    b.eq handle_cls_decode_overlap_undef
    adr  x25, case_addv_reserved_decode_overlap_undef
    cmp  x24, x25
    b.eq handle_addv_reserved_decode_overlap_undef
    adr  x25, case_sadalp_reserved_decode_overlap_undef
    cmp  x24, x25
    b.eq handle_sadalp_reserved_decode_overlap_undef
    adr  x25, case_pmul_absent_decode_overlap_undef
    cmp  x24, x25
    b.eq handle_pmul_absent_decode_overlap_undef
    adr  x25, case_pmull_absent_decode_overlap_undef
    cmp  x24, x25
    b.eq handle_pmull_absent_decode_overlap_undef
    adr  x25, case_mul_by_element_invalid_size_decode_overlap_undef
    cmp  x24, x25
    b.eq handle_mul_by_element_invalid_size_decode_overlap_undef
    adr  x25, case_mla_vector_invalid_size_decode_overlap_undef
    cmp  x24, x25
    b.eq handle_mla_vector_invalid_size_decode_overlap_undef
    adr  x25, case_fcadd_absent_decode_overlap_undef
    cmp  x24, x25
    b.eq handle_fcadd_absent_decode_overlap_undef
    adr  x25, case_fcmla_absent_decode_overlap_undef
    cmp  x24, x25
    b.eq handle_fcmla_absent_decode_overlap_undef
    adr  x25, case_sqrdmlah_absent_decode_overlap_undef
    cmp  x24, x25
    b.eq handle_sqrdmlah_absent_decode_overlap_undef
    adr  x25, case_sqrdmlsh_absent_decode_overlap_undef
    cmp  x24, x25
    b.eq handle_sqrdmlsh_absent_decode_overlap_undef
    adr  x25, case_udot_absent_decode_overlap_undef
    cmp  x24, x25
    b.eq handle_udot_absent_decode_overlap_undef
    adr  x25, case_sdot_absent_decode_overlap_undef
    cmp  x24, x25
    b.eq handle_sdot_absent_decode_overlap_undef
    adr  x25, case_aese_absent_decode_overlap_undef
    cmp  x24, x25
    b.eq handle_aese_absent_decode_overlap_undef
    adr  x25, case_aesd_absent_decode_overlap_undef
    cmp  x24, x25
    b.eq handle_aesd_absent_decode_overlap_undef
    adr  x25, case_aesmc_absent_decode_overlap_undef
    cmp  x24, x25
    b.eq handle_aesmc_absent_decode_overlap_undef
    adr  x25, case_aesimc_absent_decode_overlap_undef
    cmp  x24, x25
    b.eq handle_aesimc_absent_decode_overlap_undef
    adr  x25, case_sha1c_absent_decode_overlap_undef
    cmp  x24, x25
    b.eq handle_sha1c_absent_decode_overlap_undef
    adr  x25, case_sha256h_absent_decode_overlap_undef
    cmp  x24, x25
    b.eq handle_sha256h_absent_decode_overlap_undef
    adr  x25, case_sha1h_absent_decode_overlap_undef
    cmp  x24, x25
    b.eq handle_sha1h_absent_decode_overlap_undef
    adr  x25, case_sha1su1_absent_decode_overlap_undef
    cmp  x24, x25
    b.eq handle_sha1su1_absent_decode_overlap_undef
    adr  x25, case_sha1p_absent_decode_overlap_undef
    cmp  x24, x25
    b.eq handle_sha1p_absent_decode_overlap_undef
    adr  x25, case_sha1m_absent_decode_overlap_undef
    cmp  x24, x25
    b.eq handle_sha1m_absent_decode_overlap_undef
    adr  x25, case_sha1su0_absent_decode_overlap_undef
    cmp  x24, x25
    b.eq handle_sha1su0_absent_decode_overlap_undef
    adr  x25, case_sha256h2_absent_decode_overlap_undef
    cmp  x24, x25
    b.eq handle_sha256h2_absent_decode_overlap_undef
    adr  x25, case_sha256su0_absent_decode_overlap_undef
    cmp  x24, x25
    b.eq handle_sha256su0_absent_decode_overlap_undef
    adr  x25, case_sha256su1_absent_decode_overlap_undef
    cmp  x24, x25
    b.eq handle_sha256su1_absent_decode_overlap_undef
    b fail_from_handler

handle_dup_vs_ins:
    adr  x26, handled
    movz x27, #1
    str  x27, [x26]
    adr  x28, after_dup_vs_ins
    msr  elr_el1, x28
    eret

handle_dup_vs_sqadd:
    adr  x26, handled
    ldr  x27, [x26]
    orr  x27, x27, #2
    str  x27, [x26]
    adr  x28, after_dup_vs_sqadd
    msr  elr_el1, x28
    eret

handle_umov_invalid_esize:
    adr  x26, handled
    ldr  x27, [x26]
    orr  x27, x27, #4
    str  x27, [x26]
    adr  x28, after_umov_invalid_esize
    msr  elr_el1, x28
    eret

handle_dup_element_invalid_esize:
    adr  x26, handled
    ldr  x27, [x26]
    orr  x27, x27, #8
    str  x27, [x26]
    adr  x28, after_dup_element_invalid_esize
    msr  elr_el1, x28
    eret

handle_dup_general_invalid_esize:
    adr  x26, handled
    ldr  x27, [x26]
    orr  x27, x27, #16
    str  x27, [x26]
    adr  x28, after_dup_general_invalid_esize
    msr  elr_el1, x28
    eret

handle_ins_general_invalid_esize:
    adr  x26, handled
    ldr  x27, [x26]
    orr  x27, x27, #32
    str  x27, [x26]
    adr  x28, after_ins_general_invalid_esize
    msr  elr_el1, x28
    eret

handle_sqxtn_decode_overlap_undef:
    adr  x26, handled
    ldr  x27, [x26]
    orr  x27, x27, #64
    str  x27, [x26]
    adr  x28, after_sqxtn_decode_overlap_undef
    msr  elr_el1, x28
    eret

handle_uqxtn_decode_overlap_undef:
    adr  x26, handled
    ldr  x27, [x26]
    orr  x27, x27, #128
    str  x27, [x26]
    adr  x28, after_uqxtn_decode_overlap_undef
    msr  elr_el1, x28
    eret

handle_luti_decode_overlap_undef:
    adr  x26, handled
    ldr  x27, [x26]
    movz x28, #256
    orr  x27, x27, x28
    str  x27, [x26]
    adr  x28, after_luti_decode_overlap_undef
    msr  elr_el1, x28
    eret

handle_ext_decode_overlap_undef:
    adr  x26, handled
    ldr  x27, [x26]
    movz x28, #512
    orr  x27, x27, x28
    str  x27, [x26]
    adr  x28, after_ext_decode_overlap_undef
    msr  elr_el1, x28
    eret

handle_cnt_decode_overlap_undef:
    adr  x26, handled
    ldr  x27, [x26]
    movz x28, #1024
    orr  x27, x27, x28
    str  x27, [x26]
    adr  x28, after_cnt_decode_overlap_undef
    msr  elr_el1, x28
    eret

handle_rev16_decode_overlap_undef:
    adr  x26, handled
    ldr  x27, [x26]
    movz x28, #2048
    orr  x27, x27, x28
    str  x27, [x26]
    adr  x28, after_rev16_decode_overlap_undef
    msr  elr_el1, x28
    eret

handle_rev32_decode_overlap_undef:
    adr  x26, handled
    ldr  x27, [x26]
    movz x28, #4096
    orr  x27, x27, x28
    str  x27, [x26]
    adr  x28, after_rev32_decode_overlap_undef
    msr  elr_el1, x28
    eret

handle_cls_decode_overlap_undef:
    adr  x26, handled
    ldr  x27, [x26]
    movz x28, #8192
    orr  x27, x27, x28
    str  x27, [x26]
    adr  x28, after_cls_decode_overlap_undef
    msr  elr_el1, x28
    eret

handle_addv_reserved_decode_overlap_undef:
    adr  x26, handled
    ldr  x27, [x26]
    movz x28, #16384
    orr  x27, x27, x28
    str  x27, [x26]
    adr  x28, after_addv_reserved_decode_overlap_undef
    msr  elr_el1, x28
    eret

handle_sadalp_reserved_decode_overlap_undef:
    adr  x26, handled
    ldr  x27, [x26]
    movz x28, #32768
    orr  x27, x27, x28
    str  x27, [x26]
    adr  x28, after_sadalp_reserved_decode_overlap_undef
    msr  elr_el1, x28
    eret

handle_pmul_absent_decode_overlap_undef:
    adr  x26, handled
    ldr  x27, [x26]
    movz x28, #1, lsl #16
    orr  x27, x27, x28
    str  x27, [x26]
    adr  x28, after_pmul_absent_decode_overlap_undef
    msr  elr_el1, x28
    eret

handle_pmull_absent_decode_overlap_undef:
    adr  x26, handled
    ldr  x27, [x26]
    movz x28, #2, lsl #16
    orr  x27, x27, x28
    str  x27, [x26]
    adr  x28, after_pmull_absent_decode_overlap_undef
    msr  elr_el1, x28
    eret

handle_mul_by_element_invalid_size_decode_overlap_undef:
    adr  x26, handled
    ldr  x27, [x26]
    movz x28, #4, lsl #16
    orr  x27, x27, x28
    str  x27, [x26]
    adr  x28, after_mul_by_element_invalid_size_decode_overlap_undef
    msr  elr_el1, x28
    eret

handle_mla_vector_invalid_size_decode_overlap_undef:
    adr  x26, handled
    ldr  x27, [x26]
    movz x28, #8, lsl #16
    orr  x27, x27, x28
    str  x27, [x26]
    adr  x28, after_mla_vector_invalid_size_decode_overlap_undef
    msr  elr_el1, x28
    eret

handle_fcadd_absent_decode_overlap_undef:
    adr  x26, handled
    ldr  x27, [x26]
    movz x28, #0x10, lsl #16
    orr  x27, x27, x28
    str  x27, [x26]
    adr  x28, after_fcadd_absent_decode_overlap_undef
    msr  elr_el1, x28
    eret

handle_fcmla_absent_decode_overlap_undef:
    adr  x26, handled
    ldr  x27, [x26]
    movz x28, #0x20, lsl #16
    orr  x27, x27, x28
    str  x27, [x26]
    adr  x28, after_fcmla_absent_decode_overlap_undef
    msr  elr_el1, x28
    eret

handle_sqrdmlah_absent_decode_overlap_undef:
    adr  x26, handled
    ldr  x27, [x26]
    movz x28, #0x40, lsl #16
    orr  x27, x27, x28
    str  x27, [x26]
    adr  x28, after_sqrdmlah_absent_decode_overlap_undef
    msr  elr_el1, x28
    eret

handle_sqrdmlsh_absent_decode_overlap_undef:
    adr  x26, handled
    ldr  x27, [x26]
    movz x28, #0x80, lsl #16
    orr  x27, x27, x28
    str  x27, [x26]
    adr  x28, after_sqrdmlsh_absent_decode_overlap_undef
    msr  elr_el1, x28
    eret

handle_udot_absent_decode_overlap_undef:
    adr  x26, handled
    ldr  x27, [x26]
    movz x28, #0x100, lsl #16
    orr  x27, x27, x28
    str  x27, [x26]
    adr  x28, after_udot_absent_decode_overlap_undef
    msr  elr_el1, x28
    eret

handle_sdot_absent_decode_overlap_undef:
    adr  x26, handled
    ldr  x27, [x26]
    movz x28, #0x200, lsl #16
    orr  x27, x27, x28
    str  x27, [x26]
    adr  x28, after_sdot_absent_decode_overlap_undef
    msr  elr_el1, x28
    eret

handle_aese_absent_decode_overlap_undef:
    adr  x26, handled
    ldr  x27, [x26]
    movz x28, #0x400, lsl #16
    orr  x27, x27, x28
    str  x27, [x26]
    adr  x28, after_aese_absent_decode_overlap_undef
    msr  elr_el1, x28
    eret

handle_aesd_absent_decode_overlap_undef:
    adr  x26, handled
    ldr  x27, [x26]
    movz x28, #0x800, lsl #16
    orr  x27, x27, x28
    str  x27, [x26]
    adr  x28, after_aesd_absent_decode_overlap_undef
    msr  elr_el1, x28
    eret

handle_aesmc_absent_decode_overlap_undef:
    adr  x26, handled
    ldr  x27, [x26]
    movz x28, #0x1000, lsl #16
    orr  x27, x27, x28
    str  x27, [x26]
    adr  x28, after_aesmc_absent_decode_overlap_undef
    msr  elr_el1, x28
    eret

handle_aesimc_absent_decode_overlap_undef:
    adr  x26, handled
    ldr  x27, [x26]
    movz x28, #0x2000, lsl #16
    orr  x27, x27, x28
    str  x27, [x26]
    adr  x28, after_aesimc_absent_decode_overlap_undef
    msr  elr_el1, x28
    eret

handle_sha1c_absent_decode_overlap_undef:
    adr  x26, handled
    ldr  x27, [x26]
    movz x28, #0x4000, lsl #16
    orr  x27, x27, x28
    str  x27, [x26]
    adr  x28, after_sha1c_absent_decode_overlap_undef
    msr  elr_el1, x28
    eret

handle_sha256h_absent_decode_overlap_undef:
    adr  x26, handled
    ldr  x27, [x26]
    movz x28, #0x8000, lsl #16
    orr  x27, x27, x28
    str  x27, [x26]
    adr  x28, after_sha256h_absent_decode_overlap_undef
    msr  elr_el1, x28
    eret

handle_sha1h_absent_decode_overlap_undef:
    adr  x26, handled
    ldr  x27, [x26]
    movz x28, #0x1, lsl #32
    orr  x27, x27, x28
    str  x27, [x26]
    adr  x28, after_sha1h_absent_decode_overlap_undef
    msr  elr_el1, x28
    eret

handle_sha1su1_absent_decode_overlap_undef:
    adr  x26, handled
    ldr  x27, [x26]
    movz x28, #0x2, lsl #32
    orr  x27, x27, x28
    str  x27, [x26]
    adr  x28, after_sha1su1_absent_decode_overlap_undef
    msr  elr_el1, x28
    eret

handle_sha1p_absent_decode_overlap_undef:
    adr  x26, handled
    ldr  x27, [x26]
    movz x28, #0x4, lsl #32
    orr  x27, x27, x28
    str  x27, [x26]
    adr  x28, after_sha1p_absent_decode_overlap_undef
    msr  elr_el1, x28
    eret

handle_sha1m_absent_decode_overlap_undef:
    adr  x26, handled
    ldr  x27, [x26]
    movz x28, #0x8, lsl #32
    orr  x27, x27, x28
    str  x27, [x26]
    adr  x28, after_sha1m_absent_decode_overlap_undef
    msr  elr_el1, x28
    eret

handle_sha1su0_absent_decode_overlap_undef:
    adr  x26, handled
    ldr  x27, [x26]
    movz x28, #0x10, lsl #32
    orr  x27, x27, x28
    str  x27, [x26]
    adr  x28, after_sha1su0_absent_decode_overlap_undef
    msr  elr_el1, x28
    eret

handle_sha256h2_absent_decode_overlap_undef:
    adr  x26, handled
    ldr  x27, [x26]
    movz x28, #0x20, lsl #32
    orr  x27, x27, x28
    str  x27, [x26]
    adr  x28, after_sha256h2_absent_decode_overlap_undef
    msr  elr_el1, x28
    eret

handle_sha256su0_absent_decode_overlap_undef:
    adr  x26, handled
    ldr  x27, [x26]
    movz x28, #0x40, lsl #32
    orr  x27, x27, x28
    str  x27, [x26]
    adr  x28, after_sha256su0_absent_decode_overlap_undef
    msr  elr_el1, x28
    eret

handle_sha256su1_absent_decode_overlap_undef:
    adr  x26, handled
    ldr  x27, [x26]
    movz x28, #0x80, lsl #32
    orr  x27, x27, x28
    str  x27, [x26]
    adr  x28, after_sha256su1_absent_decode_overlap_undef
    msr  elr_el1, x28
    eret

fail_from_handler:
    adr  x28, fail
    msr  elr_el1, x28
    eret

    .data
    .balign 8
handled:
    .quad 0
