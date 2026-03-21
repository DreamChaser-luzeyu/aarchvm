#!/usr/bin/env bash
set -euo pipefail

OUT_DIR="${1:-tests/arm64/out}"
mkdir -p "$OUT_DIR"

AARCH64_AS="aarch64-linux-gnu-as"
AARCH64_LD="aarch64-linux-gnu-ld"
AARCH64_OBJCOPY="aarch64-linux-gnu-objcopy"
AARCH64_OBJDUMP="aarch64-linux-gnu-objdump"

build_asm_prog() {
  local name="$1"
  "$AARCH64_AS" "tests/arm64/${name}.S" -o "$OUT_DIR/${name}.o"
  "$AARCH64_LD" -T tests/arm64/link.ld -o "$OUT_DIR/${name}.elf" "$OUT_DIR/${name}.o"
  "$AARCH64_OBJCOPY" -O binary "$OUT_DIR/${name}.elf" "$OUT_DIR/${name}.bin"
  "$AARCH64_OBJDUMP" -d "$OUT_DIR/${name}.elf" > "$OUT_DIR/${name}.dis"
}

build_asm_prog hello_uart
build_asm_prog branch_arith
build_asm_prog irq_minimal
build_asm_prog irq_twice
build_asm_prog irq_disabled
build_asm_prog logic_misc
build_asm_prog mem_ext
build_asm_prog branch_reg
build_asm_prog sys_ctrl
build_asm_prog cntkctl_el1
build_asm_prog cntkctl_el0_timer_access
build_asm_prog ldtr_sttr_usercopy
build_asm_prog el0_cache_ops_privilege
build_asm_prog el0_wfx_trap
build_asm_prog fpsimd_minimal
build_asm_prog fpsimd_mvni
build_asm_prog fpsimd_logic_more
build_asm_prog fp_scalar_ls
build_asm_prog fp_scalar_regoffset
build_asm_prog fp_scalar_unscaled
build_asm_prog fpsimd_ext
build_asm_prog fp_scalar_elem_ls
build_asm_prog fpsimd_uminp
build_asm_prog fpsimd_umov_lane
build_asm_prog fpsimd_dup_elem
build_asm_prog fpsimd_stringops
build_asm_prog fpsimd_bic_imm
build_asm_prog fmov_scalar_imm
build_asm_prog fpsimd_scalar_movi
build_asm_prog fmov_scalar_reg
build_asm_prog fp_scalar_arith
build_asm_prog fcmp_e
build_asm_prog fp_scalar_convert
build_asm_prog fp_fcvtzu_scalar
build_asm_prog fp_fcvt_flags
build_asm_prog fp_fcvt_rounding_scalar
build_asm_prog fp_int_to_fp_rounding
build_asm_prog fp_compare_flags
build_asm_prog fp_scalar_compare_flags
build_asm_prog fp_absneg_nan_flags
build_asm_prog fpsimd_compare_flags
build_asm_prog fp_minmax_nan_flags
build_asm_prog fp_minmax_dn
build_asm_prog fp_cond_compare
build_asm_prog fp_scalar_fcsel
build_asm_prog fp_scalar_fma
build_asm_prog fp_arith_fpcr_flags
build_asm_prog fp_fz_arith_compare
build_asm_prog fp_fz_minmax
build_asm_prog fp_fz_misc
build_asm_prog fp_fz_to_int
build_asm_prog fp_scalar_misc
build_asm_prog fp_sqrt_flags
build_asm_prog fp_sqrt_rounding
build_asm_prog fp_roundint_flags
build_asm_prog fp_dn_arith
build_asm_prog fp_dn_misc
build_asm_prog fp_scalar_compare_misc
build_asm_prog fp_scalar_pairwise
build_asm_prog fp_scalar_frecpx
build_asm_prog fpsimd_ins_xtl
build_asm_prog fpsimd_fcvt_rounding
build_asm_prog fpsimd_fcvtxn_roundodd
build_asm_prog fpsimd_fp_estimate
build_asm_prog fpsimd_fp_step
build_asm_prog fpsimd_fp_convert_long_narrow
build_asm_prog fpsimd_fp_reducev
build_asm_prog fpsimd_fp_misc_rounding
build_asm_prog fpsimd_arith_fpcr_flags
build_asm_prog fpsimd_fp_pairwise
build_asm_prog fpsimd_misc_more
build_asm_prog fpsimd_arith_shift_perm
build_asm_prog fpsimd_fp_vector
build_asm_prog fpsimd_more_perm_fp
build_asm_prog fpsimd_structured_ls
build_asm_prog fpsimd_structured_ls_more
build_asm_prog fpsimd_structured_lane_ls
build_asm_prog fpsimd_widen_sat
build_asm_prog cpacr_fp_trap
build_asm_prog cpacr_fp_mem_trap
build_asm_prog cpacr_fp_structured_trap
build_asm_prog pstate_pan
build_asm_prog pan_span_exception
build_asm_prog id_aa64_feature_regs
build_asm_prog svc_sysreg_minimal
build_asm_prog el0_sysreg_privilege
build_asm_prog el0_idspace_undef
build_asm_prog el0_special_regs_undef
build_asm_prog el0_absent_pstate_features_undef
build_asm_prog msr_imm_absent_features_undef
build_asm_prog el0_eret_undef
build_asm_prog el0_hvc_smc_undef
build_asm_prog el1_hvc_smc_undef
build_asm_prog brk_exception
build_asm_prog hlt_undef
build_asm_prog pacm_undef
build_asm_prog flagm_sys_undef
build_asm_prog system_feature_absent_undef
build_asm_prog gcs_system_absent_undef
build_asm_prog dcps_drps_non_debug_undef
build_asm_prog illegal_state_return
build_asm_prog special_pstate_regform
build_asm_prog el0_tlbi_cache_undef
build_asm_prog el0_dc_ivac_undef
build_asm_prog dc_cva_persist_absent
build_asm_prog el0_daif_uma
build_asm_prog sysreg_optional_absent
build_asm_prog pmu_sysreg_absent
build_asm_prog pmu_sysreg_absent_more
build_asm_prog rng_sysreg_absent
build_asm_prog sysreg_optional_absent_more
build_asm_prog sme_sysreg_absent
build_asm_prog amu_sysreg_absent
build_asm_prog spe_sysreg_absent
build_asm_prog spe_pmb_sysreg_absent
build_asm_prog lse_atomics
build_asm_prog casp_pair
build_asm_prog lse_atomics_narrow
build_asm_prog irq_spsel
build_asm_prog instr_legacy_each
build_asm_prog mmu_tlb_cache
build_asm_prog mmu_ttbr1_early
build_asm_prog mmu_tlb_vae1_scope
build_asm_prog mmu_ttbr_switch
build_asm_prog mmu_unmap_data_abort
build_asm_prog mmu_cache_maint_fault
build_asm_prog mmu_tlbi_non_target
build_asm_prog mmu_l2_block_vmalle1
build_asm_prog mmu_at_tlb_observe
build_asm_prog mmu_at_el0_permissions
build_asm_prog mmu_ttbr_asid_mask
build_asm_prog mmu_tlb_asid_scope
build_asm_prog mmu_pan_user_access
build_asm_prog mmu_ldtr_sttr_pan
build_asm_prog mmu_el0_ap_fault
build_asm_prog mmu_perm_ro_write_abort
build_asm_prog mmu_dc_cva_el0_perm_fault
build_asm_prog mmu_dc_ivac_perm_fault
build_asm_prog mmu_dc_zva_fault
build_asm_prog mmu_dc_zva_el0_perm_fault
build_asm_prog mmu_ic_ivau_el0_perm_fault
build_asm_prog mmu_xn_fetch_abort
build_asm_prog mmu_cross_page_load
build_asm_prog mmu_cross_page_store
build_asm_prog mmu_cross_page_fault_far_load
build_asm_prog mmu_cross_page_fault_far_store
build_asm_prog mmu_cross_page_various
build_asm_prog mmu_cross_page_pair_fault_far
build_asm_prog mmu_table_ap_inherit
build_asm_prog mmu_table_pxn_inherit
build_asm_prog mmu_tcr_ips_mair_decode
build_asm_prog mmu_af_fault
build_asm_prog mmu_at_pan_ignore
build_asm_prog at_pan2_absent_undef
build_asm_prog sync_exception_regs
build_asm_prog exception_daif_entry
build_asm_prog eret_clears_exclusive
build_asm_prog nested_sync_depth
build_asm_prog gic_sysreg_id_consistency
build_asm_prog debug_sysreg_resource_bounds
build_asm_prog gic_timer_sysreg
build_asm_prog gic_timer_rearm_no_spurious
build_asm_prog gic_timer_phys_sysreg
build_asm_prog bitfield_basic
build_asm_prog p1_core
build_asm_prog atomics_minimal
build_asm_prog signext_loads
build_asm_prog signext_postindex
build_asm_prog atomics_small
build_asm_prog mul_high
build_asm_prog pair_non_temporal
build_asm_prog dc_zva
build_asm_prog dc_zva_device_align_fault
build_asm_prog atomics_sp_base
build_asm_prog ldrsw_regoffset
build_asm_prog signed_regoffset
build_asm_prog ldpsw_pair
build_asm_prog pair_exclusive
build_asm_prog adc_sbc_minimal
build_asm_prog snapshot_resume
build_asm_prog snapshot_perf_mailbox
build_asm_prog irq_nested_el1_wfi
build_asm_prog sp_ccmp_path
build_asm_prog sp_alias_paths
build_asm_prog addsub_shift_more
build_asm_prog uart_irq_rx_spaced
build_asm_prog uart_tx_match_reply
build_asm_prog predecode_dyn_codegen
build_asm_prog predecode_va_exec_switch
build_asm_prog predecode_load_store_min
build_asm_prog predecode_logic_min
build_asm_prog pl050_basic
build_asm_prog ps2_rx_spaced
build_asm_prog smp_mpidr_boot
build_asm_prog smp_sev_wfe
build_asm_prog smp_ldxr_invalidate
build_asm_prog smp_ldxr_invalidate_mmu
build_asm_prog smp_spinlock_ldaxr_stlxr
build_asm_prog smp_tlbi_broadcast
build_asm_prog smp_wfe_monitor_event
build_asm_prog smp_wfe_store_no_event
build_asm_prog psci_cpu_on_min
build_asm_prog smp_gic_sgi
build_asm_prog smp_timer_ppi
build_asm_prog smp_timer_rate
build_asm_prog smp_dc_zva_invalidate

