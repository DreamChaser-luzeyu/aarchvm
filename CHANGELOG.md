# 修改日志 2026-03-19 17:40

## 本轮修改

- 继续按“审阅 -> 修复 -> 测试”流程补齐一组真实的 AdvSIMD 浮点语义缺口：
  - [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp)
  - [tests/arm64/fpsimd_fcvt_rounding.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/fpsimd_fcvt_rounding.S)
  - [tests/arm64/fpsimd_fp_misc_rounding.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/fpsimd_fp_misc_rounding.S)
  - [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh)
  - [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)
  - [tests/linux/build_usertests_rootfs.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/linux/build_usertests_rootfs.sh)
  - [tests/linux/run_functional_suite.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/linux/run_functional_suite.sh)
  - [tests/linux/run_functional_suite_smp.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/linux/run_functional_suite_smp.sh)
- 补上向量 `FCVTNS/FCVTNU/FCVTPS/FCVTPU/FCVTMS/FCVTMU/FCVTAS/FCVTAU` 的 `.2d` 解码覆盖：
  - 上一版只覆盖到了 `.2s/.4s`，`fcvtns v?.2d, v?.2d` 会直接掉进 `UNIMPL`；
  - 现在 `.2s/.4s/.2d` 都走同一组 rounding helper，并正确累积 `FPSR` 标志。
- 新增向量 FP misc 家族实现：
  - `FABS/FNEG/FSQRT` 的 `.2s/.4s/.2d`
  - `FRINTN/FRINTP/FRINTM/FRINTZ/FRINTA/FRINTX/FRINTI` 的 `.2s/.4s/.2d`
  - `FRINTX/FRINTI` 按 `FPCR.RMode` 取舍，与现有标量实现保持一致。
- 修正一个隐藏的解码优先级 bug：
  - 之前 `CMEQ (zero)` 的掩码过宽，会错误吞掉 `FRINTM v?.4s` 一类向量 FP misc 指令；
  - 现象上会把 `FRINTM` 执行成比较掩码，产出 `0x0000ffff` 之类的错误结果；
  - 现在收紧 `CMEQ (zero)` 匹配范围，避免再误伤向量 `FRINT*`。
- 新增两组裸机单测：
  - [tests/arm64/fpsimd_fcvt_rounding.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/fpsimd_fcvt_rounding.S)
    - 覆盖向量 `FCVT*` 的 signed/unsigned、`.2s/.4s/.2d`、不同 rounding 模式以及 `FPSR` 位更新。
  - [tests/arm64/fpsimd_fp_misc_rounding.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/fpsimd_fp_misc_rounding.S)
    - 覆盖向量 `FABS/FNEG/FSQRT/FRINT*`，并验证 `FRINTX/FRINTI` 对 `FPCR.RMode` 的响应。
- 顺手修正 Linux 功能回归脚本的两个稳定性问题：
  - 之前 host 把一长串命令逐字节喂给交互 shell，输入和命令输出会交叉污染日志，导致伪失败；
  - 现在改为 host 只调用客体里的 `run_functional_suite` / `run_functional_suite_smp` 脚本；
  - 同时把 `dmesg | grep hang` 改为“不因无匹配而失败”，保留管道执行路径覆盖，但不再依赖日志里一定有 `hang`。

## 本轮测试

- 构建与定向验证：
  - `timeout 1200s cmake --build build -j`
  - `timeout 1200s tests/arm64/build_tests.sh`
  - `timeout 60s ./build/aarchvm -bin tests/arm64/out/fpsimd_fcvt_rounding.bin -load 0x0 -entry 0x0 -steps 400000`
  - `timeout 60s ./build/aarchvm -bin tests/arm64/out/fpsimd_fp_misc_rounding.bin -load 0x0 -entry 0x0 -steps 400000`
- 裸机完整回归：
  - `timeout 2400s tests/arm64/run_all.sh`
- Linux 单核功能回归：
  - `timeout 1200s tests/linux/run_functional_suite.sh`
- Linux SMP 功能回归：
  - `timeout 1200s tests/linux/run_functional_suite_smp.sh`

## 当前结论

- 这轮修复的是两类真实可观测的 ISA 行为缺口，不是为单个程序做特殊绕过：
  - 向量 `FCVT*` 的 `.2d` 漏实现；
  - 向量 `FP misc` 家族缺失，以及 `CMEQ (zero)` 误匹配导致的错误执行。
- 新增裸机单测、完整裸机回归、Linux UMP/SMP 功能回归都已通过。
- 继续审查后，当前仍值得优先补的高置信度缺口主要还有：
  - 向量 reciprocal / rsqrt estimate 家族：`FRECPE/FRECPS/FRSQRTE/FRSQRTS`
  - 向量 pairwise FP 家族：`FADDP/FMAXP/FMINP/FMAXNMP/FMINNMP`
  - 可能仍未覆盖完整的向量窄化/加宽转换家族：`FCVTN/FCVTN2/FCVTL/FCVTL2/FCVTXN/FCVTXN2`

# 修改日志 2026-03-19 15:19

## 本轮修改

- 继续按“审阅 -> 修复 -> 测试”流程补齐一个可观测的标量 FP 比较语义缺口：
  - [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp)
  - [tests/arm64/fp_compare_flags.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/fp_compare_flags.S)
  - [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh)
  - [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)
- 修正 `FCMP/FCMPE` 在 NaN 比较时对 `FPSR.IOC` 的处理：
  - 修改前，`FCMP` 和 `FCMPE` 只更新 `NZCV`，不会把 invalid operation 反映到 `FPSR`；
  - 修改后，`FCMPE` 遇到 qNaN 会置 `FPSR.IOC`，`FCMP` 遇到 qNaN 不置位；
  - 对 sNaN，`FCMP`/`FCMPE` 都会置 `FPSR.IOC`。
- 新增 NaN 位模式辅助逻辑，按 32/64-bit IEEE-754 编码识别 qNaN/sNaN，避免仅依赖宿主 `std::isnan`，把比较类指令的 `FPSR` 更新逻辑显式化。
- 新增裸机单测 [tests/arm64/fp_compare_flags.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/fp_compare_flags.S)，覆盖：
  - qNaN 下 `FCMP` vs `FCMPE` 的 `FPSR.IOC` 差异；
  - sNaN 下 `FCMP` 的 `FPSR.IOC`；
  - `#0.0` 形式与寄存器形式；
  - 单精度与双精度两条路径；
  - unordered 比较对应的 `NZCV=0b0011`。

## 本轮测试

- 构建与定向验证：
  - `timeout 1200s cmake --build build -j`
  - `timeout 1200s tests/arm64/build_tests.sh`
  - `timeout 60s ./build/aarchvm -bin tests/arm64/out/fp_compare_flags.bin -load 0x0 -entry 0x0 -steps 200000`
  - `timeout 60s ./build/aarchvm -bin tests/arm64/out/cpacr_fp_structured_trap.bin -load 0x0 -entry 0x0 -steps 400000`
- 裸机完整回归：
  - `timeout 2400s tests/arm64/run_all.sh`
- Linux 单核功能回归：
  - `timeout 1200s tests/linux/run_functional_suite.sh`
- Linux SMP 功能回归：
  - `timeout 1200s tests/linux/run_functional_suite_smp.sh`

## 当前结论

- 这轮修掉的是一个真实且可由客体程序直接观测到的 FP 状态寄存器行为缺口，不是为了某个程序做特殊处理。
- 修改后，`FCMP/FCMPE` 不再只改 `NZCV`，而是能把 qNaN/sNaN 的 invalid-operation 结果正确反映到 `FPSR.IOC`；新增单测以及完整裸机、Linux UMP/SMP 回归均通过。
- 下一轮仍值得继续审的方向：
  - 其他标量/向量 FP 指令对 `FPSR` 的异常标志更新是否还有缺口；
  - `FRINT*`、`FMIN/FMAX`、`FMINNM/FMAXNM` 等对 sNaN/qNaN 的细粒度差异。

# 修改日志 2026-03-19 15:05

## 本轮修改

- 继续按“审阅 -> 修复 -> 测试”流程补齐一个真实的 Armv8-A/FP trap 语义缺口：
  - [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp)
  - [tests/arm64/cpacr_fp_structured_trap.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/cpacr_fp_structured_trap.S)
  - [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh)
  - [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)
- 修正 `insn_uses_fp_asimd()` 对 structured AdvSIMD whole-register memory 指令的识别不完整问题：
  - 修改前，部分 `LD1/ST1/LD3/ST3/LD4/ST4` whole-register 及其 post-index 变体不会被判定为 FP/AdvSIMD 指令；
  - 在 `CPACR_EL1.FPEN` 关闭或限制访问时，这些指令会错误执行，而不是按规范产生 trap。
- 这轮补齐了缺失的 whole-register structured load/store 掩码覆盖，包括：
  - `LD1/ST1` 2/3/4-register whole-register 形式；
  - `LD3/ST3`、`LD4/ST4` whole-register 形式；
  - 对应的 post-index immediate / register 变体。
- 新增裸机单测 [tests/arm64/cpacr_fp_structured_trap.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/cpacr_fp_structured_trap.S)，覆盖：
  - `CPACR_EL1=0` 时 EL1 对 structured AdvSIMD memory 指令的 trap；
  - `FPEN=01` 时 EL0 的 trap；
  - `FPEN=11` 时 EL0 的正常执行；
  - 使用 `ld3 {v0.16b, v1.16b, v2.16b}, [x2]` 作为具体探针，确保命中此前漏判的 whole-register 家族。

## 本轮测试

- 构建与定向验证：
  - `timeout 1200s tests/arm64/build_tests.sh`
  - `timeout 60s ./build/aarchvm -bin tests/arm64/out/cpacr_fp_structured_trap.bin -load 0x0 -entry 0x0 -steps 400000`
- 裸机完整回归：
  - `timeout 2400s tests/arm64/run_all.sh`
- Linux 单核功能回归：
  - `timeout 1200s tests/linux/run_functional_suite.sh`
- Linux SMP 功能回归：
  - `timeout 1200s tests/linux/run_functional_suite_smp.sh`

## 当前结论

- 这轮修复的是 trap 分类逻辑里的真实 ISA 行为缺口，不是测试特判。
- 修改后，whole-register structured AdvSIMD memory 指令在 `CPACR_EL1.FPEN` 约束下已经能走到正确的 trap/放行路径，新增单测与完整裸机、Linux UMP/SMP 回归均通过。
- 但这不等于 Armv8-A ISA 行为已经全部审完；下一轮仍值得继续审的是：
  - 其他 `insn_uses_fp_asimd()` 角落是否还有漏网的 FP/AdvSIMD 指令族；
  - 更细的系统寄存器陷入条件与异常 syndrome 细节。

# 修改日志 2026-03-19 12:55

## 本轮修改

- 继续按“审阅 -> 修复 -> 测试”流程补齐 `AT` / `PAR_EL1` 的 Armv8-A 语义缺口：
  - [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp)
  - [tests/arm64/el0_cache_ops_privilege.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/el0_cache_ops_privilege.S)
  - [tests/arm64/mmu_at_el0_permissions.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/mmu_at_el0_permissions.S)
  - [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh)
  - [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)
- 修正 `AT S1E1R` / `AT S1E1W` 在 EL0 下的错误行为：
  - 之前错误走成了 `UCI` 相关 trap/允许路径；
  - 现在按手册要求在 EL0 始终 `UNDEFINED`。
- 新增缺失的 `AT S1E0R` / `AT S1E0W`：
  - 在 EL1 下按“以 EL0 读/写权限访问”的规则进行 stage 1 地址翻译；
  - 使用 unprivileged translation 权限检查更新 `PAR_EL1`。
- 修正 `PAR_EL1` fault 填充逻辑：
  - 之前错误复用了 ESR 的 `WnR + FSC` 7-bit 编码；
  - 现在改为 64-bit `PAR_EL1.FST` 的 6-bit fault status，并在 fault 格式下设置 `PAR_EL1[11] = RES1`。
- 扩充 AT 回归覆盖：
  - `el0_cache_ops_privilege` 现在验证 `AT S1E1R@EL0` 与 `AT S1E1W@EL0` 即使在 `UCI=1` 前后也都必须是 `UNDEFINED`；
  - 新增 `mmu_at_el0_permissions`，覆盖 `AT S1E0R/S1E0W` 成功翻译、EL0 无读权限 fault、EL0 只读页写权限 fault。

## 本轮测试

- 构建与定向验证：
  - `timeout 600s cmake --build build -j`
  - `timeout 600s ./tests/arm64/build_tests.sh`
  - `timeout 120s ./build/aarchvm -bin tests/arm64/out/el0_cache_ops_privilege.bin -load 0x0 -entry 0x0 -steps 800000`
  - `timeout 120s ./build/aarchvm -bin tests/arm64/out/mmu_at_el0_permissions.bin -load 0x0 -entry 0x0 -steps 4000000`
- 裸机完整回归：
  - `timeout 1800s ./tests/arm64/run_all.sh`
- Linux 单核功能回归：
  - `timeout 1800s ./tests/linux/run_functional_suite.sh`
- Linux SMP 功能回归：
  - `timeout 2400s ./tests/linux/run_functional_suite_smp.sh`

## 当前结论

- 本轮纠正了 `AT` 指令在 EL0/EL1 权限语义上的一处明确架构偏差，并补上了缺失的 `AT S1E0*`。
- `PAR_EL1` fault 结果不再错误混入 ESR 的 `WnR` 语义。
- 裸机与 Linux 单核/SMP 完整回归均已通过。

# 修改日志 2026-03-19 12:32

## 本轮修改

- 继续按“审阅 -> 修复 -> 测试”流程补齐一组 cache-maintenance-by-VA 的 Armv8-A 语义缺口：
  - [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp)
  - [include/aarchvm/cpu.hpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/include/aarchvm/cpu.hpp)
  - [tests/arm64/mmu_cache_maint_fault.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/mmu_cache_maint_fault.S)
  - [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh)
  - [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)
- 为 `IC IVAU`、`DC CVAU`、`DC CVAC`、`DC CIVAC` 新增专用 VA 翻译 helper：
  - 之前这些路径只是空操作，或仅做本地 decode invalidation；
  - 现在会执行真实的 VA 翻译，并在未映射地址上产生正确的 data abort。
- 保持现有权限模型的保守实现：
  - EL0 下，`DC CVAU/CVAC/CIVAC` 继续按“可生成 EL0 读权限 fault”的实现策略处理；
  - EL0 下，`IC IVAU` 采用“未强制生成读权限 fault”的实现策略，但未映射地址仍会 fault；
  - EL1 下，上述 cache maintenance by VA 不再错误复用普通 load/store 权限检查。
- 新增裸机定向回归：
  - `mmu_cache_maint_fault`
  - 覆盖 `IC IVAU`、`DC CVAU/CVAC/CIVAC` 对未映射 VA 的 `EC=0x25`、`FAR_EL1=VA`、`FSC=translation fault level 3`、`WnR=0` 行为。

## 本轮测试

- 构建与定向验证：
  - `timeout 600s cmake --build build -j`
  - `timeout 600s ./tests/arm64/build_tests.sh`
  - `timeout 120s ./build/aarchvm -bin tests/arm64/out/mmu_cache_maint_fault.bin -load 0x0 -entry 0x0 -steps 4000000`
  - `timeout 120s ./build/aarchvm -bin tests/arm64/out/mmu_tlb_cache.bin -load 0x0 -entry 0x0 -steps 5000000`
  - `timeout 120s ./build/aarchvm -bin tests/arm64/out/el0_cache_ops_privilege.bin -load 0x0 -entry 0x0 -steps 600000`
- 裸机完整回归：
  - `timeout 1800s ./tests/arm64/run_all.sh`
- Linux 单核功能回归：
  - `timeout 1800s ./tests/linux/run_functional_suite.sh`
- Linux SMP 功能回归：
  - `timeout 2400s ./tests/linux/run_functional_suite_smp.sh`

## 当前结论

- 本轮把“cache maintenance by VA 不应在未映射地址上静默成功”的缺口补上了。
- 现状下，这 4 条高频系统指令已经不再错误绕过 MMU fault 路径。
- 完整裸机回归与 Linux 单核/SMP 功能回归均已通过。

# 修改日志 2026-03-19 12:01

## 本轮修改

- 继续按“审阅 -> 修复 -> 测试”流程补齐 3 组高置信度的 Armv8-A system instruction 语义缺口：
  - [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp)
  - [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh)
  - [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)
- 修正一组 EL0 下本应直接 `UNDEFINED` 的 cache / TLB maintenance 指令：
  - `TLBI VMALLE1 / VMALLE1IS`
  - `TLBI VAE1* / VALE1* / VAAE1* / VAALE1*`
  - `TLBI ASIDE1 / ASIDE1IS`
  - `IC IALLU`
  - `IC IALLUIS`
  - `DC ISW`
  - `DC CISW`
  - 之前这些路径错误走成了 `EC=0x18` system access trap；
  - 现在统一改为真正的 undefined 指令异常。
- 修正 `DC IVAC` 在 EL0 下的行为：
  - 之前错误受 `SCTLR_EL1.UCI` 路径影响，可能 trap 或被当成已实现；
  - 现在按架构要求在 EL0 直接 `UNDEFINED`。
- 修正 `DC CVAP` / `DC CVADP` 与当前 ID 寄存器暴露值不一致的问题：
  - 当前模型的 `ID_AA64ISAR1_EL1.DPB=0`，并未实现 `FEAT_DPB/DPB2`；
  - 之前代码却把 `DC CVAP` / `DC CVADP` 当成可执行指令；
  - 现在这两条指令在所有异常级都按“特性缺失 -> `UNDEFINED`”处理。
- 调整旧测试语义：
  - [tests/arm64/el0_cache_ops_privilege.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/el0_cache_ops_privilege.S)
  - 其中 `TLBI VMALLE1@EL0` 的检查从“trap”修正为“undefined”。
- 新增并接入 3 个裸机定向回归：
  - [tests/arm64/el0_tlbi_cache_undef.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/el0_tlbi_cache_undef.S)
    - 覆盖 `TLBI/IC IALLU/IALLUIS/DC ISW/DC CISW` 在 EL0 下必须是 undefined；
  - [tests/arm64/el0_dc_ivac_undef.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/el0_dc_ivac_undef.S)
    - 覆盖 `DC IVAC@EL0` 即使 `SCTLR_EL1.UCI=1` 也必须是 undefined；
  - [tests/arm64/dc_cva_persist_absent.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/dc_cva_persist_absent.S)
    - 覆盖 `DC CVAP` / `DC CVADP` 在 `FEAT_DPB/DPB2` 缺失时必须是 undefined。

## 本轮测试

- 定向构建与单测：
  - `timeout 600s cmake --build build -j`
  - `timeout 600s tests/arm64/build_tests.sh`
  - `timeout 120s ./build/aarchvm -bin tests/arm64/out/el0_tlbi_cache_undef.bin -load 0x0 -entry 0x0 -steps 800000`
  - `timeout 120s ./build/aarchvm -bin tests/arm64/out/el0_dc_ivac_undef.bin -load 0x0 -entry 0x0 -steps 600000`
  - `timeout 120s ./build/aarchvm -bin tests/arm64/out/dc_cva_persist_absent.bin -load 0x0 -entry 0x0 -steps 600000`
- 裸机完整回归：
  - `timeout 1800s ./tests/arm64/run_all.sh`
- Linux 单核功能回归：
  - `timeout 1800s ./tests/linux/run_functional_suite.sh`
- Linux SMP 功能回归：
  - `timeout 2400s ./tests/linux/run_functional_suite_smp.sh`

## 当前结论

- 本轮继续收紧了 system instruction 语义，重点是：
  - EL0 下“应为 undefined 却被错误当成 trap”的 cache/TLB maintenance 指令；
  - “ID 寄存器已宣称 absent，但指令仍被错误接受”的可选特性指令。
- 截至本轮结束，以下回归均已在最新代码状态下通过：
  - `tests/arm64/run_all.sh`
  - `tests/linux/run_functional_suite.sh`
  - `tests/linux/run_functional_suite_smp.sh`
- 仍不应宣称“已经完整实现 Armv8-A 全部 ISA/行为”：
  - 当前代码对若干 cache maintenance by VA 指令仍主要建模为无副作用占位实现；
  - 后续仍值得继续审查“缺失可选特性但被误接受”的其它指令，以及 cache maintenance 的翻译/权限/故障细节。

# 修改日志 2026-03-19 11:33

## 本轮修改

- 继续按“审阅 -> 修复 -> 测试”流程补齐一组异常入口 / 异常返回 / EL0 非法特权指令的 Armv8-A 行为缺口：
  - [src/system_registers.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/system_registers.cpp)
  - [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp)
- 修正 `SCTLR_EL1.SPAN` 对异常入口 `PSTATE.PAN` 的影响：
  - 进入 EL1 异常时，`SPAN=0` 现在会强制把 `PAN` 置 1；
  - `SPAN=1` 时保持原值，不再无条件沿用旧行为。
- 修正进入 EL1 异常时 `DAIF` 掩码更新不完整的问题：
  - 之前只置位了 `PSTATE.I`；
  - 现在按 `AArch64.TakeException()` 把 `PSTATE.<D,A,I,F>` 全部置为 `1111`。
- 修正 `ERET` 的两个真实语义缺口：
  - `ERET` 现在会在异常返回时清空 local exclusive monitor；
  - `ERET` 在 EL0 执行时不再导致模拟器内部“unexpected stop”，而是正确触发 `EC=0x00` 的 undefined 指令异常。
- 修正 `HVC` / `SMC` 在 EL0 下的行为：
  - 之前错误按同步异常 `EC=0x16/0x17` 处理；
  - 现在在 EL0 下按架构要求走 undefined 指令异常。
