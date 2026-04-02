#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT_DIR"

set -x

cmake --build build -j

tests/arm64/build_tests.sh

export AARCHVM_BRK_MODE=halt

CAPTURE_STATUS=0

capture_cmd() {
  local __stdout_var="$1"
  local __stderr_var="$2"
  shift 2
  local stdout_file=""
  local stderr_file=""
  local captured_stdout=""
  local captured_stderr=""
  stdout_file="$(mktemp)"
  stderr_file="$(mktemp)"
  set +e
  "$@" >"$stdout_file" 2>"$stderr_file"
  CAPTURE_STATUS=$?
  set -e
  captured_stdout="$(cat "$stdout_file")"
  captured_stderr="$(cat "$stderr_file")"
  rm -f "$stdout_file" "$stderr_file"
  printf -v "$__stdout_var" '%s' "$captured_stdout"
  printf -v "$__stderr_var" '%s' "$captured_stderr"
}

check_simulator_stderr() {
  local stderr="$1"
  case "$stderr" in
    *FATAL:*|*UNIMPL:*|*NESTED-SYNC:*)
      printf '%s' "$stderr" >&2
      return 1
      ;;
  esac
}

run() {
  local bin="$1"
  local steps="$2"
  local stdout=""
  local stderr=""
  capture_cmd stdout stderr ./build/aarchvm -bin "tests/arm64/out/${bin}" -load 0x0 -entry 0x0 -steps "$steps"
  test "$CAPTURE_STATUS" -eq 0
  check_simulator_stderr "$stderr"
  if [ -n "$stderr" ]; then
    printf '%s' "$stderr" >&2
  fi
  printf '%s' "$stdout"
}

run_smp() {
  local bin="$1"
  local steps="$2"
  local stdout=""
  local stderr=""
  capture_cmd stdout stderr ./build/aarchvm -smp 2 -bin "tests/arm64/out/${bin}" -load 0x0 -entry 0x0 -steps "$steps"
  test "$CAPTURE_STATUS" -eq 0
  check_simulator_stderr "$stderr"
  if [ -n "$stderr" ]; then
    printf '%s' "$stderr" >&2
  fi
  printf '%s' "$stdout"
}

run_expect() {
  local bin="$1"
  local steps="$2"
  local expected="$3"
  local stdout=""
  local stderr=""
  capture_cmd stdout stderr ./build/aarchvm -bin "tests/arm64/out/${bin}" -load 0x0 -entry 0x0 -steps "$steps"
  test "$CAPTURE_STATUS" -eq 0
  check_simulator_stderr "$stderr"
  test "$(printf '%s' "$stdout" | tr -d '\r\n')" = "$expected"
}

run_expect_smp() {
  local bin="$1"
  local steps="$2"
  local expected="$3"
  local stdout=""
  local stderr=""
  capture_cmd stdout stderr ./build/aarchvm -smp 2 -bin "tests/arm64/out/${bin}" -load 0x0 -entry 0x0 -steps "$steps"
  test "$CAPTURE_STATUS" -eq 0
  check_simulator_stderr "$stderr"
  test "$(printf '%s' "$stdout" | tr -d '\r\n')" = "$expected"
}

run_expect_trap() {
  local bin="$1"
  local steps="$2"
  local expected="$3"
  local stdout=""
  local stderr=""
  capture_cmd stdout stderr env AARCHVM_BRK_MODE=trap ./build/aarchvm -bin "tests/arm64/out/${bin}" -load 0x0 -entry 0x0 -steps "$steps"
  test "$CAPTURE_STATUS" -eq 0
  check_simulator_stderr "$stderr"
  test "$(printf '%s' "$stdout" | tr -d '\r\n')" = "$expected"
}

run_expect_halt_output() {
  local bin="$1"
  local steps="$2"
  local expected="$3"
  local out=""
  local status=0
  set +e
  out="$(timeout 30s ./build/aarchvm -bin "tests/arm64/out/${bin}" -load 0x0 -entry 0x0 -steps "$steps" 2>&1)"
  status=$?
  set -e
  test "$status" -eq 0
  printf '%s' "$out" | grep -q "$expected"
  printf '%s' "$out" | grep -q 'CPU-HALT'
}

