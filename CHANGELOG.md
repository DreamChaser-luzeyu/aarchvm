# 修改日志 2026-03-15 11:08

## 本轮修改

- 修正向量 `FABD` 与向量 `FADD/FSUB` 的解码冲突，避免 `FABD` 被宽松掩码误判成 `FSUB`。
- 新增并验证一组更完整的 FP/AdvSIMD 裸机单测 `tests/arm64/fpsimd_more_perm_fp.S`，覆盖：
  - `ZIP2` / `UZP2` / `TRN2`
  - 向量 `FMLA` / `FMLS`
  - 向量 `FMINNM` / `FMAXNM`
  - 向量 `FABD` / `FACGE` / `FACGT`
- 完成 `CPACR_EL1.FPEN` 的最小程序可见语义：
  - 当前 EL 为 EL1 且 `FPEN` 为 `00`/`10` 时，对 FP/AdvSIMD 访问触发 `EC=0x07` 同步异常。
  - 当前 EL 为 EL0 且 `FPEN != 11` 时，对 FP/AdvSIMD 访问触发 `EC=0x07` 同步异常。
  - 覆盖 FP/AdvSIMD 数据处理、SIMD/FP load-store，以及 `FPCR` / `FPSR` 系统寄存器访问。
- 新增 `tests/arm64/cpacr_fp_trap.S`，验证：
  - EL1 trap-all 路径。
  - EL0 trap-only 路径。
  - 重新放开 `FPEN` 后 EL0 正常执行 `fmov` 与 `mrs fpcr`。
- 补齐上一轮 MMU/TLB 改动的验证闭环，当前裸机测试已覆盖：
  - `ID_AA64*` 特性寄存器回报。
  - EL0/EL1 权限检查与 PAN。
  - ASID 感知的 TLB 命中与定向 `TLBI`。
  - `TTBR` 切换下的 ASID 语义。
- 修复 `tests/linux/build_usertests_rootfs.sh` 生成的 `/init` 脚本兼容性问题，去掉对当前 BusyBox shell 不稳定的参数展开写法，恢复 Linux 功能回归脚本可执行性。

## 本轮测试

- `cmake --build build -j4`
- `tests/arm64/build_tests.sh`
- 目标单测：
  - `fpsimd_more_perm_fp.bin`
  - `cpacr_fp_trap.bin`
  - `id_aa64_feature_regs.bin`
  - `mmu_tlb_asid_scope.bin`
- 裸机全回归：`tests/arm64/run_all.sh`
- Linux 功能回归：`tests/linux/run_functional_suite.sh`
- Linux 算法性能回归：`tests/linux/run_algorithm_perf.sh`

## 当前结论

- 这一轮主要是“程序可见正确性补齐”，不是新的性能优化轮次。
- 当前最可信的热点仍集中在：`Cpu::translate_address()`、`Cpu::step()`、`Cpu::lookup_decoded()`、`Cpu::exec_load_store()`、`Cpu::exec_data_processing()` 与 `BusFastPath::read()`。
- Linux 用户态功能与裸机回归在本轮结束时均通过。