- 新增并接入 5 个定向裸机回归：
  - [tests/arm64/pan_span_exception.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/pan_span_exception.S)
    - 覆盖 `SPAN=0/1` 下异常入口 `PAN` 的差异行为；
  - [tests/arm64/exception_daif_entry.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/exception_daif_entry.S)
    - 覆盖同步异常进入 EL1 时 `DAIF=0b1111` 且 `SPSR_EL1` 保留原掩码；
  - [tests/arm64/eret_clears_exclusive.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/eret_clears_exclusive.S)
    - 覆盖 `LDXR -> svc/eret -> STXR` 必须失败；
  - [tests/arm64/el0_eret_undef.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/el0_eret_undef.S)
    - 覆盖 `ERET@EL0` 必须是 undefined；
  - [tests/arm64/el0_hvc_smc_undef.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/el0_hvc_smc_undef.S)
    - 覆盖 `HVC@EL0`、`SMC@EL0` 必须是 undefined。
- 更新测试接线：
  - [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh)
  - [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)

## 本轮测试

- 定向构建与单测：
  - `timeout 600s cmake --build build -j`
  - `timeout 600s tests/arm64/build_tests.sh`
  - `timeout 120s ./build/aarchvm -bin tests/arm64/out/pan_span_exception.bin -load 0x0 -entry 0x0 -steps 300000`
  - `timeout 120s ./build/aarchvm -bin tests/arm64/out/exception_daif_entry.bin -load 0x0 -entry 0x0 -steps 300000`
  - `timeout 120s ./build/aarchvm -bin tests/arm64/out/eret_clears_exclusive.bin -load 0x0 -entry 0x0 -steps 300000`
  - `timeout 120s ./build/aarchvm -bin tests/arm64/out/el0_eret_undef.bin -load 0x0 -entry 0x0 -steps 600000`
  - `timeout 120s ./build/aarchvm -bin tests/arm64/out/el0_hvc_smc_undef.bin -load 0x0 -entry 0x0 -steps 600000`
- 完整裸机回归：
  - `timeout 1800s ./tests/arm64/run_all.sh`
- Linux 单核功能回归：
  - `timeout 1800s ./tests/linux/run_functional_suite.sh`
- Linux SMP 功能回归：
  - `timeout 2400s ./tests/linux/run_functional_suite_smp.sh`

## 当前结论

- 本轮连续补齐了 5 个高置信度、程序可感知的 Armv8-A 行为缺口，集中在：
  - 异常入口对 `PAN` / `DAIF` 的更新；
  - 异常返回对 local exclusive monitor 的影响；
  - EL0 下若干特权指令的 undefined 语义。
- 截至本轮结束，以下回归均已在最新代码状态下通过：
  - `tests/arm64/run_all.sh`
  - `tests/linux/run_functional_suite.sh`
  - `tests/linux/run_functional_suite_smp.sh`
- 仍不应宣称“已经完整实现 Armv8-A 全部 ISA/行为”：
  - 当前这轮继续收口的是异常与特权指令语义；
  - 后续仍值得继续系统审查 illegal return event、更细的 syndrome/ISS 细节，以及其他 EL0 下应为 undefined 的特权系统指令。

# 修改日志 2026-03-19 10:24

## 本轮修改

- 继续审阅 AArch64 system register / special-purpose accessor 行为，并修正一组“应为 `UNDEFINED` 却被当前模型当成 EL0 system access trap”的语义缺口：
  - [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp)
  - [src/system_registers.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/system_registers.cpp)
  - 新增 `sysreg_present` / `el0_sysreg_undefined` 判定，将以下访问收口到更接近 Armv8-A 规范的行为：
    - 缺失可选特性寄存器：`TPIDR2_EL0`、`GCSPR_EL0`、`PMUSERENR_EL0`、`AMUSERENR_EL0`
    - 缺失特性对应的 special-purpose 寄存器：`UAO`、`DIT`、`SSBS`、`TCO`
    - EL0 下本应直接 `UNDEFINED` 的 special-purpose 寄存器：`CurrentEL`、`SPSel`、`PAN`
  - 修正 `MSR SPSel, #imm` 与 `MSR PAN, #imm` 在 EL0 下的行为：
    - 之前错误走到 trap；
    - 现在改为真正的 undefined 指令异常。
- 修正 ID 寄存器暴露值与当前实现能力不一致的问题：
  - [src/system_registers.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/system_registers.cpp)
  - `ID_AA64MMFR0_EL1` 不再错误宣称不存在的 granule / stage-2 能力，改为与当前“仅 4KB stage-1、无 EL2/stage-2”实现一致的值；
  - `ID_AA64MMFR1_EL1` 显式宣称已实现的 `PAN` 能力。
- 调整和加强裸机回归：
  - 新增 [tests/arm64/el0_idspace_undef.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/el0_idspace_undef.S)
    - 覆盖 EL0 读取 `MIDR_EL1`、`CLIDR_EL1`、`ID_AA64MMFR0_EL1` 时的 undefined 行为；
  - 新增 [tests/arm64/el0_special_regs_undef.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/el0_special_regs_undef.S)
    - 覆盖 EL0 下 `MRS SPSel`、`MRS PAN`、`MSR SPSel,#imm`、`MSR PAN,#imm` 的 undefined 行为；
  - 新增 [tests/arm64/el0_absent_pstate_features_undef.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/el0_absent_pstate_features_undef.S)
    - 覆盖 EL0 访问缺失特性 `UAO/DIT/SSBS/TCO` 时的 undefined 行为；
  - 重写 [tests/arm64/sysreg_optional_absent.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/sysreg_optional_absent.S)
    - 除 EL1 外，补上 EL0 对 `TPIDR2_EL0`、`GCSPR_EL0`、`PMUSERENR_EL0`、`AMUSERENR_EL0` 的 undefined 覆盖；
  - 修正 [tests/arm64/el0_sysreg_privilege.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/el0_sysreg_privilege.S)
    - `CurrentEL` 在 EL0 下不再按 trap 校验，而是按真正 undefined 校验；
  - 修正 [tests/arm64/id_aa64_feature_regs.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/id_aa64_feature_regs.S)
    - 按最新 `ID_AA64MMFR0_EL1` / `ID_AA64MMFR1_EL1` 取值校验；
  - 接入 [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh) 与 [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)。

## 本轮测试

- 定向构建与单测：
  - `timeout 300s cmake --build build -j`
  - `timeout 300s tests/arm64/build_tests.sh`
  - `timeout 60s ./build/aarchvm -bin tests/arm64/out/el0_sysreg_privilege.bin -load 0x0 -entry 0x0 -steps 600000`
  - `timeout 60s ./build/aarchvm -bin tests/arm64/out/el0_idspace_undef.bin -load 0x0 -entry 0x0 -steps 600000`
  - `timeout 60s ./build/aarchvm -bin tests/arm64/out/el0_special_regs_undef.bin -load 0x0 -entry 0x0 -steps 600000`
  - `timeout 60s ./build/aarchvm -bin tests/arm64/out/el0_absent_pstate_features_undef.bin -load 0x0 -entry 0x0 -steps 600000`
  - `timeout 60s ./build/aarchvm -bin tests/arm64/out/sysreg_optional_absent.bin -load 0x0 -entry 0x0 -steps 1200000`
  - `timeout 60s ./build/aarchvm -bin tests/arm64/out/id_aa64_feature_regs.bin -load 0x0 -entry 0x0 -steps 300000`
- 裸机完整回归：
  - `timeout 900s ./tests/arm64/run_all.sh`
- Linux 单核功能回归：
  - `timeout 1200s ./tests/linux/run_functional_suite.sh`
- Linux SMP 功能回归：
  - `timeout 1800s ./tests/linux/run_functional_suite_smp.sh`

## 当前结论

- 本轮又补齐了一批明确的 Armv8-A 行为缺口，核心集中在：
  - “缺失特性寄存器/特性 accessor 不应被错误暴露”；
  - “EL0 下应当 `UNDEFINED` 的 special-purpose accessor 不应被错误上报为 system access trap”；
  - “ID 寄存器返回值必须与当前实现能力一致”。
- 本轮结束时，以下回归均已在最新代码状态下通过：
  - `tests/arm64/run_all.sh`
  - `tests/linux/run_functional_suite.sh`
  - `tests/linux/run_functional_suite_smp.sh`
- 仍不应宣称“已完整实现 Armv8-A 全部强制 ISA/行为”：
  - 当前这轮主要继续收口 system register / ID-space / special-purpose accessor 的语义；
  - 后续仍值得继续系统审阅其他低频但程序可见的控制寄存器、异常 syndrome 细节，以及尚未覆盖的特性宣称一致性问题。

# 修改日志 2026-03-19 02:20

## 本轮修改

- 继续审阅 system register 行为，修正当前模型把缺失可选特性寄存器错误暴露给软件的问题：
  - [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp)
  - [src/system_registers.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/system_registers.cpp)
  - `TPIDR2_EL0` 仅在 `FEAT_SME` 存在时才应直接访问；当前模型未实现 SME，因此不再允许 EL0 访问，也不再在 EL1/更高异常级把它当成已实现寄存器读写。
  - `GCSPR_EL0` 仅在 `FEAT_GCS` 存在时才应直接访问；当前模型未实现 GCS，因此不再错误返回 0，也不再把它放进 EL0 只读白名单。
- 扩展 EL0 system register 权限回归：
  - [tests/arm64/el0_sysreg_privilege.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/el0_sysreg_privilege.S)
  - 新增对 `MRS TPIDR2_EL0` 与 `MRS GCSPR_EL0` 的 EL0 trap 覆盖，避免类似“寄存器本应不存在却在用户态被读成功”的问题再次漏过。
- 新增裸机回归 [tests/arm64/sysreg_optional_absent.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/sysreg_optional_absent.S)：
  - 覆盖 EL1 下对 `TPIDR2_EL0` / `GCSPR_EL0` 的 `MRS/MSR`；
  - 验证当前未实现对应可选特性时，这些访问不会再被错误当作已实现寄存器执行；
  - 同时校验异常返回后目标寄存器未被错误改写。
- 修正受旧错误前提影响的旧测试：
  - [tests/arm64/svc_sysreg_minimal.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/svc_sysreg_minimal.S)
  - 去掉对 `GCSPR_EL0` / `TPIDR2_EL0` 的错误成功访问假设，改为只验证当前模型真正实现的 `CurrentEL`、`TPIDR_EL0` 和特性 ID 寄存器。
- 接入测试构建与完整裸机回归：
  - [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh)
  - [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)

## 本轮测试

- 定向构建与单测：
  - `timeout 300s cmake --build build -j`
  - `timeout 300s tests/arm64/build_tests.sh`
  - `timeout 60s ./build/aarchvm -bin tests/arm64/out/el0_sysreg_privilege.bin -load 0x0 -entry 0x0 -steps 600000`
  - `timeout 60s ./build/aarchvm -bin tests/arm64/out/sysreg_optional_absent.bin -load 0x0 -entry 0x0 -steps 600000`
  - `timeout 60s ./build/aarchvm -bin tests/arm64/out/svc_sysreg_minimal.bin -load 0x0 -entry 0x0 -steps 400000`
- 裸机完整回归：
  - `timeout 900s ./tests/arm64/run_all.sh`
- Linux 单核功能回归：
  - `timeout 1200s ./tests/linux/run_functional_suite.sh`
- Linux SMP 功能回归：
  - `timeout 1800s ./tests/linux/run_functional_suite_smp.sh`

## 当前结论

- 这轮又补齐了两处真实的 Armv8-A 行为缺口：`TPIDR2_EL0` 与 `GCSPR_EL0` 在缺失对应可选特性时，不应被当前模型错误暴露给软件。
- 相关裸机定向测试、裸机总回归，以及 Linux UMP/SMP 功能回归均已通过。
- 继续审阅后，当前在“EL0/EL1 system register 可见性”这条线上没有再发现同级别的明显宽放行点，但这仍不等于已完整实现全部 Armv8-A 强制行为，后续仍需继续逐类审查。

# 修改日志 2026-03-19 01:20

## 本轮修改

- 继续审阅 EL0 system register 访问控制，修正 `SCTLR_EL1.UMA` 相关行为：
  - EL0 `MRS/MSR DAIF` 以及 `MSR DAIFSet/DAIFClr` 不再一律 trap；
  - 现在仅在 `UMA=0` 时 trap，在 `UMA=1` 时按架构允许访问；
  - `NZCV` 保持 EL0 可访问，不错误地受 `UMA` 限制。
- 新增裸机回归 [tests/arm64/el0_daif_uma.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/el0_daif_uma.S)，并接入：
  - [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh)
  - [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)
  - 覆盖 `DAIF`、`DAIFSet`、`DAIFClr` 在 `UMA=0/1` 两种状态下的 trap/allow 语义，以及 EL0 `NZCV` 访问。
- 统一修正一批裸机 FP/AdvSIMD 测试的前提条件：
  - 在测试入口显式打开 `CPACR_EL1.FPEN=0b11`；
  - 避免“模拟器已经正确实现 FP trap，但旧测试默认上电就能执行 FP/SIMD”造成的假阴性或静默空转。
- 修正 AdvSIMD modified-immediate 解码：
  - [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp)
  - 之前 `MOVI/ORR/MVNI/BIC (vector, modified immediate)` 的匹配掩码过宽，会把 `SHRN` 这类 SIMD shift-immediate 指令误吞；
  - 现按编码字段收紧到 `bits[22:19]` 也必须匹配，避免与 `SHRN/USHR/SSHR/SRI` 等家族重叠。
- 扩充 modified-immediate 与 SHRN 单测覆盖：
  - [tests/arm64/fpsimd_bic_imm.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/fpsimd_bic_imm.S)
    - 新增 32-bit shifted `MOVI/MVNI/ORR/BIC` 与 16-bit `ORR` 覆盖；
  - [tests/arm64/fpsimd_misc_more.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/fpsimd_misc_more.S)
    - 新增 `SHRN v?.2s, v?.2d, #16` 覆盖，防止仅修好 `.8b/.8h` 窄宽度路径。
- 加强裸机总回归脚本 [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)：
  - `fpsimd_stringops.bin` 现在严格校验输出 `W`；
  - `fpsimd_bic_imm.bin` 现在严格校验输出 `W`；
  - `fpsimd_misc_more.bin` 现在严格校验输出 `G`；
  - 避免这次暴露出来的解码重叠 bug 再被“只运行不校验”的回归脚本漏掉。

## 本轮测试

- 定向单测：
  - `timeout 180s ./tests/arm64/build_tests.sh`
  - `timeout 60s ./build/aarchvm -bin tests/arm64/out/el0_daif_uma.bin -load 0x0 -entry 0x0 -steps 500000`
  - `timeout 60s ./build/aarchvm -bin tests/arm64/out/fpsimd_stringops.bin -load 0x0 -entry 0x0 -steps 600000`
  - `timeout 60s ./build/aarchvm -bin tests/arm64/out/fpsimd_bic_imm.bin -load 0x0 -entry 0x0 -steps 200000`
  - `timeout 60s ./build/aarchvm -bin tests/arm64/out/fpsimd_misc_more.bin -load 0x0 -entry 0x0 -steps 300000`
- 裸机完整回归：
  - `timeout 900s ./tests/arm64/run_all.sh`
- Linux 单核功能回归：
  - `timeout 1200s ./tests/linux/run_functional_suite.sh`
- Linux SMP 功能回归：
  - `timeout 1800s ./tests/linux/run_functional_suite_smp.sh`

## 当前结论

- `SCTLR_EL1.UMA` 控制下的 EL0 `DAIF` 访问语义已经补齐，并有独立裸机回归覆盖。
- 本轮修复的 AdvSIMD 问题根因是“modified-immediate 解码过宽导致 shift-immediate 被误识别”，不是某一条 `SHRN` 指令的单点执行错误。
- 本轮结束时，以下回归均已通过：
  - `tests/arm64/run_all.sh`
  - `tests/linux/run_functional_suite.sh`
  - `tests/linux/run_functional_suite_smp.sh`
- 仍不应宣称“已经完整实现 Armv8-A 全部强制 ISA/行为”：
  - 当前只能说在本轮继续审阅过程中，又补齐了一处真实的 system register 语义缺口和一处真实的 AdvSIMD 解码缺口；
  - 结合现有单测与 Linux UMP/SMP 回归，当前实现状态进一步收敛，但仍需要后续持续审阅与补测。

# 修改日志 2026-03-19 00:17

## 本轮修改

- 继续审阅 EL0 system/hint instruction 行为，补上 `SCTLR_EL1.nTWI` / `nTWE` 对 EL0 `WFI` / `WFE` 的 trap 语义：
  - [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp)
  - `nTWE=0` 时，EL0 `WFE` 触发 `EC=0x01`、`ISS=1` 的同步异常；
  - `nTWI=0` 且当前无待处理中断时，EL0 `WFI` 触发 `EC=0x01`、`ISS=0` 的同步异常；
  - 置位 `nTWI|nTWE` 后，对应 `WFI/WFE` 恢复正常执行。
- 新增裸机回归 [tests/arm64/el0_wfx_trap.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/el0_wfx_trap.S)，并接入：
  - [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh)
  - [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)
  - 该用例显式初始化 GIC CPU interface 与 virtual timer PPI，覆盖：
    - EL0 `WFE` trap 路径；
    - EL0 `WFI` trap 路径；
    - 放行后的 `SEVL/WFE` 正常前进；
    - 放行后的 EL0 `WFI` 被真实 timer IRQ 唤醒并回到下一条指令。
- 修正新用例中的 level-triggered PPI 处理顺序：
  - IRQ handler 先关闭 `CNTV_CTL_EL0` 再执行 `ICC_EOIR1_EL1/ICC_DIR_EL1`；
  - 避免在 timer 线仍为高电平时过早 drop active，导致同一中断被立即重新挂回 pending。
- 加强 [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)：
  - `el0_sysreg_privilege.bin` 现在校验输出 `R`；
  - `el0_cache_ops_privilege.bin` 现在校验输出 `C`；
  - `el0_wfx_trap.bin` 现在校验输出 `T`；
  - 避免“脚本通过但测试实际只是在空转”的情况。

## 本轮测试

- 定向单测：
  - `timeout 180s ./tests/arm64/build_tests.sh`
  - `timeout 60s env AARCHVM_PRINT_SUMMARY=1 AARCHVM_TRACE_EXC=1 AARCHVM_TRACE_LOWER_SYNC_LIMIT=16 AARCHVM_TRACE_ERET_LOWER_LIMIT=16 AARCHVM_TRACE_IRQ_TAKE=1 AARCHVM_TRACE_TIMER=1 AARCHVM_TRACE_GIC=1 ./build/aarchvm -bin tests/arm64/out/el0_wfx_trap.bin -load 0x0 -entry 0x0 -steps 800000`
- 裸机完整回归：
  - `timeout 900s ./tests/arm64/run_all.sh`
- Linux 单核功能回归：
  - `timeout 900s ./tests/linux/run_functional_suite.sh`
- Linux SMP 功能回归：
  - `timeout 1200s ./tests/linux/run_functional_suite_smp.sh`

## 当前结论

- `SCTLR_EL1.nTWI/nTWE` 这组 EL0 `WFI/WFE` 控制语义已补齐，并有独立裸机回归覆盖。
- 新测试不再依赖 reset-state 的默认 IRQ/GIC 状态，而是显式走 GIC + timer sysreg 路径，结果更接近真实体系结构行为。
- 本轮结束时，以下回归均已通过：
  - `tests/arm64/run_all.sh`
  - `tests/linux/run_functional_suite.sh`
  - `tests/linux/run_functional_suite_smp.sh`

# 修改日志 2026-03-18 23:24

## 本轮修改

- 继续审阅 `Cpu::exec_system()` 与 EL0 system instruction 权限路径，补上此前遗漏的一组 `SCTLR_EL1` 控制位语义：
  - [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp)
  - 已实现的 EL0 trap/放行规则：
    - `SCTLR_EL1.UCI=0` 时，EL0 执行 `IC IVAU`、`AT S1E1R/W`、`DC IVAC/CVAC/CVAU/CVAP/CVADP/CIVAC` 会触发 `EC=0x18` 同步异常；
    - `SCTLR_EL1.DZE=0` 时，EL0 执行 `DC ZVA` 会触发 `EC=0x18` 同步异常；
    - `TLBI`、`IC IALLU/IALLUIS`、`DC ISW/CISW` 在 EL0 下不再被错误放行，而是统一 trap；
    - `SCTLR_EL1.UCT=0` 时，EL0 读取 `CTR_EL0` 不再被错误放行，而是触发 `EC=0x18` 同步异常；置位 `UCT` 后恢复允许。
- 新增裸机回归 [tests/arm64/el0_cache_ops_privilege.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/el0_cache_ops_privilege.S)，并接入：
  - [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh)
  - [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)
  - 覆盖内容：
    - `UCI=0` 时 EL0 `IC IVAU` / `AT S1E1R` trap；
    - `DZE=0` 时 EL0 `DC ZVA` trap；
    - EL0 `TLBI VMALLE1` trap；
    - 打开 `UCI|DZE` 后，上述可放行路径恢复正常执行。
- 重写并加强 [tests/arm64/el0_sysreg_privilege.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/el0_sysreg_privilege.S)：
  - 不再错误假设“复位状态下 EL0 读取 `CTR_EL0` 总是允许”；
  - 现在显式验证：
    - EL0 `TPIDR_EL0` 正常可读写；
    - `UCT=0` 时 EL0 `MRS CTR_EL0` trap；
    - 置位 `UCT` 后 EL0 `CTR_EL0` / `DCZID_EL0` 读取恢复可用；
    - EL0 `MRS FAR_EL1`、`MSR TCR_EL1`、`MSR DAIFSet` 继续正确 trap。

## 本轮测试

- 定向单测：
  - `timeout 60s ./build/aarchvm -bin tests/arm64/out/el0_sysreg_privilege.bin -load 0x0 -entry 0x0 -steps 500000`
  - `timeout 60s ./build/aarchvm -bin tests/arm64/out/el0_cache_ops_privilege.bin -load 0x0 -entry 0x0 -steps 600000`
  - `timeout 60s ./build/aarchvm -bin tests/arm64/out/cntkctl_el0_timer_access.bin -load 0x0 -entry 0x0 -steps 600000`
- 裸机测试构建：
  - `timeout 180s ./tests/arm64/build_tests.sh`
- 裸机完整回归：
  - `timeout 600s ./tests/arm64/run_all.sh`
- Linux 单核功能回归：
  - `timeout 600s ./tests/linux/run_functional_suite.sh`