run_expect instr_legacy_each.bin 3000000 E
run_expect mmu_tlb_cache.bin 5000000 Q
run_expect mmu_ttbr1_early.bin 3000000 K
run_expect mmu_tlb_vae1_scope.bin 4000000 1
run_expect mmu_ttbr_switch.bin 4000000 2
run_expect mmu_unmap_data_abort.bin 4000000 3
test "$(./build/aarchvm -bin tests/arm64/out/mmu_cache_maint_fault.bin -load 0x0 -entry 0x0 -steps 4000000 | tr -d '\r\n')" = 'M'
run_expect mmu_tlbi_non_target.bin 4000000 4
run_expect mmu_l2_block_vmalle1.bin 4000000 5
run_expect mmu_at_tlb_observe.bin 4000000 6
run_expect mmu_at_el0_permissions.bin 4000000 A
run_expect mmu_ttbr_asid_mask.bin 4000000 7
run_expect ttbr_el1_visible_bits.bin 200000 T
run_expect tcr_el1_visible_bits.bin 200000 R
run_expect mmu_perm_ro_write_abort.bin 4000000 8
run_expect mmu_dc_cva_el0_perm_fault.bin 4000000 C
run_expect mmu_dc_ivac_perm_fault.bin 4000000 I
run_expect mmu_dc_zva_fault.bin 4000000 Z
run_expect mmu_dc_zva_el0_perm_fault.bin 4000000 Z
run_expect mmu_ic_ivau_el0_perm_fault.bin 4000000 I
run_expect mmu_xn_fetch_abort.bin 4000000 9
run_expect mmu_cross_page_load.bin 4000000 L
run_expect mmu_cross_page_store.bin 4000000 C
run_expect mmu_cross_page_fault_far_load.bin 4000000 R
run_expect mmu_cross_page_fault_far_store.bin 4000000 W
run_expect mmu_fpsimd_whole_fault_far.bin 4000000 W
run_expect mmu_fpsimd_structured_fault_far.bin 4000000 V
run_expect mmu_fpsimd_lane_fault_far.bin 4000000 L
run_expect mmu_cross_page_various.bin 4000000 V
run_expect mmu_cross_page_pair_fault_far.bin 4000000 P
run_expect mmu_table_ap_inherit.bin 4000000 H
run_expect mmu_table_pxn_inherit.bin 4000000 Y
run_expect mmu_tcr_ips_mair_decode.bin 4000000 Z
run_expect mmu_af_fault.bin 4000000 0
run_expect mmu_at_par_formats.bin 4000000 P
run_expect mmu_at_pan_ignore.bin 400000 N
run_expect at_s1e0_el0_undef.bin 600000 U
run_expect at_pan2_absent_undef.bin 600000 N
run_expect sync_exception_regs.bin 2000000 X
run_expect exception_daif_entry.bin 300000 D
run_expect eret_clears_exclusive.bin 300000 E
./build/aarchvm -bin tests/arm64/out/nested_sync_depth.bin -load 0x0 -entry 0x0 -steps 400000 | grep -qx 'P'
run_expect gic_sysreg_id_consistency.bin 300000 J
run_expect gic_sysreg_manual_ack.bin 400000 M
run_expect debug_sysreg_resource_bounds.bin 800000 K
run_expect debug_break_watch_basic.bin 1200000 Q
run_expect debug_halted_sysregs_undef.bin 800000 D
run_expect debug_dcc_minimal.bin 800000 C
run_expect debug_dcc_sysregs_minimal.bin 800000 Y
run_expect debug_mdscr_dcc_flags.bin 800000 Z
run_expect debug_misc_sysregs_minimal.bin 800000 O
run_expect debug_ctrl_sysregs_minimal.bin 800000 P
run_expect_trap debug_lock_exception_gating.bin 1200000 L
run_expect_halt_output debug_software_access_halt_read.bin 400000 'TDA-READ'
run_expect_halt_output debug_software_access_halt_write.bin 400000 'TDA-WRITE'
run_expect_trap software_step_basic.bin 1200000 T
run_expect gic_timer_sysreg.bin 2000000 G
run_expect gic_timer_rearm_no_spurious.bin 2000000 S
run_expect gic_timer_phys_sysreg.bin 2000000 P
run_expect bitfield_basic.bin 400000 U
run_expect p1_core.bin 600000 V
run_expect atomics_minimal.bin 400000 A
run_expect crc32_family.bin 400000 R
run_expect signext_loads.bin 400000 N
run_expect signext_postindex.bin 400000 W
run_expect atomics_small.bin 400000 O
run_expect mul_high.bin 400000 J
run_expect pair_non_temporal.bin 400000 F
run_expect dc_zva.bin 400000 Z
run_expect dc_zva_device_align_fault.bin 400000 A
run_expect atomics_sp_base.bin 400000 S
run_expect ldrsw_regoffset.bin 400000 K
run_expect signed_regoffset.bin 400000 R
run_expect ldpsw_pair.bin 400000 L
run_expect pair_exclusive.bin 400000 X
run_expect adc_sbc_minimal.bin 200000 W
run_expect snapshot_resume.bin 200000 AB
run_expect irq_nested_el1_wfi.bin 400000 W
run_expect hello_uart.bin 4000 ASM
run_expect branch_arith.bin 4000 B
run_expect csselr_el1_res0_bits.bin 200000 S
run_expect contextidr_el1_res0_bits.bin 200000 C
run_expect hello_c.bin 400000 C
run_expect stack_c.bin 600000 S
run_expect irq_minimal.bin 1400000 IM
run_expect irq_twice.bin 2400000 T
run_expect irq_disabled.bin 1200000 D
run_expect sys_ctrl.bin 1800000 W
run_expect cntkctl_el1.bin 300000 W
run_expect cntkctl_el0_timer_access.bin 600000 K
run_expect cntkctl_el1_visible_bits.bin 200000 Q
run_expect dczid_el0_dze_visible.bin 300000 D
run_expect sctlr_el1_visible_bits.bin 200000 S
test "$(./build/aarchvm -bin tests/arm64/out/el0_sysreg_privilege.bin -load 0x0 -entry 0x0 -steps 400000 | tr -d '\r\n')" = 'R'
test "$(./build/aarchvm -bin tests/arm64/out/el0_daif_uma.bin -load 0x0 -entry 0x0 -steps 500000 | tr -d '\r\n')" = 'U'
test "$(./build/aarchvm -bin tests/arm64/out/el0_idspace_undef.bin -load 0x0 -entry 0x0 -steps 600000 | tr -d '\r\n')" = 'D'
test "$(./build/aarchvm -bin tests/arm64/out/el0_special_regs_undef.bin -load 0x0 -entry 0x0 -steps 600000 | tr -d '\r\n')" = 'S'
test "$(./build/aarchvm -bin tests/arm64/out/el0_absent_pstate_features_undef.bin -load 0x0 -entry 0x0 -steps 600000 | tr -d '\r\n')" = 'A'
test "$(./build/aarchvm -bin tests/arm64/out/msr_imm_absent_features_undef.bin -load 0x0 -entry 0x0 -steps 900000 | tr -d '\r\n')" = 'M'
test "$(./build/aarchvm -bin tests/arm64/out/el0_eret_undef.bin -load 0x0 -entry 0x0 -steps 600000 | tr -d '\r\n')" = 'E'
test "$(./build/aarchvm -bin tests/arm64/out/el0_hvc_smc_undef.bin -load 0x0 -entry 0x0 -steps 600000 | tr -d '\r\n')" = 'H'
test "$(./build/aarchvm -bin tests/arm64/out/el1_hvc_smc_undef.bin -load 0x0 -entry 0x0 -steps 600000 | tr -d '\r\n')" = 'J'
test "$(./build/aarchvm -bin tests/arm64/out/pc_alignment_fault.bin -load 0x0 -entry 0x0 -steps 600000 | tr -d '\r\n')" = 'P'
test "$(./build/aarchvm -bin tests/arm64/out/sp_special_sysreg_access.bin -load 0x0 -entry 0x0 -steps 800000 | tr -d '\r\n')" = 'R'
test "$(./build/aarchvm -bin tests/arm64/out/sysreg_xzr_semantics.bin -load 0x0 -entry 0x0 -steps 400000 | tr -d '\r\n')" = 'Z'
run_expect_trap brk_exception.bin 600000 B
run_expect_trap hlt_undef.bin 600000 H
test "$(./build/aarchvm -bin tests/arm64/out/pacm_absent_nop.bin -load 0x0 -entry 0x0 -steps 600000 | tr -d '\r\n')" = 'M'
test "$(./build/aarchvm -bin tests/arm64/out/ldraa_ldrab_absent_undef.bin -load 0x0 -entry 0x0 -steps 800000 | tr -d '\r\n')" = 'A'
test "$(./build/aarchvm -bin tests/arm64/out/ls64_absent_undef.bin -load 0x0 -entry 0x0 -steps 1200000 | tr -d '\r\n')" = 'S'
test "$(./build/aarchvm -bin tests/arm64/out/lrcpc_absent_undef.bin -load 0x0 -entry 0x0 -steps 1200000 | tr -d '\r\n')" = 'R'
test "$(./build/aarchvm -bin tests/arm64/out/mops_absent_undef.bin -load 0x0 -entry 0x0 -steps 1500000 | tr -d '\r\n')" = 'O'
test "$(./build/aarchvm -bin tests/arm64/out/pauth_branch_absent_undef.bin -load 0x0 -entry 0x0 -steps 1200000 | tr -d '\r\n')" = 'B'
test "$(./build/aarchvm -bin tests/arm64/out/pauth_absent_nop.bin -load 0x0 -entry 0x0 -steps 800000 | tr -d '\r\n')" = 'P'
test "$(./build/aarchvm -bin tests/arm64/out/pauth_absent_integer_undef.bin -load 0x0 -entry 0x0 -steps 1200000 | tr -d '\r\n')" = 'U'
test "$(./build/aarchvm -bin tests/arm64/out/pauth_lr_absent_integer_undef.bin -load 0x0 -entry 0x0 -steps 1200000 | tr -d '\r\n')" = 'L'
test "$(./build/aarchvm -bin tests/arm64/out/pauth_lr_return_imm_absent_undef.bin -load 0x0 -entry 0x0 -steps 800000 | tr -d '\r\n')" = 'I'
test "$(./build/aarchvm -bin tests/arm64/out/pauth_lr_return_absent_undef.bin -load 0x0 -entry 0x0 -steps 800000 | tr -d '\r\n')" = 'R'
test "$(./build/aarchvm -bin tests/arm64/out/flagm_sys_undef.bin -load 0x0 -entry 0x0 -steps 800000 | tr -d '\r\n')" = 'G'
test "$(./build/aarchvm -bin tests/arm64/out/flagm_integer_undef.bin -load 0x0 -entry 0x0 -steps 800000 | tr -d '\r\n')" = 'm'
test "$(./build/aarchvm -bin tests/arm64/out/hint_feature_absent_nop.bin -load 0x0 -entry 0x0 -steps 800000 | tr -d '\r\n')" = 'K'
test "$(./build/aarchvm -bin tests/arm64/out/system_feature_absent_undef.bin -load 0x0 -entry 0x0 -steps 1200000 | tr -d '\r\n')" = 'W'
test "$(./build/aarchvm -bin tests/arm64/out/wfxt_absent_undef.bin -load 0x0 -entry 0x0 -steps 800000 | tr -d '\r\n')" = 'w'
test "$(./build/aarchvm -bin tests/arm64/out/gcs_system_absent_undef.bin -load 0x0 -entry 0x0 -steps 1500000 | tr -d '\r\n')" = 'Q'
test "$(./build/aarchvm -bin tests/arm64/out/dcps_drps_non_debug_undef.bin -load 0x0 -entry 0x0 -steps 600000 | tr -d '\r\n')" = 'D'
test "$(./build/aarchvm -bin tests/arm64/out/illegal_state_return.bin -load 0x0 -entry 0x0 -steps 600000 | tr -d '\r\n')" = 'I'
test "$(./build/aarchvm -bin tests/arm64/out/special_pstate_regform.bin -load 0x0 -entry 0x0 -steps 600000 | tr -d '\r\n')" = 'Y'
test "$(./build/aarchvm -bin tests/arm64/out/el0_tlbi_cache_undef.bin -load 0x0 -entry 0x0 -steps 800000 | tr -d '\r\n')" = 'K'
test "$(./build/aarchvm -bin tests/arm64/out/el0_dc_ivac_undef.bin -load 0x0 -entry 0x0 -steps 600000 | tr -d '\r\n')" = 'V'
test "$(./build/aarchvm -bin tests/arm64/out/dc_csw_privilege.bin -load 0x0 -entry 0x0 -steps 600000 | tr -d '\r\n')" = 'J'
test "$(./build/aarchvm -bin tests/arm64/out/dc_cva_persist_absent.bin -load 0x0 -entry 0x0 -steps 600000 | tr -d '\r\n')" = 'P'
test "$(./build/aarchvm -bin tests/arm64/out/sysreg_trap_iss_rt_fields.bin -load 0x0 -entry 0x0 -steps 600000 | tr -d '\r\n')" = 'I'
test "$(./build/aarchvm -bin tests/arm64/out/sysreg_optional_absent.bin -load 0x0 -entry 0x0 -steps 400000 | tr -d '\r\n')" = 'U'
test "$(./build/aarchvm -bin tests/arm64/out/pmu_sysreg_absent.bin -load 0x0 -entry 0x0 -steps 1400000 | tr -d '\r\n')" = 'P'
test "$(./build/aarchvm -bin tests/arm64/out/pmu_sysreg_absent_more.bin -load 0x0 -entry 0x0 -steps 1600000 | tr -d '\r\n')" = 'R'
test "$(./build/aarchvm -bin tests/arm64/out/rng_sysreg_absent.bin -load 0x0 -entry 0x0 -steps 400000 | tr -d '\r\n')" = 'N'
test "$(./build/aarchvm -bin tests/arm64/out/sysreg_optional_absent_more.bin -load 0x0 -entry 0x0 -steps 1200000 | tr -d '\r\n')" = 'O'
test "$(./build/aarchvm -bin tests/arm64/out/sme_sysreg_absent.bin -load 0x0 -entry 0x0 -steps 800000 | tr -d '\r\n')" = 'Z'
test "$(./build/aarchvm -bin tests/arm64/out/amu_sysreg_absent.bin -load 0x0 -entry 0x0 -steps 2200000 | tr -d '\r\n')" = 'A'
test "$(./build/aarchvm -bin tests/arm64/out/spe_sysreg_absent.bin -load 0x0 -entry 0x0 -steps 2200000 | tr -d '\r\n')" = 'S'
test "$(./build/aarchvm -bin tests/arm64/out/spe_pmb_sysreg_absent.bin -load 0x0 -entry 0x0 -steps 1800000 | tr -d '\r\n')" = 'B'
test "$(./build/aarchvm -bin tests/arm64/out/el0_cache_ops_privilege.bin -load 0x0 -entry 0x0 -steps 600000 | tr -d '\r\n')" = 'C'
test "$(./build/aarchvm -bin tests/arm64/out/el0_wfx_trap.bin -load 0x0 -entry 0x0 -steps 800000 | tr -d '\r\n')" = 'T'
run_expect ldtr_sttr_usercopy.bin 400000 W
run_expect fpsimd_minimal.bin 400000 W
run_expect fpsimd_mvni.bin 400000 W
run_expect fpsimd_logic_more.bin 400000 W
run_expect fp_scalar_ls.bin 400000 W
run_expect fp_scalar_regoffset.bin 400000 R
run_expect fp_scalar_unscaled.bin 400000 U
run_expect fpsimd_ext.bin 400000 W
run_expect fp_scalar_elem_ls.bin 400000 W
run_expect fpsimd_uminp.bin 400000 W
run_expect fpsimd_umov_lane.bin 400000 W
run_expect fpsimd_dup_elem.bin 400000 W
run_expect fpsimd_stringops.bin 600000 W
run_expect fpsimd_bic_imm.bin 200000 W
run_expect fmov_scalar_imm.bin 200000 W
run_expect fpsimd_scalar_movi.bin 200000 W
run_expect fmov_scalar_reg.bin 200000 W
run_expect fp_scalar_arith.bin 300000 W
run_expect fcmp_e.bin 200000 W
run_expect fp_scalar_convert.bin 200000 W
run_expect fp_fcvtzu_scalar.bin 200000 U
run_expect fp_fcvt_flags.bin 200000 Z
run_expect fp_fcvtn_flags.bin 300000 N
run_expect fp_frecpe_flags.bin 300000 Y
run_expect fp_fcvt_rounding_scalar.bin 300000 R
run_expect fp_fcvt_special_scalar.bin 300000 S
run_expect fp_int_to_fp_rounding.bin 300000 I
run_expect fp_compare_flags.bin 200000 Q
run_expect fp_scalar_compare_flags.bin 200000 K
run_expect fp_absneg_nan_flags.bin 300000 N
run_expect fpsimd_compare_flags.bin 300000 G
run_expect fp_minmax_nan_flags.bin 300000 M
run_expect fp_minmax_dn.bin 400000 X
run_expect fp_cond_compare.bin 200000 C
run_expect fp_scalar_fcsel.bin 200000 W
run_expect fp_scalar_fma.bin 200000 W
run_expect fp_arith_fpcr_flags.bin 300000 A
run_expect fp_fz_arith_compare.bin 300000 F
run_expect fp_fz_minmax.bin 300000 W
run_expect fp_fz_misc.bin 500000 Z
run_expect fp_fz_to_int.bin 400000 T
run_expect fp_scalar_misc.bin 200000 P
run_expect fp_sqrt_flags.bin 300000 Q
run_expect fp_sqrt_rounding.bin 300000 R
run_expect fp_roundint_flags.bin 300000 I
run_expect fp_dn_arith.bin 400000 T
run_expect fp_dn_misc.bin 400000 D
run_expect fp_scalar_compare_misc.bin 300000 J
run_expect fp_scalar_pairwise.bin 300000 Y
run_expect fp_scalar_frecpx.bin 300000 X
run_expect fp_ah_absent_ignored.bin 400000 H
run_expect fpcr_visible_bits.bin 200000 P
run_expect fpsimd_ins_xtl.bin 300000 W
run_expect fpsimd_fcvt_rounding.bin 400000 O
run_expect fpsimd_fcvtxn_roundodd.bin 400000 X
run_expect fpsimd_fp_estimate.bin 400000 E
run_expect fpsimd_fp_step.bin 400000 S
run_expect fpsimd_fp_convert_long_narrow.bin 400000 H
run_expect fpsimd_fp_reducev.bin 400000 R
run_expect fpsimd_fp_misc_rounding.bin 400000 V
run_expect fpsimd_arith_fpcr_flags.bin 400000 N
run_expect fpsimd_fp_pairwise.bin 400000 Z
run_expect fpsimd_misc_more.bin 300000 G
run_expect fpsimd_addhn_family.bin 300000 HN
run_expect fpsimd_mov_elem.bin 300000 ME
run_expect fpsimd_arith_shift_perm.bin 300000 V
run_expect fpsimd_fp_vector.bin 400000 V
run_expect fpsimd_more_perm_fp.bin 400000 M
run_expect fpsimd_structured_ls.bin 400000 T
run_expect fpsimd_structured_ls_more.bin 600000 Y
run_expect fpsimd_structured_ls_regpost.bin 900000 T
run_expect fpsimd_structured_lane_ls.bin 800000 Y
run_expect fpsimd_widen_sat.bin 400000 Y
run_expect fpsr_qc_saturation.bin 400000 Q
run_expect cpacr_fp_trap.bin 300000 C
run_expect cpacr_visible_bits.bin 200000 P
run_expect cpacr_fp_sysreg_trap.bin 300000 R
run_expect cpacr_fp_mem_trap.bin 300000 T
run_expect cpacr_fp_structured_trap.bin 400000 T
run_expect cpacr_fp_structured_regpost_trap.bin 400000 T
run_expect pstate_pan.bin 200000 W
run_expect pan_span_exception.bin 300000 S
run_expect spsr_el1_res0_bits.bin 200000 P
run_expect vbar_el1_res0_bits.bin 200000 V
run_expect sctlr_endian_fixed_bits.bin 200000 E
run_expect sp_alignment_fault.bin 300000 S
run_expect data_alignment_fault.bin 300000 A
run_expect atomic_alignment_fault.bin 400000 O
run_expect fpsimd_q_alignment_fault.bin 400000 Q
run_expect fpsimd_q_pair_alignment_fault.bin 500000 Y
run_expect fpsimd_ld1_multi_alignment.bin 500000 J
run_expect fpsimd_sp_alignment_fault.bin 500000 Z
run_expect id_aa64_feature_regs.bin 200000 I
run_expect mmu_el0_ap_fault.bin 4000000 U
run_expect mmu_el0_uxn_fetch_abort.bin 4000000 u
run_expect mmu_pan_user_access.bin 4000000 N
run_expect mmu_ldtr_sttr_pan.bin 4000000 U
run_expect mmu_tlb_asid_scope.bin 4000000 A
run_expect mmu_tlbi_asid_zero_highbits.bin 4000000 Z
run_expect mmu_aside1_global_preserve.bin 4000000 G
run_expect mmu_tlbi_vae1_operand_encoding.bin 4000000 E
run_expect svc_sysreg_minimal.bin 300000 S
run_expect lse_atomics.bin 400000 L
run_expect casp_pair.bin 400000 C
run_expect pair_atomic_more.bin 800000 Q
run_expect pair_atomic_fault_no_partial.bin 5000000 K
run_expect lse_atomics_narrow.bin 400000 N
run_expect irq_spsel.bin 1800000 P
run_expect logic_misc.bin 300000 L
run_expect mem_ext.bin 300000 M
run_expect branch_reg.bin 300000 R
run_expect sp_ccmp_path.bin 200000 3
run_expect sp_alias_paths.bin 300000 P
run_expect addsub_shift_more.bin 300000 Q
AARCHVM_UART_RX_SCRIPT='100:0x41,1000:0x42,5000:0x43' ./build/aarchvm -bin tests/arm64/out/uart_irq_rx_spaced.bin -load 0x0 -entry 0x0 -steps 200000 | grep -qx 'ABCP'
test "$(AARCHVM_UART_TX_MATCH='=> ' AARCHVM_UART_TX_REPLY=$'A\n' ./build/aarchvm -bin tests/arm64/out/uart_tx_match_reply.bin -load 0x0 -entry 0x0 -steps 200000 | tr -d '\r\n')" = '=> P'

