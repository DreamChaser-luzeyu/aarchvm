#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT_DIR"

set -x

cmake --build build -j

tests/arm64/build_tests.sh

run() {
  local bin="$1"
  local steps="$2"
  ./build/aarchvm -bin "tests/arm64/out/${bin}" -load 0x0 -entry 0x0 -steps "$steps"
}

run_smp() {
  local bin="$1"
  local steps="$2"
  ./build/aarchvm -smp 2 -bin "tests/arm64/out/${bin}" -load 0x0 -entry 0x0 -steps "$steps"
}

run_expect() {
  local bin="$1"
  local steps="$2"
  local expected="$3"
  test "$(./build/aarchvm -bin "tests/arm64/out/${bin}" -load 0x0 -entry 0x0 -steps "$steps" | tr -d '\r\n')" = "$expected"
}

run instr_legacy_each.bin 3000000
run mmu_tlb_cache.bin 5000000
run mmu_ttbr1_early.bin 3000000
run mmu_tlb_vae1_scope.bin 4000000
run mmu_ttbr_switch.bin 4000000
run mmu_unmap_data_abort.bin 4000000
test "$(./build/aarchvm -bin tests/arm64/out/mmu_cache_maint_fault.bin -load 0x0 -entry 0x0 -steps 4000000 | tr -d '\r\n')" = 'M'
run mmu_tlbi_non_target.bin 4000000
run mmu_l2_block_vmalle1.bin 4000000
run mmu_at_tlb_observe.bin 4000000
run mmu_at_el0_permissions.bin 4000000
run mmu_ttbr_asid_mask.bin 4000000
run mmu_perm_ro_write_abort.bin 4000000
run mmu_xn_fetch_abort.bin 4000000
run mmu_cross_page_load.bin 4000000
run mmu_cross_page_store.bin 4000000
run mmu_cross_page_fault_far_load.bin 4000000
run mmu_cross_page_fault_far_store.bin 4000000
run mmu_cross_page_various.bin 4000000
run mmu_cross_page_pair_fault_far.bin 4000000
run mmu_table_ap_inherit.bin 4000000
run mmu_table_pxn_inherit.bin 4000000
run mmu_tcr_ips_mair_decode.bin 4000000
run mmu_af_fault.bin 4000000
run sync_exception_regs.bin 2000000
run_expect exception_daif_entry.bin 300000 D
run_expect eret_clears_exclusive.bin 300000 E
./build/aarchvm -bin tests/arm64/out/nested_sync_depth.bin -load 0x0 -entry 0x0 -steps 400000 | grep -qx 'P'
run gic_timer_sysreg.bin 2000000
run gic_timer_rearm_no_spurious.bin 2000000
run gic_timer_phys_sysreg.bin 2000000
run bitfield_basic.bin 400000
run p1_core.bin 600000
run atomics_minimal.bin 400000
run signext_loads.bin 400000
run signext_postindex.bin 400000
run atomics_small.bin 400000
run mul_high.bin 400000
run pair_non_temporal.bin 400000
run dc_zva.bin 400000
run atomics_sp_base.bin 400000
run ldrsw_regoffset.bin 400000
run signed_regoffset.bin 400000
run ldpsw_pair.bin 400000
run pair_exclusive.bin 400000
run adc_sbc_minimal.bin 200000
run snapshot_resume.bin 200000
run irq_nested_el1_wfi.bin 400000
run hello_uart.bin 4000
run branch_arith.bin 4000
run hello_c.bin 400000
run stack_c.bin 600000
run irq_minimal.bin 1400000
run irq_twice.bin 2400000
run irq_disabled.bin 1200000
run sys_ctrl.bin 1800000
run cntkctl_el1.bin 300000
run cntkctl_el0_timer_access.bin 600000
test "$(./build/aarchvm -bin tests/arm64/out/el0_sysreg_privilege.bin -load 0x0 -entry 0x0 -steps 400000 | tr -d '\r\n')" = 'R'
test "$(./build/aarchvm -bin tests/arm64/out/el0_daif_uma.bin -load 0x0 -entry 0x0 -steps 500000 | tr -d '\r\n')" = 'U'
test "$(./build/aarchvm -bin tests/arm64/out/el0_idspace_undef.bin -load 0x0 -entry 0x0 -steps 600000 | tr -d '\r\n')" = 'D'
test "$(./build/aarchvm -bin tests/arm64/out/el0_special_regs_undef.bin -load 0x0 -entry 0x0 -steps 600000 | tr -d '\r\n')" = 'S'
test "$(./build/aarchvm -bin tests/arm64/out/el0_absent_pstate_features_undef.bin -load 0x0 -entry 0x0 -steps 600000 | tr -d '\r\n')" = 'A'
test "$(./build/aarchvm -bin tests/arm64/out/el0_eret_undef.bin -load 0x0 -entry 0x0 -steps 600000 | tr -d '\r\n')" = 'E'
test "$(./build/aarchvm -bin tests/arm64/out/el0_hvc_smc_undef.bin -load 0x0 -entry 0x0 -steps 600000 | tr -d '\r\n')" = 'H'
test "$(./build/aarchvm -bin tests/arm64/out/el1_hvc_smc_undef.bin -load 0x0 -entry 0x0 -steps 600000 | tr -d '\r\n')" = 'J'
test "$(./build/aarchvm -bin tests/arm64/out/illegal_state_return.bin -load 0x0 -entry 0x0 -steps 600000 | tr -d '\r\n')" = 'I'
test "$(./build/aarchvm -bin tests/arm64/out/el0_tlbi_cache_undef.bin -load 0x0 -entry 0x0 -steps 800000 | tr -d '\r\n')" = 'K'
test "$(./build/aarchvm -bin tests/arm64/out/el0_dc_ivac_undef.bin -load 0x0 -entry 0x0 -steps 600000 | tr -d '\r\n')" = 'V'
test "$(./build/aarchvm -bin tests/arm64/out/dc_cva_persist_absent.bin -load 0x0 -entry 0x0 -steps 600000 | tr -d '\r\n')" = 'P'
test "$(./build/aarchvm -bin tests/arm64/out/sysreg_optional_absent.bin -load 0x0 -entry 0x0 -steps 400000 | tr -d '\r\n')" = 'U'
test "$(./build/aarchvm -bin tests/arm64/out/el0_cache_ops_privilege.bin -load 0x0 -entry 0x0 -steps 600000 | tr -d '\r\n')" = 'C'
test "$(./build/aarchvm -bin tests/arm64/out/el0_wfx_trap.bin -load 0x0 -entry 0x0 -steps 800000 | tr -d '\r\n')" = 'T'
run ldtr_sttr_usercopy.bin 400000
run fpsimd_minimal.bin 400000
run fpsimd_mvni.bin 400000
run fpsimd_logic_more.bin 400000
run fp_scalar_ls.bin 400000
run fp_scalar_regoffset.bin 400000
run fp_scalar_unscaled.bin 400000
run fpsimd_ext.bin 400000
run fp_scalar_elem_ls.bin 400000
run fpsimd_uminp.bin 400000
run fpsimd_umov_lane.bin 400000
run fpsimd_dup_elem.bin 400000
run_expect fpsimd_stringops.bin 600000 W
run_expect fpsimd_bic_imm.bin 200000 W
run fmov_scalar_imm.bin 200000
run fpsimd_scalar_movi.bin 200000
run fmov_scalar_reg.bin 200000
run fp_scalar_arith.bin 300000
run fcmp_e.bin 200000
run fp_scalar_convert.bin 200000
run fp_fcvtzu_scalar.bin 200000
run fp_fcvt_flags.bin 200000
run_expect fp_fcvt_rounding_scalar.bin 300000 R
run_expect fp_int_to_fp_rounding.bin 300000 I
run_expect fp_compare_flags.bin 200000 Q
run_expect fp_scalar_compare_flags.bin 200000 K
run_expect fp_absneg_nan_flags.bin 300000 N
run_expect fpsimd_compare_flags.bin 300000 G
run_expect fp_minmax_nan_flags.bin 300000 M
run_expect fp_minmax_dn.bin 400000 X
run_expect fp_cond_compare.bin 200000 C
run fp_scalar_fcsel.bin 200000
run fp_scalar_fma.bin 200000
run_expect fp_arith_fpcr_flags.bin 300000 A
run_expect fp_fz_arith_compare.bin 300000 F
run_expect fp_fz_minmax.bin 300000 W
run_expect fp_fz_misc.bin 500000 Z
run_expect fp_fz_to_int.bin 400000 T
run fp_scalar_misc.bin 200000
run_expect fp_sqrt_flags.bin 300000 Q
run_expect fp_sqrt_rounding.bin 300000 R
run_expect fp_roundint_flags.bin 300000 I
run_expect fp_dn_arith.bin 400000 T
run_expect fp_dn_misc.bin 400000 D
run_expect fp_scalar_compare_misc.bin 300000 J
run_expect fp_scalar_pairwise.bin 300000 Y
run_expect fp_scalar_frecpx.bin 300000 X
run fpsimd_ins_xtl.bin 300000
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
run fpsimd_arith_shift_perm.bin 300000
run fpsimd_fp_vector.bin 400000
run fpsimd_more_perm_fp.bin 400000
run fpsimd_structured_ls.bin 400000
run_expect fpsimd_structured_ls_more.bin 600000 Y
run_expect fpsimd_structured_lane_ls.bin 800000 Y
run fpsimd_widen_sat.bin 400000
run cpacr_fp_trap.bin 300000
run cpacr_fp_mem_trap.bin 300000
run cpacr_fp_structured_trap.bin 400000
run pstate_pan.bin 200000
run_expect pan_span_exception.bin 300000 S
run_expect id_aa64_feature_regs.bin 200000 I
run mmu_el0_ap_fault.bin 4000000
run mmu_pan_user_access.bin 4000000
run_expect mmu_ldtr_sttr_pan.bin 4000000 U
run mmu_tlb_asid_scope.bin 4000000
run svc_sysreg_minimal.bin 300000
run lse_atomics.bin 400000
run casp_pair.bin 400000
run lse_atomics_narrow.bin 400000
run irq_spsel.bin 1800000
run logic_misc.bin 300000
run mem_ext.bin 300000
run branch_reg.bin 300000
run sp_ccmp_path.bin 200000
run sp_alias_paths.bin 300000
run addsub_shift_more.bin 300000
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