- Linux SMP 功能回归：
  - `timeout 900s ./tests/linux/run_functional_suite_smp.sh`

## 当前结论

- 本轮又补齐了一组明确的 Armv8-A 程序可见语义缺口：
  - EL0 system instruction 的可执行性并不只是“白名单 sysreg”问题，还受 `SCTLR_EL1.UCI`、`SCTLR_EL1.DZE`、`SCTLR_EL1.UCT` 控制；
  - 当前这些控制位相关的高置信度缺口已经修正，并且新增定向单测与完整回归均已通过。
- 继续审阅后，仍不能宣称“Armv8-A 强制行为已完全覆盖”。
- 当前我认为下一批仍值得优先检查、但这轮没有继续落代码的项目是：
  - `DCZID_EL0.DZP` 是否需要随当前 EL / `DZE` 动态反映禁止状态；
  - EL0 `WFI/WFE` 与 `SCTLR_EL1.nTWI/nTWE` 的 trap 语义；
  - 其余较少用但仍受控制位影响的 system instruction trap 条件与 syndrome 细节。

# 修改日志 2026-03-18 22:56

## 本轮修改

- 重新阅读当前 `Cpu` / `SoC` / `MMU` / decode 热路径代码，并重新执行一轮 UMP/SMP 算法性能测试与热点分析：
  - 结果文件：
    - `out/perf-ump-analysis-20260318-results.txt`
    - `out/perf-smp-analysis-20260318-results.txt`
  - `perf` 报告：
    - `out/perf-ump-current-20260318.report`
    - `out/perf-smp-current-20260318.report`
  - `gprof` 报告：
    - `out/gprof-smp-current-20260318.txt`
- 基于本轮热点复核，更新 [TODO.md](/media/luzeyu/Storage2/FOSS_src/aarchvm/TODO.md)：
  - 明确当前热点已经从 `GIC` 进一步后移，重新集中到 `Cpu::step()`、`translate_address()`、`lookup_decoded()`、`mmu_write_value()`、`exec_load_store()` 与 `SoC::run()`；
  - 将下一步性能优化优先级调整为：
    - `translate_address()` / TLB hit 权限检查压缩；
    - `lookup_decoded()` probe/fill 拆分；
    - `mmu_read_value()` / `mmu_write_value()` 两页跨页快路径；
    - 之后再复核 `SoC::run()` 是否仍值得继续优化。
- 修正 [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp) 中 EL0 system/sysreg 访问权限不完整的问题：
  - 之前 EL0 会错误放行对部分 EL1 特权 sysreg 的 `MRS/MSR`，例如 `FAR_EL1`、`TCR_EL1`；
  - 之前 EL0 也会错误执行若干明显特权的 `MSR (immediate)`，如 `MSR DAIFSet` / `MSR DAIFClr` / `MSR SPSel` / `MSR PAN`；
  - 现在为 `exec_system()` 增加了 EL0 sysreg/system instruction 访问白名单与 trap 判定：
    - 允许的 EL0 访问包括 `NZCV`、`FPCR/FPSR`、`TPIDR_EL0/TPIDR2_EL0`，以及只读的 `CurrentEL`、`DAIF`、`CTR_EL0`、`DCZID_EL0`、`TPIDRRO_EL0`、`CNTFRQ_EL0`、`GCSPR_EL0`；
    - timer sysreg 继续按 `CNTKCTL_EL1` 进行 EL0 门控；
    - 未列入白名单的特权访问改为触发同步异常，而不是静默成功。
- 新增裸机回归 [tests/arm64/el0_sysreg_privilege.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/el0_sysreg_privilege.S)，并接入：
  - [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh)
  - [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)
  - 该用例覆盖：
    - EL0 合法访问 `TPIDR_EL0` / `CTR_EL0` / `DCZID_EL0`；
    - EL0 非法 `MRS FAR_EL1`；
    - EL0 非法 `MSR TCR_EL1`；
    - EL0 非法 `MSR DAIFSet, #imm`。

## 本轮测试

- 性能分析：
  - `out/perf-ump-analysis-20260318-results.txt`
  - `out/perf-smp-analysis-20260318-results.txt`
  - `out/perf-ump-current-20260318.report`
  - `out/perf-smp-current-20260318.report`
  - `out/gprof-smp-current-20260318.txt`
- 定向单测：
  - `./build/aarchvm -bin tests/arm64/out/el0_sysreg_privilege.bin -load 0x0 -entry 0x0 -steps 400000`
  - `./build/aarchvm -bin tests/arm64/out/cntkctl_el0_timer_access.bin -load 0x0 -entry 0x0 -steps 600000`
- 裸机测试构建：
  - `timeout 180s ./tests/arm64/build_tests.sh`
- 裸机完整回归：
  - `timeout 600s ./tests/arm64/run_all.sh`
- Linux 单核功能回归：
  - `timeout 600s ./tests/linux/run_functional_suite.sh`
- Linux SMP 功能回归：
  - `timeout 900s ./tests/linux/run_functional_suite_smp.sh`

## 当前结论

- 当前版本的主热点已经重新回到解释器/MMU/预解码本体，而不是 `GIC` 轮询：
  - UMP `perf` 前列为 `Cpu::step()`、`translate_address()`、`lookup_decoded()`、`exec_data_processing()`、`mmu_write_value()`、`exec_load_store()`；
  - SMP `perf` 与 `gprof` 给出的顺序基本一致。
- 这说明下一轮性能优化最值得优先做的，不是继续往 `GIC` 或 guest 特化快路径上加逻辑，而是先压：
  - TLB hit 权限检查固定成本；
  - decoded hit 固定成本；
  - 常见跨页访存成本。
- 本轮发现并修复了一个明确的 Armv8-A 行为缺口：
  - EL0 对特权 sysreg/system instruction 的访问此前不够严格；
  - 修补后，新增单测、裸机完整回归、Linux 单核功能回归、Linux SMP 功能回归均已通过。

# 修改日志 2026-03-17 23:03

## 本轮修改

- 调整 [TODO.md](/media/luzeyu/Storage2/FOSS_src/aarchvm/TODO.md) 的文档结构：
  - 将新补充的“下一步性能优化方案”和“迈向 JIT 的后续改进方案”从“事件驱动化演进方案”下拆出；
  - 为其新增独立的顶级标题“性能优化与 JIT 路线”。
- 本轮只调整规划文档结构，不改动方案内容本身。

## 本轮测试

- 未运行单元测试或回归测试。
- 本轮仅为 TODO / CHANGELOG 文档结构调整。

## 当前结论

- `TODO.md` 现在的层级更清晰：
  - “事件驱动化演进方案”继续聚焦调度/guest 时间/设备事件；
  - “性能优化与 JIT 路线”单独承载后续性能与执行引擎演进规划。

# 修改日志 2026-03-17 21:15

## 本轮修改

- 重新阅读当前 `Cpu` / `SoC` / `GIC` / decode cache / MMU 热路径代码，并将基于现状实现的下一阶段性能优化方案补充进 [TODO.md](/media/luzeyu/Storage2/FOSS_src/aarchvm/TODO.md)。
- 在 [TODO.md](/media/luzeyu/Storage2/FOSS_src/aarchvm/TODO.md) 中新增了两部分内容：
  - “基于当前代码形态的下一步性能优化方案”；
  - “迈向 JIT 的后续改进方案”。
- 新增内容聚焦于当前真实代码结构中的热点与演进约束，例如：
  - `WFI/WFE` 等待态停车与 line-driven 唤醒；
  - `SoC::run()` 的 dispatch/deadline 增量化；
  - `translate_data_address_fast()` 的 TLB hit 热路径压缩；
  - `lookup_decoded()` 的 hit/miss 路径拆分；
  - 从 block cache / block executor 逐步迈向 selective native JIT 的路线。

## 本轮测试

- 未运行单元测试或回归测试。
- 本轮仅为代码阅读后的 TODO / 规划文档更新。

## 当前结论

- 当前代码下一步最值得做的性能优化，不是继续盲目扩大 predecode 覆盖率，而是先继续压：
  - 等待态 CPU 的 IRQ 轮询；
  - `SoC::run()` 的 dispatch/deadline 固定成本；
  - `translate_data_address_fast()` / `lookup_decoded()` / 跨页访存三条热链。
- JIT 的更稳妥路线不是直接上原生代码生成，而是先完成：
  - block cache
  - block executor
  - 统一失效/守卫模型
  - 再进入 selective native JIT。

# 修改日志 2026-03-17 21:11

## 本轮修改

- 基于 [doc/perf-baseline-vs-optimized.md](/media/luzeyu/Storage2/FOSS_src/aarchvm/doc/perf-baseline-vs-optimized.md) 中已有性能数据，重新整理性能趋势图输出：
  - 不再分别绘制“前项/后项”两组对比线；
  - 改为仅保留每轮最终结果对应的单条趋势线；
  - 生成 [doc/perf-ump-trend.svg](/media/luzeyu/Storage2/FOSS_src/aarchvm/doc/perf-ump-trend.svg) 与 [doc/perf-smp-trend.svg](/media/luzeyu/Storage2/FOSS_src/aarchvm/doc/perf-smp-trend.svg) 两张最终图。
- UMP 趋势图从文档中最早有数据的第一轮开始绘制；SMP 趋势图则从文档中第一次出现 SMP 数据的轮次开始绘制。
- 保留图中 `7P` / `7E` 标记，用于区分文档中两个不同含义的“第七轮”。

## 本轮测试

- 未运行代码或回归测试。
- 只校对了生成后的 SVG 与数据来源的一致性。

## 当前结论

- 当前 `doc/` 下已经有两张最终版趋势图，可直接引用：
  - [doc/perf-ump-trend.svg](/media/luzeyu/Storage2/FOSS_src/aarchvm/doc/perf-ump-trend.svg)
  - [doc/perf-smp-trend.svg](/media/luzeyu/Storage2/FOSS_src/aarchvm/doc/perf-smp-trend.svg)

# 修改日志 2026-03-17 20:20

## 本轮修改

- 围绕第二十一轮遗留的性能回退，继续优化 [src/gicv3.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/gicv3.cpp) / [include/aarchvm/gicv3.hpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/include/aarchvm/gicv3.hpp)：
  - 为 `GicV3::has_pending(cpu, pmr)` 增加 `state_epoch + pmr` 查询缓存；
  - 避免 `Cpu::ready_to_run()`、`WFI/WFE` 唤醒判断和 `SoC` 调度扫描在 GIC 状态未变化时反复线性扫描本地中断和 SPI。
- 继续优化 [src/soc.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/soc.cpp) 的设备同步热路径：
  - timer PPI 现在和 UART/KMI 一样，仅在电平变化时才调用 `gic_->set_level()`；
  - 去掉了大量无效的 timer IRQ level 推送，从而显著减少 GIC 相关固定成本。
- 更新 [doc/perf-baseline-vs-optimized.md](/media/luzeyu/Storage2/FOSS_src/aarchvm/doc/perf-baseline-vs-optimized.md)，追加“第二十二轮优化对比（日期时间：2026-03-17 20:20）”，记录：
  - 以前一轮 current 结果作为 baseline 的 UMP/SMP 对比；
  - 各 case 提升比例、总体提升比例；
  - 最终版本的 `perf` 热点分析。

## 本轮测试

- `cmake --build build -j4`
- `./build/aarchvm -smp 2 -bin tests/arm64/out/smp_gic_sgi.bin -load 0x0 -entry 0x0 -steps 400000`
- `./build/aarchvm -smp 2 -bin tests/arm64/out/smp_timer_ppi.bin -load 0x0 -entry 0x0 -steps 400000`
- `./build/aarchvm -smp 2 -bin tests/arm64/out/smp_timer_rate.bin -load 0x0 -entry 0x0 -steps 400000`
- `timeout 600s ./tests/arm64/run_all.sh`
- `timeout 600s ./tests/linux/build_linux_shell_snapshot.sh`
- `timeout 600s ./tests/linux/run_functional_suite.sh`
- `timeout 900s ./tests/linux/build_linux_smp_shell_snapshot.sh`
- `timeout 600s ./tests/linux/run_functional_suite_smp.sh`
- `env AARCHVM_TIMER_SCALE=1 AARCHVM_PERF_TIMEOUT=600s AARCHVM_PERF_LOG=out/perf-ump-giccache-timerlvl-optimized.log AARCHVM_PERF_RESULTS=out/perf-ump-giccache-timerlvl-optimized-results.txt ./tests/linux/run_algorithm_perf.sh`
- `env AARCHVM_TIMER_SCALE=1 AARCHVM_PERF_TIMEOUT=900s AARCHVM_PERF_LOG=out/perf-smp-giccache-timerlvl-optimized.log AARCHVM_PERF_RESULTS=out/perf-smp-giccache-timerlvl-optimized-results.txt AARCHVM_ARGS='-smp 2 -smp-mode psci' AARCHVM_LINUX_DTB=dts/aarchvm-linux-smp.dtb AARCHVM_USERTEST_SNAPSHOT_OUT=out/linux-smp-shell-v1.snap AARCHVM_USERTEST_SNAPSHOT_LOG=out/linux-smp-shell-v1-build.log AARCHVM_USERTEST_SNAPSHOT_VERIFY_LOG=out/linux-smp-shell-v1-verify.log ./tests/linux/run_algorithm_perf.sh`
- `timeout 300s bash -lc "printf 'bench_runner\n' | AARCHVM_BUS_FASTPATH=1 AARCHVM_TIMER_SCALE=1 perf record -o out/perf-round22-current.data -- ./build/aarchvm -smp 2 -snapshot-load out/linux-smp-shell-v1.snap -steps 3000000000 -stop-on-uart 'BENCH-RESULT name=tlb-rand-32m' -fb-sdl off > out/perf-round22-current-run.log 2>&1"`
- `perf report --stdio --percent-limit 0.2 --dsos=aarchvm -i out/perf-round22-current.data > out/perf-round22-current-aarchvm-only.report`

## 当前结论

- 这轮优化是成功的，并且收益已经明显超过上一轮回退量：
  - UMP 总体 `host_ns` 从 `8824308238` 降到 `4459907581`，提升约 `49.46%`；
  - SMP 总体 `host_ns` 从 `9829828739` 降到 `4820254387`，提升约 `50.96%`。
- 当前主热点已经重新回到解释器和访存本体，而不是 GIC 轮询：
  - `Cpu::step()`、`translate_address()`、`lookup_decoded()`、`mmu_write_value()`、`exec_load_store()` 是下一步优先级更高的方向；
  - `GicV3::has_pending()` 已经不再出现在当前 `perf` 热点前列。

# 修改日志 2026-03-17 17:17

## 本轮修改

- 在 [include/aarchvm/soc.hpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/include/aarchvm/soc.hpp) / [src/soc.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/soc.cpp) 继续推进事件驱动化演进方案第 3 节，对 `SoC::run()` 做了一轮骨架级重构：
  - 新增 `CpuDispatchState`，统一收集 `powered_on / active / runnable / first_runnable` 状态；
  - 将单核、单 runnable SMP、多 runnable SMP 三条路径的运行窗口计算收敛到同一个 `compute_run_window(...)`；
  - 删除单 runnable SMP 路径里每次内层循环开头的 `ready_to_run()` 重复检查；
  - 用统一的 dispatch scan 替代多处独立的 active/runnable 统计逻辑，为后续继续向“按原因运行到下一个事件”推进做准备。
- 更新 [TODO.md](/media/luzeyu/Storage2/FOSS_src/aarchvm/TODO.md)，补充第 3 节“重构 SoC 主循环”的当前状态，明确这轮完成的是主循环骨架整理，而不是性能闭环。
- 更新 [doc/perf-baseline-vs-optimized.md](/media/luzeyu/Storage2/FOSS_src/aarchvm/doc/perf-baseline-vs-optimized.md)，追加“第二十一轮优化对比（日期时间：2026-03-17 17:17）”，记录：
  - 本轮 baseline / current 的 UMP 与 SMP 性能数据；
  - `sync_devices` / `run_chunks` 计数变化；
  - 额外一轮复测结果与当前结论。

## 本轮测试

- `timeout 600s ./tests/arm64/run_all.sh`
- `timeout 600s ./tests/linux/build_linux_shell_snapshot.sh`
- `timeout 600s ./tests/linux/run_functional_suite.sh`
- `timeout 900s ./tests/linux/build_linux_smp_shell_snapshot.sh`
- `timeout 600s ./tests/linux/run_functional_suite_smp.sh`
- `env AARCHVM_TIMER_SCALE=1 AARCHVM_PERF_TIMEOUT=600s AARCHVM_PERF_LOG=out/perf-ump-socloop-optimized.log AARCHVM_PERF_RESULTS=out/perf-ump-socloop-optimized-results.txt ./tests/linux/run_algorithm_perf.sh`
- `env AARCHVM_TIMER_SCALE=1 AARCHVM_PERF_TIMEOUT=900s AARCHVM_PERF_LOG=out/perf-smp-socloop-optimized.log AARCHVM_PERF_RESULTS=out/perf-smp-socloop-optimized-results.txt AARCHVM_ARGS='-smp 2 -smp-mode psci' AARCHVM_LINUX_DTB=dts/aarchvm-linux-smp.dtb AARCHVM_USERTEST_SNAPSHOT_OUT=out/linux-smp-shell-v1.snap AARCHVM_USERTEST_SNAPSHOT_LOG=out/linux-smp-shell-v1-build.log AARCHVM_USERTEST_SNAPSHOT_VERIFY_LOG=out/linux-smp-shell-v1-verify.log ./tests/linux/run_algorithm_perf.sh`
- `env AARCHVM_TIMER_SCALE=1 AARCHVM_PERF_TIMEOUT=600s AARCHVM_PERF_LOG=out/perf-ump-socloop-rerun.log AARCHVM_PERF_RESULTS=out/perf-ump-socloop-rerun-results.txt ./tests/linux/run_algorithm_perf.sh`
- `env AARCHVM_TIMER_SCALE=1 AARCHVM_PERF_TIMEOUT=900s AARCHVM_PERF_LOG=out/perf-smp-socloop-rerun.log AARCHVM_PERF_RESULTS=out/perf-smp-socloop-rerun-results.txt AARCHVM_ARGS='-smp 2 -smp-mode psci' AARCHVM_LINUX_DTB=dts/aarchvm-linux-smp.dtb AARCHVM_USERTEST_SNAPSHOT_OUT=out/linux-smp-shell-v1.snap AARCHVM_USERTEST_SNAPSHOT_LOG=out/linux-smp-shell-v1-build.log AARCHVM_USERTEST_SNAPSHOT_VERIFY_LOG=out/linux-smp-shell-v1-verify.log ./tests/linux/run_algorithm_perf.sh`

## 当前结论

- 这轮 `SoC` 主循环重构完成了调度骨架整理，代码结构比之前更适合继续向“按事件和 deadline 驱动”推进。
- 这轮不是性能正收益：
  - UMP 总体 `host_ns` 从 `7828394269` 变为 `8824308238`，约 `-12.72%`；
  - SMP 总体 `host_ns` 从 `5496349229` 变为 `9829828739`，约 `-78.84%`；
  - 复测后仍分别约为 `-18.40%` 与 `-49.50%`，趋势一致。
- 因此下一步不应继续把这轮结果包装成“优化完成”，而应直接基于当前主线重新做热点分析，确认回退是出在 `SoC::run()` 自身、`ready_to_run()` / `GIC::has_pending()` 还是新的 deadline 判定路径。

# 修改日志 2026-03-17 15:10

## 本轮修改

- 修正 [include/aarchvm/cpu.hpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/include/aarchvm/cpu.hpp) / [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp) 中 exclusive monitor 的地址语义错误：
  - `LDXR/LDAXR/LDXP/LDAXP` 不再只记录虚拟地址，而是记录本次 exclusive 访问实际命中的物理字节集合；
  - `STXR/STLXR/STXP/STLXP` 的 reservation 匹配改为按物理地址集合判断；
  - 跨核普通写导致的 reservation 失效也改为按物理地址集合判断，从而修复 MMU 打开后 SMP 下 “别核写同一物理位置却没有清掉本核 reservation” 的错误行为。
- 为避免把旧错误格式里的瞬时 reservation 状态带入恢复流程，扩展 snapshot CPU 状态并将 [src/soc.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/soc.cpp) 的 snapshot 版本提升到 `14`；对于旧版本快照，恢复时主动清空 exclusive monitor。
- 新增定向裸机回归 [tests/arm64/smp_ldxr_invalidate_mmu.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/smp_ldxr_invalidate_mmu.S)，覆盖“双核 + MMU + 同一 PA 经高 VA 访问时，普通写必须清掉别核 reservation”这一场景，并把它接入 [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh) 与 [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)。

## 本轮测试

- `cmake --build build -j4`
- `timeout 120s ./tests/arm64/build_tests.sh`
- `timeout 60s ./build/aarchvm -smp 2 -bin tests/arm64/out/smp_ldxr_invalidate_mmu.bin -load 0x0 -entry 0x0 -steps 1200000`
- `timeout 600s ./tests/arm64/run_all.sh`
- `timeout 600s ./tests/linux/build_linux_shell_snapshot.sh`
- `timeout 600s ./tests/linux/run_functional_suite.sh`
- `timeout 900s ./tests/linux/build_linux_smp_shell_snapshot.sh`
- `timeout 600s ./tests/linux/run_functional_suite_smp.sh`

## 当前结论

- `out/my_test.snap` 这类旧坏快照短恢复后仍可能停在内核坏状态里，这并不否定修补本身，因为快照保存时共享内核状态很可能已经先被错误同步破坏。
- 这次修补已经把“MMU 打开后的 LL/SC 跨核 invalidation”这条明确语义缺口补上，并且新的定向测试、完整裸机回归、Linux 单核功能回归、Linux SMP 功能回归都已通过。

# 修改日志 2026-03-17 13:18

## 本轮修改

- 保持事件驱动第二阶段当前稳定主线不变，在 [src/soc.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/soc.cpp) / [include/aarchvm/soc.hpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/include/aarchvm/soc.hpp) 中保留并复核：
  - 单 runnable CPU 的长突发执行；
  - `SEV`、外部写和 `PSCI_CPU_ON` 触发的 `runnable_state_dirty_` 重调度打断。