UART_IRQ_SNAP=tests/arm64/out/uart_irq_rx_spaced.snap
rm -f "$UART_IRQ_SNAP"
AARCHVM_UART_RX_SCRIPT='100:0x41,1000:0x42' ./build/aarchvm -bin tests/arm64/out/uart_irq_rx_spaced.bin -load 0x0 -entry 0x0 -steps 2000 -snapshot-save "$UART_IRQ_SNAP" > tests/arm64/out/uart_irq_snapshot_pre.log
AARCHVM_UART_RX_SCRIPT='5000:0x43' ./build/aarchvm -snapshot-load "$UART_IRQ_SNAP" -steps 10000 > tests/arm64/out/uart_irq_snapshot_post.log
test "$(tr -d '\r\n' < tests/arm64/out/uart_irq_snapshot_pre.log)" = 'AB'
test "$(tr -d '\r\n' < tests/arm64/out/uart_irq_snapshot_post.log)" = 'CP'

SNAP=tests/arm64/out/snapshot_resume.snap
rm -f "$SNAP"
./build/aarchvm -bin tests/arm64/out/snapshot_resume.bin -load 0x0 -entry 0x0 -steps 2000 -snapshot-save "$SNAP" > tests/arm64/out/snapshot_pre.log
./build/aarchvm -snapshot-load "$SNAP" -steps 200000 > tests/arm64/out/snapshot_post.log
test "$(tr -d '\r\n' < tests/arm64/out/snapshot_pre.log)" = 'A'
test "$(tr -d '\r\n' < tests/arm64/out/snapshot_post.log)" = 'B'