run predecode_dyn_codegen.bin 400000
run predecode_va_exec_switch.bin 5000000
run predecode_load_store_min.bin 400000
run predecode_logic_min.bin 400000
run pl050_basic.bin 400000
run_smp smp_mpidr_boot.bin 200000
run_smp smp_sev_wfe.bin 200000
run_smp smp_ldxr_invalidate.bin 200000
./build/aarchvm -smp 2 -bin tests/arm64/out/smp_ldxr_invalidate_mmu.bin -load 0x0 -entry 0x0 -steps 1200000 | grep -qx 'V'
run_smp smp_spinlock_ldaxr_stlxr.bin 600000
run_smp smp_tlbi_broadcast.bin 1200000
./build/aarchvm -smp 2 -bin tests/arm64/out/smp_wfe_monitor_event.bin -load 0x0 -entry 0x0 -steps 300000 | grep -qx 'M'
./build/aarchvm -smp 2 -bin tests/arm64/out/smp_wfe_store_no_event.bin -load 0x0 -entry 0x0 -steps 300000 | grep -qx 'N'
./build/aarchvm -smp 2 -smp-mode psci -bin tests/arm64/out/psci_cpu_on_min.bin -load 0x0 -entry 0x0 -steps 400000 | grep -qx 'P'
./build/aarchvm -smp 2 -bin tests/arm64/out/smp_gic_sgi.bin -load 0x0 -entry 0x0 -steps 400000 | grep -qx 'G'
./build/aarchvm -smp 2 -bin tests/arm64/out/smp_timer_ppi.bin -load 0x0 -entry 0x0 -steps 400000 | grep -qx 'T'
./build/aarchvm -smp 2 -bin tests/arm64/out/smp_timer_rate.bin -load 0x0 -entry 0x0 -steps 400000 | grep -qx 'R'
./build/aarchvm -smp 2 -bin tests/arm64/out/smp_dc_zva_invalidate.bin -load 0x0 -entry 0x0 -steps 400000 | grep -qx 'D'
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