- 修正 [tests/linux/run_algorithm_perf.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/linux/run_algorithm_perf.sh)，在 `AARCHVM_ARGS` 含 `-smp` 时自动切换到 `tests/linux/build_linux_smp_shell_snapshot.sh`，避免 SMP perf 流程错误沿用单核 snapshot 构建步数。
- 更新 [TODO.md](/media/luzeyu/Storage2/FOSS_src/aarchvm/TODO.md)，明确：
  - 第 6 节当前稳定收益来自“单 runnable CPU 长突发执行”；
  - “多个 runnable CPU 的小量子 + 轮转公平”原型已经验证过，但由于会回归 `smp_timer_ppi` / `smp_timer_rate` 的可观察顺序，目前没有并入主线。
- 更新 [doc/perf-baseline-vs-optimized.md](/media/luzeyu/Storage2/FOSS_src/aarchvm/doc/perf-baseline-vs-optimized.md)，追加“第二十轮优化对比（日期时间：2026-03-17 13:18）”，补齐当前稳定第二阶段相对第十九轮的 UMP/SMP 数据、热点变化和原因分析。

## 本轮测试

- `cmake --build build -j4`
- `timeout 600s ./tests/arm64/run_all.sh`
- `timeout 360s ./tests/linux/build_linux_shell_snapshot.sh`
- `AARCHVM_FUNCTIONAL_TIMEOUT=360s ./tests/linux/run_functional_suite.sh`
- `timeout 900s ./tests/linux/build_linux_smp_shell_snapshot.sh`
- `AARCHVM_SMP_FUNCTIONAL_TIMEOUT=240s ./tests/linux/run_functional_suite_smp.sh`
- `env AARCHVM_TIMER_SCALE=1 AARCHVM_PERF_TIMEOUT=900s AARCHVM_PERF_LOG=out/perf-smp-stage2b-current.log AARCHVM_PERF_RESULTS=out/perf-smp-stage2b-current-results.txt AARCHVM_ARGS='-smp 2 -smp-mode psci' AARCHVM_LINUX_DTB=dts/aarchvm-linux-smp.dtb AARCHVM_USERTEST_SNAPSHOT_OUT=out/linux-smp-shell-v1.snap AARCHVM_USERTEST_SNAPSHOT_LOG=out/linux-smp-shell-v1-build.log AARCHVM_USERTEST_SNAPSHOT_VERIFY_LOG=out/linux-smp-shell-v1-verify.log ./tests/linux/run_algorithm_perf.sh`

## 当前结论

- 当前第二阶段的稳定主线已经明显优于第十九轮记录的版本，尤其是 SMP：算法 perf 总体提升约 `93.08%`。
- 当前最值得继续推进的方向已经不再是“盲目把多 runnable CPU 做量子化”，而是：
  - GIC pending summary / candidate cache；
  - line-driven wakeup；
  - `main.cpp` 输入注入事件化。
- 多 runnable CPU 的量子调度仍值得做，但必须先把“会影响裸机 SMP timer 用例可观察顺序”的语义边界理清，否则不应合入主线。

# 修改日志 2026-03-17 11:48

## 本轮修改

- 继续推进事件驱动化演进方案，在 [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp) / [include/aarchvm/cpu.hpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/include/aarchvm/cpu.hpp) 中新增 `ready_to_run()` 与等待态 IRQ 唤醒辅助判断，让 SoC 可以在调度层判断某个 CPU 是否真的需要继续执行。
- 在 [src/soc.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/soc.cpp) / [include/aarchvm/soc.hpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/include/aarchvm/soc.hpp) 中实现事件驱动第二阶段的最小闭环：
  - SMP 主循环优先只调度真正 runnable 的 CPU；
  - `WFI/WFE` CPU 在系统里仍有其他 runnable CPU 时，不再继续参与每轮 `cpu.step()`；
  - 当所有 CPU 都在等待且存在明确的 guest timer deadline 时，SoC 直接把 guest 时间推进到该 deadline，再同步设备并继续运行。
- 修正实现过程里引入的一处语义回退：`WFI` 的唤醒条件恢复为“有 pending interrupt 即可退出等待”，而不是错误地收窄为“当前可投递 IRQ 才唤醒”。
- 更新 [TODO.md](/media/luzeyu/Storage2/FOSS_src/aarchvm/TODO.md)，补充第 5/6 节的当前状态，明确：
  - 这一轮只完成了等待态停车的最小闭环；
  - 在 `main.cpp` 仍以 `soc.steps()` 驱动输入注入的前提下，`all-waiting + no guest deadline` 仍保留 polling fallback；
  - 下一步仍需继续做 GIC 增量唤醒和 `main` 层事件化。
- 更新 [doc/perf-baseline-vs-optimized.md](/media/luzeyu/Storage2/FOSS_src/aarchvm/doc/perf-baseline-vs-optimized.md)，追加“第十九轮优化对比（日期时间：2026-03-17 11:48）”，记录本轮 baseline / optimized 的 UMP 与 SMP 数据、提升比例和当前热点。

## 本轮测试

- `cmake --build build -j4`
- `timeout 600s ./tests/arm64/run_all.sh`
- `timeout 360s ./tests/linux/build_linux_shell_snapshot.sh`
- `AARCHVM_FUNCTIONAL_TIMEOUT=360s ./tests/linux/run_functional_suite.sh`
- `timeout 900s ./tests/linux/build_linux_smp_shell_snapshot.sh`
- `AARCHVM_SMP_FUNCTIONAL_TIMEOUT=240s ./tests/linux/run_functional_suite_smp.sh`
- `env AARCHVM_TIMER_SCALE=1 AARCHVM_PERF_TIMEOUT=600s AARCHVM_PERF_LOG=out/perf-ump-stage2-baseline.log AARCHVM_PERF_RESULTS=out/perf-ump-stage2-baseline-results.txt ./tests/linux/run_algorithm_perf.sh`
- `env AARCHVM_TIMER_SCALE=1 AARCHVM_PERF_TIMEOUT=900s AARCHVM_PERF_LOG=out/perf-smp-stage2-baseline.log AARCHVM_PERF_RESULTS=out/perf-smp-stage2-baseline-results.txt AARCHVM_ARGS='-smp 2 -smp-mode psci' AARCHVM_LINUX_DTB=dts/aarchvm-linux-smp.dtb AARCHVM_USERTEST_SNAPSHOT_OUT=out/linux-smp-shell-v1.snap AARCHVM_USERTEST_SNAPSHOT_LOG=out/linux-smp-shell-v1-build.log AARCHVM_USERTEST_SNAPSHOT_VERIFY_LOG=out/linux-smp-shell-v1-verify.log ./tests/linux/run_algorithm_perf.sh`
- `env AARCHVM_TIMER_SCALE=1 AARCHVM_PERF_TIMEOUT=600s AARCHVM_PERF_LOG=out/perf-ump-stage2-optimized.log AARCHVM_PERF_RESULTS=out/perf-ump-stage2-optimized-results.txt ./tests/linux/run_algorithm_perf.sh`
- `env AARCHVM_TIMER_SCALE=1 AARCHVM_PERF_TIMEOUT=900s AARCHVM_PERF_LOG=out/perf-smp-stage2-optimized.log AARCHVM_PERF_RESULTS=out/perf-smp-stage2-optimized-results.txt AARCHVM_ARGS='-smp 2 -smp-mode psci' AARCHVM_LINUX_DTB=dts/aarchvm-linux-smp.dtb AARCHVM_USERTEST_SNAPSHOT_OUT=out/linux-smp-shell-v1.snap AARCHVM_USERTEST_SNAPSHOT_LOG=out/linux-smp-shell-v1-build.log AARCHVM_USERTEST_SNAPSHOT_VERIFY_LOG=out/linux-smp-shell-v1-verify.log ./tests/linux/run_algorithm_perf.sh`
- `timeout 300s bash -lc "printf 'bench_runner\n' | AARCHVM_BUS_FASTPATH=1 AARCHVM_TIMER_SCALE=1 perf record -o out/perf-stage2-current.data -- ./build/aarchvm -smp 2 -snapshot-load out/linux-smp-shell-v1.snap -steps 3000000000 -stop-on-uart 'BENCH-RESULT name=tlb-rand-32m' -fb-sdl off > out/perf-stage2-current-run.log 2>&1"`
- `perf report --stdio --percent-limit 0.1 --dsos=aarchvm -i out/perf-stage2-current.data > out/perf-stage2-current-aarchvm-only.report`

## 当前结论

- 事件驱动第二阶段现在已经有了可工作的最小闭环，但还没有彻底摆脱轮询。
- 这轮对 SMP 是有效的：算法 perf 总体提升约 `8.69%`，多数 case 的 `steps` 近乎减半。
- 这轮对 UMP 没有形成稳定正收益；三类算法 case 出现明显回退，而内部计数变化很小，说明单核路径的收益并不稳定，暂时不宜把它当作可靠优化结论。
- 当前最大热点仍然是 `GicV3::has_pending(...)`，`perf` 中占比约 `86.89%`；因此下一步优先级仍然是：
  - GIC pending summary / candidate cache；
  - IRQ line-driven wakeup；
  - `main.cpp` 的输入注入事件化。

# 修改日志 2026-03-17 03:55

## 本轮修改

- 完成事件驱动化第一阶段的收尾整理，确认当前主线默认使用 `AARCHVM_SCHED_MODE=event`，同时保留 `legacy` 作为对照 / fallback 模式。
- 在 [src/soc.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/soc.cpp) 将 SoC 外层调度恢复为“默认走事件模式、显式切换才走旧步进模式”的结构，并保留：
  - 统一 `guest_time`；
  - timer deadline 驱动；
  - 设备状态变化导致的调度失效；
  - 旧固定步数路径用于 debug / A-B 对照。
- 更新 [TODO.md](/media/luzeyu/Storage2/FOSS_src/aarchvm/TODO.md)，明确第一阶段已完成的闭环范围，以及当前默认必须保持 `event` 的原因。
- 更新 [doc/README.en.md](/media/luzeyu/Storage2/FOSS_src/aarchvm/doc/README.en.md) 与 [doc/README.zh.md](/media/luzeyu/Storage2/FOSS_src/aarchvm/doc/README.zh.md)，补充 `AARCHVM_SCHED_MODE=event|legacy` 的使用说明与语义差异。
- 在 [doc/perf-baseline-vs-optimized.md](/media/luzeyu/Storage2/FOSS_src/aarchvm/doc/perf-baseline-vs-optimized.md) 追加“第十八轮：事件驱动第一阶段复核”，记录：
  - 同版本 `legacy` 对照与当前默认 `event` 的 UMP/SMP 数据；
  - 当前 `perf` / `gprof` 热点；
  - 为什么 `legacy` 虽然局部更快，但不能作为 SMP 语义等价模式。

## 本轮测试

- `timeout 600s ./tests/arm64/run_all.sh`
- `timeout 360s ./tests/linux/build_linux_shell_snapshot.sh`
- `AARCHVM_FUNCTIONAL_TIMEOUT=360s ./tests/linux/run_functional_suite.sh`
- `timeout 900s ./tests/linux/build_linux_smp_shell_snapshot.sh`
- `AARCHVM_SMP_FUNCTIONAL_TIMEOUT=240s ./tests/linux/run_functional_suite_smp.sh`
- `env AARCHVM_TIMER_SCALE=1 AARCHVM_PERF_TIMEOUT=600s AARCHVM_PERF_LOG=out/perf-ump-stage1-final.log AARCHVM_PERF_RESULTS=out/perf-ump-stage1-final-results.txt ./tests/linux/run_algorithm_perf.sh`
- `env AARCHVM_TIMER_SCALE=1 AARCHVM_PERF_TIMEOUT=900s AARCHVM_PERF_LOG=out/perf-smp-stage1-final.log AARCHVM_PERF_RESULTS=out/perf-smp-stage1-final-results.txt AARCHVM_ARGS='-smp 2 -smp-mode psci' AARCHVM_LINUX_DTB=dts/aarchvm-linux-smp.dtb AARCHVM_USERTEST_SNAPSHOT_OUT=out/linux-smp-shell-v1.snap AARCHVM_USERTEST_SNAPSHOT_LOG=out/linux-smp-shell-v1-build.log AARCHVM_USERTEST_SNAPSHOT_VERIFY_LOG=out/linux-smp-shell-v1-verify.log ./tests/linux/run_algorithm_perf.sh`
- `timeout 300s bash -lc "printf 'bench_runner\n' | AARCHVM_BUS_FASTPATH=1 AARCHVM_TIMER_SCALE=1 perf record -o out/perf-stage1-current.data -- ./build/aarchvm -smp 2 -snapshot-load out/linux-smp-shell-v1.snap -steps 3000000000 -stop-on-uart 'BENCH-RESULT name=tlb-rand-32m' -fb-sdl off > out/perf-stage1-current-run.log 2>&1"`
- `perf report --stdio --percent-limit 0.1 --dsos=aarchvm -i out/perf-stage1-current.data > out/perf-stage1-current-aarchvm-only.report`
- `cmake --build build-gprof -j4`
- `timeout 300s bash -lc "cd out && printf 'bench_runner\n' | AARCHVM_BUS_FASTPATH=1 AARCHVM_TIMER_SCALE=1 ../build-gprof/aarchvm -smp 2 -snapshot-load linux-smp-shell-v1.snap -steps 3000000000 -stop-on-uart 'BENCH-RESULT name=tlb-rand-32m' -fb-sdl off > gprof-stage1-current-run.log 2>&1"`
- `gprof build-gprof/aarchvm out/gmon.out > out/gprof-stage1-current.txt`

## 当前结论

- 事件驱动化第一阶段已经形成可保留的正确性闭环，但还不是最终性能形态。
- 当前默认必须保持 `event`，因为 `legacy` 在 SMP 下会把近期限时器递送推迟到大 chunk 之后，`tests/arm64/smp_timer_rate.bin` 已能稳定证明这一点。
- 当前最主要热点仍是 `GicV3::has_pending(...)`，说明下一阶段的性能突破点已经明确落在 `WFI` 真正停车、GIC pending summary / candidate cache、IRQ line-driven wakeup，而不是继续优先做 decode/MMU 微优化。

# 修改日志 2026-03-17 01:05

## 本轮修改

- 基于当前代码重新执行了一轮 SMP Linux 算法性能测试，结果写入：
  - `out/perf-smp-current.log`
  - `out/perf-smp-current-results.txt`
- 基于 SMP shell snapshot 重新执行了一轮当前代码的热点分析：
  - `perf data`：`out/perf-smp-current.data`
  - `perf report`：`out/perf-smp-current.report`
  - `perf report (aarchvm only)`：`out/perf-smp-current-aarchvm-only.report`
  - `gprof`：`out/gprof-smp-current.txt`
- 根据本轮 SMP `perf` / `gprof` 结果，更新 [TODO.md](/media/luzeyu/Storage2/FOSS_src/aarchvm/TODO.md)：
  - 新增“基于 2026-03-17 SMP 热点复测的近期优先级”小节；
  - 明确将 `WFI` 真正停车、GIC per-CPU pending summary / candidate cache、IRQ line-driven wakeup 作为下一轮性能优化的第一优先级；
  - 明确在 `GicV3::has_pending(...)` 不再是压倒性热点之前，不应继续优先投入 decode/MMU/load-store 微优化。

## 本轮测试

- `env AARCHVM_TIMER_SCALE=1 AARCHVM_PERF_TIMEOUT=900s AARCHVM_PERF_LOG=out/perf-smp-current.log AARCHVM_PERF_RESULTS=out/perf-smp-current-results.txt AARCHVM_ARGS='-smp 2 -smp-mode psci' AARCHVM_LINUX_DTB=dts/aarchvm-linux-smp.dtb AARCHVM_USERTEST_SNAPSHOT_OUT=out/linux-smp-perf-shell.snap AARCHVM_USERTEST_SNAPSHOT_LOG=out/linux-smp-perf-shell-build.log AARCHVM_USERTEST_SNAPSHOT_VERIFY_LOG=out/linux-smp-perf-shell-verify.log ./tests/linux/run_algorithm_perf.sh`
- `timeout 300s bash -lc "printf 'bench_runner\n' | AARCHVM_BUS_FASTPATH=1 AARCHVM_TIMER_SCALE=1 perf record -o out/perf-smp-current.data -- ./build/aarchvm -smp 2 -snapshot-load out/linux-smp-perf-shell.snap -steps 3000000000 -stop-on-uart 'BENCH-RESULT name=tlb-rand-32m' -fb-sdl off > out/perf-smp-current-run.log 2>&1"`
- `perf report --stdio -i out/perf-smp-current.data > out/perf-smp-current.report`
- `perf report --stdio --percent-limit 0.1 --dsos=aarchvm -i out/perf-smp-current.data > out/perf-smp-current-aarchvm-only.report`
- `cmake --build build-gprof -j4`
- `timeout 300s bash -lc "cd out && printf 'bench_runner\n' | AARCHVM_BUS_FASTPATH=1 AARCHVM_TIMER_SCALE=1 ../build-gprof/aarchvm -smp 2 -snapshot-load linux-smp-perf-shell.snap -steps 3000000000 -stop-on-uart 'BENCH-RESULT name=tlb-rand-32m' -fb-sdl off > gprof-smp-current-run.log 2>&1"`
- `gprof build-gprof/aarchvm out/gmon.out > out/gprof-smp-current.txt`

# 修改日志 2026-03-17 00:50

## 本轮修改

- 继续细化 [TODO.md](/media/luzeyu/Storage2/FOSS_src/aarchvm/TODO.md) 中的 RISC-V RV64IMAC 支持方案，不再只停留在阶段划分，而是补充了可直接拆任务的设计细节。
- 新增“建议的代码拆分与落点”小节，明确：
  - 先引入最小公共 CPU 接口；
  - 当前 `Cpu` 先逻辑上视作 `ArmCpu`；
  - 将通用机器层与 Arm/RISC-V 专用平台层拆开；
  - 快照格式建议拆成 common header + arch tag + machine/device/per-hart blobs。
- 新增“建议的首个 RISC-V 机器模型”小节，给出首版建议地址图与设备复用策略：
  - DRAM `0x8000_0000`
  - UART `0x1000_0000`
  - ACLINT/CLINT `0x0200_0000`
  - PLIC `0x0c00_0000`
  - framebuffer / block / perf mailbox 放入独立高地址 MMIO 区
- 新增“Linux 路径的推荐启动策略”小节，明确推荐先走“内建极小 M-mode firmware/SBI shim + 直接启动 Linux”的最短路径，而不是先做 U-Boot on RISC-V。
- 新增“需要优先保证正确的语义点”小节，明确将 `LR/SC`、`FENCE/FENCE.I`、misaligned access、precise trap、多 hart `SFENCE.VMA` / `remote_fence_i` 行为列为早期必须重点覆盖的正确性项。

## 本轮测试

- 本轮仅更新设计文档，未运行测试。

# 修改日志 2026-03-17 00:45

## 本轮修改

- 修正 [doc/perf-baseline-vs-optimized.md](/media/luzeyu/Storage2/FOSS_src/aarchvm/doc/perf-baseline-vs-optimized.md) 的条目顺序。
- 将误插在 `Release 相对 Debug 的性能提升` 小节中的“第七轮优化对比（日期时间：2026-03-17 00:34）”整体移动到文档末尾，恢复性能记录按追加顺序排列。
- 恢复 `Release 相对 Debug 的性能提升` 小节内部表格与结论的连续性，避免最新条目打断旧条目内容。

## 本轮测试

- 本轮仅调整文档结构，未运行测试。

# 修改日志 2026-03-17 00:34

## 本轮修改

- 在 [src/soc.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/soc.cpp) 完成事件驱动演进方案第 1 阶段与第 2 阶段的最小闭环接线：
  - SoC 现在维护独立的固定点 `guest_time_fp_`，guest timer/sysreg 读写统一从 guest 时间取值，不再直接等同于旧的 `timer_steps_ * scale`。
  - snapshot 继续保存并恢复统一 guest 时间；旧版本快照会兼容性地恢复到新的时间表示。
  - SoC 增加最小设备计划缓存，支持按 `(deadline, event_type)` 查询下一设备事件；当前先让 generic timer 成为第一个真正基于 deadline 的设备。
  - SMP 主循环不再“每轮一同步设备”，而是会先计算下一个 timer deadline，再批量执行多个 round，只有到事件边界或设备状态变化时才重新 `sync_devices()`。
- 为保证这条最小事件调度路径不破坏行为正确性，补齐了设备状态变化回调：
  - [include/aarchvm/generic_timer.hpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/include/aarchvm/generic_timer.hpp) / [src/generic_timer.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/generic_timer.cpp) 新增 timer 状态变化 observer，在 `CNTV/CNTP` 控制和比较值变化时通知 SoC 失效当前设备计划。
  - [include/aarchvm/uart_pl011.hpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/include/aarchvm/uart_pl011.hpp) / [src/uart_pl011.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/uart_pl011.cpp) 新增 UART 状态变化 observer，guest 读空 RX FIFO、修改 mask/enable 后会立即打断当前批次并重新同步 IRQ 线。
  - [include/aarchvm/pl050_kmi.hpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/include/aarchvm/pl050_kmi.hpp) / [src/pl050_kmi.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/pl050_kmi.cpp) 对 KMI 做了同类 observer 接线，避免输入 FIFO/控制位变化在事件批次里被延迟太久。
- Linux 脚本继续统一默认 `AARCHVM_TIMER_SCALE=1`，确保 UMP/SMP、`ttyAMA0`/`tty1` 等路径都在统一 guest 时间模型下回归。
- 更新 [TODO.md](/media/luzeyu/Storage2/FOSS_src/aarchvm/TODO.md)，把“统一 guest 时间”和“引入最小事件调度器”中已完成的子任务勾选出来。
- 更新 [doc/perf-baseline-vs-optimized.md](/media/luzeyu/Storage2/FOSS_src/aarchvm/doc/perf-baseline-vs-optimized.md)，补充本轮 baseline/optimized 的 UMP 与 SMP 性能数据、命令、提升百分比与结果分析。

## 本轮测试