PERF_SNAP=tests/arm64/out/snapshot_perf_mailbox.snap
PERF_LOG=tests/arm64/out/snapshot_perf_mailbox.log
rm -f "$PERF_SNAP" "$PERF_LOG"
./build/aarchvm -bin tests/arm64/out/snapshot_perf_mailbox.bin -load 0x0 -entry 0x0 -steps 2000 -snapshot-save "$PERF_SNAP" > tests/arm64/out/snapshot_perf_mailbox_pre.log
AARCHVM_UART_RX_SCRIPT='100:0x5a' ./build/aarchvm -snapshot-load "$PERF_SNAP" -steps 200000 > "$PERF_LOG" 2>&1
test ! -s tests/arm64/out/snapshot_perf_mailbox_pre.log
grep -q '^P$' "$PERF_LOG"
grep -q 'PERF-RESULT case_id=4660 arg0=86 arg1=120 ' "$PERF_LOG"

run_expect predecode_dyn_codegen.bin 400000 AB
run_expect predecode_va_exec_switch.bin 5000000 ABA
run_expect predecode_load_store_min.bin 400000 L
run_expect predecode_logic_min.bin 400000 P
run_expect pl050_basic.bin 400000 KMI
run_expect_smp smp_mpidr_boot.bin 200000 S
run_expect_smp smp_sev_wfe.bin 200000 E
run_expect_smp smp_ldxr_invalidate.bin 200000 I
run_expect_smp smp_ldxr_invalidate_mmu.bin 1200000 V
run_expect_smp smp_spinlock_ldaxr_stlxr.bin 600000 L
run_expect_smp smp_lse_ldaddal_counter.bin 600000 J
run_expect_smp smp_tlbi_broadcast.bin 1200000 M
run_expect_smp smp_wfe_monitor_event.bin 300000 M
run_expect_smp smp_wfe_store_no_event.bin 300000 N
test "$(./build/aarchvm -smp 2 -smp-mode psci -bin tests/arm64/out/psci_cpu_on_min.bin -load 0x0 -entry 0x0 -steps 400000 | tr -d '\r\n')" = 'P'
run_expect_smp smp_gic_sgi.bin 400000 G
run_expect_smp smp_timer_ppi.bin 400000 T
run_expect_smp smp_timer_rate.bin 400000 R
run_expect_smp smp_dc_zva_invalidate.bin 400000 D
AARCHVM_PS2_RX_SCRIPT='100:0x41,1000:0x42,5000:0x43' ./build/aarchvm -bin tests/arm64/out/ps2_rx_spaced.bin -load 0x0 -entry 0x0 -steps 200000 | grep -qx 'ABCP'

