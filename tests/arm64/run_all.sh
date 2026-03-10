#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT_DIR"

cmake --build build -j

tests/arm64/build_tests.sh

run() {
  local bin="$1"
  local steps="$2"
  ./build/aarchvm -bin "tests/arm64/out/${bin}" -load 0x0 -entry 0x0 -steps "$steps"
}

run instr_legacy_each.bin 3000000
run mmu_tlb_cache.bin 5000000
run mmu_ttbr1_early.bin 3000000
run mmu_tlb_vae1_scope.bin 4000000
run mmu_ttbr_switch.bin 4000000
run mmu_unmap_data_abort.bin 4000000
run mmu_tlbi_non_target.bin 4000000
run mmu_l2_block_vmalle1.bin 4000000
run mmu_at_tlb_observe.bin 4000000
run mmu_ttbr_asid_mask.bin 4000000
run mmu_perm_ro_write_abort.bin 4000000
run mmu_xn_fetch_abort.bin 4000000
run mmu_table_ap_inherit.bin 4000000
run mmu_table_pxn_inherit.bin 4000000
run mmu_tcr_ips_mair_decode.bin 4000000
run mmu_af_fault.bin 4000000
run sync_exception_regs.bin 2000000
run gic_timer_sysreg.bin 2000000
run bitfield_basic.bin 400000
run p1_core.bin 600000
run hello_uart.bin 4000
run branch_arith.bin 4000
run hello_c.bin 400000
run stack_c.bin 600000
run irq_minimal.bin 1400000
run irq_twice.bin 2400000
run irq_disabled.bin 1200000
run sys_ctrl.bin 1800000
run irq_spsel.bin 1800000
run logic_misc.bin 300000
run mem_ext.bin 300000
run branch_reg.bin 300000