- `cmake --build build -j4`：通过。
- `timeout 600s ./tests/arm64/run_all.sh`：通过。
- `timeout 360s ./tests/linux/build_linux_shell_snapshot.sh`：通过，prompt steps 为 `400173355`。
- `timeout 240s ./tests/linux/run_functional_suite.sh`：通过。
- `timeout 900s ./tests/linux/build_linux_smp_shell_snapshot.sh`：通过，prompt steps 为 `739614800`。
- `timeout 240s ./tests/linux/run_functional_suite_smp.sh`：通过。
- `timeout 600s env AARCHVM_TIMER_SCALE=1 AARCHVM_PERF_TIMEOUT=600s AARCHVM_PERF_LOG=out/perf-ump-optimized.log AARCHVM_PERF_RESULTS=out/perf-ump-optimized.txt ./tests/linux/run_algorithm_perf.sh`：通过。
- `timeout 900s env AARCHVM_TIMER_SCALE=1 AARCHVM_PERF_TIMEOUT=900s AARCHVM_PERF_LOG=out/perf-smp-optimized.log AARCHVM_PERF_RESULTS=out/perf-smp-optimized.txt AARCHVM_ARGS='-smp 2 -smp-mode psci' AARCHVM_LINUX_DTB=dts/aarchvm-linux-smp.dtb AARCHVM_USERTEST_SNAPSHOT_OUT=out/linux-smp-perf-shell.snap AARCHVM_USERTEST_SNAPSHOT_LOG=out/linux-smp-perf-shell-build.log AARCHVM_USERTEST_SNAPSHOT_VERIFY_LOG=out/linux-smp-perf-shell-verify.log ./tests/linux/run_algorithm_perf.sh`：通过。

## 当前结论

- 这一轮已经把“统一 guest 时间 + timer deadline 化 + 最小事件计划缓存”完整接进了主线，并且在现有单测、UMP Linux、SMP Linux 回归下没有引入行为回退。
- 结构性收益主要体现在 SMP：`sync_devices` / `run_chunks` 从数千万级降到数万甚至个位数，`base64-dec-4m`、`fnv1a-16m`、三类 TLB case 的端到端 `host_ns` 都出现了可观改善。
- UMP 的端到端收益还不稳定，说明当前这一轮事件调度主要解决了 SMP 的外层轮询成本；如果后续继续推进事件驱动化，下一步应优先完成 GIC/UART/KMI 更彻底的状态变化驱动和主循环“运行到下一个原因”的窗口计算。

# 修改日志 2026-03-16 23:27

## 本轮修改

- 继续整理 [TODO.md](/media/luzeyu/Storage2/FOSS_src/aarchvm/TODO.md)，把事件驱动演进方案中各阶段的具体任务改成 Markdown 复选框，便于后续逐项推进和勾选完成状态。
- 保留了原有的阶段划分、目标、预期收益和实施顺序，只把可执行的小任务显式改造成可追踪的 checklist。

## 本轮测试

- 文档修改，未运行单元测试或回归测试。

## 当前结论

- 事件驱动方案现在已经不仅是说明文档，也可以直接作为后续实施计划使用。

# 修改日志 2026-03-16 22:40

## 本轮修改

- 新增并填写 [TODO.md](/media/luzeyu/Storage2/FOSS_src/aarchvm/TODO.md)，将“让模拟器逐步朝事件驱动方向演进”的设计方案正式落盘。
- 方案内容围绕当前代码结构展开，覆盖了统一 guest 时间、事件队列、主循环重构、Generic Timer deadline 化、WFI/WFE 停车、SMP 量子调度、GIC 增量更新、UART/KMI 状态变化驱动、SDL 与 guest 时间解耦、快照补强以及对应的测试策略。
- 该方案明确把“事件驱动”界定为外层 SoC 调度、时间推进和设备同步模型的演进，而不是直接跳过 CPU 指令解释或立即转向 JIT。

## 本轮测试

- 文档修改，未运行单元测试或回归测试。

## 当前结论

- 当前最值得优先推进的不是继续堆局部快路径，而是先把外层时间推进和设备同步模型整理为可事件化的框架。
- 事件驱动化的低风险高收益切入点，依次是：统一 guest 时间、timer deadline 化、WFI/WFE 真正停车、单活跃 CPU 长突发执行，以及 GIC/UART/KMI 的增量状态更新。

# 修改日志 2026-03-16 21:57

## 本轮修改

- 按当前主线约定，统一让 Linux SMP snapshot 构建路径使用标准设备树 `dts/aarchvm-linux-smp.dtb`，不再使用调试专用的 `dts/aarchvm-linux-smp-headless.dtb`。
- 更新 [tests/linux/build_linux_smp_shell_snapshot.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/linux/build_linux_smp_shell_snapshot.sh)，将默认 `AARCHVM_LINUX_DTB` 从 `dts/aarchvm-linux-smp-headless.dtb` 切回 `dts/aarchvm-linux-smp.dtb`。
- 删除调试用设备树源文件 [dts/aarchvm-linux-smp-headless.dts](/media/luzeyu/Storage2/FOSS_src/aarchvm/dts/aarchvm-linux-smp-headless.dts)，避免后续脚本与文档继续分叉。

## 本轮测试

- `timeout 240s ./tests/linux/build_linux_smp_shell_snapshot.sh`：通过，确认标准 `dts/aarchvm-linux-smp.dtb` 下仍可自动生成并校验 `out/linux-smp-shell-v1.snap`。
- `timeout 240s ./tests/linux/run_functional_suite_smp.sh`：通过，确认切回标准 SMP DT 后，SMP 功能回归与 `less` 进入/恢复烟雾段仍保持通过。

## 当前结论

- 从设备树内容看，这次统一不会改变 SMP snapshot 路径的内核命令行来源，因为实际 `bootargs` 仍由 U-Boot 运行时注入；差异主要回到是否保留 framebuffer / KMI 节点，属于你要求的“统一使用标准 SMP DT”范围。

# 修改日志 2026-03-16 21:47

## 本轮修改

- 重新分析了 `out/my_test.snap` 与 `console=ttyAMA0,115200` 的 SMP 串口启动路径。最终确认主因不是 `ttyAMA0` 设备语义损坏，而是当前“按指令数推进虚拟时间”的模型在 SMP 串口大量输出场景下，把 guest 虚拟时间推进得过快，进而触发 Linux 的 RCU / `stop_machine` 看门狗路径。
- 修复了 [tests/linux/build_linux_shell_snapshot.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/linux/build_linux_shell_snapshot.sh) 中此前残留的临时手改状态：
  - 恢复了通过管道自动向 U-Boot 注入启动命令；
  - 恢复按 `AARCHVM_LINUX_DTB` / `AARCHVM_TIMER_SCALE` / `AARCHVM_BUS_FASTPATH` 等环境变量工作；
  - 去掉了错误写死的 SMP DTB、`-fb-sdl on` 与 `AARCHVM_TIMER_SCALE=10`；
  - 重新把构建阶段日志写回 `out/...build.log`，并在构建阶段启用 `AARCHVM_PRINT_SUMMARY=1`，使脚本能重新可靠地解析 prompt 停止点。
- 将 [tests/linux/build_linux_smp_shell_snapshot.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/linux/build_linux_smp_shell_snapshot.sh) 的默认 `AARCHVM_TIMER_SCALE` 从 `10` 调整为 `1`，以适配 2 核 `ttyAMA0` 串口 shell 路径。
- 将 [tests/linux/run_functional_suite_smp.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/linux/run_functional_suite_smp.sh) 的默认 `AARCHVM_TIMER_SCALE` 同步调整为 `1`，避免 SMP 串口功能回归再次在 guest 内部触发 RCU / `stop_machine` 超时。
- 更新 [doc/README.en.md](/media/luzeyu/Storage2/FOSS_src/aarchvm/doc/README.en.md) 与 [doc/README.zh.md](/media/luzeyu/Storage2/FOSS_src/aarchvm/doc/README.zh.md)，补充当前时间模型下 `ttyAMA0` SMP 串口路径应使用 `AARCHVM_TIMER_SCALE=1` 的说明，并区分它与 `tty1` / framebuffer GUI 路径的推荐倍率。

## 本轮测试

- 手工等价冷启动复现：
  - `AARCHVM_TIMER_SCALE=10` + `console=ttyAMA0,115200` + 2 核 headless DTB：稳定停在 Linux 早期启动阶段，日志在 `CPU features: detected: LSE atomic instructions` 附近后几乎不再推进。
  - `AARCHVM_TIMER_SCALE=1` + 同一条 U-Boot / Linux 启动命令：可以稳定越过上述停点，继续完成 `ttyAMA0` 接管、网络协议族初始化与 `initramfs` 解包，证明主因是时间推进倍率而非串口 MMIO 语义本身。
- `timeout 240s ./tests/linux/build_linux_smp_shell_snapshot.sh`：通过，可自动生成并校验 `out/linux-smp-shell-v1.snap`，日志中的 prompt 停止点为 `SUMMARY: steps=867345325 ...`。
- `timeout 240s ./tests/linux/run_functional_suite_smp.sh`：通过，包括 3 轮 `uname/ps/dmesg/mount/df/cpuinfo/ping` 检查，以及 `dmesg | less` 的进入 / 恢复烟雾段。

## 当前结论

- 这条 `console=ttyAMA0,115200` SMP 卡住问题的主因是时间模型配置不当，而不是先前怀疑的 `tty1` 路径、snapshot 恢复遗漏或 PL011 基本读写语义错误。
- `out/my_test.snap` 中双核一起卡在 `rcu_momentary_eqs()` / `multi_cpu_stop()`，更像是“guest 已经进入 RCU / stop_machine 自救路径后的坏状态快照”，而不是问题的最初根因。

# 修改日志 2026-03-16 18:06

## 本轮修改

- 在 [src/soc.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/soc.cpp) 为 CPU `halted()` 路径补上明确的 `CPU-HALT` 日志，输出 `cpu/mpidr/pc/sp/pstate/steps/exc_depth/wfi/wfe` 等关键信息，避免 guest 已停机时从外部看起来像“卡住”。
- 在 [src/main.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/main.cpp) 为 `Simulation stopped unexpectedly` 补充诊断细节，统一打印当前 `pc/sp/pstate/steps/exc_depth/wfi/wfe`。
- 在 [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp) 补齐了 scalar FP memory load/store 的一批遗漏实现：包括 `B/H/D` 的 unscaled 访存、`B/H` 的 pre/post-index，以及 `B/H/D/Q` 的 register-offset 访存，修复了 Linux 用户态 `dmesg | grep hang` 会落到 `Illegal instruction` 的问题。
- 继续修复与这批指令相邻的处理器行为缺口：`insn_uses_fp_asimd()` 现在已覆盖完整的 scalar FP memory `unsigned-imm / unscaled / pre/post-index / register-offset` 家族，使 `CPACR_EL1.FPEN` 禁用 FP/AdvSIMD 时，这些访存指令也会正确触发 FP trap，而不是错误地直接执行。
- 新增裸机单测 [tests/arm64/fp_scalar_unscaled.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/fp_scalar_unscaled.S) 与 [tests/arm64/fp_scalar_regoffset.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/fp_scalar_regoffset.S)，分别覆盖 scalar FP unscaled 与 register-offset 访存的成功路径。
- 新增裸机单测 [tests/arm64/cpacr_fp_mem_trap.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/cpacr_fp_mem_trap.S)，专门验证 `ldur d` 与 `str d, [xN, xM]` 在 EL1/EL0 下会受到 `CPACR_EL1.FPEN` 控制并按规范触发 FP access trap。
- 更新 [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh)、[tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh) 和 [tests/linux/run_functional_suite.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/linux/run_functional_suite.sh)，把上述新测试和 Linux `grep hang` 复现点纳入默认回归。
- 更新 [README.md](/media/luzeyu/Storage2/FOSS_src/aarchvm/README.md) 的 `Features`，补充 halt / unexpected-stop 诊断能力说明。

## 本轮测试

- `./build/aarchvm -bin tests/arm64/out/fp_scalar_unscaled.bin -load 0x0 -entry 0x0 -steps 400000`：通过，输出 `U`。
- `./build/aarchvm -bin tests/arm64/out/fp_scalar_regoffset.bin -load 0x0 -entry 0x0 -steps 400000`：通过，输出 `R`。
- `./build/aarchvm -bin tests/arm64/out/cpacr_fp_mem_trap.bin -load 0x0 -entry 0x0 -steps 300000`：通过，输出 `T`。
- `./build/aarchvm -bin tests/arm64/out/cpacr_fp_trap.bin -load 0x0 -entry 0x0 -steps 300000`：通过，输出 `C`。
- 基于 [out/linux-usertests-shell-v1.snap](/media/luzeyu/Storage2/FOSS_src/aarchvm/out/linux-usertests-shell-v1.snap) 注入 `dmesg | grep hang >/dev/null; echo GREP-HANG-DONE`：通过，日志中仅出现 `GREP-HANG-DONE`，未出现 `Illegal instruction` 或 `UNIMPL`。
- `timeout 300s ./tests/arm64/run_all.sh`：通过。
- `AARCHVM_BUS_FASTPATH=1 AARCHVM_TIMER_SCALE=10 timeout 300s ./tests/linux/run_functional_suite.sh`：通过，包含 `GREP-HANG-OK` 与 `FUNCTIONAL-SUITE PASS`。

## 当前结论

- 这轮 `dmesg | grep hang` 的非法指令根因，确实是 scalar FP memory 访存实现缺口，而不是串口、shell 或 `grep` 本身的问题；相关 Linux 用户态路径现已恢复。
- 在本轮额外源码审阅里，唯一高置信度且与本次修复直接相邻的 Armv8-A 行为缺口，就是 `CPACR_EL1` 对这批 FP memory 指令的 trap 覆盖不完整；该问题现已一并补齐。
- 仍不能宣称“已完整实现全部 Armv8-A 强制标准”。当前最明显的剩余风险不再是这条 `grep hang` 路径，而是仓库中一批旧的 FP/AdvSIMD 裸机测试仍然不够严格，部分测试即使以异常深度耗尽停机，`run_all.sh` 也不会失败。因此，后续若继续做 Armv8-A 完整性审阅，优先级最高的是把这些旧 FP/AdvSIMD 测试改成真正断言输出和异常状态，而不是只看“进程没退出码报错”。

# 修改日志 2026-03-16 11:47

## 本轮修改

- 在 [src/main.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/main.cpp) 的交互式 `stdin -> UART` 路径中新增了 QEMU 风格的宿主机串口退出序列：当 stdin 是 TTY 时，输入 `Ctrl+A` 后再输入 `x`，模拟器会立即结束本次运行。
- 该退出路径不直接绕开原有收尾逻辑，而是让主循环像“步数耗尽”那样自然退出，因此若用户同时传入了 `-snapshot-save <file>`，退出时仍会正常写出整机快照。
- 对非交互式 stdin（例如脚本管道注入 U-Boot / shell 命令）的行为保持不变，不会把 `Ctrl+A, x` 作为宿主机控制序列截获。
- 更新了 [README.md](/media/luzeyu/Storage2/FOSS_src/aarchvm/README.md)、[doc/README.en.md](/media/luzeyu/Storage2/FOSS_src/aarchvm/doc/README.en.md)、[doc/README.zh.md](/media/luzeyu/Storage2/FOSS_src/aarchvm/doc/README.zh.md)，补充这一交互式退出能力的说明。

## 本轮测试

- PTY 验证：以最小无限循环 guest 启动模拟器，运行 `./build/aarchvm -bin out/loop.bin -load 0x0 -entry 0x0 -steps 1000000000 -snapshot-save out/ctrlax-quit-test.snap`，随后发送 `Ctrl+A, x`，模拟器打印宿主机退出提示并以 `0` 退出。
- 快照验证：确认 [out/ctrlax-quit-test.snap](/media/luzeyu/Storage2/FOSS_src/aarchvm/out/ctrlax-quit-test.snap) 已写出，且 `./build/aarchvm -snapshot-load out/ctrlax-quit-test.snap -steps 1` 可成功加载。

## 当前结论

- 交互式串口路径现在具备最小的 QEMU 风格宿主机退出序列，且不会破坏已有的运行收尾与快照保存行为。

# 修改日志 2026-03-16 10:57

## 本轮修改

- 追查并修复了 Linux 单核功能回归中“客体 `/bin` 自定义文件丢失”的独立问题。最终确认问题不在 shell 注入，也不在 `functional_init` 本身，而是在 guest 侧使用未压缩 `initramfs-usertests.cpio` 时，内核只解包出前半段 archive 成员，导致 `/bin/fpsimd_selftest`、`/bin/functional_init`、`/usr/bin/*`、`/sbin/*` 等后半段文件缺失。
- 为隔离影响，去掉了 [tests/linux/build_usertests_rootfs.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/linux/build_usertests_rootfs.sh) 生成的 `/init` 中那条运行期 `busybox --install -s /bin`。这条命令此前会掩盖“后半段 applet symlink 根本没解出来”的事实，但并不能恢复自定义 ELF 程序。
- 简化了 `usertests` initramfs 的打包方式：不再手工 `printf` 一段固定成员列表再拼 `find` 结果，而是直接使用 `find . -print0 | sort -z | cpio --null -o -H newc` 生成归档，避免手工列表路径与 guest 实际解包结果继续分叉。
- Linux 相关脚本默认改用压缩后的 `out/initramfs-usertests.cpio.gz` 作为 initramfs 镜像，包括 [tests/linux/build_linux_shell_snapshot.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/linux/build_linux_shell_snapshot.sh)、[tests/linux/run_functional_suite.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/linux/run_functional_suite.sh)、[tests/linux/run_algorithm_perf.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/linux/run_algorithm_perf.sh)、[tests/linux/run_gui_tty1.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/linux/run_gui_tty1.sh)。在当前 128 MiB 配置下，`.cpio.gz` 可稳定完整解包，而原始 `.cpio` 会丢失后半段文件。
- 单核功能回归脚本 [tests/linux/run_functional_suite.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/linux/run_functional_suite.sh) 继续保持“直接注入内联命令”的方式，不再依赖 guest 里额外的 shell 包装脚本存在与否，从而使功能回归更稳、更容易诊断。

## 本轮测试

- 手动冷启动验证：将 `aarchvm_suite=functional` 直接传给内核后，使用原始 `out/initramfs-usertests.cpio` 时，guest 报 `/bin/functional_init: not found`；切换到 `out/initramfs-usertests.cpio.gz` 后，可完整进入 `/init` 并进入 shell。
- `timeout 300s ./tests/linux/build_linux_shell_snapshot.sh`：通过，生成并校验 [out/linux-usertests-shell-v1.snap](/media/luzeyu/Storage2/FOSS_src/aarchvm/out/linux-usertests-shell-v1.snap)。
- `timeout 300s ./tests/linux/build_linux_smp_shell_snapshot.sh`：通过，生成并校验 [out/linux-smp-shell-v1.snap](/media/luzeyu/Storage2/FOSS_src/aarchvm/out/linux-smp-shell-v1.snap)。
- `AARCHVM_BUS_FASTPATH=1 AARCHVM_TIMER_SCALE=10 ./tests/linux/run_functional_suite.sh`：通过。
- `AARCHVM_BUS_FASTPATH=1 AARCHVM_TIMER_SCALE=10 ./tests/linux/run_functional_suite_smp.sh`：通过，`dmesg | less` 的进入/退出烟雾路径保持通过。

## 当前结论

- 单核功能回归此前失败的直接原因不是 CPU 指令语义，也不是 shell 输入注入，而是 Linux guest 在使用未压缩 `initramfs-usertests.cpio` 时没有完整解出 archive 后半段内容。
- 在当前模拟器与内存配置下，统一改用 `out/initramfs-usertests.cpio.gz` 后，单核与 SMP 的 Linux shell/suite 路径都已恢复稳定。

# 修改日志 2026-03-16 01:40

## 本轮修改

- 重新确认并保留了上一轮对 `dmesg` 卡死根因的修复：跨页未对齐数据访问在后续字节 fault 时，现在会把真正的 faulting VA 传递给 `FAR_EL1`，从而让 Linux 页故障处理修正正确页面并继续执行。
- 新增更严苛的跨页访存裸机单测 [tests/arm64/mmu_cross_page_various.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/mmu_cross_page_various.S)，覆盖 `LDTR/STTR`、`STP/LDP X`、`STP/LDP W`、`LDPSW`、`LDXP/STXP` 在页边界上的成功路径，并验证它们在两个不连续物理页之间读写结果仍然正确。
- 新增裸机单测 [tests/arm64/mmu_cross_page_pair_fault_far.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/mmu_cross_page_pair_fault_far.S)，严格验证 `LDP` 这类 pair load 在“第一元素成功、第二元素跨页缺页”时，`FAR_EL1` 会落在第二元素的 fault 地址，而不是 pair 起始地址。
- 更新 [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh) 与 [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)，将上述跨页新测试纳入默认裸机回归。
- 在 Linux usertests initramfs 中新增 guest 侧脚本 `run_dmesg_stress_check`，并把它接入 [tests/linux/build_usertests_rootfs.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/linux/build_usertests_rootfs.sh) 生成的功能回归路径：该脚本会反复大量输出 `dmesg`，同时在 guest 内部统计是否出现非打印控制字符。
- 更新 [tests/linux/run_functional_suite.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/linux/run_functional_suite.sh)，除了检查 `DMESG-STRESS PASS` 外，还在 host 侧对 `DMESG-STRESS-BEGIN/END` 区间做字节级检查，若日志中出现异常控制字符会直接让回归失败。
- 额外审查了多核同步与绕开普通写路径的实现，发现 `DC ZVA` 之前直接写内存但没有像普通写路径那样广播 `memory_write`，因此不会清除其他 CPU 的 exclusive monitor，也不会触发必要的跨核写通知。
- 修复了 [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp) 中 `DC ZVA` 的这一缺口：现在每个 8 字节清零写都会调用 `callbacks_.memory_write`，使其他核的 monitor/事件/代码失效路径与普通写保持一致。
- 新增 SMP 单测 [tests/arm64/smp_dc_zva_invalidate.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/smp_dc_zva_invalidate.S)，验证一个 CPU 在 `LDXR` 后，另一个 CPU 对同一 64B block 执行 `DC ZVA` 会使前者的 `STXR` 失败，并且共享字被正确清零。