PS2_SNAP=tests/arm64/out/ps2_rx_spaced.snap
rm -f "$PS2_SNAP"
AARCHVM_PS2_RX_SCRIPT='100:0x41,1000:0x42' ./build/aarchvm -bin tests/arm64/out/ps2_rx_spaced.bin -load 0x0 -entry 0x0 -steps 2000 -snapshot-save "$PS2_SNAP" > tests/arm64/out/ps2_snapshot_pre.log
AARCHVM_PS2_RX_SCRIPT='5000:0x43' ./build/aarchvm -snapshot-load "$PS2_SNAP" -steps 10000 > tests/arm64/out/ps2_snapshot_post.log
test "$(tr -d '\r\n' < tests/arm64/out/ps2_snapshot_pre.log)" = 'AB'
test "$(tr -d '\r\n' < tests/arm64/out/ps2_snapshot_post.log)" = 'CP'
PRE_FAST=tests/arm64/out/predecode_fast.log
PRE_SLOW=tests/arm64/out/predecode_slow.log
./build/aarchvm -bin tests/arm64/out/predecode_dyn_codegen.bin -load 0x0 -entry 0x0 -steps 400000 > "$PRE_FAST"
./build/aarchvm -decode slow -bin tests/arm64/out/predecode_dyn_codegen.bin -load 0x0 -entry 0x0 -steps 400000 > "$PRE_SLOW"
test "$(tr -d '\r\n' < "$PRE_FAST")" = 'AB'
test "$(tr -d '\r\n' < "$PRE_SLOW")" = 'AB'

