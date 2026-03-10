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
build_asm_prog irq_spsel
build_asm_prog instr_legacy_each
build_asm_prog mmu_tlb_cache
build_asm_prog mmu_ttbr1_early
build_asm_prog mmu_tlb_vae1_scope
build_asm_prog mmu_ttbr_switch
build_asm_prog mmu_unmap_data_abort
build_asm_prog mmu_tlbi_non_target
build_asm_prog mmu_l2_block_vmalle1
build_asm_prog mmu_at_tlb_observe
build_asm_prog mmu_ttbr_asid_mask
build_asm_prog mmu_perm_ro_write_abort
build_asm_prog mmu_xn_fetch_abort
build_asm_prog mmu_table_ap_inherit
build_asm_prog mmu_table_pxn_inherit
build_asm_prog mmu_tcr_ips_mair_decode
build_asm_prog mmu_af_fault
build_asm_prog sync_exception_regs
build_asm_prog gic_timer_sysreg
build_asm_prog bitfield_basic
build_asm_prog p1_core

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