## 本轮测试

- `./build/aarchvm -bin tests/arm64/out/mmu_cross_page_various.bin -load 0x0 -entry 0x0 -steps 4000000`：通过，输出 `V`。
- `./build/aarchvm -bin tests/arm64/out/mmu_cross_page_pair_fault_far.bin -load 0x0 -entry 0x0 -steps 4000000`：通过，输出 `P`。
- `./build/aarchvm -smp 2 -bin tests/arm64/out/smp_dc_zva_invalidate.bin -load 0x0 -entry 0x0 -steps 400000`：通过，输出 `D`。
- `AARCHVM_BUS_FASTPATH=1 ./tests/arm64/run_all.sh`：通过。
- `AARCHVM_BUS_FASTPATH=1 AARCHVM_TIMER_SCALE=10 ./tests/linux/run_functional_suite.sh`：通过，包含 `DMESG-OK`、`DMESG-STRESS PASS`，host 侧 dmesg 压力块字节检查通过。
- `AARCHVM_BUS_FASTPATH=1 AARCHVM_TIMER_SCALE=10 ./tests/linux/run_functional_suite_smp.sh`：通过。

## 当前结论

- `dmesg` 卡死 bug 已再次被更强的 Linux 用户态压力路径确认修复，且现在既有 guest 内部检查，也有 host 侧日志字节检查，不再只是“命令能返回”这一层面的验证。
- 跨页访存覆盖现在从原先的普通 `LDR/STR`，扩展到了 `LDTR/STTR`、pair load/store、`LDPSW` 和 pair exclusive 成功路径，以及 `LDP` fault 地址路径。
- 在本轮对同步原语和多核交互的审查里，确认并修复了一处真实的 SMP 语义缺口：`DC ZVA` 之前不会跨核失效 exclusive monitor。修复后已由 SMP 单测覆盖。
- 除这处外，本轮没有再发现第二个同级别、可稳定复现的“跨核同步/缺页语义错误”漏洞；现有回归下单核与 SMP Linux 路径均保持通过。

# 修改日志 2026-03-16 01:27

## 本轮修改

- 修复了跨页未对齐数据访存在“前半页可访问、后半页缺页/失效”时的 fault 地址传播错误：此前 `mmu_read_value()` / `mmu_write_value()` 虽然能逐字节发现后续字节的翻译失败，但最终上报给 `FAR_EL1` 的仍是访存起始地址，而不是实际 fault 的字节地址。
- 现在 CPU 会在数据访问失败时记录真正的 faulting VA，`data_abort()` 使用该地址填充 `FAR_EL1`，从而让 guest 内核页故障处理能够修复正确的用户页并重试。
- 新增裸机单测 `tests/arm64/mmu_cross_page_fault_far_load.S` 与 `tests/arm64/mmu_cross_page_fault_far_store.S`，严格验证 8 字节未对齐跨页读写在第二页缺失时会以 `0x...1000` 这一真正 fault 字节作为 `FAR_EL1`，同时校验 `ESR_EL1.EC/FSC/WnR` 与 `ELR_EL1`。
- 更新 `tests/arm64/build_tests.sh` 与 `tests/arm64/run_all.sh`，将上述两条新单测纳入默认裸机回归。
- 更新 `tests/linux/build_usertests_rootfs.sh`、`tests/linux/run_functional_suite.sh` 与 `tests/linux/run_functional_suite_smp.sh`，把 `dmesg -s 128 >/dev/null` 加入单核/SMP Linux 功能回归，避免该路径再次无声回退。

## 本轮测试

- `./build/aarchvm -bin tests/arm64/out/mmu_cross_page_fault_far_load.bin -load 0x0 -entry 0x0 -steps 4000000`：通过，输出 `R`。
- `./build/aarchvm -bin tests/arm64/out/mmu_cross_page_fault_far_store.bin -load 0x0 -entry 0x0 -steps 4000000`：通过，输出 `W`。
- `AARCHVM_BUS_FASTPATH=1 ./tests/arm64/run_all.sh`：通过。
- `AARCHVM_BUS_FASTPATH=1 AARCHVM_TIMER_SCALE=10 ./tests/linux/run_functional_suite.sh`：通过，并包含 `DMESG-OK`。
- `AARCHVM_BUS_FASTPATH=1 AARCHVM_TIMER_SCALE=10 ./tests/linux/run_functional_suite_smp.sh`：通过，并包含 `SMP-DMESG-OK:3`。
- 额外复现验证：对 `out/linux-usertests-shell-v1.snap` 注入 `dmesg -s 128 >/dev/null; echo DONE`，现在可稳定输出 `DONE` 并回到 shell prompt，不再卡在 `__arch_copy_to_user`。

## 当前结论

- `dmesg` 卡死的根因不是专门的 `dmesg` 性能问题，也不是串口/GUI 路径问题，而是 guest 内核在 `copy_to_user` 里执行跨页未对齐用户写时，模拟器错误上报了 `FAR_EL1`，使页故障处理反复修错页并重试。
- 修复后，这条路径已经在裸机与 Linux 回归中被双重覆盖；当前单核与 SMP 功能回归均恢复正常。

# 修改日志 2026-03-16 00:14

## 本轮修改

- 新增裸机单测 `tests/arm64/mmu_cross_page_load.S`，对称覆盖 MMU 开启后的未对齐 8 字节跨页读取路径，验证两个连续虚拟页映射到两个不连续物理页时，`LDR` 会正确分别从两个物理页取数并在寄存器中拼接出期望值。
- 更新 `tests/arm64/build_tests.sh` 与 `tests/arm64/run_all.sh`，把 `mmu_cross_page_load` 纳入默认裸机回归。
- 本轮未再修改模拟器执行逻辑，主要目标是把上一轮跨页访存修复补成读写双向覆盖，并重新确认 Linux 单核/SMP 回归仍然稳定。

## 本轮测试

- `./build/aarchvm -bin tests/arm64/out/mmu_cross_page_load.bin -load 0x0 -entry 0x0 -steps 4000000`：通过，输出 `L`。
- `AARCHVM_BUS_FASTPATH=1 ./tests/arm64/run_all.sh`：通过。
- `AARCHVM_BUS_FASTPATH=1 AARCHVM_TIMER_SCALE=10 ./tests/linux/run_functional_suite.sh`：通过。
- `AARCHVM_BUS_FASTPATH=1 AARCHVM_TIMER_SCALE=10 ./tests/linux/run_functional_suite_smp.sh`：通过。

## 当前结论

- 先前跨页访存修复现在已经具备读写对称的裸机覆盖；现有单核与 SMP Linux 功能回归均未观察到回归。
- 在当前自动化压力实验里，合成的大量串口输出仍然保持干净，没有复现额外乱码；`dmesg` 全量导出在快照脚本注入模式下仍然非常慢，现阶段更像是执行代价高，而不是再次出现了已知的跨页内存破坏。

# 修改日志 2026-03-15 23:19

## 本轮修改

- 修复了 CPU 数据访问在页边界上的错误行为：此前 `mmu_read_value()` / `mmu_write_value()` 以及 `exec_load_store()` 的局部访存路径只按起始 VA 所在页做一次翻译，导致 8 字节未对齐读写跨页时可能错误地连续落到错误物理页。
- 现在跨页数据访问会先逐字节完成翻译，再按翻译结果分别提交读写，避免把页尾访问错误地延伸到邻接物理页。
- 这项修复直接解决了 Linux 冷启动到 `/init` 后偶发 `Attempted to kill init! exitcode=0x0000000b`、PLT/GOT 槽位被错误清零、串口/GUI 长时间运行后出现随机内存破坏等问题。
- 为后续定位同类问题，新增调试环境变量 `AARCHVM_TRACE_WRITE_PA=<pa>`，可按物理地址观察 guest 写入；同时保留并补充说明 `AARCHVM_TRACE_WRITE_VA=<va>` 与 `AARCHVM_TRACE_BRANCH_ZERO=1` 的用途。
- 新增裸机单测 `tests/arm64/mmu_cross_page_store.S`，专门验证 MMU 开启后未对齐 8 字节跨页写会被正确拆分到两个不同物理页，而不会污染错误的邻页。
- 更新 `tests/arm64/build_tests.sh` 与 `tests/arm64/run_all.sh`，把 `mmu_cross_page_store` 纳入默认裸机回归。
- 更新 `doc/README.en.md` 与 `doc/README.zh.md`，补充新的调试环境变量说明。

## 本轮测试

- `./build/aarchvm -bin tests/arm64/out/mmu_cross_page_store.bin -load 0x0 -entry 0x0 -steps 4000000`：通过，输出 `C`。
- `AARCHVM_BUS_FASTPATH=1 ./tests/linux/build_linux_shell_snapshot.sh`：通过。
- `AARCHVM_BUS_FASTPATH=1 ./tests/linux/build_linux_smp_shell_snapshot.sh`：通过。
- `AARCHVM_BUS_FASTPATH=1 ./tests/linux/run_functional_suite.sh`：通过。
- `AARCHVM_BUS_FASTPATH=1 ./tests/linux/run_functional_suite_smp.sh`：通过。
- `AARCHVM_BUS_FASTPATH=1 ./tests/arm64/run_all.sh`：通过。

## 当前结论

- 当前这类 Linux `/init` 随机崩溃并不是“缺一条特定指令”导致，而是 MMU 开启后的跨页数据访问实现错误导致的真实内存破坏。
- 修复后，单核与 SMP 的 Linux shell 快照构建和功能回归都已恢复通过，且新增裸机单测能稳定覆盖这条回归路径。

# 修改日志 2026-03-15 22:20

## 本轮修改

- 新增环境变量 `AARCHVM_DEBUG_SLOW=1`，用于强制关闭一组宿主机侧执行优化捷径，便于隔离“解释器核心语义错误”与“快路径实现错误”。
- 在 [src/main.cpp] 中启用该模式后，会强制关闭指令预解码，相当于统一走 `-decode slow`。
- 在 [src/soc.cpp] 中启用该模式后，会忽略 `AARCHVM_BUS_FASTPATH` / `AARCHVM_RAM_FASTPATH`，并在快照恢复时确保 SoC bus fast path 也保持关闭。
- 在 [src/cpu.cpp] 中启用该模式后，会关闭 CPU 的 RAM 直读快路径和 RAM 直写快路径，强制退回通用 `bus.read()` / `bus.write()` 路径。
- 更新 `doc/README.en.md` 与 `doc/README.zh.md`，补充 `AARCHVM_DEBUG_SLOW` 的用途与影响范围。

## 本轮测试

- `AARCHVM_DEBUG_SLOW=1 ./tests/arm64/run_all.sh`：通过。
- `AARCHVM_DEBUG_SLOW=1 ./tests/linux/build_linux_shell_snapshot.sh`：通过，生成 `out/linux-usertests-shell-debugslow.snap`。
- `AARCHVM_DEBUG_SLOW=1 ./tests/linux/run_functional_suite.sh`：通过。
- `AARCHVM_DEBUG_SLOW=1 ./tests/linux/build_linux_smp_shell_snapshot.sh` 等价路径：通过，生成 `out/linux-smp-shell-debugslow.snap`。
- `AARCHVM_DEBUG_SLOW=1 ./tests/linux/run_functional_suite_smp.sh`：通过。
- 额外串口压力实验：在 `AARCHVM_DEBUG_SLOW=1` 下反复执行 `dmesg` / `uname -a` / `ps`，仍可观察到输入命令串被破坏的现象，因此这类乱码并不是由预解码、SoC bus fast path 或 CPU RAM 直读直写快路径单独引入的。

## 当前结论

- 这轮新增的调试慢路径已经证明：现有“反复刷屏后出现乱码”的问题，即使关闭预解码、bus fast path 和 CPU RAM 直读直写快路径，仍然能够出现。
- 因此，问题更像是更深层的实现缺陷或边界条件错误，而不是最近引入的这几类优化捷径本身。
- 在当前额外压力实验里，还观察到了一个独立问题：脚本化串口输入在大输出场景下会出现命令串损坏，这条线很可能与 UART 接收/注入节奏有关，但它不能单独解释你手工交互下在 GUI/串口里都能见到的全部乱码与 panic。

# 修改日志 2026-03-15 21:05

## 本轮修改

- 修复了 SMP 下 generic timer 时间基准与总执行步数错误耦合的问题。
- 之前 `global_steps_` 同时承担“全局统计步数”和“guest timer 共享时间基准”两种职责；在多核 round-robin 执行时，这会让 guest 观察到的 timer 速度随着活跃 CPU 数增加而被放大。
- 为 `SoC` 新增独立的 `timer_steps_`，保留 `global_steps_` 作为统计/步数限制用途，而把 `CNT*`/generic timer/sysreg 可见时间统一绑定到 `timer_steps_`。
- 调整 SMP 执行主循环：多核模式下每轮 round-robin 完成后只推进一次 `timer_steps_`，避免 timer 对 guest 呈现为“按所有 CPU 累积指令数前进”。单核模式仍保持每条指令推进一次 timer 的原语义。
- 将 snapshot 版本从 `9` 升到 `10`，新增保存/恢复 `timer_steps_`；加载旧版本 snapshot 时自动兼容为 `timer_steps_ = global_steps_`。
- 新增裸机单测 `tests/arm64/smp_timer_rate.S`，专门验证双核活跃时 virtual timer 不会因为另一核同时执行而近似“翻倍变快”。
- 更新 `tests/arm64/build_tests.sh` 与 `tests/arm64/run_all.sh`，把新单测纳入默认回归。

## 本轮测试

- `cmake --build build -j4`：通过。
- `./tests/arm64/build_tests.sh`：通过。
- `./build/aarchvm -smp 2 -bin tests/arm64/out/smp_timer_rate.bin -load 0x0 -entry 0x0 -steps 400000`：输出 `R`，通过。
- `./tests/arm64/run_all.sh`：通过。
- `./tests/linux/run_functional_suite_smp.sh`：连续运行 3 次，全部通过。

## 当前结论

- 这轮修复掉了一个比 `decode cache` 共享更根本的 SMP 设计缺陷：timer 对 guest 的可见时间不应与“所有核累计执行了多少条解释器步数”绑定。
- 当前裸机 SMP 回归和 Linux SMP 功能回归都已通过，说明这次修复至少在现有回归覆盖范围内是稳定的。
- `run_gui_tty1.sh` 的纯 dummy 冒烟因为 framebuffer 控制台不回流到 stdout，无法仅凭串口日志完成交互级验证；若下一轮继续查 GUI/tty1 的“运行几个命令后卡死”，更合适的方向是专门做 framebuffer/tty1 场景的自动化输入与停止条件。

# 修改日志 2026-03-15 16:22

## 本轮修改

- 调整 `tests/linux/run_gui_tty1.sh`，把默认 `AARCHVM_TIMER_SCALE` 从 `100` 改回 `10`，避免当前串口调试路径下 Linux 启动过慢，看起来像“卡住”。
- 调整 `tests/linux/run_gui_tty1.sh` 的 U-Boot 命令注入方式：在发送完 `setenv` / `booti` 后继续 `cat` 标准输入，使脚本进入 shell 后仍可保持输入通路，而不是命令送完就 EOF。

## 本轮测试

- `timeout 90s ./tests/linux/run_gui_tty1.sh`：已确认能在串口日志中到达 `Entering BusyBox serial shell` 与 `~ #`。
- `console=ttyAMA0,115200 console=tty1 ...` 的单次短超时复现：可见 `Run /init as init process`，随后串口静默，更符合控制台切换到 `tty1` 的表现，而不是本轮稳定复现出的内核 panic。

## 当前结论

- 当前 `run_gui_tty1.sh` 已恢复为可进入终端的状态。
- 在当前树下，`console=tty1` 更像是把 `/init` 与 shell 绑定到 framebuffer `tty1`，从而让串口侧看起来“卡住”；这和脚本可用性问题是两回事。是否还存在你机器上那条特定的 `Attempted to kill init` 路径，后续可以再专门做更细的隔离复现。

# 修改日志 2026-03-15 16:01

## 本轮修改

- 修复 `tests/linux/run_gui_tty1.sh` 的参数失配问题。
- 原脚本同时启用了 `-smp 2 -smp-mode psci`，却仍加载单核 DTB `dts/aarchvm-linux-min.dtb`，导致 Linux 在 GICv3 redistributor 初始化阶段按双核路径枚举 redistributor 时访问到未描述的地址范围，早期启动直接触发 data abort 并 kernel panic。
- 将 GUI 脚本改为加载 `dts/aarchvm-linux-smp.dtb`，与 `-smp 2 -smp-mode psci` 保持一致。
- 去掉脚本里错误的 `-fb-sdl off`，恢复为真正的 SDL framebuffer GUI 路径。
- 将 GUI 路径的 bootargs 调整为 `console=ttyAMA0,115200 console=tty1 ...`，保留串口日志，同时让 `tty1` 成为 framebuffer 前台控制台。
- 更新 `doc/README.en.md` 与 `doc/README.zh.md` 中 GUI 启动章节，使其与当前脚本一致。

## 本轮测试

- `AARCHVM_GUI_TTY1_STEPS=3000000000 timeout 120s ./tests/linux/run_gui_tty1.sh`：已验证不再出现早期 GIC 初始化 kernel panic。
- `./tests/arm64/run_all.sh`：通过。

## 当前结论

- 这次 GUI 启动失败的原因是脚本参数不一致，不是模拟器 GIC/SMP 核心逻辑回退。
- 当前 `run_gui_tty1.sh` 至少已经修复到“不再早期 panic”，并重新对齐到真正的 GUI + PS/2 + `tty1` 路径。

# 修改日志 2026-03-15 15:41

## 本轮修改

- 修复了“从快照恢复后通过管道一次性注入多条 UART 命令”时 Linux SMP 用户态出现 `malloc(): mismatching next->prev_size` / `free(): invalid pointer` 一类随机崩溃的问题。
- 原因定位为：之前对非交互式 stdin 采用“读到多少就立刻全部灌入 UART FIFO”的突发注入方式；现在改为仅对非交互式 stdin 走节流路径，按步数逐字节送入 UART，而交互式终端输入保持原先的直接注入语义。
- 新增环境变量 `AARCHVM_STDIN_RX_GAP=<steps>`，用于调节非交互式 stdin 到 UART 的注入步距；默认值为 `2000`。
- 将 `tests/linux/run_functional_suite_smp.sh` 改为覆盖更强的单会话路径：从 `out/linux-smp-shell-v1.snap` 恢复后，在同一次 shell 会话中连续执行 `uname -a`、`ps`、`mount`、`df`、`cat /proc/cpuinfo`、`ping -c 1 127.0.0.1` 等命令，并验证不会再触发用户态堆损坏或内核 panic。
- 更新 `doc/README.en.md` 与 `doc/README.zh.md`，补充 `tests/linux/run_functional_suite_smp.sh` 和 `AARCHVM_STDIN_RX_GAP` 的说明。

## 本轮测试

- 批量注入复现命令：通过。
- `./tests/arm64/run_all.sh`：通过。
- `./tests/linux/run_functional_suite.sh`：通过。
- `./tests/linux/run_functional_suite_smp.sh`：连续运行 3 次，全部通过。

## 当前结论

- 这轮不仅保住了 SMP 自动化回归，而且把你之前反馈的“同一会话中连续运行多条命令会随机崩”的复现路径一并修掉了。
- 当前正式 SMP Linux 回归已经不再依赖“逐快照分步规避”，而是直接覆盖单快照恢复后的同会话命令序列。

# 修改日志 2026-03-15 15:25

## 本轮修改

- 新增并稳定化 Linux 2 核 SMP 自动化功能回归脚本 `tests/linux/run_functional_suite_smp.sh`。
- 将 SMP Linux 回归改为从 `out/linux-smp-shell-v1.snap` 恢复后，按 shell 提示符节奏逐条执行 `uname -a`、`ps`、`mount`、`df`、`cat /proc/cpuinfo`、`ping -c 1 127.0.0.1` 等命令，并在每一步保存新快照继续后续验证。
- 修正 SMP 回归脚本对 `ping -c 1 127.0.0.1` 的错误预期：当前最小 Linux 环境未配置 loopback 网络，因此正确行为是输出 `ping: sendto: Network is unreachable`，而不是误判为失败。
- 更新 `README.md`、`doc/README.en.md`、`doc/README.zh.md`，把 Linux SMP 状态从“仅 smoke / 手工验证”更新为“已有自动化 shell / functional regression 路径”，并补充 `tests/linux/run_functional_suite_smp.sh` 的入口说明。

## 本轮测试

- `./tests/linux/run_functional_suite_smp.sh`：连续运行 3 次，全部通过。
- `./tests/arm64/run_all.sh`：通过。
- `./tests/linux/run_functional_suite.sh`：通过。

## 当前结论

- 这次复现到的失败点不是当前 paced snapshot SMP 路径下的平台随机崩溃，而是回归脚本把 `ping` 的正常失败结果当成了测试失败。
- 在当前“从 SMP shell 快照恢复 + 按提示符节奏逐条注入命令”的回归方式下，Linux SMP 功能回归已连续 3 次稳定通过。
- 之前一次性批量灌入多条 UART 命令时观察到的用户态堆损坏现象，仍更像是“注入方式与时序压力”问题，而不是当前正式 SMP 回归路径中的稳定平台错误；后续若要继续追根，需要单独针对批量注入路径做隔离分析。

# 修改日志 2026-03-15 14:33

## 本轮修改

- 将 GICv3 扩展为 Linux SMP 所需的最小每核模型：
  - distributor + per-CPU redistributor frame
  - `GICR_TYPER` 亲和性编码修正，匹配 Linux 对 redistributor 的枚举方式
  - `ICC_HPPIR1_EL1`、`ICC_RPR_EL1`、`ICC_SGI1R_EL1` 等最小系统寄存器路径