if command -v aarch64-linux-gnu-gcc >/dev/null 2>&1; then
  aarch64-linux-gnu-gcc -nostdlib -static -ffreestanding -fomit-frame-pointer -fno-stack-protector \
    -fno-asynchronous-unwind-tables -fno-unwind-tables -Wl,-T,tests/arm64/link.ld \
    -o "$OUT_DIR/hello_c.elf" tests/arm64/start.S tests/arm64/hello_c.c
  "$AARCH64_OBJCOPY" -O binary "$OUT_DIR/hello_c.elf" "$OUT_DIR/hello_c.bin"
  "$AARCH64_OBJDUMP" -d "$OUT_DIR/hello_c.elf" > "$OUT_DIR/hello_c.dis"

  aarch64-linux-gnu-gcc -O0 -nostdlib -static -ffreestanding -fno-stack-protector \
    -fno-asynchronous-unwind-tables -fno-unwind-tables -Wl,-T,tests/arm64/link.ld \
    -o "$OUT_DIR/stack_c.elf" tests/arm64/start.S tests/arm64/stack_c.c
  "$AARCH64_OBJCOPY" -O binary "$OUT_DIR/stack_c.elf" "$OUT_DIR/stack_c.bin"
  "$AARCH64_OBJDUMP" -d "$OUT_DIR/stack_c.elf" > "$OUT_DIR/stack_c.dis"
elif command -v clang >/dev/null 2>&1; then
  clang --target=aarch64-linux-gnu -ffreestanding -fomit-frame-pointer -fno-stack-protector \
    -fno-asynchronous-unwind-tables -fno-unwind-tables -nostdlib -c tests/arm64/hello_c.c -o "$OUT_DIR/hello_c.o"
  "$AARCH64_AS" tests/arm64/start.S -o "$OUT_DIR/start.o"
  "$AARCH64_LD" -T tests/arm64/link.ld -o "$OUT_DIR/hello_c.elf" "$OUT_DIR/start.o" "$OUT_DIR/hello_c.o"
  "$AARCH64_OBJCOPY" -O binary "$OUT_DIR/hello_c.elf" "$OUT_DIR/hello_c.bin"
  "$AARCH64_OBJDUMP" -d "$OUT_DIR/hello_c.elf" > "$OUT_DIR/hello_c.dis"
fi

echo "Built test binaries in $OUT_DIR"
ls -1 "$OUT_DIR"/*.bin