VA_FAST=tests/arm64/out/predecode_va_fast.log
VA_SLOW=tests/arm64/out/predecode_va_slow.log
./build/aarchvm -bin tests/arm64/out/predecode_va_exec_switch.bin -load 0x0 -entry 0x0 -steps 5000000 > "$VA_FAST"
./build/aarchvm -decode slow -bin tests/arm64/out/predecode_va_exec_switch.bin -load 0x0 -entry 0x0 -steps 5000000 > "$VA_SLOW"
test "$(tr -d '\r\n' < "$VA_FAST")" = 'ABA'
test "$(tr -d '\r\n' < "$VA_SLOW")" = 'ABA'

LS_FAST=tests/arm64/out/predecode_ls_fast.log
LS_SLOW=tests/arm64/out/predecode_ls_slow.log
./build/aarchvm -bin tests/arm64/out/predecode_load_store_min.bin -load 0x0 -entry 0x0 -steps 400000 > "$LS_FAST"
./build/aarchvm -decode slow -bin tests/arm64/out/predecode_load_store_min.bin -load 0x0 -entry 0x0 -steps 400000 > "$LS_SLOW"
test "$(tr -d '\r\n' < "$LS_FAST")" = 'L'
test "$(tr -d '\r\n' < "$LS_SLOW")" = 'L'

LOGIC_FAST=tests/arm64/out/predecode_logic_fast.log
LOGIC_SLOW=tests/arm64/out/predecode_logic_slow.log
./build/aarchvm -bin tests/arm64/out/predecode_logic_min.bin -load 0x0 -entry 0x0 -steps 400000 > "$LOGIC_FAST"
./build/aarchvm -decode slow -bin tests/arm64/out/predecode_logic_min.bin -load 0x0 -entry 0x0 -steps 400000 > "$LOGIC_SLOW"
test "$(tr -d '\r\n' < "$LOGIC_FAST")" = 'P'
test "$(tr -d '\r\n' < "$LOGIC_SLOW")" = 'P'