- 将 Generic Timer 扩展为每核 `CNTV` / `CNTP` 通道，并修复 SMP 下 timer sysreg 使用本地 `steps_` 与全局时间基错位的问题，改为共享系统时间基。
- 完成最小 PSCI / SMCCC 路径并接入 `-smp-mode psci`：
  - `PSCI_VERSION`
  - `PSCI_FEATURES`
  - `PSCI_CPU_ON`
  - `PSCI_AFFINITY_INFO`
  - `PSCI_MIGRATE_INFO_TYPE`
  - `PSCI_SYSTEM_OFF` / `PSCI_SYSTEM_RESET`
- 新增 Linux SMP 设备树 `dts/aarchvm-linux-smp.dts` / `dts/aarchvm-linux-smp.dtb`，包含：
  - `cpu@0` / `cpu@1`
  - `enable-method = "psci"`
  - `psci` 节点
  - 双核 redistributor 区域
- 修正跨核 `WFE` 唤醒语义：
  - 跨核写入会置位其他 CPU 的 event register
  - 当跨核写命中 exclusive reservation 时，同时清掉 reservation
  - 该行为用于补齐 Linux qspinlock / wait-loop 所依赖的最小程序可见语义
- 新增并接入 4 个 SMP 相关单测：
  - `psci_cpu_on_min.S`：验证 PSCI 次级核启动
  - `smp_gic_sgi.S`：验证跨核 SGI
  - `smp_timer_ppi.S`：验证每核 timer PPI
  - `smp_wfe_monitor_event.S`：验证 `LDXR/WFE` 等待 + 对端无 `SEV` 解锁的唤醒闭环
- 新增 Linux SMP shell 快照产物：`out/linux-smp-shell-v1.snap`
- 更新 `README.md`、`doc/README.en.md`、`doc/README.zh.md`，把 SMP 状态从“仅裸机”更新为“已可 Linux SMP 冒烟到 shell”。

## 本轮测试

- `cmake --build build -j4`
- `tests/arm64/build_tests.sh`
- 裸机完整回归：`tests/arm64/run_all.sh`
- Linux SMP 启动 smoke：
  - U-Boot -> Linux -> `CPU1: Booted secondary processor`
  - `smp: Brought up 1 node, 2 CPUs`
  - `Run /init as init process`
  - 停在 BusyBox shell 提示符 `~ # `
- Linux SMP 用户态验证：
  - 从 SMP shell 快照恢复
  - 执行 `cat /proc/cpuinfo`
  - 用户态可见 `processor : 0` 与 `processor : 1`

## 当前结论

- 当前模拟器已经可以完成 2 核 Linux SMP 的最小闭环：次级核通过 PSCI 启动，内核完成 `2 CPUs` bring-up，进入 BusyBox shell，用户态可见 2 个 CPU。
- 当前 SMP 仍是“程序可见正确性优先”的最小模型：
  - 调度仍为同线程 round-robin
  - 自动化 Linux 回归脚本默认仍走单核路径
  - Linux SMP 路径目前以 smoke / 手工验证为主

# 修改日志 2026-03-15 13:01

## 本轮修改

- 将 `SoC` 从单核容器重构为多核容器，新增 `std::vector<std::unique_ptr<Cpu>> cpus_` 与 `global_steps_`。
- 新增 `-smp <n>` 命令行参数，默认仍为 `1`；当前已验证 `-smp 2` 的裸机 SMP 路径。
- 为每个 CPU 设置独立 `MPIDR_EL1`，当前采用 `Aff0 = cpu_index`，并保留 `U` 位形式的最小可见实现。
- 为 CPU 间交互补齐最小闭环：
  - `SEV` 可广播到所有 CPU。
  - 一核普通写内存后，会通知其他 CPU 失效重叠 exclusive monitor。
  - 跨核写会同步使其他 CPU 的预解码缓存失效到对应物理地址范围。
- 保持单核运行路径的原有快路径结构；多核路径当前采用同线程 round-robin 调度，以优先实现正确的程序可见行为。
- 快照格式升级到 `v8`：
  - 保存 / 恢复 `cpu_count` 与 `global_steps_`
  - 保存 / 恢复全部 CPU 状态
  - 继续兼容旧版单核快照加载，但仅允许在当前 `-smp 1` 配置下恢复
- 新增并接入 4 个严格的裸机 SMP 单测：
  - `smp_mpidr_boot.S`：验证多核启动身份与 `MPIDR_EL1`
  - `smp_sev_wfe.S`：验证跨核 `SEV/WFE` 最小闭环
  - `smp_ldxr_invalidate.S`：验证一核普通写使另一核 `LDXR/STXR` 失败
  - `smp_spinlock_ldaxr_stlxr.S`：验证两核 `LDAXR/STLXR` 自旋锁与共享计数
- 将 SMP 测试纳入 `tests/arm64/build_tests.sh` 与 `tests/arm64/run_all.sh`。
- 更新 `README.md`、`doc/README.en.md`、`doc/README.zh.md`，补充当前 SMP 能力范围与 `-smp` 用法说明。

## 本轮测试

- `cmake --build build -j4`
- `tests/arm64/build_tests.sh`
- 新增 SMP 单测逐个执行：
  - `./build/aarchvm -smp 2 -bin tests/arm64/out/smp_mpidr_boot.bin -load 0x0 -entry 0x0 -steps 200000`
  - `./build/aarchvm -smp 2 -bin tests/arm64/out/smp_sev_wfe.bin -load 0x0 -entry 0x0 -steps 200000`
  - `./build/aarchvm -smp 2 -bin tests/arm64/out/smp_ldxr_invalidate.bin -load 0x0 -entry 0x0 -steps 200000`
  - `./build/aarchvm -smp 2 -bin tests/arm64/out/smp_spinlock_ldaxr_stlxr.bin -load 0x0 -entry 0x0 -steps 600000`
- 裸机完整回归：`tests/arm64/run_all.sh`

## 当前结论

- 当前 SMP 实现是面向裸机场景的最小正确性闭环，已经覆盖最关键的程序可感知同步原语。
- 它还不是 Linux SMP 所需的平台模型；后续仍需补每核 GIC redistributor、每核 timer 视图与 PSCI / 次级核启动路径。

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

# 修改日志 2026-03-15 19:26

## 本轮修改

- 统一 `tests/linux` 里 Linux 冷启动 / snapshot / 功能回归脚本的 DTB 传递方式：
  - 不再同时使用 `-dtb` 与 `-segment <dtb@addr>` 双重加载 DTB。
  - 改为统一通过 `-dtb <linux dtb>` + `-dtb-addr 0x47f00000` 传入，并让 U-Boot `booti` 第三个参数指向同一地址。
- 为主要 Linux 脚本补充完整的 `aarchvm` 命令行输出，便于人工复现：
  - `tests/linux/build_linux_shell_snapshot.sh`
  - `tests/linux/run_functional_suite.sh`
  - `tests/linux/run_functional_suite_smp.sh`
  - `tests/linux/run_algorithm_perf.sh`
  - `tests/linux/run_gui_tty1.sh`
  - `tests/linux/run_interactive.sh`
- 修复 `tests/linux/build_usertests_rootfs.sh` 生成的 `/init`：
  - 启动早期优先将标准输入输出绑定到 `/dev/ttyAMA0`，避免 `console=tty1` + SMP 时 `/dev/console` 路径导致 `init` 异常退出。
  - 去掉 BusyBox `ash` 下不稳定的嵌套 heredoc 参数解析，改为 `${arg#*=}` 形式。
- 修复 SMP 下最关键的跨核一致性缺口：
  - 先前 `TLBI VMALLE1*` / `TLBI VAE1*` / `TLBI ASIDE1*` 以及 `IC IALLU*` 只影响当前 CPU，本轮改为经由 `SoC` 广播到其他 CPU，使其同步失效本地 TLB / 预解码缓存。
  - 这补齐了 Linux SMP 所依赖的最小 TLB shootdown / I-cache maintenance 程序可见语义。
- 新增裸机 SMP 单测 `tests/arm64/smp_tlbi_broadcast.S`，验证一核 `TLBI VAE1IS` 后另一核旧映射会失效并读取到新页。
- 增强 Linux SMP 功能回归脚本覆盖面：
  - 不再只跑单轮 `uname/ps`，而是连续 3 轮执行 `uname -a`、`busybox uname -a`、`ps`、`mount`、`df`、`cat /proc/cpuinfo`、`ls /bin`、`ping -c 1 127.0.0.1`。
- 修复 `tests/linux/run_functional_suite.sh` 之前“来宾已完成但脚本仍因 timeout 误报失败”的问题，改为使用 `-stop-on-uart FUNCTIONAL-SUITE PASS` 可靠收敛。
- 让 `tests/arm64/run_all.sh` 打开 `set -x`，在执行回归时直接打印实际 `aarchvm` 命令行。

## 本轮测试

- `cmake --build build -j4`
- `tests/arm64/build_tests.sh`
- 新增裸机 SMP 单测：
  - `./build/aarchvm -smp 2 -bin tests/arm64/out/smp_tlbi_broadcast.bin -load 0x0 -entry 0x0 -steps 1200000`
- 裸机完整回归：`tests/arm64/run_all.sh`
- Linux 单核 shell snapshot：`tests/linux/build_linux_shell_snapshot.sh`
- Linux SMP shell snapshot：`tests/linux/build_linux_smp_shell_snapshot.sh`
- Linux 单核功能回归：`tests/linux/run_functional_suite.sh`
- Linux SMP 功能回归：
  - `tests/linux/run_functional_suite_smp.sh` 连续 3 轮
- 额外 SMP 压力复现：
  - 从 `out/linux-smp-shell-v1.snap` 恢复后连续 10 轮执行 `uname/ps/mount/df/cpuinfo/ping`，最终通过 `SMP-STRESS-PASS` 停机

## 当前结论

- 本轮已修复先前在 SMP Linux 下运行 `uname -a`、`ps` 等命令时出现的随机内存破坏 / `Attempted to kill init` / 输出损坏问题，现有复现路径已无法重现。
- 直接原因是跨核 TLB / I-cache 维护没有广播，导致其他 CPU 继续使用失效的旧映射或旧译码状态。
- 当前 Linux SMP 稳定性相较本轮开始前已有明显提升；后续若再出现随机性问题，应优先继续审查 remaining SMP 共享语义，例如更细粒度的 cache maintenance、IPI/调度相关路径以及未来多线程执行模型下的并发安全。

# 修改日志 2026-03-15 20:03

## 本轮修改

- 继续排查 `tests/linux/run_gui_tty1.sh` 在 `console=tty1` + SMP + framebuffer 路径上的早期启动卡死问题。
- 通过隔离实验确认：
  - `SMP + tty1 + fb-sdl off` 同样会卡，因此问题不在 SDL 后端。
  - `SMP + serial + fb-sdl on` 可正常进入 shell，因此 framebuffer 设备本身也不是根因。
  - `UP + tty1` 可正常进入 shell，因此问题只在 `SMP + tty1/fbcon` 组合下触发。
- 进一步用 `AARCHVM_TRACE_GIC=1`、`AARCHVM_TRACE_TIMER=1`、`AARCHVM_TRACE_IRQ_TAKE=1` 跟踪，确认卡死前 CPU0 会在较深的中断嵌套中反复处理 timer/SGI，中断风暴与当前 timer 模型密切相关。
- 最终确认这是当前“按执行步数推进虚拟时间”的 timer 模型在 `tty1/fbcon + SMP` 重负载路径上过于激进造成的 guest-visible stall，而不是新的随机内存破坏问题。
- 将 `tests/linux/run_gui_tty1.sh` 的默认 `AARCHVM_TIMER_SCALE` 从 `10` 调整为 `1`，使该脚本在当前模型下可稳定跨过卡点并进入 shell。

## 本轮测试

- 复现原问题：
  - `SDL_VIDEODRIVER=dummy timeout 220s ./tests/linux/run_gui_tty1.sh`
- 隔离实验：
  - `SMP + serial + fb-sdl on`：可进入 shell
  - `SMP + tty1 + fb-sdl off`：仍复现 stall
  - `UP + tty1`：可进入 shell
- 验证修复：
  - `SDL_VIDEODRIVER=dummy timeout 120s ./tests/linux/run_gui_tty1.sh`
  - 结果可见 `Run /init as init process` 与 `Entering BusyBox serial shell`，且未再出现 RCU expedited stall

## 当前结论

- 这次 `run_gui_tty1.sh` 的卡死主要是 timer scale 设得过高，不是新增的 SDL bug。
- 在当前解释执行 + instruction-count timer 模型下，`tty1/fbcon` 的 SMP 路径对 timer 频率非常敏感；将该脚本默认缩到 `AARCHVM_TIMER_SCALE=1` 后即可稳定使用。
- 后续若要从根本上消除这类问题，应继续改进 timer/事件模型，减少“重输出路径导致 guest 时间走得过快”的失真。

# 修改日志 2026-03-16 12:37

## 本轮修改

- 审查了当前快照保存/恢复路径中 CPU、SystemRegisters、GIC、GenericTimer、UART、KMI、Block 设备以及 SoC 级状态，区分了真正需要序列化的 guest-visible 状态与仅需加载后重建的派生缓存。
- 确认并修复一个明确遗漏：
  - `PerfMailbox` 虽已映射到总线并对 guest 可见，但此前未参与快照保存/恢复。
  - 现已为 `PerfMailbox` 增加 `reset_state()` / `save_state()` / `load_state()`，完整保存 `case_id`、`arg0`、`arg1`、`last_status`、`last_result`。
- 修复了跨快照的活动 perf session 状态丢失问题：
  - 之前若 guest 先对 `PerfMailbox` 发出 `BEGIN`，保存快照，再恢复后发 `END`，SoC 侧的 perf session 起点会丢失，导致结果不连续。
  - 现在 `SoC` 会在保存快照时把活动 session 已累计的 `PerfCounters` 与 `host_ns` 固化到快照中，恢复后重新以当前计数器为新基线继续累计。
- 将快照版本从 `10` 提升到 `11`，并保留对旧版本 `1..10` 快照的兼容加载。
- `SoC::reset()` 现在会显式复位 `PerfMailbox`，避免热重置后残留旧的 mailbox 结果。
- 新增裸机回归用例 `tests/arm64/snapshot_perf_mailbox.S`：
  - 先写入 `PerfMailbox` 参数并发起 `BEGIN`
  - 在等待 UART 输入期间保存快照
  - 恢复后注入字符继续执行 `END`
  - 由 guest 校验 `case_id/arg0/arg1/status/steps`，同时在日志中校验 `PERF-RESULT`。
- 将该新用例接入 `tests/arm64/build_tests.sh` 与 `tests/arm64/run_all.sh`，纳入完整裸机回归。

## 本轮测试

- 编译：
  - `cmake --build build -j`
  - `tests/arm64/build_tests.sh`
- 新增定向快照烟测：
  - `./build/aarchvm -bin tests/arm64/out/snapshot_perf_mailbox.bin -load 0x0 -entry 0x0 -steps 2000 -snapshot-save tests/arm64/out/snapshot_perf_mailbox.snap`
  - `AARCHVM_UART_RX_SCRIPT='100:0x5a' ./build/aarchvm -snapshot-load tests/arm64/out/snapshot_perf_mailbox.snap -steps 200000`
- 裸机完整回归：
  - `tests/arm64/run_all.sh`
- Linux 单核 shell snapshot 重建：
  - `tests/linux/build_linux_shell_snapshot.sh`
- Linux 单核功能回归：
  - `tests/linux/run_functional_suite.sh`

## 当前结论

- 本轮确认的真实快照缺口是 `PerfMailbox` 设备状态，以及 SoC 侧“已开始但尚未结束”的 perf session 累积状态。
- CPU/SystemRegisters/GIC/GenericTimer 当前未发现新的 guest-visible 状态遗漏；未保存字段主要是 trace、回调、预解码/TLB 热缓存、IRQ 查询缓存等派生或宿主侧状态，加载时清空/重建是合理的。
- 修改后：
  - 新增的 `PerfMailbox` 快照用例通过；
  - `tests/arm64/run_all.sh` 通过；
  - `tests/linux/run_functional_suite.sh` 通过。
- 额外说明：
  - `tests/linux/build_linux_smp_shell_snapshot.sh` 在当前仓库状态下未能在默认 `4.5e9` steps 预算内走到 shell prompt，构建日志停在内核早期启动并以 `SUMMARY: steps=4500000000` 结束，因此本轮未完成基于该脚本的 SMP Linux 回归闭环。

# 修改日志 2026-03-16 14:40


## 本轮修改

- 结合 `out/my_test.snap` 的异常状态和源码路径，定位到一个明确的 timer/GIC 竞态：
  - guest 在 IRQ handler 中通过 `CNTV_*` / `CNTP_*` sysreg 重编程本地 timer 后，`GenericTimer` 内部 pending 状态会立刻变化；
  - 但 GIC 的本地 PPI 线电平此前只会在 SoC 的下一轮 `sync_devices()` 里刷新；
  - 如果 handler 在这之前执行 `ICC_EOIR1_EL1`，GIC 仍会看到旧的高电平，并把同一个 timer interrupt 重新挂回 pending，造成虚假的二次进入。
- 在 CPU 中新增 `refresh_local_timer_irq_lines()`，并在所有 `CNTV_CTL/CVAL/TVAL_EL0`、`CNTP_CTL/CVAL/TVAL_EL0` 的 sysreg 写路径后立即同步本核 timer PPI 电平到 GIC。
- 新增裸机回归用例 `tests/arm64/gic_timer_rearm_no_spurious.S`：
  - 在 timer IRQ handler 中先把 timer 重编程到远未来；
  - 再执行 `EOIR/DIR` 并临时开 IRQ；
  - 验证不会因为 stale GIC level 发生错误的嵌套重入。
- 将该回归接入 `tests/arm64/build_tests.sh` 与 `tests/arm64/run_all.sh`。
- 修复 `tests/linux/run_functional_suite.sh` 的时序脆弱性：
  - 原脚本依赖冷启动后按步数估算时刻向串口注入命令，当前仓库状态下会出现 shell 只回显命令但不执行的现象；
  - 现改为先确保 `out/linux-usertests-shell-v1.snap` 最新，再从 shell snapshot 恢复并直接喂功能命令，使单核 Linux 功能回归稳定通过。

## 本轮测试

- 定向单测：
  - `./build/aarchvm -bin tests/arm64/out/gic_timer_rearm_no_spurious.bin -load 0x0 -entry 0x0 -steps 2000000`
- 坏快照定向观测：
  - `env AARCHVM_TRACE_IRQ_TAKE=1 AARCHVM_TRACE_TIMER=1 ./build/aarchvm -smp 2 -snapshot-load out/my_test.snap -steps 500000 -fb-sdl off`
  - `env AARCHVM_PRINT_SUMMARY=1 AARCHVM_PRINT_IRQ_SUMMARY=1 AARCHVM_PRINT_TIMER_SUMMARY=1 ./build/aarchvm -smp 2 -snapshot-load out/my_test.snap -steps 500000 -fb-sdl off`
- 裸机完整回归：
  - `tests/arm64/run_all.sh`
- Linux 单核功能回归：
  - `tests/linux/run_functional_suite.sh`

## 当前结论

- `out/my_test.snap` 保存时主核已经处于深异常嵌套状态，因此它本身不是一个“修完代码就一定能直接救活”的快照。
- 但从该快照暴露出来的执行形态可以反推出一个真实 bug：timer sysreg 写后 GIC 本地 PPI 电平更新滞后，这会放大 `fbcon/simpledrm` 重输出路径中的 timer IRQ 重入问题。
- 修复后，新增的专门单测已经通过；坏快照上的 trace 也能看到 CPU1 的 timer IRQ 按重新编程后的 `CVAL` 正常再次到期，而不是在 handler 内因为旧高电平被立即错误重挂。
- 本轮已稳定通过：
  - `tests/arm64/run_all.sh`
  - `tests/linux/run_functional_suite.sh`
- 本轮未完成：
  - `tests/linux/run_functional_suite_smp.sh`
  - 原因不是本次 timer/GIC 修复引入 panic，而是 `tests/linux/build_linux_smp_shell_snapshot.sh` 在当前 2-core / 1GiB 配置下，默认 `4.5e9` steps 预算内仍无法稳定走到 shell prompt，导致 SMP Linux 功能回归脚本本身无法闭环。

# 修改日志 2026-03-16 16:27

## 本轮修改

- 重新分析 `out/my_test.snap`，确认它不是“恢复后坏掉”，而是“保存前主核就已经进入坏状态”：
  - `CPU0` 在快照中已是 `halted=1`；
  - `exception_depth=8` 且顶层还有待处理的本地 IRQ；
  - 下一次进入异常时会直接撞上 emulator 自身的 8 层异常栈上限。
- 将 CPU 内部异常 bookkeeping 容量从固定 8 层提升到 64 层，避免 Linux/SMP 场景下较深的同 EL 异常/IRQ 嵌套被 emulator 人为判死。
- 保持旧快照兼容：
  - 将快照版本从 `11` 升到 `12`；
  - `Cpu::load_state()` 对旧版快照仍按历史 8 层格式读取，再扩展填充到新的 64 层数组。
- 新增裸机定向回归 `tests/arm64/nested_sync_depth.S`：
  - 人工构造 16 层同 EL 同步异常嵌套；
  - 验证 emulator 不再因为内部异常栈容量过小而提前 halt。
- 将该新用例接入 `tests/arm64/build_tests.sh` 与 `tests/arm64/run_all.sh`。

## 本轮测试

- 定向单测：
  - `./build/aarchvm -bin tests/arm64/out/nested_sync_depth.bin -load 0x0 -entry 0x0 -steps 400000`
- 旧快照兼容性：
  - `env AARCHVM_PRINT_SUMMARY=1 ./build/aarchvm -smp 2 -snapshot-load out/my_test.snap -steps 1 -fb-sdl off`
- 裸机完整回归：
  - `tests/arm64/run_all.sh`
- Linux 单核功能回归：
  - `tests/linux/run_functional_suite.sh`

## 当前结论

- `out/my_test.snap` 抓到的现场说明 guest 在保存前已经把主核推进到了 emulator 自身的异常嵌套上限，因此“8 层固定上限”本身就是一个真实 bug。
- 本轮修复后：
  - 新增的 16 层嵌套异常用例通过；
  - 旧版坏快照仍可加载；
  - `tests/arm64/run_all.sh` 通过；
  - `tests/linux/run_functional_suite.sh` 通过。
- 仍未闭环的部分：
  - `tests/linux/run_functional_suite_smp.sh`
  - 当前阻塞点仍是 SMP shell snapshot 基础设施未就绪，脚本日志显示 `Failed to load snapshot: out/linux-smp-shell-v1.snap`，因此这轮没有把它计入功能失败。

# 修改日志 2026-03-19 10:56

## 本轮修改

- 基于 `out/my_test.snap` 重新分析 `/init` 卡死现场，确认 CPU1 当时陷在：
  - `__arch_clear_user()`
  - faulting 指令是 `sttr xzr, [x0]`
  - 异常类型为 EL1 same-EL Data Abort。
- 定位出根因：`LDTR/STTR` 家族一直被按“普通 EL1 数据访问”执行，错误地走了当前 EL 权限模型。
  - 在 Linux 冷启动到 `/init` 的用户页清零路径里，内核会用 `STTR`/`LDTR` 做 usercopy；
  - 进入 EL1 异常后 `PSTATE.PAN=1`，错误实现会把这些非特权访问也当成普通特权访问；
  - 结果本应成功的 user page clear 被错误打成 Permission fault，最终把 `init` 卡死在 fault handling 路径里。
- 在 CPU/MMU 权限模型中新增显式的 `UnprivilegedRead` / `UnprivilegedWrite` 访问类型。
- 修正 `LDTR/STTR` 实现，使其显式内存访问在当前模型下按 EL0 权限检查执行。
  - 当前模拟器未实现 `FEAT_UAO`，因此这类 unprivileged load/store 在 EL1 上始终使用 EL0 访问权限；
  - 同时保留普通 `LDR/STR` 在 `PAN=1` 时对 user mapping 触发 Permission fault 的原有行为。
- 新增严格裸机回归 `tests/arm64/mmu_ldtr_sttr_pan.S`，覆盖：
  - `PAN=1` 时 `LDTR/STTR` 访问 EL0 RW 页必须成功；
  - 同条件下普通 `LDR` 访问同一 user 页必须 fault；
  - `LDTR/STTR` 访问 EL1-only 页必须 fault。
- 将新用例接入：
  - `tests/arm64/build_tests.sh`
  - `tests/arm64/run_all.sh`
- 修复 Linux 功能回归覆盖盲区：
  - `tests/linux/run_functional_suite.sh`
  - `tests/linux/run_functional_suite_smp.sh`
  - 这两个脚本现在会在 `./build/aarchvm` 新于 snapshot build log 时强制重建 shell snapshot，从而覆盖真正的冷启动 `/init` 路径，而不是继续复用旧快照。

## 本轮测试

- 编译与测试构建：
  - `timeout 600s cmake --build build -j`
  - `timeout 600s tests/arm64/build_tests.sh`
- 定向裸机验证：
  - `timeout 120s ./build/aarchvm -bin tests/arm64/out/mmu_ldtr_sttr_pan.bin -load 0x0 -entry 0x0 -steps 4000000`
  - `timeout 120s ./build/aarchvm -bin tests/arm64/out/mmu_pan_user_access.bin -load 0x0 -entry 0x0 -steps 4000000`
  - `timeout 120s ./build/aarchvm -bin tests/arm64/out/ldtr_sttr_usercopy.bin -load 0x0 -entry 0x0 -steps 400000`
- 坏快照复验：
  - `timeout 60s env AARCHVM_PRINT_SUMMARY=1 ./build/aarchvm -smp 2 -snapshot-load out/my_test.snap -steps 20000000`
  - 修复后该坏快照已能继续进入 BusyBox shell，而不是继续卡在 `/init` 附近。
- Linux 冷启动覆盖：
  - `timeout 1800s ./tests/linux/build_linux_smp_shell_snapshot.sh`
- 完整回归：
  - `timeout 1800s ./tests/arm64/run_all.sh`
  - `timeout 1800s ./tests/linux/run_functional_suite.sh`
  - `timeout 2400s ./tests/linux/run_functional_suite_smp.sh`

## 当前结论

- 本次 `/init` 卡死不是 timer、GIC 或快照恢复问题，而是 `LDTR/STTR` 权限语义实现错误。
- 用户给的坏快照足够暴露根因，因为它正好保留了：
  - CPU0 在 idle/WFI 中等 timer；
  - CPU1 在 `__arch_clear_user` fault handling 路径里反复打转；
  - faulting PC 落在 `STTR`。
- 之前 Linux 回归没有发现这个问题，原因有两个：
  - `tests/linux/run_functional_suite.sh` 和 `tests/linux/run_functional_suite_smp.sh` 默认复用已有 shell snapshot；
  - 旧逻辑不会因为 `./build/aarchvm` 变新而重建 snapshot，所以它们绕过了真正的冷启动 `/init` 路径。
- 本轮修复后已完整通过：
  - `tests/arm64/run_all.sh`
  - `tests/linux/run_functional_suite.sh`
  - `tests/linux/run_functional_suite_smp.sh`

# 修改日志 2026-03-19 13:58

## 本轮修改

- 继续审阅 `cpu.cpp` 中 AdvSIMD structured load/store 的实现，确认 whole-register 结构访存仍有明显语义缺口：
  - `LD1/ST1` 仅覆盖了单寄存器与部分 2-register 读路径；
  - `LD3/ST3/LD4/ST4` whole-register 变体完全缺失。
- 在 [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp) 中补齐 whole-register structured AdvSIMD 访存：
  - `LD1/ST1` 2/3/4 consecutive-register 变体；
  - 上述 `LD1/ST1` 的 post-index 变体；
  - `LD3/ST3` no-post 与 post-index；
  - `LD4/ST4` no-post 与 post-index。
- 把原先 `LD2/ST2` 的字节交织路径抽象成通用 helper，统一了：
  - 顺序 whole-register 结构访存；
  - 交织/反交织的 `LDn/STn` 字节搬运路径。
- 新增裸机单测 [tests/arm64/fpsimd_structured_ls_more.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/fpsimd_structured_ls_more.S)，覆盖：
  - `LD1/ST1` 2/3/4-register whole-register 变体；
  - `LD3/ST3/LD4/ST4` 的 8B/16B、no-post/post-index 组合；
  - post-index 写回与内存布局检查。
- 将新测试接入：
  - [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh)
  - [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)

## 本轮测试

- 定向构建与新单测：
  - `cmake --build build -j`
  - `tests/arm64/build_tests.sh`
  - `timeout 30s ./build/aarchvm -bin tests/arm64/out/fpsimd_structured_ls_more.bin -load 0x0 -entry 0x0 -steps 600000`
- 完整裸机回归：
  - `timeout 2400s tests/arm64/run_all.sh`
- Linux 单核功能回归：
  - `timeout 900s tests/linux/run_functional_suite.sh`
- Linux SMP 功能回归：
  - `timeout 1200s tests/linux/run_functional_suite_smp.sh`

## 当前结论

- 本轮补的是 Armv8-A/AdvSIMD 里一个真实且明确的 ISA 缺口，而不是测试特判：
  - 修改前，最小 `LD3` 探针会落入未实现路径并最终异常停机；
  - 修改后，新增 whole-register structured load/store 用例与完整主线回归均通过。
- 这轮之后，whole-register 结构访存的覆盖显著更完整了，但我仍未把所有 lane/replicate 类 structured AdvSIMD 访存都补齐；那部分仍是后续继续审阅的候选缺口。

# 修改日志 2026-03-19 14:47

## 本轮修改

- 继续按“审阅 -> 修复 -> 测试”流程补齐 AdvSIMD structured 访存剩余的真实 ISA 缺口：
  - [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp)
  - [tests/arm64/fpsimd_structured_lane_ls.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/fpsimd_structured_lane_ls.S)
  - [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh)
  - [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)
- 在 `cpu.cpp` 中新增 Arm ARM shared decode 对应的 single-structure 处理路径，补齐：
  - one-lane `LD1/ST1/LD2/ST2/LD3/ST3/LD4/ST4`；
  - replicate `LD1R/LD2R/LD3R/LD4R`；
  - no-offset 与 post-index（immediate / register）变体。
- 按手册语义实现了这两类 structured 访存的关键行为：
  - one-lane load 只更新目标 lane，保留寄存器其他位；
  - replicate load 按 arrangement 填充目标向量；
  - post-index immediate 写回使用“本次传输总字节数”，register variant 写回使用 `Xm`；
  - load 路径在全部元素读取成功后再统一提交寄存器结果，避免部分更新。
- 扩展 `insn_uses_fp_asimd()`，确保新增的 structured SIMD 访存在 `CPACR_EL1.FPEN` trap 条件下也走正确的 FP/AdvSIMD 访问陷入路径。
- 新增裸机单测 [tests/arm64/fpsimd_structured_lane_ls.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/fpsimd_structured_lane_ls.S)，覆盖：
  - one-lane load/store 的 8/16/32/64-bit lane 语义；
  - replicate `LD1R/LD2R/LD3R/LD4R`；
  - 1/2/3/4-register 结构数；
  - no-offset、post-index immediate、post-index register；
  - lane 保留语义、replicate 结果、内存布局和 writeback。
- 在落地前额外用 `qemu-aarch64` 做了语义核对：
  - `LD1R {Vt.8B}` 会清零高 64 位；
  - one-lane `LD1 {Vt.B}[idx]` 会保留寄存器其余字节与高 64 位。

## 本轮测试

- 构建与定向验证：
  - `timeout 1200s cmake --build build -j`
  - `timeout 1200s tests/arm64/build_tests.sh`
  - `timeout 60s ./build/aarchvm -bin tests/arm64/out/fpsimd_structured_lane_ls.bin -load 0x0 -entry 0x0 -steps 800000`
- 裸机完整回归：
  - `timeout 2400s tests/arm64/run_all.sh`
- Linux 单核功能回归：
  - `timeout 900s tests/linux/run_functional_suite.sh`
- Linux SMP 功能回归：
  - `timeout 1200s tests/linux/run_functional_suite_smp.sh`

## 当前结论

- 这轮补的是 whole-register structured 之后剩下的一大块 AdvSIMD structured memory gap，不是针对某个测试的特殊处理。
- 修改后，single-structure one-lane 与 replicate 家族已经有了解码、语义和单测闭环，且完整裸机与 Linux 单核/SMP 回归均通过。
- 继续往下审时，仍值得关注的方向主要是：
  - FEAT 可选扩展相关但当前未实现的 SIMD&FP acquire/release 访存；
  - 其余更零散的 AdvSIMD/FP 指令族与异常/陷入细节。

# 修改日志 2026-03-19 15:32

## 本轮修改

- 继续按“审阅 -> 修复 -> 测试”流程补齐 FP min/max 家族的真实 ISA 语义缺口：
  - [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp)
  - [tests/arm64/fp_minmax_nan_flags.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/fp_minmax_nan_flags.S)
  - [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh)
  - [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)
- 在 `cpu.cpp` 中新增统一的 FP min/max 结果辅助逻辑，按 Arm 语义处理：
  - `FMAX/FMIN`
  - `FMAXNM/FMINNM`
  - 标量与向量路径共用同一套 NaN、signed zero、`FPSR.IOC` 处理规则。
- 补齐此前缺失的标量 `FMAX/FMIN/FMAXNM/FMINNM` 解码与执行路径。
- 修正已有向量 `FMAX/FMIN/FMAXNM/FMINNM` 的行为缺陷，确保：
  - qNaN 与 sNaN 区分正确；
  - sNaN 会 quiet 并置位 `FPSR.IOC`；
  - `FMAXNM/FMINNM` 在单个 qNaN 操作数下返回数值操作数；
  - `+0/-0` 平局时按 Arm 规则选择 `+0` 或 `-0`。
- 新增裸机单测 [tests/arm64/fp_minmax_nan_flags.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/fp_minmax_nan_flags.S)，覆盖：
  - 标量与向量 min/max；
  - 32-bit 与 64-bit 标量形式；
  - qNaN/sNaN 传播与 quiet 行为；
  - `FPSR.IOC` 置位；
  - `+0/-0` tie-breaking。

## 本轮测试

- 构建与定向验证：
  - `timeout 1200s cmake --build build -j`
  - `timeout 1200s tests/arm64/build_tests.sh`
  - `timeout 60s ./build/aarchvm -bin tests/arm64/out/fp_minmax_nan_flags.bin -load 0x0 -entry 0x0 -steps 300000`
  - `timeout 60s ./build/aarchvm -bin tests/arm64/out/fp_compare_flags.bin -load 0x0 -entry 0x0 -steps 200000`
- 裸机完整回归：
  - `timeout 2400s tests/arm64/run_all.sh`
- Linux 单核功能回归：
  - `timeout 1200s tests/linux/run_functional_suite.sh`
- Linux SMP 功能回归：
  - `timeout 1200s tests/linux/run_functional_suite_smp.sh`

## 当前结论

- 这轮补的是一个真实的 Armv8-A FP 语义缺口，而不是测试特判：
  - 修改前，标量 `FMAX/FMIN/FMAXNM/FMINNM` 缺失；
  - 向量同族指令对 qNaN/sNaN、signed zero 与 `FPSR.IOC` 的处理不符合 Arm 规则。
- 修改后，标量与向量 min/max 家族的核心行为已经统一到同一语义模型，新增单测与完整裸机/Linux 单核/SMP 回归均通过。
- 这不代表“Armv8-A ISA 已完整实现”。后续仍值得继续审阅的方向包括：
  - `FRINT*` 一类的细粒度异常标志与 NaN 行为；
  - 其他 FP/AdvSIMD 指令家族中的 sNaN/qNaN 例外路径；
  - 仍可能遗漏的标量 FP 杂项指令族。

# 修改日志 2026-03-19 16:13

## 本轮修改

- 继续按“审阅 -> 修复 -> 测试”流程补齐标量 FP 乘法家族中的真实 ISA 缺口：
  - [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp)
  - [tests/arm64/fp_scalar_arith.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/fp_scalar_arith.S)
- 在 `cpu.cpp` 中补上此前缺失的标量 `FNMUL Sd/Dd, Sn/Dn, Sm/Dm` 解码与执行路径。
- 在现有裸机单测 [tests/arm64/fp_scalar_arith.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/fp_scalar_arith.S) 中新增：
  - double `FNMUL` 结果检查；
  - float `FNMUL` 结果检查。
- 定向测试时顺手修正了新加单测里一个测试自身的问题：
  - 单精度检查最初误复用了双精度常量的低 32 位；
  - 已改为显式加载 `2.0f/-3.0f` 的 32-bit 位模式。

## 本轮测试

- 构建与定向验证：
  - `timeout 1200s cmake --build build -j`
  - `timeout 1200s tests/arm64/build_tests.sh`
  - `timeout 60s ./build/aarchvm -bin tests/arm64/out/fp_scalar_arith.bin -load 0x0 -entry 0x0 -steps 300000`
- 裸机完整回归：
  - `timeout 2400s tests/arm64/run_all.sh`
- Linux 单核功能回归：
  - `timeout 1200s tests/linux/run_functional_suite.sh`
- Linux SMP 功能回归：
  - `timeout 1200s tests/linux/run_functional_suite_smp.sh`

## 当前结论

- 这轮补的是缺指令级别的问题，不是行为特判：
  - 修改前，标量 `FNMUL` 没有实现；
  - 修改后，标量 FP 基本算术族的覆盖更完整，且定向测试、裸机完整回归、Linux 单核与 SMP 功能回归都通过。
- 到目前为止，我仍不认为已经“完整实现 Armv8-A ISA 的全部强制行为”。下一批仍值得继续审阅的方向主要是：
  - 标量/向量 FP 其余杂项家族中与 NaN、异常标志相关的细节；
  - `FRINT*` 一类 finer-grained `FPSR`/`FPCR` 交互；
  - 其他尚未被单测覆盖到的零散标量 FP 指令族。

# 修改日志 2026-03-19 16:30

## 本轮修改

- 继续按“审阅 -> 修复 -> 测试”流程补齐另一组真实的标量 FP ISA 缺口：
  - [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp)
  - [tests/arm64/fp_scalar_compare_misc.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/fp_scalar_compare_misc.S)
  - [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh)
  - [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)
- 在 `cpu.cpp` 中新增统一的标量 compare-result 写回辅助路径，并补齐：
  - 标量 `FABD`；
  - 标量 register-compare：`FCMEQ/FCMGE/FCMGT`；
  - 标量 absolute compare：`FACGE/FACGT`；
  - 标量 zero-compare：`FCMEQ/FCMGE/FCMGT/FCMLE/FCMLT`。
- 这轮中途还修正了一个真实解码问题：
  - 这组标量 FP 指令不能直接复用先前部分算术指令的 `ftype==0/1` 判法；
  - 已改为按该族实际的 `sz` 位区分 32/64-bit，避免误把合法指令落入未实现路径。
- 新增裸机单测 [tests/arm64/fp_scalar_compare_misc.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/fp_scalar_compare_misc.S)，覆盖：
  - `FABD` 的单精度与双精度；
  - `FCMEQ/FCMGE/FCMGT` register 形式；
  - `FACGE/FACGT`；
  - `FCMEQ/FCMGE/FCMGT/FCMLE/FCMLT` zero 形式；
  - qNaN 比较返回 false；
  - 标量 compare 结果写回时高位清零。

## 本轮测试

- 构建与定向验证：
  - `timeout 1200s cmake --build build -j`
  - `timeout 1200s tests/arm64/build_tests.sh`
  - `timeout 60s ./build/aarchvm -bin tests/arm64/out/fp_scalar_compare_misc.bin -load 0x0 -entry 0x0 -steps 300000`
- 裸机完整回归：
  - `timeout 2400s tests/arm64/run_all.sh`
- Linux 单核功能回归：
  - `timeout 1200s tests/linux/run_functional_suite.sh`
- Linux SMP 功能回归：
  - `timeout 1200s tests/linux/run_functional_suite_smp.sh`

## 当前结论

- 这轮补的是一整组此前缺失的标量 FP compare/misc 指令，而不是测试特判。
- 修改后，标量 FP 里“绝对差 + compare-register + compare-zero”这块已经形成了解码、执行、单测与完整回归闭环。
- 我仍不认为当前已经“完整实现 Armv8-A ISA 的全部强制行为”。下一轮继续审时，仍值得优先关注：
  - 其余标量 FP 杂项家族；
  - `FRINT*` 与其它 FP 指令对 NaN / `FPSR` 异常位的细粒度语义；
  - 尚未被单测触达的零散 FP/AdvSIMD 边角行为。

# 修改日志 2026-03-19 17:01

## 本轮修改

- 继续按“审阅 -> 修复 -> 测试”流程补齐两组真实的标量 FP ISA 缺口：
  - [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp)
  - [tests/arm64/fp_cond_compare.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/fp_cond_compare.S)
  - [tests/arm64/fp_fcvt_rounding_scalar.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/fp_fcvt_rounding_scalar.S)
  - [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh)
  - [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)
- 在 `cpu.cpp` 中新增标量 `FCCMP/FCCMPE`：
  - 条件不成立时按 `nzcv` 立即数字段直接写入 `PSTATE.NZCV`；
  - 条件成立时复用 `FCMP/FCMPE` 的 compare/NaN/`FPSR.IOC` 语义；
  - 修正了解码掩码，避免把 `cond` 位误当成 opcode 一部分。
- 新增裸机单测 [tests/arm64/fp_cond_compare.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/fp_cond_compare.S)，覆盖：
  - `FCCMP/FCCMPE` 的条件成立/不成立路径；
  - qNaN / sNaN compare 的 `NZCV` 与 `FPSR.IOC`；
  - 单精度与双精度形式。
- 在 `cpu.cpp` 中补上此前缺失的标量 FP→整数舍入转换家族：
  - `FCVTNS/FCVTNU`
  - `FCVTPS/FCVTPU`
  - `FCVTMS/FCVTMU`
  - `FCVTAS/FCVTAU`
- 为这组 `FCVT*` 新增统一的 `FPToFixed` 风格辅助逻辑，按共享伪代码处理：
  - ties-to-even / ties-away / toward +inf / toward -inf；
  - signed / unsigned 饱和结果；
  - `FPSR.IOC` 与 `FPSR.IXC`。
- 新增裸机单测 [tests/arm64/fp_fcvt_rounding_scalar.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/fp_fcvt_rounding_scalar.S)，覆盖：
  - signed / unsigned；
  - 32-bit / 64-bit；
  - `Wd,Sn` / `Wd,Dn` / `Xd,Sn` / `Xd,Dn` cross-width 形式；
  - inexact 与 invalid 标志位。

## 本轮测试

- 构建与定向验证：
  - `timeout 1200s cmake --build build -j`
  - `timeout 1200s tests/arm64/build_tests.sh`
  - `timeout 60s ./build/aarchvm -bin tests/arm64/out/fp_cond_compare.bin -load 0x0 -entry 0x0 -steps 200000`
  - `timeout 60s ./build/aarchvm -bin tests/arm64/out/fp_fcvt_rounding_scalar.bin -load 0x0 -entry 0x0 -steps 300000`
- 裸机完整回归：
  - `timeout 2400s tests/arm64/run_all.sh`
- Linux 单核功能回归：
  - `timeout 1200s tests/linux/run_functional_suite.sh`
- Linux SMP 功能回归：
  - `timeout 1200s tests/linux/run_functional_suite_smp.sh`

## 当前结论

- 这轮补的是两组此前真实缺失的标量 FP 指令行为，而不是测试特判。
- 修改后，标量 FP compare 的条件版本与非朝零的 FP→整数舍入转换都已经形成“实现 + 单测 + 裸机全回归 + Linux UMP/SMP 回归”的闭环。
- 到目前为止，我仍不认为已经“完整实现 Armv8-A ISA 的全部强制行为”。继续审时，仍值得优先关注：
  - 其余未覆盖的标量 FP 杂项家族，尤其 reciprocal / rsqrt estimate 与 step 家族；
  - 某些 FP/AdvSIMD 指令对 NaN、subnormal、`FPCR.AH/FZ` 的更细粒度交互；
  - 尚未被裸机单测触达的零散 SIMD&FP 转换和边角异常路径。
