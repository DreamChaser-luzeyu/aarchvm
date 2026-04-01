# 修改日志 2026-04-01 20:43

## 本轮修改

- 继续沿着“异常 / 系统寄存器 / trap 语义收尾”审 `MRS/MSR` 的 corner case；这轮没有改模拟器执行逻辑，而是把一个此前靠代码阅读确认、但还没被正式回归锁住的 system-register 细节补成了裸机回归：
  - [tests/arm64/sysreg_xzr_semantics.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/sysreg_xzr_semantics.S)
  - [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh)
  - [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)
  - [TODO.md](/media/luzeyu/Storage2/FOSS_src/aarchvm/TODO.md)
- 新增正式裸机回归 `sysreg_xzr_semantics`，显式锁定 system register 指令里 `Rt==31 -> XZR/WZR` 的程序可见语义：
  - `MRS XZR, <sysreg>` 必须丢弃结果，不能把结果误写到当前 `SP`；
  - `MSR <sysreg>, XZR` 必须写入 0，不能把当前 `SP` 当成源寄存器值；
  - 同时覆盖 generic sysreg 路径（`TPIDR_EL0`）和 special sysreg 路径（`SP_EL0`）。

## 本轮测试

- `timeout 1800s ./tests/arm64/build_tests.sh`
- `timeout 120s ./build/aarchvm -bin tests/arm64/out/sysreg_xzr_semantics.bin -load 0x0 -entry 0x0 -steps 400000`
- `timeout 5400s ./tests/arm64/run_all.sh`
- `timeout 5400s ./tests/linux/run_functional_suite.sh`
- `timeout 5400s ./tests/linux/run_functional_suite_smp.sh`

## 当前结论

- 这轮先把一个真实且容易回归坏掉的 system-register 角落正式钉进了回归；代码阅读确认当前实现是正确的。
- 但我仍不能自信宣称“Armv8-A 程序可见最小集已完整收口”，接下来仍要继续审 `ESR/FAR/PAR/ISS`、剩余 trap/undef/no-op 边界，以及 MMU/SMP 里尚未彻底钉死的程序可见角落。

# 修改日志 2026-04-01 20:31

## 本轮修改

- 继续沿着“浮点 / AdvSIMD 程序可见语义收尾”审 `FCVT*` 的特殊值与 `FZ` 边界；这轮没有改模拟器执行逻辑，而是把一组此前只在临时探针里核过、但还没被正式回归锁死的 scalar `FP -> int` 语义补成了裸机回归：
  - [tests/arm64/fp_fcvt_special_scalar.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/fp_fcvt_special_scalar.S)
  - [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh)
  - [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)
  - [TODO.md](/media/luzeyu/Storage2/FOSS_src/aarchvm/TODO.md)
- 新增正式裸机回归 `fp_fcvt_special_scalar`，显式锁定以下 guest 可见边界：
  - `FCVT*` 对 `qNaN/sNaN` 的 invalid 结果与 `FPSR.IOC`；
  - `FCVT*` 对 `+Inf/-Inf` 的 signed/unsigned 饱和值与 `FPSR.IOC`；
  - `-0.6/-0.5` 这类最容易把“inexact”与“invalid”混淆的近零负数边界；
  - `FPCR.FZ=1` 下 subnormal 输入做 `FCVTZS/FCVTNU` 时的 `FPSR.IDC` 行为；
  - `W` / `X` 目的寄存器的 scalar 路径。

## 本轮测试

- `timeout 1800s ./tests/arm64/build_tests.sh`
- `timeout 120s ./build/aarchvm -bin tests/arm64/out/fp_fcvt_special_scalar.bin -load 0x0 -entry 0x0 -steps 300000`
- `timeout 5400s ./tests/arm64/run_all.sh`
- `timeout 5400s ./tests/linux/run_functional_suite.sh`
- `timeout 5400s ./tests/linux/run_functional_suite_smp.sh`

## 当前结论

- 这轮没有坐实新的执行语义 bug，但把 `FCVT*` scalar 路径一组高风险特殊值边界正式锁进了回归。
- 结合这轮补强后，`FRINT* / FCVT* / FCVTN / FCVTXN / FRECPE` 这一批近期最可疑的 FP/AdvSIMD 边界已经有了更系统的正式覆盖。
- 但我仍不能自信宣称“Armv8-A 程序可见最小集已完整收口”；当前更值得继续审的仍是：
  - `FP/AdvSIMD` 里尚未完全穷尽的 `NaN/payload/default-NaN` 传播一致性；
  - `trap / undef / no-op` 判定边界里仍未系统收口的剩余 system/debug 指令；
  - `SMP` 下 barrier、exclusive/LSE、`TLBI/IC IVAU` 跨核传播与 Linux 压力路径。

# 修改日志 2026-04-01 20:02

## 本轮修改

- 继续沿着“浮点 / AdvSIMD 程序可见语义收尾”审 `estimate` 家族里的 `FPSR` 尾差；这轮包含一处真实的 guest 可见语义修复，并新增一条正式裸机回归：
  - [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp)
  - [tests/arm64/fp_frecpe_flags.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/fp_frecpe_flags.S)
  - [tests/arm64/fpsimd_fp_estimate.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/fpsimd_fp_estimate.S)
  - [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh)
  - [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)
  - [TODO.md](/media/luzeyu/Storage2/FOSS_src/aarchvm/TODO.md)
- 修正了 `FRECPE` 对“倒数估计会溢出到 `Inf` 的极小 subnormal”缺少 `FPSR.OFC` 的问题：
  - 旧实现里，`fp_recip_estimate_bits()` 在 `single`/`double` 的 overflow-subnormal fast path 只会置 `FPSR.IXC`；
  - 但 guest 可见语义应为 `FPSR.OFC|IXC`，因为这是一次真正的 overflow-to-infinity / overflow-to-max-finite 结果，而不只是普通 inexact；
  - 这会让数值自检、差分验证以及依赖 `FPSR` 的运行时看到错误 flags。
- 新增正式裸机回归 `fp_frecpe_flags`：
  - 覆盖 smallest positive `single` subnormal、smallest positive `double` subnormal，以及 smallest negative `single` subnormal；
  - 显式锁定结果位型和 `FPSR=0x14`；
  - 同时把既有 [fpsimd_fp_estimate.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/fpsimd_fp_estimate.S) 里 `src_frecpe_tiny=0x00010000` 的旧错误期望从 `IXC` 收正为 `OFC|IXC`。

## 本轮测试

- `timeout 120s ./build/aarchvm -bin tests/arm64/out/fp_frecpe_flags.bin -load 0x0 -entry 0x0 -steps 300000`
- `timeout 120s env AARCHVM_BRK_MODE=halt ./build/aarchvm -bin tests/arm64/out/fpsimd_fp_estimate.bin -load 0x0 -entry 0x0 -steps 400000`
- `timeout 5400s ./tests/arm64/run_all.sh`
- `timeout 5400s ./tests/linux/run_functional_suite.sh`
- `timeout 5400s ./tests/linux/run_functional_suite_smp.sh`

## 当前结论

- 这轮已经坐实并修正了 `FRECPE` estimate family 的一处真实 `FPSR` 语义缺口。
- 当前整套裸机回归、Linux UMP 回归、Linux SMP 回归都通过。
- 但我仍不能自信宣称“Armv8-A 程序可见最小集已完整收口”。当前剩余最值得继续审的仍然是：
  - `FP/AdvSIMD` 中尚未系统穷尽的 `NaN/subnormal/default-NaN/payload` 传播一致性；
  - `SMP` 下 exclusive/LSE、barrier、`TLBI/IC IVAU` 的跨核传播边界；
  - `MMU/fault` 与 Linux 压力路径的 system-level 差分验证。

# 修改日志 2026-04-01 22:31

## 本轮修改

- 继续沿着“浮点 / AdvSIMD 程序可见语义收尾”审 `double -> float` narrowing 的 `FPSR` 尾差；这轮包含一处真实的 guest 可见语义修复，并新增一条正式裸机回归：
  - [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp)
  - [tests/arm64/fp_fcvtn_flags.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/fp_fcvtn_flags.S)
  - [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh)
  - [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)
  - [TODO.md](/media/luzeyu/Storage2/FOSS_src/aarchvm/TODO.md)
- 修正了 `FCVTN/FCVTXN` 的 narrowing exception flags 缺口：
  - 旧实现里，`fp64_to_fp32_bits()` 与 `fp64_to_fp32_bits_round_to_odd()` 在 `double -> float` narrowing overflow 时只会置 `FPSR.IXC`，漏掉了 architecturally visible 的 `FPSR.OFC`；
  - 同时，在 tiny/inexact 的 underflow 路径上，旧实现也只会留下 `IXC`，漏掉 `FPSR.UFC`；
  - 这会让 guest 在 `FCVTN` / `FCVTXN` 上读到错误的 `FPSR`，尤其会影响数值自检、libm / 编译器生成代码和差分验证；
  - 现在 regular `FCVTN` 与 round-to-odd `FCVTXN` 都会在 overflow 时置 `OFC|IXC`，在 underflow/tiny inexact 时置 `UFC|IXC`。
- 新增正式裸机回归 `fp_fcvtn_flags`：
  - 覆盖 `FCVTN` overflow、`FCVTN` underflow-to-min-normal、`FCVTN` exact min-subnormal；
  - 覆盖 `FCVTXN` overflow、`FCVTXN` tiny underflow-to-odd-subnormal 与一个 exact case；
  - 显式锁定结果位型与 `FPSR` 位图，避免后续再把这组 narrowing flags 弄回只有 `IXC` 的宽松实现。

## 本轮测试

- `timeout 120s qemu-aarch64 ./out/fcvt_round_probe`
- `timeout 120s qemu-aarch64 ./out/fcvtn_ofc_probe`
- `timeout 120s qemu-aarch64 ./out/fcvtxn_flags_probe`
- `timeout 120s qemu-aarch64 ./out/fcvtn_underflow_probe`

## 当前结论

- 这轮已经坐实并修正了 `FCVTN/FCVTXN` narrowing 的一处真实 `FPSR` 语义缺口。
- 但我仍不能自信宣称“Armv8-A 程序可见最小集已完整收口”。当前剩余最值得继续审的仍然是：
  - `FP/AdvSIMD` 其它 `NaN/subnormal/default-NaN/flags` 组合边界；
  - `SMP` 下 exclusive/LSE、barrier、`TLBI/IC IVAU` 的跨核传播；
  - `MMU/fault` 与快路径/预解码在复杂 Linux 压力下的尾差。

# 修改日志 2026-04-01 13:48

## 本轮修改

- 继续沿着“异常 / 系统寄存器 / trap 语义收尾”审 `WFET/WFIT` 这组 `FEAT_WFxT absent` 边界；这轮没有改模拟器执行逻辑，而是把此前只被一个较宽松回归顺带覆盖、但还没被单独强断言锁死的负向路径固化成正式裸机回归：
  - [tests/arm64/wfxt_absent_undef.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/wfxt_absent_undef.S)
  - [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh)
  - [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)
  - [TODO.md](/media/luzeyu/Storage2/FOSS_src/aarchvm/TODO.md)
- 新增正式裸机回归 `wfxt_absent_undef`：
  - 覆盖当前模型下 `WFET` 与 `WFIT` 在 `EL1/EL0` 两级的执行；
  - 显式锁定两者都应表现为 `UNDEFINED`，并检查 `ESR_EL1.EC=0`、`IL=1`、`ISS=0`、`FAR_EL1=0`；
  - 同时断言寄存器形式的 timeout 操作数不应被修改，并额外检查 `EL0` 异常保存下来的 `NZCV/DAIF/PAN/M/IL`，避免后续 `WFE/WFI` trap 逻辑或 system decode 调整把 `WFET/WFIT` 误并到错误路径。

## 本轮测试

- `timeout 1200s ./tests/arm64/build_tests.sh`
- `timeout 1200s ./build/aarchvm -bin tests/arm64/out/wfxt_absent_undef.bin -load 0x0 -entry 0x0 -steps 800000`
- `timeout 5400s ./tests/arm64/run_all.sh`
- `timeout 5400s ./tests/linux/run_functional_suite.sh`
- `timeout 5400s ./tests/linux/run_functional_suite_smp.sh`
- `timeout 1800s ./tests/linux/run_qemu_user_diff.sh`

## 当前结论

- 这轮之后，`WFET/WFIT` 这组 `!FEAT_WFxT` negative path 不再只依赖较宽松的大杂烩回归，而是有了单独的强断言覆盖。
- 当前整套裸机回归、Linux UMP 回归、Linux SMP 回归与现有 qemu-user 差分入口都通过。
- 但我仍不能自信宣称“Armv8-A 程序可见最小集已完整收口”。当前我认为剩余最值得继续审的仍然是：
  - `FP/AdvSIMD` 的尾差，尤其 `FPCR/FPSR`、`NaN/subnormal/flag` 传播一致性；
  - `SMP` 下 barrier / exclusive / LSE 与 fault / exception 交错边界；
  - `MMU/TLB/fault` 在更复杂 Linux 压力与 system-level 差分下的剩余语义角落。

# 修改日志 2026-04-01 13:39

## 本轮修改

- 继续沿着“异常 / 系统寄存器 / trap 语义收尾”审 `AT` 指令在不同异常级别下的程序可见边界；这轮没有改模拟器执行逻辑，而是把此前未被正式锁死的一条 `EL0` 负向路径固化成裸机强断言回归：
  - [tests/arm64/at_s1e0_el0_undef.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/at_s1e0_el0_undef.S)
  - [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh)
  - [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)
  - [TODO.md](/media/luzeyu/Storage2/FOSS_src/aarchvm/TODO.md)
- 新增正式裸机回归 `at_s1e0_el0_undef`：
  - 覆盖当前模型下 `EL0` 执行 `AT S1E0R` 与 `AT S1E0W` 的行为；
  - 显式锁定它们都应表现为 `UNDEFINED`，并检查 `ESR_EL1.EC=0`、`IL=1`、`ISS=0`、`FAR_EL1=0`；
  - 同时断言 `PAR_EL1` 与源寄存器都不应被修改，避免后续 `AT` / system decode 调整把这组指令误宽放行、或在异常前偷偷产生寄存器副作用。

## 本轮测试

- `timeout 1200s ./tests/arm64/build_tests.sh`
- `timeout 1200s ./build/aarchvm -bin tests/arm64/out/at_s1e0_el0_undef.bin -load 0x0 -entry 0x0 -steps 800000`
- `timeout 5400s ./tests/arm64/run_all.sh`
- `timeout 5400s ./tests/linux/run_functional_suite.sh`
- `timeout 5400s ./tests/linux/run_functional_suite_smp.sh`
- `timeout 1800s ./tests/linux/run_qemu_user_diff.sh`

## 当前结论

- 这轮之后，`EL0` 执行 `AT S1E0R/W` 的负向语义不再只靠实现代码本身，而是有了正式回归持续锁定。
- 当前整套裸机回归、Linux UMP 回归、Linux SMP 回归与现有 qemu-user 差分入口都通过。
- 但我仍不能自信宣称“Armv8-A 程序可见最小集已完整收口”。当前我认为剩余最值得继续审的仍然是：
  - `FP/AdvSIMD` 的尾差，尤其 `FPCR/FPSR`、`NaN/subnormal/flag` 传播一致性；
  - `SMP` 下 barrier / exclusive / LSE 与 fault / exception 交错边界；
  - `MMU/TLB/fault` 在更复杂 Linux 压力与 system-level 差分下的剩余语义角落。

# 修改日志 2026-04-01 13:23

## 本轮修改

- 继续沿着“异常 / 系统寄存器 / trap 语义收尾”审 `ID` 已声明 absent 的 system-encoding 边界；这轮没有改模拟器执行逻辑，而是把 `FEAT_PAN2 absent` 下此前只有实现、还没被正式回归单独锁住的一条边界固化成裸机强断言回归：
  - [tests/arm64/at_pan2_absent_undef.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/at_pan2_absent_undef.S)
  - [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh)
  - [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)
  - [TODO.md](/media/luzeyu/Storage2/FOSS_src/aarchvm/TODO.md)
- 新增正式裸机回归 `at_pan2_absent_undef`：
  - 覆盖当前 `ID_AA64MMFR1_EL1.PAN=0`、`!FEAT_PAN2` 模型下 `AT S1E1RP` 与 `AT S1E1WP` 在 `EL1` / `EL0` 下的 absent-feature 行为；
  - 显式锁定 `ESR_EL1.EC=0`、`IL=1`、`ISS=0`、`FAR_EL1=0`；
  - 同时断言 `PAR_EL1` 与源寄存器都不应被修改，避免后续 system decode 调整把这两条 PAN2 专属 `AT` 指令误吞进普通 `AT` 路径。

## 本轮测试

- `timeout 1200s ./tests/arm64/build_tests.sh`
- `timeout 1200s ./build/aarchvm -bin tests/arm64/out/at_pan2_absent_undef.bin -load 0x0 -entry 0x0 -steps 800000`
- `timeout 5400s ./tests/arm64/run_all.sh`
- `timeout 5400s ./tests/linux/run_functional_suite.sh`
- `timeout 5400s ./tests/linux/run_functional_suite_smp.sh`
- `timeout 1800s ./tests/linux/run_qemu_user_diff.sh`

## 当前结论

- 这轮之后，`FEAT_PAN2 absent` 相关的 `AT` system-encoding 边界已经不再只靠实现代码本身，而是有了正式负向回归持续锁定。
- 当前整套裸机回归、Linux UMP 回归、Linux SMP 回归与现有 qemu-user 差分入口都通过。
- 但我仍不能自信宣称“Armv8-A 程序可见最小集已完整收口”。在当前代码状态下，我认为剩余最值得继续审的仍然是：
  - `FP/AdvSIMD` 细节，尤其 `FPCR/FPSR` 与 `NaN/subnormal/flags` 传播的一致性尾差；
  - `SMP` 下同步原语、barrier、exclusive/LSE 与异常/fault 交错时的边界；
  - `MMU/TLB/fault` 在更复杂 Linux 压力和差分验证下是否还会暴露新的尾差。

# 修改日志 2026-04-01 12:50

## 本轮修改

- 继续沿着“异常 / 系统寄存器 / trap 语义收尾”审 `ID` 已声明 absent 的低频 memory-operation 指令边界；这轮没有改模拟器执行逻辑，而是把此前尚未正式接回归的 `FEAT_MOPS absent` 行为固化成裸机强断言回归：
  - [tests/arm64/mops_absent_undef.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/mops_absent_undef.S)
  - [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh)
  - [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)
  - [TODO.md](/media/luzeyu/Storage2/FOSS_src/aarchvm/TODO.md)
- 新增正式裸机回归 `mops_absent_undef`：
  - 覆盖当前 `ID_AA64ISAR2_EL1.MOPS=0` 模型下 `SETP/SETM/SETE` 与 `CPYP/CPYM/CPYE` 在 `EL1` / `EL0` 下的 absent-feature 行为；
  - 显式锁定 `ESR_EL1.EC=0`、`IL=1`、`ISS=0`、`FAR_EL1=0`；
  - 同时断言目的/源/长度寄存器都不应被修改，避免后续解码调整把这组 memory-operation 指令误吞成其它 load/store 路径或错误发生写回。

## 本轮测试

- `timeout 1200s ./tests/arm64/build_tests.sh`
- `timeout 1200s ./build/aarchvm -bin tests/arm64/out/mops_absent_undef.bin -load 0x0 -entry 0x0 -steps 1500000`
- `timeout 5400s ./tests/arm64/run_all.sh`
- `timeout 5400s ./tests/linux/run_functional_suite.sh`
- `timeout 5400s ./tests/linux/run_functional_suite_smp.sh`

## 当前结论

- 这轮之后，`FEAT_MOPS absent` 至少已有一条正式回归，能持续锁住当前模型下最基础的 `SET*` / `CPY*` memory-operation 指令都应表现为 `UNDEFINED` 这一程序可见边界。
- 但我仍不能自信宣称“Armv8-A 程序可见最小集已完整收口”。当前剩余的不确定性仍主要集中在：
  - `ESR_EL1/FAR_EL1/PAR_EL1/ISS` 的系统化复查是否已经覆盖到所有已实现异常家族；
  - `SMP barrier / exclusive monitor / TLBI / IC IVAU` 在更极端交错场景下的程序可见尾差；
  - `FP/AdvSIMD` 剩余的 NaN / subnormal / rounding / exception-flag 细部语义是否已经被足够系统地锁定。

# 修改日志 2026-04-01 13:18

## 本轮修改

- 继续沿着“异常 / 系统寄存器 / trap 语义收尾”审 `ID` 已声明 absent 的 load/store 指令边界，这轮包含一处真实的 guest 可见语义修复，并新增一条正式裸机回归：
  - [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp)
  - [tests/arm64/ls64_absent_undef.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/ls64_absent_undef.S)
  - [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh)
  - [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)
  - [TODO.md](/media/luzeyu/Storage2/FOSS_src/aarchvm/TODO.md)
- 修正了 `FEAT_LS64 absent` 下的一处真实解码缺口：
  - 当前模型把 `ID_AA64ISAR1_EL1.LS64` 声明为 `0`，因此 `LD64B/ST64B/ST64BV/ST64BV0` 不应对软件可见；
  - 旧实现里，这组 64-byte single-copy atomic 指令会落进 [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp) 的 generic load/store decode 空间，其中 `LD64B/ST64B` 已可稳定复现被误吞进普通 `STUR` 类路径；
  - 这会让 guest 既看不到 `UNDEFINED`，又可能执行出真实访存或寄存器副作用；
  - 现在这四条编码在当前模型下都会稳定走同步异常 `EC=0x00` 的 `UNDEFINED` 语义，不再被 generic load/store handler 误分类。
- 新增正式裸机回归 `ls64_absent_undef`：
  - 覆盖 `LD64B/ST64B/ST64BV/ST64BV0` 在 `EL1` 与 `EL0` 下的 absent-feature 行为；
  - 显式锁定 `ESR_EL1.EC=0`、`IL=1`、`ISS=0`、`FAR_EL1=0`；
  - 同时断言结果寄存器与基址寄存器都不应被修改，避免后续又被别的 generic load/store 路径误吞。

## 本轮测试

- `timeout 1200s cmake --build build -j`
- `timeout 1200s ./tests/arm64/build_tests.sh`
- `timeout 1200s ./build/aarchvm -bin tests/arm64/out/ls64_absent_undef.bin -load 0x0 -entry 0x0 -steps 1200000`
- `timeout 5400s ./tests/arm64/run_all.sh`
- `timeout 5400s ./tests/linux/run_functional_suite.sh`
- `timeout 5400s ./tests/linux/run_functional_suite_smp.sh`

## 当前结论

- 这轮之后，当前 `!FEAT_LS64` 模型下最容易被 generic load/store decode 误吞的 64-byte single-copy atomic 指令已经有了模拟器修复、正式裸机回归，以及 Linux UMP/SMP 回归闭环。
- 但我仍不能自信宣称“Armv8-A 程序可见最小集已完整收口”。按当前代码状态，下一批仍值得继续优先审的点是：
  - 其它 `ID` 已声明 absent 的低频 load/store / system 指令，是否还存在被 generic decode 误吞的边界；
  - `MOPS` 一类较新的 memory-operation 指令族，除了已 probe 过的 `CPY*` 外，`SET*` 路径是否也全部稳定表现为 `UNDEFINED`；
  - `ESR_EL1/FAR_EL1/PAR_EL1/ISS` 在剩余异常家族上的系统化复查。

# 修改日志 2026-04-01 12:13

## 本轮修改

- 继续沿着“异常 / 系统寄存器 / trap 语义收尾”审 `ID` 已声明 absent 的 load/store 指令边界，这轮包含一处真实的 guest 可见语义修复，并新增一条正式裸机回归：
  - [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp)
  - [tests/arm64/lrcpc_absent_undef.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/lrcpc_absent_undef.S)
  - [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh)
  - [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)
  - [TODO.md](/media/luzeyu/Storage2/FOSS_src/aarchvm/TODO.md)
- 修正了 `FEAT_LRCPC absent` 下的一处真实解码缺口：
  - 当前模型把 `ID_AA64ISAR1_EL1.LRCPC` 声明为 `0`，因此 `LDAPR*` / `LDAPR[BH]` 不应对软件可见；
  - 旧实现里，`LDAPR W`、`LDAPRB W`、`LDAPRH W` 会在 [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp) 的 `exec_load_store()` 中被后续 generic unscaled load 路径误吞；
  - 结果分别退化成 `LDURSW`、`LDURSB Xt`、`LDURSH Xt`，软件侧既看不到 `UNDEFINED`，还会错误地发生实际访存和目标寄存器写回；
  - 现在这些编码在当前模型下都会稳定走同步异常 `EC=0x00` 的 `UNDEFINED` 语义，不再被 generic load/store decode 误分类。
- 新增正式裸机回归 `lrcpc_absent_undef`：
  - 覆盖 `LDAPR W`、`LDAPR X`、`LDAPRB W`、`LDAPRH W` 在 `EL1` 与 `EL0` 下的 absent-feature 行为；
  - 显式锁定 `ESR_EL1.EC=0`、`IL=1`、`ISS=0`、`FAR_EL1=0`；
  - 同时断言目标寄存器与基址寄存器都不应被修改，避免后续又被别的 generic decode 路径误吞。

## 本轮测试

- `timeout 1200s cmake --build build -j`
- `timeout 1200s ./tests/arm64/build_tests.sh`
- `timeout 1200s ./build/aarchvm -bin tests/arm64/out/lrcpc_absent_undef.bin -load 0x0 -entry 0x0 -steps 1200000`
- `timeout 1200s ./tests/arm64/run_all.sh`
- `timeout 5400s ./tests/linux/run_functional_suite.sh`
- `timeout 5400s ./tests/linux/run_functional_suite_smp.sh`

## 当前结论

- 这轮之后，当前 `!FEAT_LRCPC` 模型下最容易被 generic load/store decode 误吞的 `LDAPR*` 边界已经有了模拟器修复、正式裸机回归，以及 Linux UMP/SMP 回归闭环。
- 但我仍不能自信宣称“Armv8-A 程序可见最小集已完整收口”。按当前代码状态，下一批仍值得继续优先审的点是：
  - 其它 `ID` 已声明 absent 的低频 load/store / system 指令，是否还存在被 generic decode 误吞的边界；
  - `ESR_EL1/FAR_EL1/PAR_EL1/ISS` 在剩余异常家族上的系统化复查；
  - `SMP barrier / exclusive monitor / TLBI / IC IVAU` 在更极端交错场景下的程序可见尾差。

# 修改日志 2026-04-01 10:19

## 本轮修改

- 继续沿着“异常 / 系统寄存器 / trap 语义收尾”审 system-register visible bits，这轮包含两处真实的 guest 可见语义修复，并新增两条正式裸机回归：
  - [src/system_registers.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/system_registers.cpp)
  - [tests/arm64/cpacr_visible_bits.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/cpacr_visible_bits.S)
  - [tests/arm64/vbar_el1_res0_bits.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/vbar_el1_res0_bits.S)
  - [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh)
  - [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)
  - [TODO.md](/media/luzeyu/Storage2/FOSS_src/aarchvm/TODO.md)
- 修正了 `CPACR_EL1` 的 architected visible-bits 缺口：
  - 旧实现对 `CPACR_EL1` 做 raw read/write，会把当前模型根本没有实现的 `TTA/SMEN/ZEN/E0POE/TAM/TCPAC` 等位错误暴露给 guest；
  - 现在 direct `MRS/MSR CPACR_EL1`、以及 snapshot load 后的状态，都统一收敛到当前模型真正支持的 `FPEN[21:20]`。
- 修正了 `VBAR_EL1` 的 `RES0` 低位缺口：
  - 旧实现允许 guest 写入并读回 bits `[10:0]`，异常入口也会直接使用这个未对齐值；
  - 现在 `VBAR_EL1` direct read/write 与 snapshot load 后状态都会强制清零 bits `[10:0]`，CPU 异常入口也因此稳定落在 2KB 对齐的向量表基址。
- 新增正式裸机回归：
  - `cpacr_visible_bits` 显式锁定 `CPACR_EL1` 初值、全 1 写入后的读回，以及混合脏位写入后只保留 `FPEN`；
  - `vbar_el1_res0_bits` 同时锁定 `VBAR_EL1` read-back 对齐结果，以及一次真实 `SVC` 是否落到对齐后的向量入口，而不是 misaligned 基址。

## 本轮测试

- `timeout 1200s cmake --build build -j`
- `timeout 1200s ./tests/arm64/build_tests.sh`
- `timeout 120s env AARCHVM_BRK_MODE=halt ./build/aarchvm -bin tests/arm64/out/cpacr_visible_bits.bin -load 0x0 -entry 0x0 -steps 200000`
- `timeout 120s env AARCHVM_BRK_MODE=halt ./build/aarchvm -bin tests/arm64/out/vbar_el1_res0_bits.bin -load 0x0 -entry 0x0 -steps 200000`
- `timeout 5400s ./tests/arm64/run_all.sh`
- `timeout 5400s ./tests/linux/run_functional_suite.sh`
- `timeout 5400s ./tests/linux/run_functional_suite_smp.sh`

## 当前结论

- 这轮之后，`CPACR_EL1` 和 `VBAR_EL1` 两条最直接的 system-register visible-bits 缺口都已经有了模拟器实现、正式裸机回归，以及 Linux UMP/SMP 功能回归闭环。
- 但我仍不能自信宣称“Armv8-A 程序可见最小集已完整收口”。当前剩余的不确定性仍主要集中在：
  - `SMP barrier / exclusive monitor / TLBI / IC IVAU` 的系统化复查；
  - 更多 system register / exception visible bits 的继续审计，尤其是 `TTBR/TCR/MAIR/cache ID` 这类目前仍偏宽松的 direct read/write 路径；
  - 更系统的差分验证和长期 Linux 压力回归。

# 修改日志 2026-03-31 23:34

## 本轮修改

- 继续沿着“异常 / 系统寄存器 / trap 语义收尾”审 `PC alignment fault` 这条线，这轮包含一处真实的模拟器执行逻辑修复，并新增正式裸机回归：
  - [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp)
  - [tests/arm64/pc_alignment_fault.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/pc_alignment_fault.S)
  - [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh)
  - [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)
  - [TODO.md](/media/luzeyu/Storage2/FOSS_src/aarchvm/TODO.md)
- 修正了当前 AArch64 取指路径一处真实缺口：
  - 旧实现没有在取指前检查 `PC[1:0]`，所以通过 `BR/RET/ERET` 写入 misaligned `PC` 后，会直接从错地址继续取指；
  - 现在在 CPU 主循环里补上了真正的 `PC alignment fault` 检查，misaligned `PC` 会稳定走 `EC=0x22`，并把 `FAR_EL1/ELR_EL1` 都指向 faulting `PC`。
- 这轮还顺手补齐了它与 `Illegal Execution state` 的优先级边界：
  - ARM ARM 的同步异常优先级里，`PC alignment fault` 高于 `Illegal Execution state`；
  - 旧实现先看 `PSTATE.IL`，因此“`ERET` 返回到 `IL=1 + misaligned PC`”会被错误报成 `EC=0x0E`；
  - 现在顺序已经改正，`PC alignment fault` 会先发生。
- 新增的 `pc_alignment_fault` 裸机回归显式覆盖两条正式边界：
  - `EL1 BR` 到 misaligned 目标时，目标指令不得执行；
  - `ERET` 返回到 `EL0`，且同时满足 `IL=1 + misaligned PC` 时，仍必须先报 `EC=0x22`，而不是 `Illegal Execution state`。

## 本轮测试

- `timeout 1200s cmake --build build -j`
- `timeout 1200s ./tests/arm64/build_tests.sh`
- `timeout 120s env AARCHVM_BRK_MODE=halt ./build/aarchvm -bin tests/arm64/out/pc_alignment_fault.bin -load 0x0 -entry 0x0 -steps 600000`
- `timeout 5400s ./tests/arm64/run_all.sh`
- `timeout 5400s ./tests/linux/run_functional_suite.sh`
- `timeout 5400s ./tests/linux/run_functional_suite_smp.sh`

## 当前结论

- 这轮之后，AArch64 `PC alignment fault` 这条程序可见最小语义已经有了模拟器实现、正式裸机回归，以及 Linux UMP/SMP 全回归闭环。
- 但我仍不能自信宣称“Armv8-A 程序可见最小集已完整收口”。当前剩余的不确定性仍主要集中在：
  - `SMP barrier / exclusive monitor / TLBI / IC IVAU` 的系统化复查；
  - 其余低频 `trap / undef / no-op` 边界继续审计；
  - 更系统的差分验证和长期 Linux 压力回归。

# 修改日志 2026-03-31 23:19

## 本轮修改

- 继续沿着“异常 / 系统寄存器 / trap 语义收尾”审 `WFE/WFI` 这条线，这轮包含一处真实的模拟器执行逻辑修复，不只是测试收紧：
  - [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp)
  - [tests/arm64/el0_wfx_trap.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/el0_wfx_trap.S)
  - [TODO.md](/media/luzeyu/Storage2/FOSS_src/aarchvm/TODO.md)
- 修正了 `EL0 WFE/WFI` 在 `SCTLR_EL1.nTWE/nTWI=0` 下的一处真实 trap 顺序缺口：
  - 旧实现会先看 `event_register_` 或已有 pending IRQ，再决定是否 trap；
  - 但按 ARM ARM `CheckForWFxTrap` 伪代码，这一类 trap 必须先判定；
  - 现在 `EL0 WFE` 在 `nTWE=0` 时会先 trap，不再因为本地 event register 已置位就错误地“直接完成”；
  - 现在 `EL0 WFI` 在 `nTWI=0` 时也会先 trap，不再因为已有 pending IRQ 就绕过 `EC=0x01` 的 `WFx` trap。
- 同步把对应裸机回归从旧错误假设修正为正式强断言：
  - `el0_wfx_trap` 现在显式覆盖“event register 已置位但 `WFE` 仍必须 trap”；
  - 以及“已有 pending IRQ 但 `WFI` 仍必须 trap”；
  - 并额外锁定：被 trap 的 `WFE` 不应偷偷消费 event register。

## 本轮测试

- `timeout 1200s cmake --build build -j`
- `timeout 1200s ./tests/arm64/build_tests.sh`
- `timeout 120s ./build/aarchvm -bin tests/arm64/out/el0_wfx_trap.bin -load 0x0 -entry 0x0 -steps 800000`
- `timeout 5400s ./tests/arm64/run_all.sh`
- `timeout 5400s ./tests/linux/run_functional_suite.sh`
- `timeout 5400s ./tests/linux/run_functional_suite_smp.sh`

## 当前结论

- 这轮之后，`WFE/WFI` 这条程序可见 trap 边界与 ARM ARM 的优先级已经对齐，并且已经被裸机单测和 Linux UMP/SMP 功能回归锁住。
- 但我仍不能自信宣称“Armv8-A 程序可见最小集已完整收口”。当前剩余的不确定性仍主要集中在：
  - `SMP barrier / exclusive monitor / TLBI / IC IVAU` 在更极端交错场景下的系统化复查；
  - 若干低频 `trap / undef / no-op` 边界的继续审计；
  - 更系统的差分验证与长期 Linux 压力回归。

# 修改日志 2026-03-30 22:12

## 本轮修改

- 继续沿着“异常 / 系统寄存器 / trap 语义收尾”审 `ICC_*` 这条线，这轮包含真实的模拟器执行逻辑修复，不只是测试收紧：
  - [include/aarchvm/gicv3.hpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/include/aarchvm/gicv3.hpp)
  - [src/gicv3.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/gicv3.cpp)
  - [include/aarchvm/cpu.hpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/include/aarchvm/cpu.hpp)
  - [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp)
  - [src/soc.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/soc.cpp)
  - [tests/arm64/gic_sysreg_manual_ack.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/gic_sysreg_manual_ack.S)
  - [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh)
  - [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)
- 修正了当前 GIC CPU-interface sysreg 一处真实缺口：
  - 之前 `ICC_HPPIR1_EL1 / ICC_IAR1_EL1 / ICC_EOIR1_EL1 / ICC_DIR_EL1` 基本只在“已经走进 IRQ 异常入口”的路径上有完整语义；
  - 当 guest 在同步异常上下文或普通 EL1 代码里，手动通过这些 sysreg 查询并处理 pending IRQ 时，模拟器会给出不完整甚至错误的程序可见结果；
  - 现在 CPU 侧新增了独立的 manual-IRQ stack，能在非 IRQ 异常上下文下正确维护 `running_priority`、priority drop 与 deactivate 语义；
  - `ICC_IAR1_EL1` 会真正执行一次手动 acknowledge，`ICC_EOIR1_EL1` / `ICC_DIR_EL1` 会按 `EOImode` 与当前 running priority 状态正确收尾；
  - 这部分新增状态已并入 snapshot save/load，因此 snapshot 版本从 `20` 升到 `21`。
- 同时修正了 GIC acknowledge 的优先级选择策略：
  - 之前存在按 `INTID` 顺序扫 pending 的路径；
  - 现在 `GicV3` 统一先选“当前可递送的最高优先级 pending 中断”，同优先级再按 `INTID` 打破平局；
  - `has_pending()` 也复用这一逻辑，避免“能看到有 pending，但手动 `IAR` 拿到的不是同一个候选”这类尾差。
- 在继续审计时还发现一处测试本身的真实错误：
  - [tests/arm64/casp_pair.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/casp_pair.S) 原先把 64-bit `CASP*` 测试槽放在仅 8-byte 对齐的位置；
  - 依据 Arm ARM，`CASP` 必须按总访问大小对齐，因此 64-bit pair 需要 16-byte 对齐；
  - 这轮把 `pair64_slot` 改成显式 `16-byte` 对齐，避免把正确的 alignment fault 误当成模拟器 bug。
- 顺手补上了裸机测试框架的一处盲点：
  - `run_all.sh` 原先有一类 `run` 用例只看退出码，可能把 stderr 里的 `FATAL/UNIMPL/NESTED-SYNC` 静默吞掉；
  - 现在 `run` / `run_expect` / `run_expect_smp` / `run_expect_trap` 都会统一扫描 stderr，一旦出现这类内部异常立即失败；
  - `casp_pair.bin` 也从“只运行”改成“必须输出 `C`”，防止同类问题再次漏检。
- 更新：
  - [TODO.md](/media/luzeyu/Storage2/FOSS_src/aarchvm/TODO.md)

## 本轮测试

- `timeout 300s ./tests/arm64/build_tests.sh`
- `timeout 60s env AARCHVM_BRK_MODE=halt ./build/aarchvm -bin tests/arm64/out/el0_wfx_trap.bin -load 0x0 -entry 0x0 -steps 1200000`
- `timeout 60s env AARCHVM_BRK_MODE=halt ./build/aarchvm -bin tests/arm64/out/gic_sysreg_manual_ack.bin -load 0x0 -entry 0x0 -steps 400000`
- `timeout 60s env AARCHVM_BRK_MODE=halt ./build/aarchvm -bin tests/arm64/out/casp_pair.bin -load 0x0 -entry 0x0 -steps 400000`
- `timeout 5400s ./tests/arm64/run_all.sh`
- `timeout 5400s ./tests/linux/run_functional_suite.sh`
- `timeout 5400s ./tests/linux/run_functional_suite_smp.sh`

## 当前结论

- 这轮之后，`ICC_*` 手动查询 / acknowledge / EOI / DIR 这条程序可见路径已经有了模拟器实现与正式裸机回归闭环，`CASP` 这条线也纠正了一处测试本身的对齐错误，并顺带补上了 `run_all.sh` 的 stderr 漏检盲点。
- 但我仍不认为现在可以自信宣称“Armv8-A 程序可见最小集已完整收口”。当前剩余的不确定性仍主要集中在：
  - `ESR_EL1/FAR_EL1/PAR_EL1/ISS` 在剩余异常家族上的系统化复查；
  - 仍未完全系统化锁死的 `trap / undef / no-op` 边界；
  - `SMP barrier / TLBI / fault / exception return` 在更极端交错场景下的尾差。

# 修改日志 2026-03-30 16:58

## 本轮修改

- 继续沿着“异常 / 系统寄存器 / trap 语义收尾”推进，这轮仍然没有修改模拟器执行逻辑，而是把一批此前已经存在、但断言还不够硬的正式回归继续收紧：
  - [tests/arm64/sync_exception_regs.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/sync_exception_regs.S)
  - [tests/arm64/svc_sysreg_minimal.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/svc_sysreg_minimal.S)
  - [tests/arm64/software_step_basic.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/software_step_basic.S)
  - [tests/arm64/el0_cache_ops_privilege.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/el0_cache_ops_privilege.S)
  - [tests/arm64/el0_tlbi_cache_undef.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/el0_tlbi_cache_undef.S)
  - [tests/arm64/mmu_at_par_formats.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/mmu_at_par_formats.S)
  - [tests/arm64/brk_exception.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/brk_exception.S)
  - [tests/arm64/hlt_undef.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/hlt_undef.S)
  - [tests/arm64/el0_hvc_smc_undef.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/el0_hvc_smc_undef.S)
  - [tests/arm64/el1_hvc_smc_undef.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/el1_hvc_smc_undef.S)
  - [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh)
  - [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)
- 这轮把几组关键异常语义从“发生了异常就算过”推进到“异常细节也必须对”：
  - `sync_exception_regs` 现在锁定 same-EL instruction abort 的 `ESR_EL1/ELR_EL1/FAR_EL1`、异常入口 live `DAIF`，以及 `SPSR_EL1` 保存的源 `NZCV/DAIF/PAN/IL/M`；
  - `svc_sysreg_minimal` 现在显式检查 `SVC` 的 `ESR_EL1.IL`、`ISS imm16`、`FAR_EL1=0`，以及 `SPSR_EL1` 里保存的源 `NZCV/DAIF/PAN/M`；
  - `software_step_basic` 新增 “`SS + EL0 BRK` 走 Breakpoint Instruction exception，且 `SPSR_EL1.SS` 保持置位” 的正式断言；
  - `mmu_at_par_formats` 新增 `AT -> PAR_EL1` 成功态与 fault 态格式回归，显式锁定 `RES1 bit11`、shareability 与 permission fault 的 `FST`；
  - `el0_cache_ops_privilege` / `el0_tlbi_cache_undef` 现在会显式拒绝把 `TLBI from EL0` 一类路径误报成 `EC=0x18` system-access trap，并断言 `IL=1`、`ISS=0`、`FAR_EL1=0`；
  - `brk_exception`、`hlt_undef`、`el0_hvc_smc_undef`、`el1_hvc_smc_undef` 现在统一检查 `IL/ISS/FAR` 与 `SPSR_EL1` 中保存的源 `NZCV/DAIF/PAN/M`，把 `BRK/HLT/HVC/SMC` 从 smoke 收紧成正式异常回归。
- 这一轮的重点不是“再堆几条测试”，而是继续补齐我们对异常路径的信心边界：让更多 same-EL / lower-EL、`trap / undef / breakpoint`、以及 `AT / cache / TLBI` 路径都必须给出正确的 syndrome、`FAR_EL1` 和保存状态。
- 更新：
  - [TODO.md](/media/luzeyu/Storage2/FOSS_src/aarchvm/TODO.md)

## 本轮测试

- `timeout 300s ./tests/arm64/build_tests.sh`
- `timeout 60s env AARCHVM_BRK_MODE=halt ./build/aarchvm -bin tests/arm64/out/mmu_at_par_formats.bin -load 0x0 -entry 0x0 -steps 4000000`
- `timeout 60s env AARCHVM_BRK_MODE=halt ./build/aarchvm -bin tests/arm64/out/el0_cache_ops_privilege.bin -load 0x0 -entry 0x0 -steps 600000`
- `timeout 60s env AARCHVM_BRK_MODE=halt ./build/aarchvm -bin tests/arm64/out/el0_tlbi_cache_undef.bin -load 0x0 -entry 0x0 -steps 800000`
- `timeout 60s env AARCHVM_BRK_MODE=trap ./build/aarchvm -bin tests/arm64/out/brk_exception.bin -load 0x0 -entry 0x0 -steps 600000`
- `timeout 60s env AARCHVM_BRK_MODE=trap ./build/aarchvm -bin tests/arm64/out/hlt_undef.bin -load 0x0 -entry 0x0 -steps 600000`
- `timeout 60s env AARCHVM_BRK_MODE=halt ./build/aarchvm -bin tests/arm64/out/el0_hvc_smc_undef.bin -load 0x0 -entry 0x0 -steps 600000`
- `timeout 60s env AARCHVM_BRK_MODE=halt ./build/aarchvm -bin tests/arm64/out/el1_hvc_smc_undef.bin -load 0x0 -entry 0x0 -steps 600000`
- `timeout 5400s ./tests/arm64/run_all.sh`
- `timeout 5400s ./tests/linux/run_functional_suite.sh`
- `timeout 5400s ./tests/linux/run_functional_suite_smp.sh`

## 当前结论

- 这轮之后，same-EL instruction abort、`SVC`、software step、`AT -> PAR_EL1`、`EL0 cache/TLBI/AT`、以及 `BRK/HLT/HVC/SMC` 这几条线上，`ESR_EL1/FAR_EL1/PAR_EL1/SPSR_EL1` 的程序可见关键位已经比前一轮更系统地被正式回归锁住。
- 但我仍不能自信宣称“Armv8-A 程序可见最小集已完整收口”。按当前代码状态，下一批仍值得继续优先审的点是：
  - `WFE/WFI` trap、EL0 timer trap 与一般 `system-access trap` 路径中，尚未系统锁定的 `FAR_EL1/IL/SPSR_EL1` 保存状态；
  - `SMP barrier / TLBI / fault / exception return` 在更极端交错场景下是否还存在程序可见尾差；
  - 仍未系统化差分验证的一些低频 `trap / undef / no-op` 边界。

# 修改日志 2026-03-30 15:58

## 本轮修改

- 继续沿着 “异常 / 系统寄存器 / trap 语义收尾” 审 `ERET/SPSR/PSTATE` 这条线，这轮仍然没有改模拟器执行逻辑，而是把 [tests/arm64/illegal_state_return.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/illegal_state_return.S) 从“两条 smoke case”继续收紧成三条正式边界：
  - 保留的 AArch64 `SPSR_EL1.M` 非法返回；
  - 合法返回但 `SPSR_EL1.IL=1`，首条目标指令触发 `Illegal State`；
  - 返回到当前模型不支持的 AArch32 state，首条目标指令同样触发 `Illegal State`。
- 同一轮继续把 [tests/arm64/el0_eret_undef.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/el0_eret_undef.S) 从 “EL0 执行 `ERET` 会进 `UNDEFINED`” 的基本 smoke，收紧成同时校验：
  - `ESR_EL1.IL=1`
  - `ESR_EL1.ISS=0`
  - `FAR_EL1=0`
  - `SPSR_EL1` 中保存的 EL0 源 `NZCV/DAIF/PAN/M/IL`
- 这条回归现在不再只验证“有没有进 `EC=0x0E`”，而是把 `ERET` 非法返回后的保存状态也一起锁死，显式断言 `SPSR_EL1` 中的：
  - `NZCV`
  - `DAIF`
  - `PAN`
  - `IL`
  - `M`
  都与非法返回后当前 EL 真正可见的 `PSTATE` 一致。
- 这轮的意义是把 `ERET` 这条尾状态路径再往前推一格，既覆盖非法返回，也覆盖 `EL0 -> UNDEFINED` 这条 trap 语义，防止后续继续优化异常返回时，只保住“能 trap”，却把 `PSTATE` / syndrome 某些程序可见位悄悄弄错。
- 更新：
  - [tests/arm64/illegal_state_return.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/illegal_state_return.S)
  - [tests/arm64/el0_eret_undef.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/el0_eret_undef.S)
  - [TODO.md](/media/luzeyu/Storage2/FOSS_src/aarchvm/TODO.md)

## 本轮测试

- `timeout 300s ./tests/arm64/build_tests.sh`
- `timeout 60s env AARCHVM_BRK_MODE=halt ./build/aarchvm -bin tests/arm64/out/illegal_state_return.bin -load 0x0 -entry 0x0 -steps 800000`
- `timeout 60s env AARCHVM_BRK_MODE=halt ./build/aarchvm -bin tests/arm64/out/el0_eret_undef.bin -load 0x0 -entry 0x0 -steps 600000`
- `timeout 5400s ./tests/arm64/run_all.sh`
- `timeout 5400s ./tests/linux/run_functional_suite.sh`
- `timeout 5400s ./tests/linux/run_functional_suite_smp.sh`

## 当前结论

- `ERET/SPSR/PSTATE` 这条线现在又多了一条正式回归，覆盖了“AArch64-only 模型下返回到 AArch32 state”这类之前仍未被显式锁住的非法返回边界。
- 但我仍不能自信宣称“Armv8-A 程序可见最小集已完整收口”。当前仍值得继续优先审的点是：
  - `ESR_EL1/FAR_EL1/PAR_EL1/ISS` 在更多异常家族上的一致性，尤其是 cache/TLB/system 指令与 fault 交错时的 syndrome；
  - 剩余 `trap / undef / no-op` 边界里低频 debug/system 指令的系统化收口；
  - `SMP barrier / TLBI / fault / exception return` 交错路径下是否还存在只在更极端场景才暴露的程序可见尾差。

# 修改日志 2026-03-30 15:34

## 本轮修改

- 继续沿着 “异常 / 系统寄存器 / trap 语义收尾” 审 `EC=0x18 system access trap` 这条线，这轮没有改模拟器执行逻辑，而是补上了一处此前仍偏弱的 syndrome 覆盖：
  - [tests/arm64/sysreg_trap_iss_rt_fields.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/sysreg_trap_iss_rt_fields.S)
  - [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh)
  - [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)
  - [TODO.md](/media/luzeyu/Storage2/FOSS_src/aarchvm/TODO.md)
- 这条新回归把 `EC=0x18` 的验证从“只看 trap 有没有发生”收紧到“显式锁死 syndrome 编码字段”，覆盖四条代表性 EL0 trap：
  - `mrs x5, ctr_el0` with `SCTLR_EL1.UCT=0`
  - `msr tcr_el1, x9`
  - `ic ivau, x17` with `SCTLR_EL1.UCI=0`
  - `dc zva, x13` with `SCTLR_EL1.DZE=0`
- 回归现在会逐项断言：
  - `ESR_EL1.EC == 0x18`
  - `ISS` 中的 `Rt` 字段不被错误固定
  - read/write bit 与实际指令方向一致
  - `op0/op1/CRn/CRm/op2` 编码与 trap 指令一致
  - `FAR_EL1 == 0`
- 这轮的意义不是把测试“堆多一点”，而是把 `system-access trap` 这条线上最容易悄悄错、但之前不一定会被发现的 `ISS` 细节正式锁进回归。

## 本轮测试

- `timeout 300s ./tests/arm64/build_tests.sh`
- `timeout 60s env AARCHVM_BRK_MODE=halt ./build/aarchvm -bin tests/arm64/out/sysreg_trap_iss_rt_fields.bin -load 0x0 -entry 0x0 -steps 600000`
- `timeout 5400s ./tests/arm64/run_all.sh`
- `timeout 5400s ./tests/linux/run_functional_suite.sh`
- `timeout 5400s ./tests/linux/run_functional_suite_smp.sh`

## 当前结论

- 这轮之后，`EC=0x18` system-access trap 至少已有一条正式回归会检查 `ISS` 的关键编码字段，不再只是笼统验证“发生了 trap”。
- 但我仍不能自信宣称“Armv8-A 程序可见最小集已完整收口”。按当前代码状态，仍值得继续审的高优先级缺口主要是：
  - `ESR_EL1/FAR_EL1/PAR_EL1/ISS` 在其余异常家族上的全覆盖一致性，尤其是更多 cache/TLB/system 指令与 fault 交错的场景；
  - 剩余 `trap / undef / no-op` 边界里尚未系统化锁住的低频 system/debug 指令族；
  - `ERET/SPSR/PSTATE` 尾状态与 `SMP barrier / TLBI / fault` 交错边界。

# 修改日志 2026-03-30 13:35

## 本轮修改

- 继续沿着 “异常 / 系统寄存器 / trap 语义收尾” 审 `CPACR_EL1` 下 direct `FPCR/FPSR` 访问这条线，补上了一处会影响我们对回归结论信心的覆盖空洞：
  - [tests/arm64/cpacr_fp_sysreg_trap.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/cpacr_fp_sysreg_trap.S)
  - [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh)
  - [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)
- 具体修正了两部分：
  - 修正了新回归自身的一处标签错误：handler 原先拿 `ELR_EL1` 去比的是准备指令地址，而不是真正会 trap 的 `MRS/MSR FPCR/FPSR` 地址，导致测试误报失败；
  - 把 `FPEN=10` 也正式纳入覆盖，不再只测 `00 / 01 / 11`。
- 现在这条回归会显式覆盖：
  - `FPEN=00` 时 EL1 direct `MRS FPCR` trap；
  - `FPEN=01` 时 EL0 direct `MSR FPSR` trap；
  - `FPEN=10` 时 EL1 direct `MRS FPCR` trap；
  - `FPEN=10` 时 EL0 direct `MSR FPSR` trap；
  - `FPEN=11` 时 EL0 direct `MSR/MRS FPCR/FPSR` 正常访问。
- 这轮没有修改模拟器执行逻辑，修改的是正式回归覆盖本身；目的不是把测试“改绿”，而是把 `CPACR_EL1.FPEN` 对 direct `FPCR/FPSR` special-purpose sysreg access 的程序可见语义真正锁死。
- 更新：
  - [TODO.md](/media/luzeyu/Storage2/FOSS_src/aarchvm/TODO.md)

## 本轮测试

- `timeout 300s ./tests/arm64/build_tests.sh`
- `timeout 60s env AARCHVM_BRK_MODE=halt ./build/aarchvm -bin tests/arm64/out/cpacr_fp_sysreg_trap.bin -load 0x0 -entry 0x0 -steps 500000`
- `timeout 5400s ./tests/arm64/run_all.sh`
- `timeout 5400s ./tests/linux/run_functional_suite.sh`
- `timeout 5400s ./tests/linux/run_functional_suite_smp.sh`

## 当前结论

- direct `FPCR/FPSR` 访问在 `CPACR_EL1.FPEN` 各主要模式下的 trap/allow 语义，现在已有正式回归闭环，且整套裸机与 Linux UMP/SMP 回归通过。
- 但我仍不能自信宣称“Armv8-A 程序可见最小集已完整收口”；当前剩余的不确定性更集中在：
  - `ESR_EL1/FAR_EL1/PAR_EL1/ISS` 的全覆盖一致性；
  - 一些低频 `trap / undef / no-op` 边界是否还存在未正式覆盖的系统指令族；
  - `SMP barrier / fault / TLB` 这几类路径是否还有只在更极端场景下才会暴露的程序可见尾差。

# 修改日志 2026-03-30 13:15

## 本轮修改

- 继续沿着 “Armv8-A 程序可见正确性收尾计划” 往前审时，发现了一处会直接降低我们对回归结论可信度的测试空洞：
  - [tests/arm64/fpsimd_minimal.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/fpsimd_minimal.S)
  - [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)
- 具体问题是：
  - 上一轮把 `FPCR/FPSR` direct access 掩码收口后，`fpsimd_minimal` 里那段“写 `FPCR/FPSR` 再读回”的老检查其实已经失效；
  - 但 `run_all.sh` 对这个用例用的是 `run` 而不是 `run_expect`，所以即便它打印失败字符，整套裸机回归也不会报错。
- 这轮修正了两部分：
  - 把 `fpsimd_minimal` 的预期改成当前模型真实的 architected 语义：
    - `FPCR` 现在期望读回当前模型允许可见的掩码值；
    - `FPSR` 现在期望只保留 `QC/IDC/IOC..IXC` 这些当前模型实现的位。
  - 把 `run_all.sh` 中的 `fpsimd_minimal` 从“只运行”改成“必须断言输出为 `W`”，避免今后再把这类失败静默吞掉。
- 更新：
  - [TODO.md](/media/luzeyu/Storage2/FOSS_src/aarchvm/TODO.md)

## 本轮测试

- `timeout 300s ./tests/arm64/build_tests.sh`
- `timeout 60s ./build/aarchvm -bin tests/arm64/out/fpsimd_minimal.bin -load 0x0 -entry 0x0 -steps 400000`
- `timeout 5400s ./tests/arm64/run_all.sh`
- `timeout 5400s ./tests/linux/run_functional_suite.sh`
- `timeout 5400s ./tests/linux/run_functional_suite_smp.sh`

## 当前结论

- 这轮没有改模拟器执行逻辑，但修掉了一处真实的“回归能绿、语义其实已经坏了”的验证缺口。
- 这也说明当前我仍不能自信宣称“Armv8-A 程序可见最小集已完整收口”：
  - 一方面还存在剩余语义尾差需要继续审；
  - 另一方面也还需要继续把这种只 smoke、不断言的旧测试逐步收紧。

# 修改日志 2026-03-30 12:38

## 本轮修改

- 继续沿着 “Armv8-A 程序可见正确性收尾计划” 审 `FPCR/FP` 这条线，补上了一处直接 `MRS/MSR FPCR` 就能观察到的程序可见缺口：
  - [src/system_registers.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/system_registers.cpp)
  - [include/aarchvm/system_registers.hpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/include/aarchvm/system_registers.hpp)
- 收口了当前 AArch64-only 模型的 `FPCR` direct read/write 掩码：
  - 现在只保留当前模型真正支持、且对 guest architecturally visible 的 `AHP/DN/FZ/RMode/Stride/Len`。
  - `!FEAT_AFP` 下的 `NEP/AH/FIZ`、`!FEAT_FP16` 下的 `FZ16`、`!FEAT_EBF16` 下的 `EBF`，以及当前未实现 trapped FP exception 时本应 `RAZ/WI` 的 `IDE/IXE/UFE/OFE/DZE/IOE`，现在都不会再被 guest 错误写回并读出。
  - snapshot 恢复路径也会对 `FPCR` 做同样归一化，避免旧快照把本模型未实现的位重新暴露给 guest。
- 修正了一个测试覆盖错误：
  - [tests/arm64/fp_ah_absent_ignored.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/fp_ah_absent_ignored.S) 之前把 `FPCR.AH` 错写成了 bit `26`，实际应为 bit `1`；现在已改成真正覆盖 `AH` 这条语义。
- 新增正式裸机回归：
  - [tests/arm64/fpcr_visible_bits.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/fpcr_visible_bits.S)
  - [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh)
  - [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)
  - 覆盖：
    - `MSR FPCR, Xt` 写入全 1 后，只保留当前模型允许 guest 观察到的位；
    - `AH/FIZ/NEP` 等当前模型里应为 `RES0/RAZ/WI` 的位不会被错误读回；
    - 保留的 `AHP/DN/FZ/RMode/Stride/Len` 仍可正常读回。
- 更新：
  - [TODO.md](/media/luzeyu/Storage2/FOSS_src/aarchvm/TODO.md)

## 本轮测试

- `timeout 600s cmake --build build -j`
- `timeout 300s ./tests/arm64/build_tests.sh`
- `timeout 60s ./build/aarchvm -bin tests/arm64/out/fpcr_visible_bits.bin -load 0x0 -entry 0x0 -steps 200000`
- `timeout 60s ./build/aarchvm -bin tests/arm64/out/fp_ah_absent_ignored.bin -load 0x0 -entry 0x0 -steps 400000`
- `timeout 5400s ./tests/arm64/run_all.sh`
- `timeout 5400s ./tests/linux/run_functional_suite.sh`
- `timeout 5400s ./tests/linux/run_functional_suite_smp.sh`

## 当前结论

- 这轮之后，`FPCR` direct access 至少不再把当前模型里本就不该对 AArch64 guest 可见的 feature-absent / trap-absent 位暴露出去。
- 但我仍不能自信宣称“Armv8-A 程序可见最小集已完整收口”；当前更值得继续审的，仍然是：
  - `FPSR.IOC/DZC/OFC/UFC/IXC` 在剩余标量/向量 FP 路径中的一致性尾差；
  - `FPCR.DN/FZ` 在少数尚未系统覆盖的 conversion / compare / misc 路径边界；
  - `ESR_EL1/FAR_EL1/PAR_EL1/ISS` 与异常类型的完全对齐。

# 修改日志 2026-03-30 12:19

## 本轮修改

- 继续沿着 “Armv8-A 程序可见正确性收尾计划” 审 `FP/AdvSIMD + FPSR` 这条线，补上了一处真实的程序可见缺口：
  - [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp)
  - [src/system_registers.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/system_registers.cpp)
  - [include/aarchvm/system_registers.hpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/include/aarchvm/system_registers.hpp)
- 修正当前已实现的饱和 AdvSIMD 整数指令没有更新 `FPSR.QC` 的问题：
  - `SQADD/UQADD` 向量形式现在在任一 lane 发生饱和时都会置位 `FPSR.QC`。
  - `SQXTN/UQXTN` 及其 `*2` 路径现在在窄化时发生饱和也会置位 `FPSR.QC`。
  - 未发生饱和时，这些路径不会错误污染 `FPSR.QC`。
- 收口了 `FPSR` 的 direct read/write 掩码：
  - 当前 AArch64-only 模型下，只保留 `QC`、`IDC`、`IOC/DZC/OFC/UFC/IXC` 这些已实现且 architecturally visible 的位。
  - 之前可被 `MSR FPSR, Xt` 错误写回的 `N/Z/C/V` 和其它 `RES0` 位现在不再对 guest 可见。
- 新增正式裸机回归：
  - [tests/arm64/fpsr_qc_saturation.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/fpsr_qc_saturation.S)
  - [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh)
  - [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)
  - 覆盖：
    - `FPSR` 仅保留当前模型支持的 architected 位；
    - `SQADD` 饱和/非饱和两条路径；
    - `SQXTN` 与 `UQXTN` 的饱和置位行为。
- 更新：
  - [TODO.md](/media/luzeyu/Storage2/FOSS_src/aarchvm/TODO.md)

## 本轮测试

- `timeout 600s cmake --build build -j`
- `timeout 300s ./tests/arm64/build_tests.sh`
- `timeout 60s ./build/aarchvm -bin tests/arm64/out/fpsr_qc_saturation.bin -load 0x0 -entry 0x0 -steps 400000`
- `timeout 5400s ./tests/arm64/run_all.sh`
- `timeout 5400s ./tests/linux/run_functional_suite.sh`
- `timeout 5400s ./tests/linux/run_functional_suite_smp.sh`

## 当前结论

- 这一轮之后，当前模型里已经实现出来的那批饱和 AdvSIMD 整数指令，至少不会再出现“结果被夹到了边界，但 `FPSR.QC` 仍旧静默不变”的程序可见错误。
- `FPSR` 的 direct access 也不再把一堆当前模型里本就不该对 AArch64 guest 可见的保留位暴露出去。

# 修改日志 2026-03-30 00:35

## 本轮修改

- 继续按“Armv8-A 程序可见正确性收尾计划”收 debug / DCC 这条线，补上了一个之前还留着的程序可见一致性缺口：
  - [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp)
- 修正 `MDSCR_EL1[30:29]` 与 DCC live 状态脱节的问题：
  - `MRS MDSCR_EL1` 现在会把 `RXfull/TXfull` 与 CPU 内部 DCC 状态统一，不再读到过期的保存值。
  - `OS Lock` 锁定时，`MSR MDSCR_EL1` 对 `RXfull/TXfull` 的 save/restore 写入现在会同步更新 DCC full 标志，因此 `MDCCSR_EL0` 与后续 `MRS MDSCR_EL1` 看到的是同一份状态。
  - `OS Lock` 解锁后，对这两个 full 位的 direct write 继续保持只读语义，不会错误清掉 live DCC 状态。
- 新增正式裸机回归：
  - [tests/arm64/debug_mdscr_dcc_flags.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/debug_mdscr_dcc_flags.S)
  - [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh)
  - [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)
  - 覆盖两类关键边界：
    - `OS Lock` 锁定态下，`MDSCR_EL1` save/restore 写入 `TXfull/RXfull` 后，`MDCCSR_EL0` 与 `MDSCR_EL1` 要同步可见。
    - `OS Lock` 解锁态下，`DBGDTRTX_EL0` 产生的 live `TXfull` 必须能被 `MRS MDSCR_EL1` 观察到，且 direct `MSR MDSCR_EL1` 不能错误清掉它。
- 更新：
  - [TODO.md](/media/luzeyu/Storage2/FOSS_src/aarchvm/TODO.md)

## 本轮测试

- `timeout 300s ./tests/arm64/build_tests.sh`
- `timeout 60s ./build/aarchvm -bin tests/arm64/out/debug_mdscr_dcc_flags.bin -load 0x0 -entry 0x0 -steps 800000`
- `timeout 5400s ./tests/arm64/run_all.sh`
- `timeout 5400s ./tests/linux/run_functional_suite.sh`
- `timeout 5400s ./tests/linux/run_functional_suite_smp.sh`

## 当前结论

- `MDSCR_EL1` 和 DCC/EDSCR 的这一小块不再各管各的，`MDCCSR_EL0`、`DBGDTRTX_EL0`、`MRS/MSR MDSCR_EL1` 在 `TXfull/RXfull` 上现在是统一的 guest 可见状态。

# 修改日志 2026-03-29 21:04

## 本轮修改

- 继续沿着“异常 / 系统寄存器 / trap 语义收尾”审计 `debug / DCC` 这条线，补上此前确实缺失的三类 self-hosted debug sysreg 最小语义：
  - [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp)
  - [include/aarchvm/cpu.hpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/include/aarchvm/cpu.hpp)
  - [src/soc.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/soc.cpp)
- 新增 `MDCCINT_EL1` 最小实现：
  - 读写现在不再错误落成 `UNDEFINED`；
  - 仅保留架构定义的 `RX/TX` 使能位，其他位 `RES0`；
  - 由于这是新增 CPU 快照状态，snapshot 版本从 `19` 升到 `20`，并兼容旧版 `19` 快照加载时把该寄存器复位为 `0`。
- 新增 `OSDTRRX_EL1 / OSDTRTX_EL1` 最小实现：
  - 它们现在作为 `DTRRX/DTRTX` 的 side-effect-free save/restore 视图存在；
  - `MRS/MSR OSDTRRX_EL1` 只读写 `DTRRX` 的低 32 位，不改变 `RXfull`；
  - `MRS/MSR OSDTRTX_EL1` 只读写 `DTRTX` 的低 32 位，不改变 `TXfull`；
  - EL0 下对 `MDCCINT_EL1`、`OSDTRRX_EL1`、`OSDTRTX_EL1` 的访问现明确为 `UNDEFINED`，而不是落成 system access trap。
- 新增正式裸机回归：
  - [tests/arm64/debug_dcc_sysregs_minimal.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/debug_dcc_sysregs_minimal.S)
  - [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh)
  - [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)
  - 覆盖 `MDCCINT_EL1` 的 `RES0`-masked `RW`、`OSDTRRX_EL1 / OSDTRTX_EL1` 的 side-effect-free 行为、以及这些 sysreg 在 EL0 下的 `UNDEFINED` 边界。
- 更新：
  - [TODO.md](/media/luzeyu/Storage2/FOSS_src/aarchvm/TODO.md)

## 本轮测试

- `timeout 300s ./tests/arm64/build_tests.sh`
- `timeout 60s env AARCHVM_BRK_MODE=halt ./build/aarchvm -bin tests/arm64/out/debug_dcc_sysregs_minimal.bin -load 0x0 -entry 0x0 -steps 800000`
- `timeout 5400s ./tests/arm64/run_all.sh`
- `timeout 5400s ./tests/linux/run_functional_suite.sh`
- `timeout 5400s ./tests/linux/run_functional_suite_smp.sh`

## 当前结论

- `DCC` 这组当前模型已声明存在的 debug sysreg 又收掉了一块真实缺口：`MDCCINT_EL1`、`OSDTRRX_EL1`、`OSDTRTX_EL1` 现在至少在程序可见层面不再是错误的 `UNDEFINED`。
- 这轮之后，`debug/system register` 的剩余工作更集中在“是否还存在别的已声明 present 但未实现、或 trap/undef/no-op 边界仍不对的 self-hosted debug sysreg”这条线上。

# 修改日志 2026-03-29 17:26

## 本轮修改

- 继续按“检查并完善 `SMP` 同步原语与 barrier / `MMU-TLB-fault` 一致性 / `FP-AdvSIMD` 细节 / 差分验证与 Linux 压力回归”这四条线补强验证基础设施，这一轮没有改模拟器执行逻辑，重点是把原先覆盖不足的高风险路径变成正式、可回归、可断言的测试：
  - [tests/arm64/fp_ah_absent_ignored.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/fp_ah_absent_ignored.S)
  - [tests/arm64/smp_lse_ldaddal_counter.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/smp_lse_ldaddal_counter.S)
  - [tests/linux/pthread_sync_stress.c](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/linux/pthread_sync_stress.c)
  - [tests/linux/mprotect_exec_stress.c](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/linux/mprotect_exec_stress.c)
  - [tests/linux/run_qemu_user_diff.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/linux/run_qemu_user_diff.sh)
- 补上两条新的裸机回归：
  - `fp_ah_absent_ignored`：验证当前 `ID_AA64ISAR1_EL1=0`、`!FEAT_AFP` 模型下 `FPCR.AH` 被忽略，覆盖 `FRECPE/FRECPS/FRSQRTE/FRSQRTS/FMAX` 的结果和 `FPSR`。
  - `smp_lse_ldaddal_counter`：验证 2 核 `LDADDAL` 原子累加在 `LDAR/STLR + SEV/WFE` 同步下的程序可见结果。
- 把一组此前“只运行、不校验输出”的高价值裸机用例升级为正式强断言：
  - `MMU/TLB/fault`：`mmu_tlb_cache`、`mmu_ttbr1_early`、`mmu_tlb_vae1_scope`、`mmu_ttbr_switch`、`mmu_unmap_data_abort`、`mmu_tlbi_non_target`、`mmu_l2_block_vmalle1`、`mmu_at_tlb_observe`、`mmu_at_el0_permissions`、`mmu_ttbr_asid_mask`、`mmu_perm_ro_write_abort`、`mmu_xn_fetch_abort`
  - `FP/AdvSIMD`：`fp_ah_absent_ignored`、`fpsimd_arith_shift_perm`、`fpsimd_fp_vector`、`fpsimd_more_perm_fp`、`fpsimd_structured_ls`、`fpsimd_widen_sat`、`cpacr_fp_*`、`pstate_pan`
  - `SMP`：`smp_mpidr_boot`、`smp_sev_wfe`、`smp_ldxr_invalidate`、`smp_spinlock_ldaxr_stlxr`、`smp_lse_ldaddal_counter`、`smp_tlbi_broadcast`、`smp_wfe_*`、`smp_gic_sgi`、`smp_timer_*`、`smp_dc_zva_invalidate`
- 补强 Linux 用户态压力回归：
  - [tests/linux/build_usertests_rootfs.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/linux/build_usertests_rootfs.sh)
  - [tests/linux/run_functional_suite.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/linux/run_functional_suite.sh)
  - [tests/linux/run_functional_suite_smp.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/linux/run_functional_suite_smp.sh)
  - 新增并接入 `mprotect_exec_stress` 与 `pthread_sync_stress`
  - `UMP/SMP` 功能回归现在都会显式检查 `EXECVE-OK` / `MPROTECT-EXEC PASS`，`SMP` 额外检查 `PTHREAD-SYNC PASS`
  - `SMP` 回归补上 `DMESG-STRESS` 输出块的可打印字符检查，防止刷屏乱码回归
- 修复了一处回归基础设施缺陷：
  - [tests/linux/run_functional_suite_smp.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/linux/run_functional_suite_smp.sh)
  - 之前 `SMP` 功能回归只按 `build_linux_smp_shell_snapshot.sh` 的时间戳决定是否重建快照，导致 `build_usertests_rootfs.sh` 或 `build_linux_shell_snapshot.sh` 更新后仍可能继续使用旧快照。
  - 现在它会在 `aarchvm`、`build_usertests_rootfs.sh`、`build_linux_shell_snapshot.sh`、`build_linux_smp_shell_snapshot.sh` 任一更新时自动重建 `SMP` 快照。
- 新增 host 侧差分验证入口：
  - [tests/linux/run_qemu_user_diff.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/linux/run_qemu_user_diff.sh)
  - 用 `qemu-aarch64` 固化 `fpsimd_selftest`、`fpint_selftest`、`mprotect_exec_stress`、`pthread_sync_stress` 的输出检查，给 `FP/AdvSIMD` 与 Linux 用户态同步/权限切换语义提供一条轻量差分路径。
- 更新：
  - [TODO.md](/media/luzeyu/Storage2/FOSS_src/aarchvm/TODO.md)

## 本轮测试

- `timeout 600s cmake --build build -j`
- `timeout 300s ./tests/arm64/build_tests.sh`
- `timeout 600s ./tests/linux/build_usertests_rootfs.sh`
- `timeout 60s ./build/aarchvm -bin tests/arm64/out/fp_ah_absent_ignored.bin -load 0x0 -entry 0x0 -steps 400000`
- `timeout 60s ./build/aarchvm -smp 2 -bin tests/arm64/out/smp_lse_ldaddal_counter.bin -load 0x0 -entry 0x0 -steps 600000`
- `timeout 60s qemu-aarch64 ./out/mprotect_exec_stress`
- `timeout 60s qemu-aarch64 ./out/pthread_sync_stress`
- `timeout 300s ./tests/linux/run_qemu_user_diff.sh`
- `timeout 5400s ./tests/linux/run_functional_suite.sh`
- `timeout 5400s ./tests/linux/run_functional_suite_smp.sh`
- `timeout 5400s ./tests/arm64/run_all.sh`

## 当前结论

- 这轮新增的 `FP/SMP/MMU/Linux` 强断言回归全部通过，当前没有被新测试直接打出的模拟器执行语义缺口。
- 现阶段 `DMB/DSB/ISB` 虽仍是保守实现，但结合新增的 `LDADDAL`、`pthread_sync_stress`、`mprotect_exec_stress`、`TLBI`、`dmesg` 压力回归，至少当前模型下最容易出现程序可见错误的几条路径都已有正式覆盖。
- 这轮真正修到的行为问题不在 CPU，而在测试基础设施：`SMP` 快照重建条件此前不完整，已经修正。

# 修改日志 2026-03-24 21:27

## 本轮修改

- 继续沿着 pair-exclusive / `CASP` 这条线收口程序可见 fault 语义，这轮没有继续改模拟器执行逻辑，而是修正并补强了刚加上的裸机回归，使其真正验证架构定义而不是被测试自身误伤：
  - [tests/arm64/pair_atomic_more.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/pair_atomic_more.S)
  - [tests/arm64/pair_atomic_fault_no_partial.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/pair_atomic_fault_no_partial.S)
  - [TODO.md](/media/luzeyu/Storage2/FOSS_src/aarchvm/TODO.md)
- 修正了 `pair_atomic_more` 里的两个测试假设错误：
  - `CASP` misaligned fault 的 `WnR` 断言此前写成了 `1`，但按 ARM ARM 对 atomic read-modify-write fault 的定义，这类“read 也会先触发同一 fault”的场景应报告 `WnR=0`；
  - `CASP` fault 后要检查的 `x2/x3/x8/x9` 之前被异常处理器自己当 scratch 寄存器踩掉，现已在 handler 中显式保存/恢复。
- 重写了 [tests/arm64/pair_atomic_fault_no_partial.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/pair_atomic_fault_no_partial.S) 的 fault 构造方式：
  - 原来试图做“跨页 pair store 第二半 fault”；
  - 但对 `LDXP/STXP/CASP` 这类 pair access 来说，访问必须按 pair 总大小自然对齐，配合 4KiB 页粒度时，真正可达的跨页 pair fault 场景并不存在；
  - 现改为先在 `RW` 页表上下文执行 `LDXP` 建立 monitor，再无访存切换到 `RO` 页表上下文，用 `TTBR0_EL1 + TLBI` 触发 `STXP/CASP` 写权限 fault；
  - 新测试确认：
    - `STXP` fault 时内存不更新；
    - `STXP` 的 status 寄存器不被写回；
    - `CASP` fault 时内存不更新；
    - `FAR_EL1` 指向 fault address，`DFSC=permission fault level 3`，`WnR=1`。
- 同时修正了新测试自身的异常处理器寄存器保存问题，避免 `x6` 被 handler 覆盖后误判成 guest 行为错误。

## 本轮测试

- `timeout 300s tests/arm64/build_tests.sh`
- `timeout 60s bash -lc 'AARCHVM_BRK_MODE=halt ./build/aarchvm -bin tests/arm64/out/pair_atomic_more.bin -load 0x0 -entry 0x0 -steps 1200000 > out/pair_atomic_more.log 2>&1'`
- `timeout 60s bash -lc 'AARCHVM_BRK_MODE=halt ./build/aarchvm -bin tests/arm64/out/pair_atomic_fault_no_partial.bin -load 0x0 -entry 0x0 -steps 6000000 > out/pair_atomic_fault_no_partial.log 2>&1'`
- `timeout 5400s bash -lc 'tests/arm64/run_all.sh > out/arm64_run_all_20260324_pair.log 2>&1'`
- `timeout 3600s bash -lc 'tests/linux/run_functional_suite.sh > out/linux_functional_suite_20260324_pair.log 2>&1'`
- `timeout 3600s bash -lc 'tests/linux/run_functional_suite_smp.sh > out/linux_functional_suite_smp_20260324_pair.log 2>&1'`

## 当前结论

- 这轮确认此前新增的 pair-exclusive / `CASP` 执行逻辑没有被新回归推翻，真正的问题在于测试假设和 handler 自身寄存器破坏。
- 当前正式回归已经覆盖：
  - `32-bit pair exclusive` 成功路径；
  - `LDXP/CASP` pair 对齐 fault；
  - `STXP/CASP` 在合法对齐下遇到写权限 fault 时的“无部分提交 / 正确 syndrome / status 不写回”行为。
- 本轮完成后：
  - `tests/arm64/run_all.sh` 通过；
  - Linux UMP 功能回归通过；
  - Linux SMP 功能回归通过。

# 修改日志 2026-03-24 16:42

## 本轮修改

- 继续审 `trap / undef / no-op` 边界，这轮没有发现新的模拟器执行行为 bug，但把一组仍未正式覆盖的 hint-space absent-feature 语义并入已有回归：
  - [tests/arm64/hint_feature_absent_nop.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/hint_feature_absent_nop.S)
  - 补充覆盖：
    - `ESB`
    - `TSB CSYNC`
    - `GCSB DSYNC`
    - `STSHH KEEP`
    - `STSHH STRM`
- 这使当前 `hint_feature_absent_nop` 正式回归可一次性覆盖：
  - `DGH`
  - `ESB`
  - `TSB CSYNC`
  - `GCSB DSYNC`
  - `CLRBHB`
  - `BTI c`
  - `BTI jc`
  - `CHKFEAT X16`
  - `STSHH KEEP`
  - `STSHH STRM`
- 验证点保持不变：
  - EL1 / EL0 下都不取异常；
  - `CHKFEAT X16` 在 `!FEAT_CHK` 下保持输入值不变；
  - 这些 hint-space 指令不会改写其它通用寄存器。
- 更新：
  - [TODO.md](/media/luzeyu/Storage2/FOSS_src/aarchvm/TODO.md)

## 本轮测试

- `timeout 300s tests/arm64/build_tests.sh`
- `timeout 60s ./build/aarchvm -bin tests/arm64/out/hint_feature_absent_nop.bin -load 0x0 -entry 0x0 -steps 800000`
- `timeout 5400s bash -lc 'tests/arm64/run_all.sh > out/arm64_run_all_20260324_hint_feature_absent_nop_round2.log 2>&1'`
- `timeout 5400s bash -lc 'AARCHVM_FUNCTIONAL_LOG=out/linux_functional_20260324_hint_round2_ump.log tests/linux/run_functional_suite.sh'`
- `timeout 5400s bash -lc 'tests/linux/run_functional_suite_smp.sh'`

## 当前结论

- 当前模型下一批依赖 hint-space 默认 `NOP` 的 optional 指令已经有正式回归兜底，不再只依赖“代码看起来会落到 default case”。
- 继续审下来，下一处更值得投入的程序可见缺口，已经更偏向：
  - `DMB/DSB/ISB` 与 SMP 可见内存序；
  - `SEV/WFE/WFI` 与跨核唤醒/事件寄存器传播；
  - `TLBI / IC IVAU / TTBR-SCTLR-TCR` 切换在多核下的传播边界。

# 修改日志 2026-03-24 16:31

## 本轮修改

- 继续审 `trap / undef / no-op` 边界时，修正了 `!FEAT_FlagM` 下 integer-encoding `RMIF/SETF8/SETF16` 的真实语义缺口：
  - [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp)
  - 旧实现里 `RMIF` 会落到 catch-all `UNDEFINED`，但 `SETF8/SETF16` 会被 generic integer decode 误吞；
  - 现已在 `exec_data_processing()` 开头把 `RMIF` 与 `SETF8/SETF16` 显式按 `UNDEFINED` 收口，避免 future decode 调整再次别名到其它整数指令。
- 修正了新加 `FlagM` 回归自身的两个问题，使它真正验证 faulting instruction 的程序可见行为，而不是被测试脚本自身的寄存器/标签错误误伤：
  - [tests/arm64/flagm_integer_undef.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/flagm_integer_undef.S)
- 新增一条 hint-space absent-feature 正式回归，覆盖当前模型下应执行为 `NOP` 的几类指令：
  - [tests/arm64/hint_feature_absent_nop.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/hint_feature_absent_nop.S)
  - [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh)
  - [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)
  - 覆盖：
    - `DGH`
    - `CLRBHB`
    - `BTI c`
    - `BTI jc`
    - `CHKFEAT X16`
  - 验证点：
    - EL1 / EL0 两种执行路径都不取异常；
    - `CHKFEAT X16` 在 `!FEAT_CHK` 下保持输入值不变；
    - 这些 hint-space 指令不会偷偷改写其它通用寄存器。
- 更新：
  - [TODO.md](/media/luzeyu/Storage2/FOSS_src/aarchvm/TODO.md)

## 本轮测试

- `timeout 600s cmake --build build -j`
- `timeout 300s tests/arm64/build_tests.sh`
- `timeout 60s ./build/aarchvm -bin tests/arm64/out/flagm_integer_undef.bin -load 0x0 -entry 0x0 -steps 800000`
- `timeout 5400s bash -lc 'tests/arm64/run_all.sh > out/arm64_run_all_20260324_flagm_fix.log 2>&1'`
- `timeout 5400s bash -lc 'AARCHVM_FUNCTIONAL_LOG=out/linux_functional_20260324_flagm_fix_ump.log tests/linux/run_functional_suite.sh'`
- `timeout 5400s bash -lc 'tests/linux/run_functional_suite_smp.sh'`
- `timeout 60s ./build/aarchvm -bin tests/arm64/out/hint_feature_absent_nop.bin -load 0x0 -entry 0x0 -steps 800000`
- `timeout 5400s bash -lc 'tests/arm64/run_all.sh > out/arm64_run_all_20260324_hint_feature_absent_nop.log 2>&1'`

## 当前结论

- `RMIF/SETF8/SETF16` 在当前 `!FEAT_FlagM` 模型下已稳定表现为 `UNDEFINED`，不再依赖 catch-all 路径，也不会再被 generic integer family 误吞。
- hint-space `DGH/CLRBHB/BTI/CHKFEAT` 在当前 feature 声明下已由正式裸机回归覆盖，确认它们稳定表现为 `NOP`。
- 这轮完成后：
  - `tests/arm64/run_all.sh` 通过；
  - Linux UMP 功能回归通过；
  - Linux SMP 功能回归通过。

# 修改日志 2026-03-24 16:10

## 本轮修改

- 继续审 `FlagM/FlagM2` 相关 absent-feature 边界，并把 `RMIF/SETF8/SETF16` 固化进正式回归：
  - [tests/arm64/flagm_integer_undef.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/flagm_integer_undef.S)
  - [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh)
  - [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)
  - [TODO.md](/media/luzeyu/Storage2/FOSS_src/aarchvm/TODO.md)
- 这轮先做了临时 probe，确认当前实现里 `RMIF/SETF8/SETF16` 虽然没有专门 helper，但会自然落到未实现路径，表现为 `UNDEFINED`，没有被误执行成别的整数指令。
- 随后删除了临时 probe，并新增正式回归，覆盖：
  - `RMIF`
  - `SETF8`
  - `SETF16`
  - EL1 / EL0 两种执行路径
  - `ESR_EL1/FAR_EL1` 的当前未定义指令异常形态

## 本轮测试

- `timeout 300s tests/arm64/build_tests.sh`
- `timeout 60s ./build/aarchvm -bin tests/arm64/out/flagm_integer_undef.bin -load 0x0 -entry 0x0 -steps 800000`
- `timeout 5400s bash -lc 'tests/arm64/run_all.sh > out/arm64_run_all_20260324_flagm_integer_undef.log 2>&1'`
- `timeout 5400s bash -lc 'AARCHVM_FUNCTIONAL_LOG=out/linux_functional_20260324_flagm_integer_undef_ump.log tests/linux/run_functional_suite.sh'`
- `timeout 5400s bash -lc 'tests/linux/run_functional_suite_smp.sh'`

## 当前结论

- `FlagM` 相关的 system-encoding 和 integer-encoding absent-feature 边界现在都已有正式回归覆盖：
  - `CFINV/AXFLAG/XAFLAG`
  - `RMIF/SETF8/SETF16`

# 修改日志 2026-03-24 16:02

## 本轮修改

- 继续收 `PAuth_LR` 返回类 absent-feature 边界，并补上上一轮还没正式覆盖的 immediate-return 形式：
  - [tests/arm64/pauth_lr_return_imm_absent_undef.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/pauth_lr_return_imm_absent_undef.S)
  - [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh)
  - [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)
  - [TODO.md](/media/luzeyu/Storage2/FOSS_src/aarchvm/TODO.md)
- 这轮使用 `llvm-mc` 求出了当前 binutils 还不认助记符的两条 opcode，并固化为 `.inst`：
  - `RETAASPPC` -> `0x5500001f`
  - `RETABSPPC` -> `0x5520003f`
- 新用例验证当前 `!FEAT_PAuth_LR` 模型下：
  - `RETAASPPC/RETABSPPC` 应取 `UNDEFINED`
  - 不应被通用分支或返回路径误当成其它指令执行

## 本轮测试

- `timeout 300s tests/arm64/build_tests.sh`
- `timeout 60s ./build/aarchvm -bin tests/arm64/out/pauth_lr_return_imm_absent_undef.bin -load 0x0 -entry 0x0 -steps 800000`
- `timeout 5400s bash -lc 'tests/arm64/run_all.sh > out/arm64_run_all_20260324_pauth_lr_return_imm_absent_undef.log 2>&1'`
- `timeout 5400s bash -lc 'AARCHVM_FUNCTIONAL_LOG=out/linux_functional_20260324_pauth_lr_return_imm_absent_undef_ump.log tests/linux/run_functional_suite.sh'`
- `timeout 5400s bash -lc 'tests/linux/run_functional_suite_smp.sh'`

## 当前结论

- `PAuth_LR` 返回类的 immediate / register 两种 absent-feature 形式现在都已有正式回归覆盖。
- 这块继续收窄了“主循环 fast path 改动后悄悄把高级返回类编码误吞”的风险。

# 修改日志 2026-03-24 15:54

## 本轮修改

- 在上一轮修正 `LDRAA/LDRAB` 真 bug 后，继续审 `trap / undef / no-op` 的 `PAuth` 分支/返回边界，并把之前只在 `agent_work/` 中验证过的 probe 固化成正式回归：
  - [tests/arm64/pauth_branch_absent_undef.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/pauth_branch_absent_undef.S)
  - [tests/arm64/pauth_lr_return_absent_undef.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/pauth_lr_return_absent_undef.S)
  - [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh)
  - [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)
  - [TODO.md](/media/luzeyu/Storage2/FOSS_src/aarchvm/TODO.md)
- 这轮没有再改模拟器执行语义，重点是把已确认正确的边界变成长期覆盖：
  - `BRAA/BRAAZ/BRAB/BRABZ`
  - `BLRAA/BLRAAZ/BLRAB/BLRABZ`
  - `RETAA/RETAB`
  - `ERETAA/ERETAB`
  - `RETAASPPCR/RETABSPPCR`
- 这些测试共同验证：
  - 当前 `!FEAT_PAuth` / `!FEAT_PAuth_LR` 模型下，它们都应走 `UNDEFINED`
  - 不应被 generic `BR/BLR/RET/ERET` 路径误执行
  - ESR/FAR 维持当前未定义指令异常的程序可见形态

## 本轮测试

- `timeout 300s tests/arm64/build_tests.sh`
- `timeout 60s ./build/aarchvm -bin tests/arm64/out/pauth_branch_absent_undef.bin -load 0x0 -entry 0x0 -steps 1200000`
- `timeout 60s ./build/aarchvm -bin tests/arm64/out/pauth_lr_return_absent_undef.bin -load 0x0 -entry 0x0 -steps 800000`
- `timeout 5400s bash -lc 'tests/arm64/run_all.sh > out/arm64_run_all_20260324_pauth_branch_absent_undef.log 2>&1'`
- `timeout 5400s bash -lc 'AARCHVM_FUNCTIONAL_LOG=out/linux_functional_20260324_pauth_branch_absent_undef_ump.log tests/linux/run_functional_suite.sh'`
- `timeout 5400s bash -lc 'tests/linux/run_functional_suite_smp.sh'`

## 当前结论

- 当前主循环的 `RET/BR/BLR/ERET` fast path 没有把这批 `PAuth` 分支/返回编码误吞。
- 这块现在已经有正式回归覆盖，后续继续审别的 absent-feature 语义时，不容易把这批边界带坏而不自知。

# 修改日志 2026-03-24 15:42

## 本轮修改

- 继续按“审阅 -> 修复 -> 测试”流程推进“Armv8-A 程序可见正确性收尾计划”，这轮收的是 `LDRAA/LDRAB` 在 `!FEAT_PAuth` 下的程序可见语义：
  - [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp)
  - [tests/arm64/ldraa_ldrab_absent_undef.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/ldraa_ldrab_absent_undef.S)
  - [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh)
  - [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)
  - [TODO.md](/media/luzeyu/Storage2/FOSS_src/aarchvm/TODO.md)
- 重新对照 ARM ARM 后确认：
  - `LDRAA/LDRAB` 明确要求在 `!FEAT_PAuth` 时 `Decode_UNDEF`；
  - 它们不是 hint-space `NOP`，也不能退化成普通 `LDR`。
- 旧实现里，这组编码同时撞上了 generic X load/store post/pre-index 掩码：
  - 慢路径 `exec_load_store()` 会把它们执行成普通 `LDR/STR` 家族；
  - 预解码路径也会把它们缓存成 generic `LoadStore`，因此即使开着 predecode 也会稳定误执行。
- 这轮把 `LDRAA/LDRAB` 的编码识别提取成通用 helper，并在两处同时收口：
  - 预解码阶段不再把它们缓存成 generic load/store；
  - 慢路径显式按 `UNDEFINED` 处理，保证当前 `ID_AA64ISAR*` 声明的 `FEAT_PAuth absent` 与行为一致。
- 新增 [tests/arm64/ldraa_ldrab_absent_undef.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/ldraa_ldrab_absent_undef.S)，覆盖：
  - `LDRAA` / `LDRAB`
  - offset / pre-index 两种 addressing mode
  - pre-index 形式在 `UNDEFINED` 时不得偷偷执行访存或写回 base register

## 本轮测试

- `timeout 1200s cmake --build build -j`
- `timeout 300s tests/arm64/build_tests.sh`
- `timeout 60s ./build/aarchvm -bin tests/arm64/out/ldraa_ldrab_absent_undef.bin -load 0x0 -entry 0x0 -steps 800000`
- `timeout 60s env AARCHVM_DEBUG_SLOW=1 ./build/aarchvm -bin tests/arm64/out/ldraa_ldrab_absent_undef.bin -load 0x0 -entry 0x0 -steps 800000`
- `timeout 5400s bash -lc 'tests/arm64/run_all.sh > out/arm64_run_all_20260324_ldraa_ldrab_absent_undef.log 2>&1'`
- `timeout 5400s bash -lc 'AARCHVM_FUNCTIONAL_LOG=out/linux_functional_20260324_ldraa_ldrab_absent_undef_ump.log tests/linux/run_functional_suite.sh'`
- `timeout 5400s bash -lc 'tests/linux/run_functional_suite_smp.sh'`

## 当前结论

- `LDRAA/LDRAB` 在当前 `!FEAT_PAuth` 模型下已与 ARM ARM 对齐为 `UNDEFINED`。
- 这组编码不再被 predecode 或慢路径误当成 generic X load/store post/pre-index 指令执行。

# 修改日志 2026-03-24 15:25

## 本轮修改

- 继续按“审阅 -> 修复 -> 测试”流程推进“Armv8-A 程序可见正确性收尾计划”，这轮继续收 `trap / undef / no-op` 的 `PAuth_LR` 边界，并纠正了上一轮遗留的一个错误结论：
  - [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp)
  - [tests/arm64/pacm_absent_nop.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/pacm_absent_nop.S)
  - [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh)
  - [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)
  - [TODO.md](/media/luzeyu/Storage2/FOSS_src/aarchvm/TODO.md)
- 重新对照 ARM ARM 后确认：
  - `PACM` 虽然属于 `FEAT_PAuth_LR`，但它位于 architectural hint space；
  - 在 `!FEAT_PAuth_LR` 时，它的 architected 语义是 `Decode_NOP`，不是 `UNDEFINED`。
- 旧实现把 [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp) 中的 `PACM` 当成 `UNDEFINED`，因此 guest 会错误收到 `EC=0x00` 的同步异常。
- 这轮已把 `PACM` 修正为 absent-feature `NOP`，不再在 EL1/EL0 下错误取未定义指令异常。
- 原来的 [tests/arm64/pacm_undef.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/pacm_undef.S) 结论也因此被证实是错的；本轮将其更正为 [tests/arm64/pacm_absent_nop.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/pacm_absent_nop.S)，覆盖：
  - EL1 下执行 `PACM` 不取异常；
  - EL0 下执行 `PACM` 不取异常；
  - EL0 后续通过 `SVC` 返回 EL1，验证 `PACM` 本身没有偷偷走进 `UNDEFINED` 路径。

## 本轮测试

- `timeout 1200s cmake --build build -j`
- `timeout 300s tests/arm64/build_tests.sh`
- `timeout 60s ./build/aarchvm -bin tests/arm64/out/pacm_absent_nop.bin -load 0x0 -entry 0x0 -steps 600000`
- `timeout 5400s bash -lc 'tests/arm64/run_all.sh > out/arm64_run_all_20260324_pacm_absent_nop.log 2>&1'`
- `timeout 5400s bash -lc 'AARCHVM_FUNCTIONAL_LOG=out/linux_functional_20260324_pacm_absent_nop_ump.log tests/linux/run_functional_suite.sh'`
- `timeout 5400s bash -lc 'tests/linux/run_functional_suite_smp.sh'`

## 当前结论

- `PACM` 在当前 `!FEAT_PAuth_LR` 模型下已经与 ARM ARM 对齐为 `NOP`。
- 这也说明此前把 `PACM` 归类到“absent-feature 仍应 `UNDEFINED`”的判断是错误的；当前 `PAuth` / `PAuth_LR` 相关边界应区分：
  - hint-space `PAuth` / `PACM`: absent 时 `NOP`
  - integer / direct `PAuth` / `PAuth_LR`: absent 时 `UNDEFINED`

# 修改日志 2026-03-24 15:10

## 本轮修改

- 继续按“审阅 -> 修复 -> 测试”流程推进“Armv8-A 程序可见正确性收尾计划”，这轮收的是 `FEAT_PAuth` absent 时一批已分配 `PAuth` 指令的程序可见语义：
  - [include/aarchvm/system_registers.hpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/include/aarchvm/system_registers.hpp)
  - [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp)
  - [tests/arm64/pauth_absent_nop.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/pauth_absent_nop.S)
  - [tests/arm64/pauth_absent_integer_undef.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/pauth_absent_integer_undef.S)
  - [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh)
  - [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)
- 结合 ARM ARM 与当前 `ID_AA64ISAR1_EL1` / `ID_AA64ISAR2_EL1` 的声明复查后确认，当前模型对外声明 `FEAT_PAuth` absent。
- 上一轮曾误把 direct / integer `PAuth` encodings 也放宽成 `NOP`；重新对照 ARM ARM 后确认这条结论是错的：
  - hint-space `PAuth` encodings 在 `!FEAT_PAuth` 时为 `NOP`
  - integer / direct `PAuth` encodings 在 `!FEAT_PAuth` 时为 `UNDEFINED`
- 这轮据此修正了 [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp) 中的语义边界：
  - 保留 hint-space `XPACLRI`、`PACIA1716/PACIB1716/AUTIA1716/AUTIB1716`、`PACIASP/PACIBSP/AUTIASP/AUTIBSP`、`PACIAZ/PACIBZ/AUTIAZ/AUTIBZ` 的 absent-feature `NOP` 行为；
  - 删除对 integer / direct `XPACI/XPACD/PACIA/PACIB/PACDA/PACDB/AUTIA/AUTIB/AUTDA/AUTDB/...` 的错误 `NOP` 放宽，改为 `UNDEFINED`。
- 同时修复了一个更隐蔽的 decode bug：
  - 一批 integer / direct `PAuth` 编码会误撞到通用 integer `Data-processing (1 source)` 解码分支，被当成 `RBIT/REV16/REV/CLZ/CLS` 之类指令吞掉；
  - 这轮在 generic integer decode 之前显式拦截该家族，确保当前 `!FEAT_PAuth` 模型下稳定走 `UNDEFINED`，不再 alias 成其它整数指令。
- 测试侧相应调整为两组裸机单测：
  - [tests/arm64/pauth_absent_nop.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/pauth_absent_nop.S)：只覆盖 hint-space `PAuth` absent -> `NOP`
  - [tests/arm64/pauth_absent_integer_undef.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/pauth_absent_integer_undef.S)：覆盖 integer / direct `PAuth` absent -> `UNDEFINED`

## 本轮测试

- `timeout 300s tests/arm64/build_tests.sh`
- `timeout 60s ./build/aarchvm -bin tests/arm64/out/pacm_undef.bin -load 0x0 -entry 0x0 -steps 600000`
- `timeout 60s ./build/aarchvm -bin tests/arm64/out/pauth_absent_nop.bin -load 0x0 -entry 0x0 -steps 800000`
- `timeout 60s ./build/aarchvm -bin tests/arm64/out/pauth_absent_integer_undef.bin -load 0x0 -entry 0x0 -steps 1200000`
- `timeout 5400s bash -lc 'tests/arm64/run_all.sh > out/arm64_run_all_20260324_pauth_integer_undef.log 2>&1'`
- `timeout 5400s bash -lc 'AARCHVM_FUNCTIONAL_LOG=out/linux_functional_20260324_pauth_integer_undef_ump.log tests/linux/run_functional_suite.sh'`
- `timeout 5400s bash -lc 'tests/linux/run_functional_suite_smp.sh'`

## 当前结论

- 当前 `ID_AA64ISAR*` 所声明的 `FEAT_PAuth absent` 已与两类编码空间的程序可见行为重新对齐：
  - hint-space `PAuth` 保持 `NOP`
  - integer / direct `PAuth` 稳定 `UNDEFINED`
- 一批原本会 alias 到通用 integer decode 的 integer `PAuth` 编码也已经被拦下，不再悄悄执行成其它数据处理指令。

# 修改日志 2026-03-23 11:18

## 本轮修改

- 继续收口了 `SIMD&FP whole-register memory` 的跨页 fault 语义：
  - [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp) 中 `load_vec_whole()` / `store_vec_whole()` 在子访问失败时会显式沿用 `last_data_fault_va_`，把 faulting byte 一直带到最终 `data_abort()`。
  - 这样普通 `LDR/STR D/Q` 以及复用这两个 helper 的相关整寄存器 FP/SIMD 访存路径，在跨页、尤其是未对齐跨页场景下，都能稳定把 `FAR_EL1` 指向真实 fault byte。
- 新增了针对上述边界的裸机单测：
  - [tests/arm64/mmu_fpsimd_whole_fault_far.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/mmu_fpsimd_whole_fault_far.S)
  - 覆盖 `LDR D` / `STR D` / `LDR Q` / `STR Q` 在跨页 translation fault 下的 `FAR_EL1` 与 `WnR`。
- 更新了测试接线：
  - [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh)
  - [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)

## 本轮测试

- 定向构建：
  - `timeout 300s tests/arm64/build_tests.sh`
  - `timeout 1200s cmake --build build -j`
- 定向语义验证：
  - `timeout 120s bash -lc 'AARCHVM_BRK_MODE=halt ./build/aarchvm -bin tests/arm64/out/mmu_fpsimd_whole_fault_far.bin -load 0x0 -entry 0x0 -steps 4000000 2>/dev/null | tr -d "\r\n"'`
  - `timeout 120s bash -lc 'AARCHVM_BRK_MODE=halt ./build/aarchvm -bin tests/arm64/out/mmu_fpsimd_structured_fault_far.bin -load 0x0 -entry 0x0 -steps 4000000 2>/dev/null | tr -d "\r\n"'`
  - `timeout 120s bash -lc 'AARCHVM_BRK_MODE=halt ./build/aarchvm -bin tests/arm64/out/mmu_fpsimd_lane_fault_far.bin -load 0x0 -entry 0x0 -steps 4000000 2>/dev/null | tr -d "\r\n"'`
  - `timeout 120s bash -lc 'AARCHVM_BRK_MODE=halt ./build/aarchvm -bin tests/arm64/out/fpsimd_structured_ls_regpost.bin -load 0x0 -entry 0x0 -steps 900000 2>/dev/null | tr -d "\r\n"'`
- 裸机完整回归：
  - `timeout 5400s bash -lc 'tests/arm64/run_all.sh > out/arm64_run_all_20260323_fpsimd_whole_far.log 2>&1'`
- Linux 单核功能回归：
  - `timeout 5400s bash -lc 'tests/linux/run_functional_suite.sh > out/linux_functional_20260323_fpsimd_whole_far_ump.log 2>&1'`
- Linux SMP 功能回归：
  - `timeout 5400s bash -lc 'tests/linux/run_functional_suite_smp.sh > out/linux_functional_20260323_fpsimd_whole_far_smp.log 2>&1'`

## 当前结论

- 目前已经有裸机单测分别覆盖：
  - 普通标量 load/store 的 faulting byte / FAR。
  - 普通 FP/SIMD whole-register `LDR/STR` 的 faulting byte / FAR。
  - AdvSIMD structured whole-register load/store 的 faulting byte / FAR。
  - AdvSIMD single-structure lane/replicate load/store 的 faulting byte / FAR。
- 这一组 MMU/fault 边界收口之后，下一步仍值得优先继续审的方向是：
  - 其它 trap / undef / no-op 边界里尚未单独锁死的 debug/system 指令族。
  - 浮点 / AdvSIMD 其它已实现路径在 `NaN / FPCR / FPSR` 传播上的一致性。

# 修改日志 2026-03-22 18:08

## 本轮修改

- 继续按“审阅 -> 修复 -> 测试”流程推进“Armv8-A 程序可见正确性收尾计划”，这轮收的是 whole-register structured `AdvSIMD` load/store 缺失 register post-index 形式的问题：
  - [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp)
  - [tests/arm64/fpsimd_structured_ls_regpost.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/fpsimd_structured_ls_regpost.S)
  - [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh)
  - [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)
  - [TODO.md](/media/luzeyu/Storage2/FOSS_src/aarchvm/TODO.md)
- 审阅现有 structured load/store 实现并结合工具链编码确认后，旧实现只覆盖了：
  - no-offset 形式 `[Xn|SP]`
  - `Rm == 31` 的 immediate post-index 形式
- 但 whole-register structured `LD1/ST1/LD2/ST2/LD3/ST3/LD4/ST4` 还缺 `Rm != 31` 的 register post-index 形式 `[Xn|SP], Xm`，这属于当前已声明 `FEAT_AdvSIMD` 下应支持的 A64 基本编码。
- 这轮在 [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp) 做了两件事：
  - 新增两个小 helper，分别统一处理 sequential whole-register post-index 与 interleaved whole-register post-index；
  - 把原来只识别 immediate post-index 的分支扩展成同时支持 `#imm` 和 `Xm`，其中：
    - `Rm == 31` 仍按架构隐含立即数写回；
    - `Rm != 31` 时写回偏移取 `X[m]`，不做额外缩放。
- 在继续复查 `CPACR/CPTR` trap 语义时，又发现 [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp) 的 `insn_uses_fp_asimd()` 仍只按旧的 immediate post-index 掩码识别 whole-register structured `AdvSIMD` load/store，导致新补的 `[Xn|SP], Xm` 形式在禁用 `FP/AdvSIMD` 时会漏掉 `EC=0x07` trap 判定。
- 为此，这轮进一步把 `insn_uses_fp_asimd()` 中相关 post-index 判定掩码同步更新为与执行路径一致的 `#imm|Xm` 识别方式，保证 `CPACR/CPTR` trap 与真实执行解码覆盖同一批编码。
- 同时新增裸机单测 [tests/arm64/fpsimd_structured_ls_regpost.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/fpsimd_structured_ls_regpost.S)，覆盖：
  - `LD1/ST1` 1/2/3/4 寄存器 whole-register register post-index
  - `LD2/ST2` whole-register register post-index
  - `LD3/ST3` whole-register register post-index
  - `LD4/ST4` whole-register register post-index
  - 并显式校验 load/store 数据结果与 `Xm` 写回。
- 另外新增 [tests/arm64/cpacr_fp_structured_regpost_trap.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/cpacr_fp_structured_regpost_trap.S)，专门验证：
  - `whole-register structured LD1 ... [Xn], Xm` 在 EL1 与 EL0 被 `CPACR_EL1` 禁用时会先触发 `FP/AdvSIMD access trap`；
  - trap 不会错误执行写回；
  - 在重新启用后，指令与 register post-index 写回都能正常完成。
- 继续复查 MMU / fault 边界时，又发现 whole-register structured `AdvSIMD` load/store 的 byte-wise helper 只返回成功/失败，不回传实际 faulting byte，因此 [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp) 在 `LD1/ST1/LD2/ST2/LD3/ST3/LD4/ST4` 发生跨页 translation/data abort 时，会把 `FAR_EL1` 错报成起始地址。
- 这轮把 sequential/interleaved whole-register helper 统一改成显式回传 `fault_addr`，并同步修正 no-offset 与 post-index 两条执行路径，让 `data_abort()` 使用真实 faulting byte 地址。
- 同时新增 [tests/arm64/mmu_fpsimd_structured_fault_far.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/mmu_fpsimd_structured_fault_far.S)，覆盖：
  - sequential `LD1` 跨页 fault 的 `FAR_EL1`
  - sequential `ST1 [Xn], Xm` 跨页 fault 的 `FAR_EL1` 与 fault 时禁止写回
  - interleaved `LD2` 跨页 fault 的 `FAR_EL1`
  - interleaved `ST2 [Xn], Xm` 跨页 fault 的 `FAR_EL1` 与 fault 时禁止写回
- 在同一片路径继续往下审时，又确认 single-structure lane/replicate `AdvSIMD` load/store 仍然按整元素宽度直接调用 `mmu_read/mmu_write`，因此对 `H/S/D` 多字节元素跨页 fault 只会把 `FAR_EL1` 报成元素起始地址。
- 这轮进一步把 [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp) 中 `access_vec_single_struct()` 改成：
  - 先按元素宽度保留原有 alignment fault 检查；
  - 真正访问时改为按字节走 `raw_mmu_read/raw_mmu_write`，从而把 `fault_addr` 精确推进到 faulting byte。
- 另外新增 [tests/arm64/mmu_fpsimd_lane_fault_far.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/mmu_fpsimd_lane_fault_far.S)，覆盖：
  - `LD1 {Vt.H}[lane]` 跨页 fault 的 `FAR_EL1`
  - `LD1R {Vt.4H}` 跨页 fault 的 `FAR_EL1`
  - `ST1 {Vt.S}[lane], [Xn], Xm` 跨页 fault 的 `FAR_EL1` 与 fault 时禁止写回

## 本轮测试

- `timeout 1200s cmake --build build -j`
- `timeout 300s tests/arm64/build_tests.sh`
- `timeout 120s bash -lc 'AARCHVM_BRK_MODE=halt ./build/aarchvm -bin tests/arm64/out/fpsimd_structured_ls_regpost.bin -load 0x0 -entry 0x0 -steps 400000 2>/dev/null | tr -d "\\r\\n"'`
- `timeout 120s bash -lc 'AARCHVM_BRK_MODE=halt ./build/aarchvm -bin tests/arm64/out/cpacr_fp_structured_regpost_trap.bin -load 0x0 -entry 0x0 -steps 400000 2>/dev/null | tr -d "\\r\\n"'`
- `timeout 120s bash -lc 'AARCHVM_BRK_MODE=halt ./build/aarchvm -bin tests/arm64/out/mmu_fpsimd_structured_fault_far.bin -load 0x0 -entry 0x0 -steps 4000000 2>/dev/null | tr -d "\\r\\n"'`
- `timeout 120s bash -lc 'AARCHVM_BRK_MODE=halt ./build/aarchvm -bin tests/arm64/out/mmu_fpsimd_lane_fault_far.bin -load 0x0 -entry 0x0 -steps 4000000 2>/dev/null | tr -d "\\r\\n"'`
- `timeout 5400s tests/arm64/run_all.sh`
- `timeout 5400s tests/linux/run_functional_suite.sh`
- `timeout 5400s tests/linux/run_functional_suite_smp.sh`

# 修改日志 2026-03-22 17:38

## 本轮修改

- 继续按“审阅 -> 修复 -> 测试”流程推进“Armv8-A 程序可见正确性收尾计划”，这轮收的是 `FP/AdvSIMD` memory 指令在 `Rn==SP` 时缺失 `CheckSPAlignment()` 的程序可见行为：
  - [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp)
  - [tests/arm64/fpsimd_sp_alignment_fault.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/fpsimd_sp_alignment_fault.S)
  - [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh)
  - [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)
  - [TODO.md](/media/luzeyu/Storage2/FOSS_src/aarchvm/TODO.md)
- 审阅 ARM ARM 与现有 `src/cpu.cpp` 后确认，当前 `SIMD&FP` / `AdvSIMD` memory subset 的多个执行分支会直接读取 `sp_or_reg(rn)` 并继续访存，但手册要求在 `Rn==SP` 时先执行 `CheckSPAlignment()`。
- 旧实现因此会让以下路径绕过本应优先发生的 `SP alignment fault`：
  - 普通 `SIMD&FP` 整寄存器 `LDR/STR`
  - AdvSIMD single-structure lane / replicate
  - whole-register structured `LD1/ST1/LD2/LD3/LD4` / `ST1/ST2/ST3/ST4`
- 这轮在对应的实际访存入口前统一补上了 `maybe_take_sp_alignment_fault(rn)`，从根上修正 fault 优先级，确保：
  - `SP` misaligned 时先产生 `EC=0x26` 的同步异常；
  - fault 发生后不会继续访问内存；
  - post-index 形式也不会错误执行写回。
- 同时新增裸机单测 [tests/arm64/fpsimd_sp_alignment_fault.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/fpsimd_sp_alignment_fault.S)，覆盖三条不同子路径：
  - `ldr q0, [sp]`
  - `ld1 {v1.b}[0], [sp]`
  - `st1 {v2.16b}, [sp], #16`
- 该测试显式校验：
  - `ESR_EL1.EC == 0x26`
  - `ISS == 0`
  - `FAR_EL1 == 0`
  - `ELR_EL1` 指向 faulting instruction
  - post-index store fault 后 `SP` 不写回、目标内存不被修改

## 本轮测试

- `timeout 1200s cmake --build build -j`
- `timeout 300s tests/arm64/build_tests.sh`
- `timeout 120s bash -lc 'AARCHVM_BRK_MODE=halt ./build/aarchvm -bin tests/arm64/out/fpsimd_sp_alignment_fault.bin -load 0x0 -entry 0x0 -steps 200000 2>/dev/null | tr -d "\\r\\n"'`
- `timeout 5400s tests/arm64/run_all.sh`
- `timeout 5400s tests/linux/run_functional_suite.sh`
- `timeout 5400s tests/linux/run_functional_suite_smp.sh`

# 修改日志 2026-03-22 21:25

## 本轮修改

- 继续按“审阅 -> 修复 -> 测试”流程推进“Armv8-A 程序可见正确性收尾计划”，这轮在上一处 `SCTLR_EL1` 固定位收口通过后，继续收口同类的 `SPSR_EL1` 程序可见状态位一致性问题：
  - [src/system_registers.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/system_registers.cpp)
  - [tests/arm64/spsr_el1_res0_bits.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/spsr_el1_res0_bits.S)
  - [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh)
  - [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)
  - [TODO.md](/media/luzeyu/Storage2/FOSS_src/aarchvm/TODO.md)
- 审阅 [src/system_registers.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/system_registers.cpp) 后确认，旧实现会把 guest 写入 `SPSR_EL1` 的值原样保存。
- 但当前模型只支持 AArch64 guest，并且当前 ID/feature 声明下 `UAO/DIT/TCO/SSBS/BTI/NMI/EBEP/SEBEP/GCS/PAuth_LR/UINJ` 都 absent，因此对应的 `SPSR_EL1` 位在当前模型下应表现为 `RES0`，而不是可写可读的影子状态。
- 这轮给 `SPSR_EL1` 增加了 sanitize：
  - 保留当前模型真正支持或允许保留的 AArch64 位：`NZCV`、`PAN`、`SS`、`IL`、`DAIF`、`M[4:0]`；
  - 清掉当前模型下 architecturally `RES0` 或 feature-absent 的位，包括 `UAO/DIT/TCO/SSBS/BTYPE/ALLINT/PM/PPEND/EXLOCK/PACM/UINJ` 以及 AArch64 视图中的保留位；
  - 该规则同时覆盖普通 `MSR SPSR_EL1, Xt` 写路径和快照恢复路径。
- 同时新增裸机单测 [tests/arm64/spsr_el1_res0_bits.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/spsr_el1_res0_bits.S)，覆盖：
  - guest 向 `SPSR_EL1` 写入一组当前模型应为 `RES0`/absent 的位后，再次读回时这些位保持为 0；
  - `NZCV/PAN/SS/IL/DAIF/M[4:0]` 这组当前模型允许保留的 AArch64 位仍可正常写回。

## 本轮测试

- `timeout 1200s cmake --build build -j`
- `timeout 300s tests/arm64/build_tests.sh`
- `timeout 120s bash -lc 'AARCHVM_BRK_MODE=halt ./build/aarchvm -bin tests/arm64/out/spsr_el1_res0_bits.bin -load 0x0 -entry 0x0 -steps 200000 2>/dev/null | tr -d "\\r\\n"'`
- `timeout 5400s tests/arm64/run_all.sh`
- `timeout 5400s tests/linux/run_functional_suite.sh`
- `timeout 5400s tests/linux/run_functional_suite_smp.sh`

# 修改日志 2026-03-22 18:05

## 本轮修改

- 继续按“审阅 -> 修复 -> 测试”流程推进“Armv8-A 程序可见正确性收尾计划”，这轮收的是 `ID_AA64MMFR0_EL1` 与当前 MMU granule 实现不一致的问题：
  - [include/aarchvm/system_registers.hpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/include/aarchvm/system_registers.hpp)
  - [src/system_registers.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/system_registers.cpp)
  - [tests/arm64/id_aa64_feature_regs.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/id_aa64_feature_regs.S)
  - [TODO.md](/media/luzeyu/Storage2/FOSS_src/aarchvm/TODO.md)
- 审阅 `walk_page_tables()` 后确认当前模型只支持 `4KB granule`，`TG0==00` / `TG1==10` 之外都会被当作 fault 处理。
- 旧的 `ID_AA64MMFR0_EL1=0x000000000F000005` 却把 `TGran16=0b0000` 对外宣称为“16KB granule supported”，这会让 guest 能力探测看到错误的内存系统能力声明。
- 这轮把 `ID_AA64MMFR0_EL1` 改为 `0x000000000FF00005`，使其程序可见语义与当前实现一致：
  - `TGran4=0b0000`，支持 4KB granule；
  - `TGran16=0b1111`，不支持 16KB granule；
  - `TGran64=0b1111`，不支持 64KB granule；
  - 其他当前字段保持不变。
- 同时把裸机测试 [tests/arm64/id_aa64_feature_regs.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/id_aa64_feature_regs.S) 从“比较整寄存器原始常量”改成“按字段断言 `PARange/ASIDBits/TGran4/TGran16/TGran64`”，让它直接覆盖这次修复的程序可见行为。
- 继续顺着 `ID_AA64MMFR0_EL1` 审阅后，又发现另一处程序可见矛盾：
  - 当前模型通过 `BigEnd=0` / `BigEndEL0=0` 声明 mixed-endian absent；
  - 但旧实现会把 guest 写入的 `SCTLR_EL1.EE/E0E` 原样保存在寄存器里，导致 guest 仍能把这两个本应 fixed 的位读回成 1。
- 这轮进一步在 [src/system_registers.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/system_registers.cpp) 对 `SCTLR_EL1` 做了基于 `ID_AA64MMFR0_EL1` 的 sanitize：
  - `BigEnd=0` 时，`SCTLR_EL1.EE` 固定清零；
  - `BigEnd=0` 且 `BigEndEL0=0` 时，`SCTLR_EL1.E0E` 也固定清零；
  - 该规则同时覆盖普通 `MSR SCTLR_EL1, Xt` 路径和快照恢复路径，避免旧快照带回不可能的 endianness 状态。
- 同时新增裸机单测 [tests/arm64/sctlr_endian_fixed_bits.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/sctlr_endian_fixed_bits.S)，覆盖：
  - `ID_AA64MMFR0_EL1.BigEnd/BigEndEL0` 都为 0；
  - guest 尝试把 `SCTLR_EL1.EE/E0E` 写成 1 后再次读回时，这两个位仍保持 0；
  - 其他非 fixed 位，例如 `SCTLR_EL1.UCI`，在 sanitize 后仍可正常写回。
- 另外在 [TODO.md](/media/luzeyu/Storage2/FOSS_src/aarchvm/TODO.md) 的当前进展里补记了这一项 ID/实现一致性收口。

## 本轮测试

- `timeout 1200s cmake --build build -j`
- `timeout 300s tests/arm64/build_tests.sh`
- `timeout 120s bash -lc 'AARCHVM_BRK_MODE=halt ./build/aarchvm -bin tests/arm64/out/id_aa64_feature_regs.bin -load 0x0 -entry 0x0 -steps 200000 2>/dev/null | tr -d "\\r\\n"'`
- `timeout 5400s tests/arm64/run_all.sh`
- `timeout 5400s tests/linux/run_functional_suite.sh`
- `timeout 5400s tests/linux/run_functional_suite_smp.sh`

# 修改日志 2026-03-22 01:42

## 本轮修改

- 继续按“审阅 -> 修复/补强 -> 测试”流程推进“Armv8-A 程序可见正确性收尾计划”，这轮没有改模拟器执行语义，重点是把 TODO 与当前代码状态重新对齐，并把一个“已声明存在但此前没有独立裸机覆盖”的指令族补进回归：
  - [TODO.md](/media/luzeyu/Storage2/FOSS_src/aarchvm/TODO.md)
  - [tests/arm64/crc32_family.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/crc32_family.S)
  - [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh)
  - [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)
- 对照当前代码状态更新了 [TODO.md](/media/luzeyu/Storage2/FOSS_src/aarchvm/TODO.md)：
  - 在“异常 / 系统寄存器 / trap 语义收尾”下补记了近期已经收口的小项，包括：
  - `ID_AA64PFR0_EL1.GIC` 与 `ICC_*` system register 一致性；
  - debug sysreg 资源数量与 `ID_AA64DFR0_EL1` 声明一致性；
  - `AT S1E1R/W` 与 `PSTATE.PAN` / `FEAT_PAN2` 的边界；
  - 一组 absent feature system-encoding / sysreg 在 EL0 下应为 `UNDEFINED` 的边界。
- 结合当前 `ID_AA64ISAR0_EL1=0x0000000000210000` 的对外声明，确认当前模型明确宣称支持：
  - `FEAT_LSE` / `Atomic`
  - `FEAT_CRC32`
- 其中 `Atomic` 相关路径之前已经有较多裸机覆盖，而 `CRC32` 族此前缺少独立回归，因此这轮新增了 `crc32_family` 裸机单测，覆盖：
  - `CRC32B/H/W/X`
  - `CRC32CB/CH/CW/CX`
  - 结果寄存器按 `Wd` 语义写回后的零扩展可见行为
- 在补这个测试时，先用新用例复核了一次实现与期望值：
  - 模拟器里的 `CRC32*` / `CRC32C*` 实现本身是正确的；
  - 最初写入测试的 `CRC32*` 期望值误用了宿主侧 `zlib.crc32` 风格结果，和 Arm 指令的逐字节 reflected 更新算法不同；
  - 因此这轮修正的是测试期望值，而不是模拟器执行逻辑。

## 本轮测试

- `timeout 1200s cmake --build build -j`
- `timeout 300s tests/arm64/build_tests.sh`
- `timeout 120s bash -lc 'AARCHVM_BRK_MODE=halt ./build/aarchvm -bin tests/arm64/out/crc32_family.bin -load 0x0 -entry 0x0 -steps 400000 2>/dev/null | tr -d "\\r\\n"'`
- `timeout 5400s bash -lc 'tests/arm64/run_all.sh > out/arm64_run_all_20260322_crc32.log 2>&1'`
- `timeout 5400s bash -lc 'tests/linux/run_functional_suite.sh > out/linux_functional_20260322_crc32_ump.log 2>&1'`
- `timeout 5400s bash -lc 'tests/linux/run_functional_suite_smp.sh > out/linux_functional_20260322_crc32_smp.log 2>&1'`
- 结果：通过。

# 修改日志 2026-03-22 03:07

## 本轮修改

- 继续按“审阅 -> 修复 -> 测试”流程推进“Armv8-A 程序可见正确性收尾计划”，这轮收的是 `AT S1E1R/W` 与 `PSTATE.PAN` 的程序可见行为缺口：
  - [include/aarchvm/cpu.hpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/include/aarchvm/cpu.hpp)
  - [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp)
  - [include/aarchvm/system_registers.hpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/include/aarchvm/system_registers.hpp)
  - [tests/arm64/mmu_at_pan_ignore.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/mmu_at_pan_ignore.S)
  - [tests/arm64/at_pan2_absent_undef.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/at_pan2_absent_undef.S)
  - [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh)
  - [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)
- 审阅 ARM ARM 后确认：
  - 当前模型通过 `ID_AA64MMFR1_EL1.PAN = 0b0001` 声明实现了 `FEAT_PAN`，但没有声明 `FEAT_PAN2`；
  - 在这种配置下，普通的 `AT S1E1R` / `AT S1E1W` 不应受 `PSTATE.PAN` 影响；
  - 只有实现了 `FEAT_PAN2` 时，`AT S1E1RP` / `AT S1E1WP` 这类 PAN-sensitive 变体才存在并把 `PSTATE.PAN` 纳入权限判断。
- 旧实现里，`AT S1E1R/W` 直接复用了普通 privileged data access 的权限检查路径，因此在 `PAN=1` 且目标页 `EL0-accessible` 时会错误地产生 permission fault，并把错误结果写进 `PAR_EL1`。
- 这轮修改做了两件事：
  - 给 `translate_address()` / `walk_page_tables()` / `access_permitted()` 显式增加 `apply_pan` 开关，让地址翻译类指令可以按架构要求选择是否参与 PAN 权限裁决；
  - 把 `AT S1E1R` / `AT S1E1W` 改为在当前模型下使用 `apply_pan=false`，从而与“`FEAT_PAN2` absent”这一对外声明保持一致。
- 另外，继续顺着这条线检查后又发现另一个边界缺口：
  - `AT S1E1RP` / `AT S1E1WP` 在 `FEAT_PAN2` absent 时本身就应该是 `UNDEFINED`；
  - 旧实现里它们在 EL1 下会落入通用未实现指令异常，但在 EL0 下会先被 generic system-access 控制逻辑误分类成 `EC=0x18` system access trap。
- 因此这轮又把 `AT S1E1RP/WP` 的编码显式收口为 `UNDEFINED`，避免 EL0/EL1 行为分叉。
- 同时补了两组裸机单测：
  - `mmu_at_pan_ignore`，覆盖：
  - `PAN=1` 时 `AT S1E1R` 对 EL0 RW 页仍成功；
  - `PAN=1` 时 `AT S1E1W` 对 EL0 RW 页仍成功；
  - 返回的 `PAR_EL1` PA 字段与页表映射一致。
  - `at_pan2_absent_undef`，覆盖：
  - EL1 执行 `AT S1E1RP` / `AT S1E1WP` 得到 `UNDEFINED`；
  - EL0 执行 `AT S1E1RP` / `AT S1E1WP` 同样得到 `UNDEFINED`，而不是 system access trap；
  - 统一校验 `ESR_EL1.EC=0`、`IL=1`、`ISS=0`、`FAR_EL1=0`，并校验 `ELR_EL1` 指向 faulting instruction。
- 另外顺手把 [include/aarchvm/system_registers.hpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/include/aarchvm/system_registers.hpp) 里的 `ID_AA64MMFR0_EL1` / `ID_AA64MMFR1_EL1` 头文件默认值对齐到 reset 后的真实值，避免“头文件初值”和运行时 reset 值不一致。

## 本轮测试

- `timeout 1200s cmake --build build -j > out/build_20260322_1.log 2>&1`
- `timeout 300s tests/arm64/build_tests.sh`
- `timeout 120s bash -lc 'AARCHVM_BRK_MODE=halt ./build/aarchvm -bin tests/arm64/out/mmu_at_pan_ignore.bin -load 0x0 -entry 0x0 -steps 400000 | tr -d "\\r\\n"'`
- `timeout 120s bash -lc 'AARCHVM_BRK_MODE=halt ./build/aarchvm -bin tests/arm64/out/at_pan2_absent_undef.bin -load 0x0 -entry 0x0 -steps 600000 | tr -d "\\r\\n"'`
- `timeout 5400s bash -lc 'tests/arm64/run_all.sh > out/arm64_run_all_20260322_2.log 2>&1'`
- `timeout 5400s bash -lc 'tests/linux/run_functional_suite.sh > out/linux_functional_20260322_ump_2.log 2>&1'`
- `timeout 5400s bash -lc 'tests/linux/run_functional_suite_smp.sh > out/linux_functional_20260322_smp_2.log 2>&1'`
- 结果：通过。

# 修改日志 2026-03-22 02:44

## 本轮修改

- 继续按“审阅 -> 修复 -> 测试”流程推进“Armv8-A 程序可见正确性收尾计划”，这轮收的是 AArch64 self-hosted debug 资源数量与 debug sysreg 可见性不一致的问题：
  - [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp)
  - [src/system_registers.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/system_registers.cpp)
  - [include/aarchvm/system_registers.hpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/include/aarchvm/system_registers.hpp)
  - [tests/arm64/debug_sysreg_resource_bounds.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/debug_sysreg_resource_bounds.S)
  - [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh)
  - [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)
- 审阅 ARM ARM 与现有实现后确认：
  - 当前模型通过 `ID_AA64DFR0_EL1` 声明仅实现 2 个 breakpoint 与 2 个 watchpoint 资源；
  - 但旧实现对 `DBGBVR<n>_EL1` / `DBGBCR<n>_EL1` / `DBGWVR<n>_EL1` / `DBGWCR<n>_EL1` 的 system register 访问在 `n < 16` 时一律放行；
  - 这会让 guest 观察到“ID 只宣称 2 个资源，但第 3~16 个资源 sysreg 依然存在且可访问”的矛盾状态。
- 这轮修改做了两件事：
  - 在 `SystemRegisters` 中按 `ID_AA64DFR0_EL1` / `ID_AA64DFR1_EL1` 计算实际实现的 breakpoint/watchpoint 资源数量，并据此限制 debug value/control sysreg 的读写范围；
  - 在 `Cpu::exec_system()` 的 `sysreg_present()` 路径里同步收紧 debug sysreg 的“存在性”，确保未实现的 `DBGB*2+` / `DBGW*2+` 在 EL1 与 EL0 下都作为“不存在的系统寄存器”处理，而不是先走成“存在但无权限”的 EL0 system access trap。
- 同时新增裸机单测 `debug_sysreg_resource_bounds`，覆盖：
  - EL1 对已实现的 `DBGBVR1_EL1` / `DBGBCR1_EL1` / `DBGWVR1_EL1` / `DBGWCR1_EL1` 读写仍可见；
  - EL1 访问未实现的 `DBGBVR2_EL1` / `DBGWCR2_EL1` 时得到 `UNDEFINED`；
  - EL0 访问已实现但特权的 `DBGBVR1_EL1` 时仍得到 `EC=0x18` system access trap；
  - EL0 访问未实现的 `DBGBVR2_EL1` / `DBGWCR2_EL1` 时得到 `UNDEFINED`，而不是被误当成特权 trap。

## 本轮测试

- `timeout 1200s cmake --build build -j`
- `timeout 300s tests/arm64/build_tests.sh`
- `timeout 120s ./build/aarchvm -bin tests/arm64/out/debug_sysreg_resource_bounds.bin -load 0x0 -entry 0x0 -steps 800000`
- `timeout 5400s bash -lc 'tests/arm64/run_all.sh > out/arm64_run_all_20260322_debug_bounds.log 2>&1'`
- `timeout 5400s bash -lc 'tests/linux/run_functional_suite.sh > out/linux_functional_20260322_debug_bounds_ump.log 2>&1'`
- `timeout 5400s bash -lc 'tests/linux/run_functional_suite_smp.sh > out/linux_functional_20260322_debug_bounds_smp.log 2>&1'`
- 结果：通过。

# 修改日志 2026-03-22 00:32

## 本轮修改

- 继续按“审阅 -> 修复 -> 测试”流程推进“Armv8-A 程序可见正确性收尾计划”，这轮收的是 `ID_AA64PFR0_EL1` 的 GIC 声明与当前 GICv3 system register 接口实现不一致的问题：
  - [src/system_registers.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/system_registers.cpp)
  - [include/aarchvm/system_registers.hpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/include/aarchvm/system_registers.hpp)
  - [tests/arm64/id_aa64_feature_regs.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/id_aa64_feature_regs.S)
  - [tests/arm64/gic_sysreg_id_consistency.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/gic_sysreg_id_consistency.S)
  - [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh)
  - [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)
- 审阅 ARM ARM 和现有实现后确认：
  - 当前模型已经实现并暴露了 `ICC_PMR_EL1`、`ICC_SRE_EL1`、`ICC_IAR1_EL1`、`ICC_EOIR1_EL1`、`ICC_DIR_EL1` 等 GIC CPU interface system registers；
  - 但 `ID_AA64PFR0_EL1.GIC` 之前仍返回 `0b0000`，这会让 guest 观察到“GIC system register interface absent，但 ICC_* sysreg 却可直接访问”的矛盾状态。
- 这轮将 `ID_AA64PFR0_EL1` 调整为与当前实现一致的值 `0x0000000001000011`，即在保留现有 `EL0/EL1`、`FP/AdvSIMD` 声明不变的基础上，把 `GIC` 字段改为 `0b0001`。
- 同时：
  - 更新了现有 `id_aa64_feature_regs` 裸机测试，固定新的 `ID_AA64PFR0_EL1` 声明值；
  - 新增 `gic_sysreg_id_consistency` 裸机测试，直接校验 `ID_AA64PFR0_EL1.GIC == 1`，并验证 `ICC_PMR_EL1` / `ICC_SRE_EL1` 的最小读写可见性与该声明一致。

## 本轮测试

- `timeout 1200s cmake --build build -j`
- `timeout 300s tests/arm64/build_tests.sh`
- `timeout 120s bash -lc 'v=$(AARCHVM_BRK_MODE=halt ./build/aarchvm -bin tests/arm64/out/id_aa64_feature_regs.bin -load 0x0 -entry 0x0 -steps 200000 | tr -d "\\r\\n"); printf "[%s]\\n" "$v"; test "$v" = I'`
- `timeout 120s bash -lc 'v=$(AARCHVM_BRK_MODE=halt ./build/aarchvm -bin tests/arm64/out/gic_sysreg_id_consistency.bin -load 0x0 -entry 0x0 -steps 300000 | tr -d "\\r\\n"); printf "[%s]\\n" "$v"; test "$v" = J'`
- `timeout 5400s bash -lc 'tests/arm64/run_all.sh > out/arm64_run_all_20260322_gic_id.log 2>&1'`
- `timeout 5400s bash -lc 'tests/linux/run_functional_suite.sh > out/linux_functional_20260322_gic_id_ump.log 2>&1'`
- `timeout 5400s bash -lc 'tests/linux/run_functional_suite_smp.sh > out/linux_functional_20260322_gic_id_smp.log 2>&1'`
- 结果：通过。

# 修改日志 2026-03-20 20:55

## 本轮修改

- 继续按“审阅 -> 修复 -> 测试”流程推进“Armv8-A 程序可见正确性收尾计划”，这轮收的是一组 absent extension 的 system-encoding 指令在 EL0 下会被误分类为 `system access trap` 的问题：
  - [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp)
  - [tests/arm64/system_feature_absent_undef.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/system_feature_absent_undef.S)
  - [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh)
  - [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)
- 审阅 ARM ARM 并对照现有解码后确认，当前模型未声明也未实现以下特性，因此相应指令在任意异常级的程序可见行为都必须是 `UNDEFINED`：
  - `SB` (`FEAT_SB`)
  - `WFET` / `WFIT` (`FEAT_WFxT`)
  - `CFP RCTX` / `DVP RCTX` / `CPP RCTX` (`FEAT_SPECRES`)
  - `COSP RCTX` (`FEAT_SPECRES2`)
- 旧实现里，这些编码在 EL1 下通常会因为未命中已知 sysreg 写路径而最终落到未定义指令异常，但在 EL0 下会先被通用 `MSR sysreg` 访问控制逻辑拦住，错误地产生 `EC=0x18` 的 system access trap。
- 这轮在 `exec_system()` 中把这组 absent-feature system encodings 显式建模为 `UNDEFINED`，从根上避免 EL0 误 trap。
- 同时新增裸机单测 `system_feature_absent_undef`，覆盖：
  - EL1 与 EL0 执行 `SB`
  - EL1 与 EL0 执行 `WFET` / `WFIT`
  - EL1 与 EL0 执行 `CFP RCTX` / `DVP RCTX` / `COSP RCTX` / `CPP RCTX`
  - 统一校验 `ESR_EL1.EC=0`、`IL=1`、`ISS=0`、`FAR_EL1=0`，并校验 `ELR_EL1` 指向 faulting instruction。

## 本轮测试

- `timeout 1200s cmake --build build -j`
- `timeout 300s tests/arm64/build_tests.sh`
- `timeout 120s ./build/aarchvm -bin tests/arm64/out/system_feature_absent_undef.bin -load 0x0 -entry 0x0 -steps 1200000`
- `timeout 5400s bash -lc 'tests/arm64/run_all.sh > out/arm64_run_all_system_absent.log 2>&1'`
- `timeout 5400s bash -lc 'tests/linux/run_functional_suite.sh > out/linux_functional_system_absent_ump.log 2>&1'`
- `timeout 5400s bash -lc 'tests/linux/run_functional_suite_smp.sh > out/linux_functional_system_absent_smp.log 2>&1'`
- 结果：通过。

# 修改日志 2026-03-20 20:40

## 本轮修改

- 继续按“审阅 -> 修复 -> 测试”流程推进“Armv8-A 程序可见正确性收尾计划”，这轮收的是 `FlagM/FlagM2` 缺失时三条 system-encoding 指令的 EL0 可见语义：
  - [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp)
  - [tests/arm64/flagm_sys_undef.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/flagm_sys_undef.S)
  - [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh)
  - [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)
- 审阅 ARM ARM 并对照现有解码后确认：
  - `CFINV` 依赖 `FEAT_FlagM`；
  - `AXFLAG` / `XAFLAG` 依赖 `FEAT_FlagM2`；
  - 当前模型没有实现这些特性，也没有在 ID 寄存器中声明它们存在；
  - 因此 guest 在任意异常级执行这三条指令时，程序可见行为都必须是 `UNDEFINED`。
- 旧实现里，EL1 下这三条编码大多会因为“未命中已知 sysreg”最终落到通用未定义路径，但 EL0 下会先被通用 `MSR sysreg` 访问控制逻辑拦住，错误地产生 `EC=0x18` 的 system access trap。
- 这轮将 `CFINV/AXFLAG/XAFLAG` 在 `exec_system()` 中显式建模为当前模型下的 `UNDEFINED`，避免它们在 EL0 被误分类为系统寄存器访问 trap。
- 同时新增裸机单测 `flagm_sys_undef`，覆盖：
  - EL1 执行 `CFINV`、`AXFLAG`、`XAFLAG`；
  - EL0 执行 `CFINV`、`AXFLAG`、`XAFLAG`；
  - 统一校验 `ESR_EL1.EC=0`、`IL=1`、`ISS=0`、`FAR_EL1=0`，并校验 `ELR_EL1` 指向 faulting instruction。

## 本轮测试

- `timeout 1200s cmake --build build -j`
- `timeout 300s tests/arm64/build_tests.sh`
- `timeout 120s ./build/aarchvm -bin tests/arm64/out/flagm_sys_undef.bin -load 0x0 -entry 0x0 -steps 800000`
- `timeout 5400s bash -lc 'tests/arm64/run_all.sh > out/arm64_run_all_flagm.log 2>&1'`
- `timeout 5400s bash -lc 'tests/linux/run_functional_suite.sh > out/linux_functional_flagm_ump.log 2>&1'`
- `timeout 5400s bash -lc 'tests/linux/run_functional_suite_smp.sh > out/linux_functional_flagm_smp.log 2>&1'`
- 结果：通过。

# 修改日志 2026-03-20 20:26

## 本轮修改

- 继续按“审阅 -> 修复 -> 测试”流程推进“Armv8-A 程序可见正确性收尾计划”，这轮收的是 hint 空间里 `PACM` 的程序可见语义边界：
  - [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp)
  - [tests/arm64/pacm_undef.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/pacm_undef.S)
  - [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh)
  - [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)
- 审阅 ARM ARM 后确认，`PACM` 虽然编码位于 A64 hint 空间，但它不是“无条件可吞掉”的普通 hint：
  - `PACM` 依赖 `FEAT_PAuth_LR`；
  - 当前模型没有实现，也没有在 ID 寄存器中声明 `FEAT_PAuth_LR`；
  - 因此 guest 执行 `PACM` 时，程序可见行为必须是 `UNDEFINED`，而不是静默 no-op。
- 旧实现把整段 hint 空间里未专门处理的编码默认按 no-op 吞掉，这会把 `PACM` 错误地伪装成“可执行但无效果”。
- 这轮把 `PACM` 从通用 hint/no-op 路径中摘出来，在当前模型下显式走 `EC=0` 的同步未定义指令异常。
- 同时新增裸机单测 `pacm_undef`，覆盖：
  - EL1 执行 `PACM`；
  - EL0 执行 `PACM`；
  - 校验 `ESR_EL1.EC=0`、`IL=1`、`ISS=0`、`FAR_EL1=0`，并校验 `ELR_EL1` 指向 faulting instruction。

## 本轮测试

- `timeout 1200s cmake --build build -j`
- `timeout 300s tests/arm64/build_tests.sh`
- `timeout 120s ./build/aarchvm -bin tests/arm64/out/pacm_undef.bin -load 0x0 -entry 0x0 -steps 600000`
- `timeout 5400s bash -lc 'tests/arm64/run_all.sh > out/arm64_run_all_pacm.log 2>&1'`
- `timeout 5400s bash -lc 'tests/linux/run_functional_suite.sh > out/linux_functional_pacm_ump.log 2>&1'`
- `timeout 5400s bash -lc 'tests/linux/run_functional_suite_smp.sh > out/linux_functional_pacm_smp.log 2>&1'`
- 结果：通过。

# 修改日志 2026-03-20 20:10

## 本轮修改

- 继续按“审阅 -> 修复 -> 测试”流程推进“Armv8-A 程序可见正确性收尾计划”，这轮收的是 debug system register 可见行为与 `ID_AA64DFR0_EL1` 声明不一致的问题：
  - [src/system_registers.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/system_registers.cpp)
  - [include/aarchvm/system_registers.hpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/include/aarchvm/system_registers.hpp)
  - [tests/arm64/id_aa64_feature_regs.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/id_aa64_feature_regs.S)
- 审阅代码时发现，当前模型已经向 guest 暴露了一组最小 debug/OS-lock sysreg：
  - `MDSCR_EL1`
  - `OSDLR_EL1` / `OSLAR_EL1` / `OSLSR_EL1`
  - `DBGBVR<n>_EL1` / `DBGBCR<n>_EL1`
  - `DBGWVR<n>_EL1` / `DBGWCR<n>_EL1`
- 但 `ID_AA64DFR0_EL1` 之前仍返回全 0，这会让 guest 观察到“debug 架构 absent，但相关 sysreg 却可直接访问”的矛盾状态。
- 这轮将 `ID_AA64DFR0_EL1` 调整为与当前最小实现一致的声明值：
  - `DebugVer=0b0110`，即 Armv8.0 debug architecture；
  - `BRPs=0b0001`，即 2 个断点；
  - `WRPs=0b0001`，即 2 个观察点；
  - `CTX_CMPs=0b0000`，即 1 个 context-aware breakpoint；
  - 其它相关字段继续保持 0；
  - `ID_AA64DFR1_EL1` 保持 0，表示没有扩展数量字段。
- 同时扩充了现有的 `id_aa64_feature_regs` 裸机测试，把 `ID_AA64DFR0_EL1/ID_AA64DFR1_EL1` 的当前声明固定进回归。

## 本轮测试

- `timeout 1200s cmake --build build -j`
- `timeout 300s tests/arm64/build_tests.sh`
- `timeout 120s bash -lc 'v=$(./build/aarchvm -bin tests/arm64/out/id_aa64_feature_regs.bin -load 0x0 -entry 0x0 -steps 400000 | tr -d "\r\n"); printf "[%s]\n" "$v"; test "$v" = I'`
- `timeout 120s bash -lc 'v=$(AARCHVM_BRK_MODE=halt ./build/aarchvm -bin tests/arm64/out/instr_legacy_each.bin -load 0x0 -entry 0x0 -steps 3000000 | tr -d "\r\n"); printf "[%s]\n" "$v"; test "$v" = E'`
- `timeout 5400s bash -lc 'tests/arm64/run_all.sh > out/arm64_run_all_20260320_dfr0_debug.log 2>&1'`
- `timeout 5400s bash -lc 'tests/linux/run_functional_suite.sh > out/linux_functional_20260320_dfr0_debug.log 2>&1'`
- `timeout 5400s bash -lc 'tests/linux/run_functional_suite_smp.sh > out/linux_functional_smp_20260320_dfr0_debug.log 2>&1'`
- 结果：通过。

# 修改日志 2026-03-20 20:01

## 本轮修改

- 继续按“审阅 -> 修复 -> 测试”流程推进“Armv8-A 程序可见正确性收尾计划”，这轮收的是 `debug/system` 边界里 `DCPS<n>/DRPS` 的 Non-debug 可见语义：
  - [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp)
  - [tests/arm64/dcps_drps_non_debug_undef.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/dcps_drps_non_debug_undef.S)
  - [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh)
  - [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)
- 审阅 ARM ARM 后确认：
  - `DCPS1/DCPS2/DCPS3` 只在 Debug state 下有定义；当前模型未实现 Debug state，因此 guest 在 Non-debug state 执行时必须观察到 `UNDEFINED`。
  - `DRPS` 同样是 Debug-state-only 指令；在当前模型里对 guest 也必须表现为 `UNDEFINED`。
- 旧实现虽然大多数情况下会因为“未命中任何解码路径”最终落到通用 `UNDEFINED`，但这条语义并没有被显式建模，也没有单测固定。
- 这轮修改将这四个编码显式接入 system/exception 路径，统一走 `EC=0` 的同步未定义指令异常，并新增裸机单测覆盖：
  - `dcps_drps_non_debug_undef`：依次执行 `DCPS1/DCPS2/DCPS3/DRPS`，校验 `ESR_EL1.EC=0`、`IL=1`、`ISS=0`、`ELR_EL1` 指向 faulting instruction、`FAR_EL1=0`。

## 本轮测试

- `timeout 1200s cmake --build build -j`
- `timeout 300s tests/arm64/build_tests.sh`
- `timeout 120s bash -lc 'v=$(./build/aarchvm -bin tests/arm64/out/dcps_drps_non_debug_undef.bin -load 0x0 -entry 0x0 -steps 600000 | tr -d "\r\n"); printf "[%s]\n" "$v"; test "$v" = D'`
- `timeout 5400s bash -lc 'tests/arm64/run_all.sh > out/arm64_run_all_20260320_dcps_drps.log 2>&1'`
- `timeout 5400s bash -lc 'tests/linux/run_functional_suite.sh > out/linux_functional_20260320_dcps_drps.log 2>&1'`
- `timeout 5400s bash -lc 'tests/linux/run_functional_suite_smp.sh > out/linux_functional_smp_20260320_dcps_drps.log 2>&1'`
- 结果：通过。

# 修改日志 2026-03-20 19:48

## 本轮修改

- 继续按“审阅 -> 修复 -> 测试”流程推进“Armv8-A 程序可见正确性收尾计划”，这轮收的是 `trap / undef / no-op` 边界里 `BRK/HLT` 的程序可见语义：
  - [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp)
  - [tests/arm64/brk_exception.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/brk_exception.S)
  - [tests/arm64/hlt_undef.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/hlt_undef.S)
  - [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh)
  - [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)
  - [doc/README.en.md](/media/luzeyu/Storage2/FOSS_src/aarchvm/doc/README.en.md)
  - [doc/README.zh.md](/media/luzeyu/Storage2/FOSS_src/aarchvm/doc/README.zh.md)
- 审阅 ARM ARM 后，这轮明确并固定了两条边界：
  - `BRK #imm16` 默认不再直接停机，而是按架构要求进入 Breakpoint Instruction exception，`EC=0x3c`，`ISS[15:0]=imm16`，且返回地址保持在 `BRK` 指令本身；
  - `HLT #imm16` 在当前没有 Debug state / halting debug 支持的模型下，按 `UNDEFINED` 处理，而不是伪装成可执行 no-op 或其它异常。
- 为兼容现有大量把 `BRK` 当“测试结束指令”的裸机回归，这轮新增行为控制环境变量：
  - `AARCHVM_BRK_MODE=trap|halt`；
  - 默认值是 `trap`；
  - `tests/arm64/run_all.sh` 导出 `AARCHVM_BRK_MODE=halt`，保持历史 bare-metal 测试停机语义不变。
- 同时新增两条专门的裸机单测：
  - `brk_exception`：覆盖 EL1/EL0 `BRK`，校验 `ESR_EL1/ELR_EL1/FAR_EL1` 与 `imm16`；
  - `hlt_undef`：覆盖 EL1/EL0 `HLT`，校验其走 `UNDEFINED` 异常路径。

## 本轮测试

- `timeout 1200s cmake --build build -j`
- `timeout 300s tests/arm64/build_tests.sh`
- `timeout 120s bash -lc 'v=$(AARCHVM_BRK_MODE=trap ./build/aarchvm -bin tests/arm64/out/brk_exception.bin -load 0x0 -entry 0x0 -steps 600000 | tr -d "\r\n"); printf "[%s]\n" "$v"; test "$v" = B'`
- `timeout 120s bash -lc 'v=$(AARCHVM_BRK_MODE=trap ./build/aarchvm -bin tests/arm64/out/hlt_undef.bin -load 0x0 -entry 0x0 -steps 600000 | tr -d "\r\n"); printf "[%s]\n" "$v"; test "$v" = H'`
- `timeout 5400s bash -lc 'tests/arm64/run_all.sh > out/arm64_run_all_20260320_brk_hlt.log 2>&1'`
- `timeout 5400s bash -lc 'tests/linux/run_functional_suite.sh > out/linux_functional_20260320_brk_hlt.log 2>&1'`
- `timeout 5400s bash -lc 'tests/linux/run_functional_suite_smp.sh > out/linux_functional_smp_20260320_brk_hlt.log 2>&1'`
- 结果：通过。

# 修改日志 2026-03-20 18:29

## 本轮修改

- 继续按“审阅 -> 修复 -> 测试”流程推进“Armv8-A 程序可见正确性收尾计划”，这轮收的是 EL0 cache maintenance by-VA 的权限 fault 语义与覆盖缺口：
  - [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp)
  - [tests/arm64/mmu_ic_ivau_el0_perm_fault.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/mmu_ic_ivau_el0_perm_fault.S)
  - [tests/arm64/mmu_dc_cva_el0_perm_fault.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/mmu_dc_cva_el0_perm_fault.S)
  - [tests/arm64/mmu_dc_zva_el0_perm_fault.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/mmu_dc_zva_el0_perm_fault.S)
  - [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh)
  - [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)
- 根据 ARM ARM 与当前模型的特性边界，这轮明确了三类行为：
  - `IC IVAU` 在 `EL0 + SCTLR_EL1.UCI=1` 下，对 EL0 无读权限的 VA，当前模型选择报告 Permission fault；
  - `DC CVAC/CVAU/CIVAC` 在 `EL0 + SCTLR_EL1.UCI=1` 下，对 EL0 无读权限的 VA，当前模型同样报告 Permission fault；
  - `DC ZVA` 是 store-like 语义，权限 fault 仍走普通写 fault 语义，因此 `CM=0`、`WnR=1`。
- 这轮唯一的模拟器行为修复是：
  - `IC IVAU` 从原先不会对 EL0 无读权限地址 fault，改为走 `translate_cache_maintenance_address(..., true)`，从而在当前模型里向 guest 暴露一致的 Permission fault。
- 同时补了两条新的裸机单测，把当前行为固定进回归：
  - `mmu_dc_cva_el0_perm_fault`：覆盖 `DC CVAC/CVAU/CIVAC` 在 EL0 的 Permission fault，校验 `EC=0x24`、`CM=1`、`WnR=1`、`FSC=permission fault level 3`、`FAR_EL1/ELR_EL1`；
  - `mmu_dc_zva_el0_perm_fault`：覆盖 `DC ZVA` 在 EL0 对只读页的 Permission fault，校验 `EC=0x24`、`CM=0`、`WnR=1`、`FSC=permission fault level 3`、`FAR_EL1/ELR_EL1`。

## 本轮测试

- `timeout 1200s cmake --build build -j`
- `timeout 300s tests/arm64/build_tests.sh`
- `timeout 120s bash -lc "test \"$(./build/aarchvm -bin tests/arm64/out/mmu_ic_ivau_el0_perm_fault.bin -load 0x0 -entry 0x0 -steps 4000000 | tr -d '\r\n')\" = I"`
- `timeout 120s bash -lc "test \"$(./build/aarchvm -bin tests/arm64/out/mmu_dc_cva_el0_perm_fault.bin -load 0x0 -entry 0x0 -steps 4000000 | tr -d '\r\n')\" = C"`
- `timeout 120s bash -lc "test \"$(./build/aarchvm -bin tests/arm64/out/mmu_dc_zva_el0_perm_fault.bin -load 0x0 -entry 0x0 -steps 4000000 | tr -d '\r\n')\" = Z"`
- `timeout 3600s tests/arm64/run_all.sh`
- `timeout 3600s tests/linux/run_functional_suite.sh`
- `timeout 3600s tests/linux/run_functional_suite_smp.sh`
- 结果：通过。

# 修改日志 2026-03-20 18:04

## 本轮修改

- 继续按“审阅 -> 修复 -> 测试”流程推进“Armv8-A 程序可见正确性收尾计划”，这轮修的是 `DC ZVA` 的两处程序可见语义偏差：
  - [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp)
  - [tests/arm64/mmu_cache_maint_fault.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/mmu_cache_maint_fault.S)
  - [tests/arm64/mmu_dc_zva_fault.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/mmu_dc_zva_fault.S)
  - [tests/arm64/dc_zva_device_align_fault.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/dc_zva_device_align_fault.S)
  - [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh)
  - [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)
- 根据 ARM ARM，`DC ZVA` 虽然编码在 `DC` 指令组里，但程序可见行为并不是普通 cache maintenance fault：
  - 它“按一组 stores 处理”；
  - 发生同步 Data Abort / watchpoint 时，`ESR_ELx.ISS.CM` 必须为 `0`；
  - 对 Device memory 必须产生 Alignment fault，而不是把目标地址当普通可写内存继续写下去。
- 旧实现里有两处偏差：
  - `DC ZVA` fault 路径错误复用了 cache-maintenance syndrome 组装，导致 `CM=1`；
  - 在 `MMU off` 时，翻译结果把所有物理地址都默认为 `Normal` memory，导致 `DC ZVA` 会错误地对 MMIO/Device 区域执行实际写入。
- 这轮修复内容：
  - `DC ZVA` 的 translation/permission fault 现在走普通 store-like data abort 路径，保留 `WnR=1`，但不再错误置位 `CM`；
  - `DC ZVA` 命中 Device memory 时，改为报告 Alignment fault；
  - `MMU off` 时，翻译结果现在会基于总线是否为 RAM-backed 区域区分 `Normal` 和 `Device`，避免把 MMIO 当 RAM。
- 对应测试调整：
  - 从 `mmu_cache_maint_fault` 中移除了 `DC ZVA`，因为它不属于 `CM=1` 那一类 fault；
  - 新增 `mmu_dc_zva_fault`，覆盖未映射地址上的 `DC ZVA`，并显式校验 `CM=0`、`WnR=1`、translation fault；
  - 新增 `dc_zva_device_align_fault`，覆盖 `MMU off` 下对 UART Device memory 执行 `DC ZVA` 时必须抛出 Alignment fault。

## 本轮测试

- `timeout 1200s cmake --build build -j`
- `timeout 300s tests/arm64/build_tests.sh`
- `timeout 120s bash -lc "test \"$(./build/aarchvm -bin tests/arm64/out/mmu_dc_zva_fault.bin -load 0x0 -entry 0x0 -steps 4000000 | tr -d '\r\n')\" = Z"`
- `timeout 120s bash -lc "test \"$(./build/aarchvm -bin tests/arm64/out/dc_zva_device_align_fault.bin -load 0x0 -entry 0x0 -steps 400000 | tr -d '\r\n')\" = A"`
- `timeout 120s bash -lc "test \"$(./build/aarchvm -bin tests/arm64/out/mmu_cache_maint_fault.bin -load 0x0 -entry 0x0 -steps 4000000 | tr -d '\r\n')\" = M"`
- `timeout 3600s tests/arm64/run_all.sh`
- `timeout 3600s tests/linux/run_functional_suite.sh`
- `timeout 3600s tests/linux/run_functional_suite_smp.sh`
- 结果：通过。

# 修改日志 2026-03-20 17:46

## 本轮修改

- 继续按“审阅 -> 修复 -> 测试”流程推进“Armv8-A 程序可见正确性收尾计划”，这轮补的是 `DC IVAC` 的程序可见 fault 语义缺口：
  - [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp)
  - [tests/arm64/mmu_cache_maint_fault.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/mmu_cache_maint_fault.S)
  - [tests/arm64/mmu_dc_ivac_perm_fault.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/mmu_dc_ivac_perm_fault.S)
  - [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh)
  - [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)
- 审阅 ARM ARM 后确认，`DC IVAC` 与 `DC CVAC/CVAU/CIVAC` 不同：
  - `DC IVAC` 对目标 VA 需要写权限；
  - 执行时可能触发地址翻译 fault；
  - fault 时必须按 cache maintenance fault 报告，即 `CM=1`，且 `WnR=1`。
- 旧实现中，`DC IVAC` 在 `EL1+` 只是直接 `return true`，因此：
  - 对未映射地址不会 fault；
  - 对只读映射也不会给出 permission fault；
  - 与当前模型已实现的其它 cache maintenance by-VA 路径不一致。
- 这轮将 `DC IVAC` 接入真实的写权限翻译路径：
  - 现在会走 `translate_address(..., AccessType::Write, ...)`；
  - 失败时复用现有 cache maintenance data abort syndrome 组装；
  - 从而对 guest 正确暴露 translation fault / permission fault。
- 同时补了两层测试：
  - 扩充 `mmu_cache_maint_fault`，把未映射地址上的 `DC IVAC` 纳入现有 `CM/WnR/FAR/ELR` 校验；
  - 新增 `mmu_dc_ivac_perm_fault`，专门覆盖 `EL1` 只读页上的 `DC IVAC` permission fault。

## 本轮测试

- `timeout 1200s cmake --build build -j`
- `timeout 300s tests/arm64/build_tests.sh`
- `timeout 120s bash -lc "test \"$(./build/aarchvm -bin tests/arm64/out/mmu_cache_maint_fault.bin -load 0x0 -entry 0x0 -steps 4000000 | tr -d '\r\n')\" = M"`
- `timeout 120s bash -lc "test \"$(./build/aarchvm -bin tests/arm64/out/mmu_dc_ivac_perm_fault.bin -load 0x0 -entry 0x0 -steps 4000000 | tr -d '\r\n')\" = I"`
- `timeout 3600s tests/arm64/run_all.sh`
- `timeout 3600s tests/linux/run_functional_suite.sh`
- `timeout 3600s tests/linux/run_functional_suite_smp.sh`
- 结果：通过。

# 修改日志 2026-03-20 17:33

## 本轮修改

- 继续按“审阅 -> 修复 -> 测试”流程推进“Armv8-A 程序可见正确性收尾计划”，这轮收的是 `trap / undef / no-op` 边界里一处被老测试静默掩盖的回归缺口：
  - [tests/arm64/instr_legacy_each.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/instr_legacy_each.S)
  - [tests/arm64/special_pstate_regform.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/special_pstate_regform.S)
  - [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh)
  - [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)
- 审阅过程中发现：
  - `instr_legacy_each` 仍把 `DC CVAP/CVADP` 放在“基础 legacy 指令 smoke test”里当成普通可继续执行路径；
  - 但当前模型明确宣告未实现 `FEAT_DPB / FEAT_DPB2`，而且已有专门用例 `dc_cva_persist_absent` 校验这两条在本模型里应为 `UNDEFINED`；
  - 结果就是 `instr_legacy_each` 实际一直会失败，只是旧版 `run_all.sh` 对它没有做输出断言，导致该失败被静默漏报。
- 这轮因此没有改模拟器行为，而是修正了测试体系本身：
  - 从 `instr_legacy_each` 中移除了 `DC CVAP/CVADP` 这两条与当前特性声明矛盾的路径，只保留 `DC CVAU`；
  - 将 `tests/arm64/run_all.sh` 对 `instr_legacy_each` 改为强制断言成功输出 `E`，避免再次静默漏报；
  - 新增 `special_pstate_regform` 单测，显式覆盖：
    - `MSR SPSel, Xt` 在 `EL1` 的正常读写；
    - `MSR PAN, Xt` 在 `EL1` 的正常读写；
    - `MSR SPSel, Xt` / `MSR PAN, Xt` 在 `EL0` 的 `UNDEFINED` 行为。

## 本轮测试

- `timeout 300s tests/arm64/build_tests.sh`
- `timeout 60s ./build/aarchvm -bin tests/arm64/out/instr_legacy_each.bin -load 0x0 -entry 0x0 -steps 3000000`
- `timeout 60s ./build/aarchvm -bin tests/arm64/out/special_pstate_regform.bin -load 0x0 -entry 0x0 -steps 600000`
- `timeout 60s ./build/aarchvm -bin tests/arm64/out/dc_cva_persist_absent.bin -load 0x0 -entry 0x0 -steps 600000`
- `timeout 2400s tests/arm64/run_all.sh`
- `timeout 2400s tests/linux/run_functional_suite.sh`
- `timeout 2400s tests/linux/run_functional_suite_smp.sh`
- 结果：通过。

# 修改日志 2026-03-20 15:00

## 本轮修改

- 继续按“审阅 -> 修复 -> 测试”流程推进“Armv8-A 程序可见正确性收尾计划”，这轮补的是 cache maintenance fault 的 `ESR_EL1.ISS` 语义尾差：
  - [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp)
  - [include/aarchvm/cpu.hpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/include/aarchvm/cpu.hpp)
  - [tests/arm64/mmu_cache_maint_fault.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/mmu_cache_maint_fault.S)
- 根据 ARM ARM，cache maintenance 和 address translation system instruction 触发 synchronous Data Abort 时：
  - `ISS.CM` 必须为 `1`；
  - `ISS.WnR` 必须为 `1`；
  - 即使具体翻译路径内部是按读权限做地址解析，guest 看到的 syndrome 也不能退化成普通 read fault。
- 旧实现里，`IC IVAU`、`DC CVAC/CVAU/CIVAC` 以及 `DC ZVA` 的 fault 路径会直接复用普通 `data_abort(...)` 组装：
  - 导致 `CM=0`；
  - 且 `IC IVAU` / `DC CVA*` 这类路径会把 `WnR` 错报成 `0`。
- 这轮新增了 `data_abort_iss(...)` 辅助逻辑，并让上述 cache maintenance fault 路径统一走 `CM=1 + WnR=1` 的 syndrome 组装。
- 对应地增强了裸机用例 `mmu_cache_maint_fault`：
  - 现在会显式校验 `ESR_EL1.CM==1`；
  - 会校验 `ESR_EL1.WnR==1`；
  - 在原有 `DC CVAC/CVAU/CIVAC`、`IC IVAU` 基础上新增了 `DC ZVA` fault 覆盖。

## 本轮测试

- `timeout 1200s cmake --build build -j`
- `timeout 300s tests/arm64/build_tests.sh`
- `timeout 60s ./build/aarchvm -bin tests/arm64/out/mmu_cache_maint_fault.bin -load 0x0 -entry 0x0 -steps 4000000`
- `timeout 60s ./build/aarchvm -bin tests/arm64/out/el0_cache_ops_privilege.bin -load 0x0 -entry 0x0 -steps 800000`
- `timeout 2400s tests/arm64/run_all.sh`
- `timeout 1800s tests/linux/run_functional_suite.sh`
- `timeout 2400s tests/linux/run_functional_suite_smp.sh`
- 结果：通过。

# 修改日志 2026-03-20 14:54

## 本轮修改

- 继续按“审阅 -> 修复 -> 测试”流程推进“Armv8-A 程序可见正确性收尾计划”，这轮收的是通用定时器 sysreg 这一组的 guest 可见语义与 trap 覆盖：
  - [src/generic_timer.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/generic_timer.cpp)
  - [tests/arm64/cntkctl_el0_timer_access.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/cntkctl_el0_timer_access.S)
- 将 `CNT{P,V}_TVAL_EL0` 的读回实现显式写成“取 `(CVAL - Count)` 的低 32 位后零扩展到 64 位”，避免依赖宿主机对有符号窄化/再扩展的实现细节。
- 增强了 `cntkctl_el0_timer_access` 裸机用例：
  - denied path 现在会校验 `CNTVCT_EL0`、`CNTPCT_EL0`、`CNTV_TVAL_EL0`、`CNTP_CTL_EL0` 在 `EL0` 且 `CNTKCTL_EL1` 未放开时 trap 的完整 `ISS[24:0]`；
  - allowed path 新增了负 `TVAL` 读回检查，确认 `MRS CNT{P,V}_TVAL_EL0` 返回值高 32 位保持为 0，低 32 位保留倒计时视图，不会错误符号扩展成 `0xffffffffffffxxxx`。

## 本轮测试

- `timeout 1200s cmake --build build -j`
- `timeout 300s tests/arm64/build_tests.sh`
- `timeout 60s ./build/aarchvm -bin tests/arm64/out/cntkctl_el0_timer_access.bin -load 0x0 -entry 0x0 -steps 800000`
- `timeout 60s ./build/aarchvm -bin tests/arm64/out/gic_timer_sysreg.bin -load 0x0 -entry 0x0 -steps 2000000`
- `timeout 60s ./build/aarchvm -bin tests/arm64/out/gic_timer_phys_sysreg.bin -load 0x0 -entry 0x0 -steps 2000000`
- `timeout 2400s tests/arm64/run_all.sh`
- `timeout 1800s tests/linux/run_functional_suite.sh`
- `timeout 2400s tests/linux/run_functional_suite_smp.sh`
- 结果：通过。

# 修改日志 2026-03-20 14:44

## 本轮修改

- 继续按“审阅 -> 修复 -> 测试”流程推进“Armv8-A 程序可见正确性收尾计划”，这轮补的是 `EC=0x18` system instruction trap syndrome 的程序可见编码错误：
  - [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp)
  - [tests/arm64/el0_daif_uma.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/el0_daif_uma.S)
  - [tests/arm64/el0_sysreg_privilege.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/el0_sysreg_privilege.S)
  - [tests/arm64/el0_cache_ops_privilege.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/el0_cache_ops_privilege.S)
- 根据 ARM ARM，`ESR_ELx.EC=0x18` 的 `ISS` 字段布局应为：
  - `ISS[21:20]=Op0`
  - `ISS[19:17]=Op2`
  - `ISS[16:14]=Op1`
  - `ISS[13:10]=CRn`
  - `ISS[9:5]=Rt`
  - `ISS[4:1]=CRm`
  - `ISS[0]=Direction`
- 旧实现把 `Rt` 错放到了 `ISS[4:0]`，并把 `Direction` 错放到了 `ISS[21]`，导致所有 `MRS/MSR/system instruction trap` 的 syndrome 编码都可能错位。
- 这轮修正了 `sysreg_trap_iss(...)` 的打包逻辑，并修正了 `MSR DAIFSet/#imm4`、`MSR DAIFClr/#imm4` 在 `EL0` 且 `SCTLR_EL1.UMA=0` 时错误上报 `ISS=0` 的问题；现在它们会像其他 system instruction trap 一样携带完整的 `ISS` 编码。
- 对应地增强了裸机单测：
  - `el0_daif_uma` 现在会校验 `MRS/ MSR DAIF` 以及 `MSR DAIFSet/DAIFClr` trap 的完整 `ISS[24:0]`；
  - `el0_sysreg_privilege` 现在会校验 `MRS CTR_EL0`、`MRS FAR_EL1`、`MSR TCR_EL1, X0`、`MSR DAIFSet, #0xf` 的完整 `ISS`；
  - `el0_cache_ops_privilege` 现在会校验 `IC IVAU`、`DC ZVA` trap 的完整 `ISS`。

## 本轮测试

- `timeout 1200s cmake --build build -j`
- `timeout 300s tests/arm64/build_tests.sh`
- `timeout 60s ./build/aarchvm -bin tests/arm64/out/el0_daif_uma.bin -load 0x0 -entry 0x0 -steps 800000`
- `timeout 60s ./build/aarchvm -bin tests/arm64/out/el0_sysreg_privilege.bin -load 0x0 -entry 0x0 -steps 800000`
- `timeout 60s ./build/aarchvm -bin tests/arm64/out/el0_cache_ops_privilege.bin -load 0x0 -entry 0x0 -steps 800000`
- `timeout 2400s tests/arm64/run_all.sh`
- `timeout 1800s tests/linux/run_functional_suite.sh`
- `timeout 2400s tests/linux/run_functional_suite_smp.sh`
- 结果：通过。

# 修改日志 2026-03-20 13:02

## 本轮修改

- 继续按“审阅 -> 修复 -> 测试”流程推进“Armv8-A 程序可见正确性收尾计划”，这轮补的是 `HVC/SMC` 在当前仅实现 `EL0/EL1` 模型下的程序可见异常语义缺口：
  - [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp)
  - [tests/arm64/el1_hvc_smc_undef.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/el1_hvc_smc_undef.S)
  - [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh)
  - [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)
- 根据手册，`EL2` 未实现时 `HVC` 在 `EL1` 必须是 `UNDEFINED`；`EL3` 未实现且未被更高层 trap 时，`SMC` 在 `EL1` 也必须是 `UNDEFINED`。
- 旧实现里，`EL1` 执行 `HVC/SMC` 在没有 `SMCCC` 回调处理时，会伪造 `EC=0x16/0x17` 的同步异常，这与当前处理器模型只实现 `EL0/EL1` 的事实不一致。
- 现在的行为调整为：
  - `EL0` 的 `HVC/SMC` 继续按 `UNDEFINED` 处理；
  - `EL1` 的 `HVC/SMC` 在 `SMCCC` 回调成功处理时仍可直接返回，保持现有 `PSCI` 路径可用；
  - `EL1` 的 `HVC/SMC` 在回调未处理时改为真正的 `UNDEFINED` 异常，`ESR_EL1.EC=0`、`ISS=0`、`ELR_EL1` 指向故障指令。
- 新增裸机单测 [tests/arm64/el1_hvc_smc_undef.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/el1_hvc_smc_undef.S)：
  - 覆盖 `EL1 HVC #imm16` 与 `EL1 SMC #imm16` 在无回调兜底时的 `UNDEFINED` 行为；
  - 校验 `ESR_EL1.EC==0`；
  - 校验 `ESR_EL1.ISS==0`；
  - 校验 `ELR_EL1` 指向原始 `HVC/SMC` 指令，而不是下一条指令。

## 本轮测试

- `timeout 300s tests/arm64/build_tests.sh`
- `timeout 1200s cmake --build build -j`
- `timeout 60s ./build/aarchvm -bin tests/arm64/out/el1_hvc_smc_undef.bin -load 0x0 -entry 0x0 -steps 600000`
- `timeout 2400s tests/arm64/run_all.sh`
- `timeout 1800s tests/linux/run_functional_suite.sh`
- `timeout 2400s tests/linux/run_functional_suite_smp.sh`
- 结果：通过。

# 修改日志 2026-03-20 03:30

## 本轮修改

- 继续按“审阅 -> 修复 -> 测试”流程推进“Armv8-A 程序可见正确性收尾计划”，这轮补的是 `FPCR.FZ` 在 unary/misc/estimate/round-int 路径上的程序可见语义缺口：
  - [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp)
  - [tests/arm64/fp_fz_misc.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/fp_fz_misc.S)
  - [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh)
  - [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)
- 修正内容：
  - `FSQRT` 现在会在 `NaN` 处理后先按 `FZ` flush subnormal 输入，并在结果侧统一复用输出 flush helper；
  - `FRINT*` 共用 round-int helper 现在会先按 `FZ` flush subnormal 输入，因此 `FRINTN/FRINTX/FRINTI` 等路径对 subnormal 输入会返回带符号零，并正确置 `FPSR.IDC`；
  - `FRECPX` 现在对 `FZ=1` 的 subnormal 输入按 zero 语义处理，结果与 `FPSR.IDC` 对齐；
  - `FRECPE/FRSQRTE` 现在同样先按 `FZ` flush 输入，并补上此前缺失的 zero-input `FPSR.DZC`，因此“literal zero”和“`FZ` 把 subnormal flush 成 zero”两种情况都能得到正确的 `DZC`；
  - `FRECPE/FRSQRTE/FSQRT` 的计算结果在需要时会经过统一的输出 denormal flush 路径，避免 `FZ` 下残留 guest 可见 subnormal 结果。
- 新增裸机单测 [tests/arm64/fp_fz_misc.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/fp_fz_misc.S)：
  - 覆盖 scalar `FSQRT S/D`、`FRINTN S/D`、`FRECPX S/D`、`FRECPE S/D`、`FRSQRTE S/D`；
  - 覆盖 `FRECPE(0.0)` 与 `FRSQRTE(0.0)` 的 `FPSR.DZC`；
  - 覆盖 vector `FSQRT v4s`、`FRINTN v4s`、`FRECPE v4s`、`FRSQRTE v4s`；
  - 同时校验结果 bit pattern 与 `FPSR.IDC/DZC`。

# 修改日志 2026-03-22 17:28

## 本轮修改

- 继续收口了 AdvSIMD structured load/store 与 pair-Q 对齐语义里剩余的“内部子访问稀释掉架构对齐规则”的边界：
  - [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp)
- 修正了 `LDP/STP Qt1,Qt2` pair transfer 的对齐判定：
  - 修复前：pair-Q 仍按 4 次 8-byte 子访问执行，`SCTLR_EL1.A=1` 时会把“8-byte 对齐但 16-byte 不对齐”的地址错误放行。
  - 修复后：pair-Q 现在按每个 Q 元素的 16-byte access size 检查对齐，fault 的 `FAR/WnR/ELR` 行为已被单测锁定。
- 修正了 structured `LD1/ST1` multiple-structures whole-register `.8B/.16B` 的 element-size 对齐语义：
  - 修复前：`LD1/ST1 {Vt.16B}`、`{Vt.16B,Vt2.16B}` 等路径复用了整寄存器 `load_vec()/store_vec()`，对 `.16B` 形式会隐含 8-byte 对齐检查；在 `SCTLR_EL1.A=1` 下，misaligned base 会被错误 fault。
  - 修复后：这些路径改为逐 byte element 访问，保持 structured load/store 应有的 byte-element 对齐语义；misaligned `.16B` multiple-structures 在 `A=1` 下仍可正常完成。
- 新增/接入的裸机单测：
  - [tests/arm64/fpsimd_q_pair_alignment_fault.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/fpsimd_q_pair_alignment_fault.S)
  - [tests/arm64/fpsimd_ld1_multi_alignment.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/fpsimd_ld1_multi_alignment.S)
- 更新了测试接线：
  - [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh)
  - [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)

## 本轮测试

- 定向构建：
  - `timeout 300s tests/arm64/build_tests.sh`
  - `timeout 1200s cmake --build build -j`
- 定向语义验证：
  - `timeout 120s bash -lc 'AARCHVM_BRK_MODE=halt ./build/aarchvm -bin tests/arm64/out/fpsimd_q_pair_alignment_fault.bin -load 0x0 -entry 0x0 -steps 700000 2>/dev/null | tr -d "\r\n"'`
  - `timeout 120s bash -lc 'AARCHVM_BRK_MODE=halt ./build/aarchvm -bin tests/arm64/out/fpsimd_ld1_multi_alignment.bin -load 0x0 -entry 0x0 -steps 700000 2>/dev/null | tr -d "\r\n"'`
- 裸机完整回归：
  - `timeout 5400s bash -lc 'tests/arm64/run_all.sh > out/arm64_run_all_struct_ld1_align.log 2>&1'`
- Linux 单核功能回归：
  - `timeout 5400s bash -lc 'tests/linux/run_functional_suite.sh > out/linux_functional_struct_ld1_align_ump.log 2>&1'`
- Linux SMP 功能回归：
  - `timeout 5400s bash -lc 'tests/linux/run_functional_suite_smp.sh > out/linux_functional_struct_ld1_align_smp.log 2>&1'`

## 当前结论

- 这轮继续把 AdvSIMD memory access 的程序可见对齐语义往前收了一步，尤其修掉了两类容易被“内部拆成多个子访问”掩盖的问题：
  - `LDP/STP Q,Q` 应按每个 Q 元素 16-byte 对齐，而不是按 8-byte 子访问。
  - structured `LD1/ST1` whole-register `.8B/.16B` 应按 byte element 对齐，而不是因为内部实现细节平白变成 8-byte 对齐。
- 沿着这条线继续审，当前最像剩余缺口的点是：
  - AdvSIMD structured load/store 显式 `<align>` 编码位尚未系统化校验；
  - 其它 structured lane / multiple-structures 变体的“显式对齐编码”和 fault 语义仍值得继续单测化。

## 本轮测试

- 定向构建：
  - `timeout 1200s cmake --build build -j`
  - `timeout 300s tests/arm64/build_tests.sh`
- 定向验证：
  - `timeout 60s ./build/aarchvm -bin tests/arm64/out/fp_fz_misc.bin -load 0x0 -entry 0x0 -steps 500000`
  - `timeout 60s ./build/aarchvm -bin tests/arm64/out/fpsimd_fp_estimate.bin -load 0x0 -entry 0x0 -steps 400000`
  - `timeout 60s ./build/aarchvm -bin tests/arm64/out/fp_roundint_flags.bin -load 0x0 -entry 0x0 -steps 300000`
  - `timeout 60s ./build/aarchvm -bin tests/arm64/out/fp_scalar_frecpx.bin -load 0x0 -entry 0x0 -steps 300000`
- 裸机完整回归：
  - `timeout 2400s tests/arm64/run_all.sh`
- Linux 单核功能回归：
  - `timeout 1800s tests/linux/run_functional_suite.sh`
- Linux SMP 功能回归：
  - `timeout 2400s tests/linux/run_functional_suite_smp.sh`
- 结果：通过。

# 修改日志 2026-03-20 03:15

## 本轮修改

- 继续按“审阅 -> 修复 -> 测试”流程推进“Armv8-A 程序可见正确性收尾计划”第一阶段，这一轮补的是 `FPCR.FZ` 在普通浮点算术与 compare 路径上的 guest 可见语义缺口：
  - [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp)
  - [tests/arm64/fp_fz_arith_compare.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/fp_fz_arith_compare.S)
  - [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh)
  - [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)
- 当前实现此前只把 `FPCR.RMode` 与 `FPCR.DN` 接到了主要 helper，`FPCR.FZ` 基本没有进入执行路径，导致以下程序可见错误：
  - subnormal 输入没有被 flush 成带符号的零；
  - subnormal 输出没有被 flush 成带符号的零；
  - `FPSR.IDC` 与 `FPSR.UFC` 没有按 `FZ` 语义更新；
  - compare 指令把 subnormal 当成普通非零值比较，和真实 Armv8-A 行为不一致。
- 这轮补了一个保守但真实的闭环：
  - 新增 `FZ` 控制位抽取与 `FPSR.IDC` 常量；
  - 新增输入/输出 denormal flush helper；
  - 将 helper 接入 `fp_binary_arith_bits(...)` 与 `fp_fma_bits(...)`，使 `FADD/FSUB/FMUL/FDIV/FABD` 及依赖这些 helper 的向量/标量路径获得 `FZ` 语义；
  - 将 `FCMP/FCMPE/FCCMP/FCCMPE` 以及 `FCMEQ/FCMGE/FCMGT/FACGE/FACGT` 的标量/向量路径接入输入 denormal flush，使比较结果与 `FPSR.IDC` 对齐 guest 可见行为。
- 新增裸机单测 [tests/arm64/fp_fz_arith_compare.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/fp_fz_arith_compare.S)：
  - 覆盖 scalar `FADD S/D` 的输入 flush；
  - 覆盖 scalar `FMUL S/D` 的输出 flush；
  - 覆盖 `FCMP` 旗标比较与 `FCMEQ` 结果比较；
  - 覆盖向量 `FADD` 与向量 `FCMEQ`；
  - 同时检查 `FPSR.IDC == 0x80` 与 `FPSR.UFC == 0x8`。

## 本轮测试

- `timeout 1200s cmake --build build -j`
- `timeout 300s tests/arm64/build_tests.sh`
- `timeout 60s ./build/aarchvm -bin tests/arm64/out/fp_fz_arith_compare.bin -load 0x0 -entry 0x0 -steps 300000`
- `timeout 60s ./build/aarchvm -bin tests/arm64/out/fp_compare_flags.bin -load 0x0 -entry 0x0 -steps 200000`
- `timeout 60s ./build/aarchvm -bin tests/arm64/out/fp_scalar_compare_flags.bin -load 0x0 -entry 0x0 -steps 200000`
- `timeout 60s ./build/aarchvm -bin tests/arm64/out/fp_arith_fpcr_flags.bin -load 0x0 -entry 0x0 -steps 300000`
- `timeout 2400s tests/arm64/run_all.sh`
- `timeout 1800s tests/linux/run_functional_suite.sh`
- `timeout 2400s tests/linux/run_functional_suite_smp.sh`
- 结果：通过。

# 修改日志 2026-03-20 02:54

## 本轮修改

- 继续按“审阅 -> 修复 -> 测试”流程推进“Armv8-A 程序可见正确性收尾计划”第一阶段，这一轮补的是 `FSQRT` 的 guest 可见舍入与异常语义缺口：
  - [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp)
  - [tests/arm64/fp_sqrt_rounding.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/fp_sqrt_rounding.S)
  - [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh)
  - [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)
- 修正 `FSQRT` 此前直接调用 `std::sqrt(...)` 的问题：
  - 旧实现绕过了统一的 `host_fp_eval(...)` 路径，因此 `FPCR.RMode` 并不真正参与 `FSQRT` 的结果舍入；
  - 旧实现只在“非精确平方根”时手工置位 `FPSR.IXC`，没有和其它浮点指令一样通过宿主浮点异常统一映射 `FPSR`；
  - `sNaN/qNaN/default-NaN` 传播也没有复用现有的统一 NaN 处理 helper。
- 现在的行为调整为：
  - `fp_sqrt_bits(...)` 新增 `fpcr_mode` 参数，并通过 `host_fp_eval(...)` 执行实际开方；
  - 标量与向量 `FSQRT` 都会按 `FPCR.RMode` 产生 guest 可见结果；
  - `FPSR` 由统一的宿主异常映射产生，不再单独手搓 `IXC`；
  - `NaN`、`-0`、负有限值、`-Inf`、`+Inf` 的程序可见结果继续按当前已实现规则保留，并统一走 NaN 语义处理路径。
- 新增裸机单测 [tests/arm64/fp_sqrt_rounding.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/fp_sqrt_rounding.S)：
  - 覆盖 scalar `FSQRT S` 的 `RNE/RU`；
  - 覆盖 scalar `FSQRT D` 的 `RNE/RM`；
  - 覆盖 vector `FSQRT v4s` 与 `v2d`；
  - 同时检查结果 bit pattern 与 `FPSR.IXC`。

## 本轮测试

- `timeout 1200s cmake --build build -j`
- `timeout 300s tests/arm64/build_tests.sh`
- `timeout 60s ./build/aarchvm -bin tests/arm64/out/fp_sqrt_rounding.bin -load 0x0 -entry 0x0 -steps 300000`
- `timeout 60s ./build/aarchvm -bin tests/arm64/out/fp_sqrt_flags.bin -load 0x0 -entry 0x0 -steps 300000`
- `timeout 60s ./build/aarchvm -bin tests/arm64/out/fpsimd_fp_misc_rounding.bin -load 0x0 -entry 0x0 -steps 400000`
- `timeout 2400s tests/arm64/run_all.sh`
- `timeout 1800s tests/linux/run_functional_suite.sh`
- `timeout 2400s tests/linux/run_functional_suite_smp.sh`
- 结果：通过。

# 修改日志 2026-03-20 02:20

## 本轮修改

- 继续按“审阅 -> 修复 -> 测试”流程推进“Armv8-A 程序可见正确性收尾计划”第一阶段，这一轮补的是整数转浮点路径里一个真实的解码与语义缺口：
  - [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp)
  - [tests/arm64/fp_int_to_fp_rounding.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/fp_int_to_fp_rounding.S)
  - [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh)
  - [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)
- 修正 `SCVTF/UCVTF (scalar SIMD&FP)` 与 `FRECPE/FRSQRTE` 的 opcode 解码冲突：
  - 之前 `exec_data_processing()` 里更早的 `switch (insn & 0xFF3FFC00u)` 先吃掉了 `0x5E21D800` / `0x7E21D800`；
  - 当 `ftype=0/1` 时，这两个 case 会直接 `return false`，导致 `scvtf s?, s?` 与 `ucvtf d?, d?` 永远落到 `UNIMPL`，再被同步异常向量误导到测试代码附近；
  - 现改为在同一组 case 中按 `ftype` 正确区分：
    - `ftype=0/1` 走 `SCVTF/UCVTF (scalar SIMD&FP)`；
    - `ftype=2/3` 保持 `FRECPE/FRSQRTE`。
- 延续上一轮已经加入的整数转浮点 helper，使以下路径都遵守 guest 可见语义：
  - `SCVTF/UCVTF` 现在按 `FPCR.RMode` 舍入；
  - 对不可精确表示的整数转浮点，正确置位 `FPSR.IXC`；
  - 标量 GP 源寄存器路径与标量 SIMD&FP 源寄存器路径行为一致。
- 清理了新单测里的临时调试分支，不再保留仅用于定位问题的 `x24='a'/'b'` 与额外 `fail*_flags` 分支。
- 新增裸机单测 [tests/arm64/fp_int_to_fp_rounding.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/fp_int_to_fp_rounding.S)：
  - 覆盖 `SCVTF/UCVTF` 的 GP 源寄存器路径与 scalar SIMD&FP 源寄存器路径；
  - 覆盖 single / double；
  - 覆盖 `RNE`、`+Inf`、`-Inf`；
  - 覆盖 exact / inexact 与 `FPSR.IXC`。

## 本轮测试

- `timeout 1200s cmake --build build -j`
- `timeout 300s tests/arm64/build_tests.sh`
- `timeout 60s ./build/aarchvm -bin tests/arm64/out/fp_int_to_fp_rounding.bin -load 0x0 -entry 0x0 -steps 300000`
- `timeout 2400s tests/arm64/run_all.sh`
- `timeout 1800s tests/linux/run_functional_suite.sh`
- `timeout 2400s tests/linux/run_functional_suite_smp.sh`
- 结果：通过。

# 修改日志 2026-03-20 01:29

## 本轮修改

- 继续按“审阅 -> 修复 -> 测试”流程推进“Armv8-A 程序可见正确性收尾计划”第一阶段，这一轮针对 `FPCR.DN` 在普通浮点算术路径上的遗漏做收口：
  - [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp)
  - [tests/arm64/fp_dn_arith.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/fp_dn_arith.S)
  - [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh)
  - [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)
- 修正了两层叠加导致的 `DN=1` 语义缺口：
  - 多个标量/向量算术、融合乘加、pairwise 加法等调用点此前只把 `FPCR.RMode` 两位传给底层 helper，直接丢掉了 `FPCR.DN`；
  - 更关键的是，`fp_binary_arith_bits(...)` 与 `fp_fma_bits(...)` 自身此前直接把 NaN 交给宿主 `libm`，绕过了已有的 `fp_process_nan_binary/ternary(...)`，导致 `DN=1` 即使在调用点上传递下来也无法真正生效。
- 现在的行为调整为：
  - 相关算术/融合乘加/舍入相关 `FP` 路径统一读取完整的 `current_fpcr_ctl()`；
  - 普通二元浮点算术与融合乘加在进入宿主计算前，先按 guest 规则处理 `qNaN/sNaN/default-NaN`；
  - 因此 `FPCR.DN=1` 时，`FADD/FSUB/FMUL/FDIV/FABD/FNMUL/FMADD/FMSUB/FNMADD/FNMSUB` 以及对应的多条向量/pairwise 路径，不再错误保留输入 NaN payload，而会返回 guest 可见的 default-NaN；
  - `sNaN` 仍会正确置位 `FPSR.IOC`，`qNaN` 不会错误置位。
- 新增裸机单测 [tests/arm64/fp_dn_arith.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/fp_dn_arith.S)：
  - 覆盖 scalar `FADD`；
  - 覆盖 scalar `FMADD`；
  - 覆盖 scalar `FDIV`；
  - 覆盖 vector `FADD`；
  - 覆盖 vector `FADDP`；
  - 同时覆盖 `qNaN/sNaN`、`default-NaN` 与 `FPSR.IOC`。

## 本轮测试

- `timeout 1200s cmake --build build -j`
- `timeout 300s tests/arm64/build_tests.sh`
- `timeout 60s ./build/aarchvm -bin tests/arm64/out/fp_dn_arith.bin -load 0x0 -entry 0x0 -steps 400000`
- `timeout 60s ./build/aarchvm -bin tests/arm64/out/fp_dn_misc.bin -load 0x0 -entry 0x0 -steps 400000`
- `timeout 60s ./build/aarchvm -bin tests/arm64/out/fp_arith_fpcr_flags.bin -load 0x0 -entry 0x0 -steps 400000`
- `timeout 60s ./build/aarchvm -bin tests/arm64/out/fpsimd_arith_fpcr_flags.bin -load 0x0 -entry 0x0 -steps 500000`
- `timeout 2400s tests/arm64/run_all.sh`
- `timeout 1800s tests/linux/run_functional_suite.sh`
- `timeout 2400s tests/linux/run_functional_suite_smp.sh`
- 结果：通过。

# 修改日志 2026-03-20 00:19

## 本轮修改

- 继续按“审阅 -> 修复 -> 测试”流程推进“Armv8-A 程序可见正确性收尾计划”的前两阶段，分别补了一个 `FP/AdvSIMD` 缺口和一个 `SMP/WFE` 语义缺口：
  - [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp)
  - [tests/arm64/fpsimd_compare_flags.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/fpsimd_compare_flags.S)
  - [tests/arm64/smp_wfe_store_no_event.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/smp_wfe_store_no_event.S)
  - [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh)
  - [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)
- 修正向量 `FACGE/FACGT/FCMEQ/FCMGE/FCMGT` 以及 `FCM* ..., #0.0` 的 `FPSR` 语义：
  - 之前这些向量 compare 只生成掩码结果，没有像标量 quiet compare 那样累计 `FPSR.IOC`；
  - 现在对每个 lane 统一复用 `fp_compare_fpsr_bits(...)`；
  - `qNaN` quiet compare 保持不置 `IOC`，`sNaN` 会正确置位 `IOC`。
- 修正 `WFE` 事件语义中过宽的远端写唤醒：
  - 之前任意远端内存写都会把目标 CPU 的 `event_register_` 置位；
  - 现在只有远端写真正打破该 CPU 的 exclusive monitor 时，才生成对应 event；
  - 保留 `SEV/SEVL` 与 exclusive-monitor-loss 这两类现有可见唤醒路径。
- 新增裸机单测：
  - [tests/arm64/fpsimd_compare_flags.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/fpsimd_compare_flags.S)
    - 覆盖向量寄存器 compare、zero compare、absolute compare；
    - 覆盖 single/double；
    - 覆盖 `qNaN/sNaN` 与 `FPSR.IOC`。
  - [tests/arm64/smp_wfe_store_no_event.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/smp_wfe_store_no_event.S)
    - 验证无 `SEV` 且无 exclusive monitor 相关性的普通远端写不会错误唤醒 `WFE`。

## 本轮测试

- `timeout 1200s cmake --build build -j`
- `timeout 300s tests/arm64/build_tests.sh`
- `timeout 60s ./build/aarchvm -bin tests/arm64/out/fpsimd_compare_flags.bin -load 0x0 -entry 0x0 -steps 300000`
- `timeout 60s ./build/aarchvm -smp 2 -bin tests/arm64/out/smp_wfe_store_no_event.bin -load 0x0 -entry 0x0 -steps 300000`
- `timeout 2400s tests/arm64/run_all.sh`
- `timeout 1800s tests/linux/run_functional_suite.sh`
- `timeout 2400s tests/linux/run_functional_suite_smp.sh`
- 结果：通过。

# 修改日志 2026-03-19 23:19

## 本轮修改

- 继续按“审阅 -> 修复 -> 测试”流程推进“Armv8-A 程序可见正确性收尾计划”第一阶段，先修补一个明确且可验证的浮点语义缺口：
  - [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp)
  - [tests/arm64/fp_absneg_nan_flags.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/fp_absneg_nan_flags.S)
  - [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh)
  - [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)
- 修正标量与向量 `FABS/FNEG` 的 guest 可见 NaN 语义：
  - 不再经宿主 `std::fabs` / 一元负号路径处理；
  - 改为纯 bitwise 实现，避免把 `sNaN` 错误 quiet 成 `qNaN`；
  - 不再错误置位 `FPSR.IOC`；
  - 保持当前模型下 `AH=0` 的 NaN 符号处理行为。
- 新增裸机单测 [tests/arm64/fp_absneg_nan_flags.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/fp_absneg_nan_flags.S)：
  - 覆盖 scalar/vector；
  - 覆盖 single/double；
  - 覆盖 `sNaN/qNaN`；
  - 覆盖 `FABS/FNEG` 的结果位型与 `FPSR`。

## 本轮测试

- `timeout 1200s cmake --build build -j`
- `timeout 300s tests/arm64/build_tests.sh`
- `timeout 60s ./build/aarchvm -bin tests/arm64/out/fp_absneg_nan_flags.bin -load 0x0 -entry 0x0 -steps 300000`
- `timeout 60s ./build/aarchvm -bin tests/arm64/out/fp_scalar_misc.bin -load 0x0 -entry 0x0 -steps 200000`
- `timeout 60s ./build/aarchvm -bin tests/arm64/out/fpsimd_fp_misc_rounding.bin -load 0x0 -entry 0x0 -steps 400000`
- `timeout 2400s tests/arm64/run_all.sh`
- `timeout 1800s tests/linux/run_functional_suite.sh`
- `timeout 2400s tests/linux/run_functional_suite_smp.sh`
- 结果：通过。

# 修改日志 2026-03-19 22:36

## 本轮修改

- 继续按“审阅 -> 修复 -> 测试”流程补齐普通浮点算术/融合乘加的 AArch64 `FPCR/FPSR` 语义缺口：
  - [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp)
  - [tests/arm64/fp_arith_fpcr_flags.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/fp_arith_fpcr_flags.S)
  - [tests/arm64/fpsimd_arith_fpcr_flags.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/fpsimd_arith_fpcr_flags.S)
  - [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh)
  - [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)
- 新增统一的宿主 `fenv` 浮点执行 helper：
  - 将 guest `FPCR.RMode` 映射到宿主舍入模式；
  - 将宿主 `FE_INVALID/FE_DIVBYZERO/FE_OVERFLOW/FE_UNDERFLOW/FE_INEXACT` 映射到 guest `FPSR` 的 `IOC/DZC/OFC/UFC/IXC`；
  - 用统一 helper 收敛普通标量/向量浮点算术与融合乘加路径。
- 修正了下列指令族的 guest 可见行为：
  - 标量 `FADD/FSUB/FMUL/FNMUL/FDIV/FABD/FADDP/FMADD/FMSUB/FNMADD/FNMSUB`
  - 向量 `FADD/FSUB/FMUL/FDIV/FABD/FADDP/FMLA/FMLS`
- 本轮修复的重点语义：
  - 普通浮点算术现在遵守 guest `FPCR.RMode`，不再固定使用宿主默认舍入模式；
  - `FDIV` 等运算现在会累计 `FPSR.DZC/OFC/UFC/IXC/IOC`；
  - 向量 `FMLA/FMLS` 改为明确走 fused 语义，而不是依赖宿主是否会偶然做 contraction；
  - `FABD`/`FNMUL` 现在在保留算术异常语义的同时，分别执行绝对值/符号翻转。
- 新增裸机单测：
  - [tests/arm64/fp_arith_fpcr_flags.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/fp_arith_fpcr_flags.S)
    - 覆盖标量 `FADD` 的 `FPCR.RMode=+Inf` 与 `FPSR.IXC`
    - 覆盖标量 `FDIV` 的 `FPSR.DZC`
    - 覆盖标量 `FMADD` 的 fused 语义
  - [tests/arm64/fpsimd_arith_fpcr_flags.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/fpsimd_arith_fpcr_flags.S)
    - 覆盖向量 `FADD` 的 `FPCR.RMode=+Inf` 与 `FPSR.IXC`
    - 覆盖向量 `FDIV` 的 `FPSR.DZC`
    - 覆盖向量 `FMLA` 的 fused 语义

## 本轮测试

- `timeout 1200s cmake --build build -j`
- `timeout 300s tests/arm64/build_tests.sh`
- `timeout 60s ./build/aarchvm -bin tests/arm64/out/fp_arith_fpcr_flags.bin -load 0x0 -entry 0x0 -steps 400000`
- `timeout 60s ./build/aarchvm -bin tests/arm64/out/fpsimd_arith_fpcr_flags.bin -load 0x0 -entry 0x0 -steps 500000`
- `timeout 60s ./build/aarchvm -bin tests/arm64/out/fp_scalar_fma.bin -load 0x0 -entry 0x0 -steps 300000`
- `timeout 60s ./build/aarchvm -bin tests/arm64/out/fp_scalar_pairwise.bin -load 0x0 -entry 0x0 -steps 300000`
- `timeout 60s ./build/aarchvm -bin tests/arm64/out/fpsimd_fp_vector.bin -load 0x0 -entry 0x0 -steps 500000`
- `timeout 60s ./build/aarchvm -bin tests/arm64/out/fpsimd_fp_pairwise.bin -load 0x0 -entry 0x0 -steps 500000`
- `timeout 2400s tests/arm64/run_all.sh`
- `timeout 1200s tests/linux/run_functional_suite.sh`
- `timeout 1200s tests/linux/run_functional_suite_smp.sh`
- 结果：全部通过。

# 修改日志 2026-03-19 21:37

## 本轮修改

- 继续按“审阅 -> 修复 -> 测试”流程补齐 `FRINT*` 家族的 AArch64 `FPSR` 语义缺口：
  - [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp)
  - [tests/arm64/fp_roundint_flags.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/fp_roundint_flags.S)
  - [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh)
  - [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)
- 新增统一的 round-to-integral helper：
  - 对 `sNaN` 执行 quieting 并置 `FPSR.IOC`
  - `qNaN` 透传
  - `Inf` 与 `±0` 保持原值
  - 对舍入到零的结果保留输入符号，生成 `-0.0`
- 修正了标量与向量 `FRINTN/P/M/Z/A/I/X` 的标志行为：
  - `FRINTX` 在结果与输入数值不相等时置 `FPSR.IXC`
  - `FRINTI` 与 `FRINTN/P/M/Z/A` 不再错误地产生 `IXC`
  - 向量路径逐 lane 聚合 `IOC/IXC`
- 新增裸机单测 [tests/arm64/fp_roundint_flags.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/fp_roundint_flags.S)：
  - 覆盖标量与向量
  - 覆盖 `FRINTN/FRINTX/FRINTI`
  - 覆盖 `sNaN/qNaN`
  - 覆盖 `FRINTX` 的 `IXC`
  - 覆盖舍入为零时的负零符号

## 本轮测试

- `timeout 1200s cmake --build build -j`
- `timeout 180s tests/arm64/build_tests.sh`
- `timeout 60s ./build/aarchvm -bin tests/arm64/out/fp_roundint_flags.bin -load 0x0 -entry 0x0 -steps 300000`
- `timeout 60s ./build/aarchvm -bin tests/arm64/out/fp_scalar_misc.bin -load 0x0 -entry 0x0 -steps 300000`
- `timeout 60s ./build/aarchvm -bin tests/arm64/out/fpsimd_fp_misc_rounding.bin -load 0x0 -entry 0x0 -steps 400000`
- `timeout 2400s tests/arm64/run_all.sh`
- `timeout 1200s tests/linux/run_functional_suite.sh`
- `timeout 1200s tests/linux/run_functional_suite_smp.sh`
- 结果：全部通过。

# 修改日志 2026-03-19 21:27

## 本轮修改

- 继续按“审阅 -> 修复 -> 测试”流程补齐 `FSQRT` 的 AArch64 语义缺口：
  - [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp)
  - [tests/arm64/fp_sqrt_flags.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/fp_sqrt_flags.S)
  - [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh)
  - [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)
- 新增统一 helper：
  - `fp_is_inf_bits()`
  - `fp_is_exact_sqrt_positive_finite_bits()`
  - `fp_sqrt_bits()`
- 修正标量与向量 `FSQRT` 的边界值与异常标志行为：
  - `sNaN` 会 quiet 并置 `FPSR.IOC`
  - `qNaN` 透传
  - 负的非零有限数与 `-Inf` 返回 default NaN，并置 `FPSR.IOC`
  - `-0.0` 保持为 `-0.0`
  - `+Inf` 保持为 `+Inf`
  - 非精确平方根如 `sqrt(2)` 现在会置 `FPSR.IXC`
- 新增裸机单测 [tests/arm64/fp_sqrt_flags.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/fp_sqrt_flags.S)：
  - 覆盖标量/向量
  - 覆盖单精度/双精度
  - 覆盖 `sNaN`、负数 invalid、`-0.0`、`+Inf`、`sqrt(2)` 的 `IOC/IXC`

## 本轮测试

- `timeout 1200s cmake --build build -j`
- `timeout 180s tests/arm64/build_tests.sh`
- `timeout 60s ./build/aarchvm -bin tests/arm64/out/fp_sqrt_flags.bin -load 0x0 -entry 0x0 -steps 300000`
- `timeout 60s ./build/aarchvm -bin tests/arm64/out/fp_scalar_misc.bin -load 0x0 -entry 0x0 -steps 300000`
- `timeout 2400s tests/arm64/run_all.sh`
- `timeout 1200s tests/linux/run_functional_suite.sh`
- `timeout 1200s tests/linux/run_functional_suite_smp.sh`
- 结果：全部通过。

# 修改日志 2026-03-19 21:13

## 本轮修改

- 继续按“审阅 -> 修复 -> 测试”流程补齐 AArch64 标量浮点 reciprocal-exponent 语义缺口：
  - [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp)
  - [tests/arm64/fp_scalar_frecpx.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/fp_scalar_frecpx.S)
  - [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh)
  - [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)
- 新增标量 `FRECPX`：
  - `FRECPX Sd, Sn`
  - `FRECPX Dd, Dn`
- 新增 `fp_recip_exponent_bits<UIntT>()` helper，按手册 `FPRecpX()` 语义处理：
  - qNaN 透传；
  - sNaN quiet 后置 `FPSR.IOC`；
  - 零与非规格化数返回 `sign:max_exp:0`；
  - 规格化数与无穷大返回 `sign:NOT(exp):0`。
- 修正了一处真实语义错误，而不是绕过问题：
  - 之前指数翻转错误地与 `max_exp` 做与运算，会丢掉指数最低位；
  - 现改为 `exp == 0 ? max_exp : ((~exp) & exp_all_ones)`，从而修正了如 `frecpx s?, 2.0` 这类输入的结果。
- 新增裸机单测 [tests/arm64/fp_scalar_frecpx.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/fp_scalar_frecpx.S)：
  - 覆盖单精度与双精度；
  - 覆盖 `1.0/2.0/0.0/subnormal/Inf/negative/sNaN`；
  - 覆盖 `sNaN` quieting 与 `FPSR.IOC` 置位。

## 本轮测试

- `timeout 60s ./build/aarchvm -bin tests/arm64/out/fp_scalar_frecpx.bin -load 0x0 -entry 0x0 -steps 300000`
- `timeout 2400s tests/arm64/run_all.sh`
- `timeout 1200s tests/linux/run_functional_suite.sh`
- `timeout 1200s tests/linux/run_functional_suite_smp.sh`
- 结果：全部通过。

# 修改日志 2026-03-19 20:55

## 本轮修改

- 继续按“审阅 -> 修复 -> 测试”流程补齐 reciprocal / rsqrt step 家族的 AArch64 FP 语义缺口：
  - [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp)
  - [tests/arm64/fpsimd_fp_step.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/fpsimd_fp_step.S)
  - [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh)
  - [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)
- 补上 `FRECPS/FRSQRTS`：
  - 标量 `FRECPS Sd/Dd, Sn/Dm`
  - 标量 `FRSQRTS Sd/Dd, Sn/Dm`
  - 向量 `FRECPS Vd.2S/.4S/.2D, Vn, Vm`
  - 向量 `FRSQRTS Vd.2S/.4S/.2D, Vn, Vm`
- 新增 fused-step helper，按手册的 A64 `FPRecipStepFused/FPRSqrtStepFused` 语义实现：
  - 第一个操作数先做符号翻转，再参与 NaN 处理；
  - `sNaN` 会 quiet 并置 `FPSR.IOC`；
  - `Inf x 0` / `0 x Inf` 的特殊值分别返回 `2.0` 与 `1.5`；
  - 精确零结果按 `FPCR.RMode` 生成 `+0/-0`；
  - 向量路径逐 lane 累积 `FPSR`。
- 修正了一个真实的 decode bug，而不是绕过问题：
  - vector `FRSQRTS` 与 `FDIV` 的掩码存在重叠，之前会落入错误的 `FDIV` 路径；
  - 现在将更具体的 `FRECPS/FRSQRTS` 识别放在前面，并分别匹配 `FRECPS` 与 `FRSQRTS` 的 opcode 基值。
- 新增裸机单测 [tests/arm64/fpsimd_fp_step.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/fpsimd_fp_step.S)：
  - 覆盖标量与向量、单精度与双精度；
  - 覆盖 `sNaN` quieting、`FPSR.IOC`、`Inf x 0` 特例；
  - 覆盖 `FPCR.RMode = RM` 时精确零结果的负零符号。

## 本轮测试

- 构建与定向验证：
  - `timeout 1200s cmake --build build -j`
  - `timeout 180s tests/arm64/build_tests.sh`
  - `timeout 60s ./build/aarchvm -bin tests/arm64/out/fpsimd_fp_step.bin -load 0x0 -entry 0x0 -steps 400000`
- 裸机完整回归：
  - `timeout 2400s tests/arm64/run_all.sh`
- Linux 单核功能回归：
  - `timeout 1200s tests/linux/run_functional_suite.sh`
- Linux SMP 功能回归：
  - `timeout 1200s tests/linux/run_functional_suite_smp.sh`

## 当前结论

- 这轮补齐的是 `FRECPS/FRSQRTS` 的真实 ISA 行为，不是测试特判。
- 新增裸机单测以及完整 bare-metal、Linux UMP、Linux SMP 回归均已通过。
- 继续往下审时，当前仍值得优先关注的高置信度缺口主要还有：
  - `FPCR` 更细的 `DN/FZ/AH` 影响；
  - reciprocal / rsqrt 家族中尚未覆盖的 `FRECPX`；
  - 更系统的 IEEE 例外位更新，尤其普通 FP 算术对 `IXC/UFC/OFC` 的一致性。

# 修改日志 2026-03-19 20:33

## 本轮修改

- 继续按“审阅 -> 修复 -> 测试”流程补齐 reciprocal / rsqrt estimate 家族的一组真实 FP 语义缺口：
  - [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp)
  - [tests/arm64/fpsimd_fp_estimate.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/fpsimd_fp_estimate.S)
  - [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh)
  - [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)
- 补上 `FRECPE/FRSQRTE`：
  - 标量 `FRECPE Sd/Dd, Sn/Dn`
  - 标量 `FRSQRTE Sd/Dd, Sn/Dn`
  - 向量 `FRECPE Vd.2S/.4S/.2D, Vn`
  - 向量 `FRSQRTE Vd.2S/.4S/.2D, Vn`
- 新增 reciprocal / rsqrt estimate helper：
  - 复用位级 NaN / Infinity / Zero 分类逻辑；
  - 按 Arm ARM 的整数 estimate 伪码生成近似值；
  - 正确处理 `sNaN` quieting 与 `FPSR.IOC`；
  - 处理 tiny 输入、零、无穷大与负数输入的边界语义。
- 修正了这组实现过程中的两个真实问题：
  - 标量 `FRECPE/FRSQRTE` 最初使用了未掩码的 case 常量，导致 masked `switch` 无法命中；
  - 标量 `ftype` 实际编码是 `2`/`3`，而不是 `0`/`1`。
- 同时修正了测试期望中的一个错误：
  - [tests/arm64/fpsimd_fp_estimate.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/fpsimd_fp_estimate.S)
  - `frsqrte s, 4.0f` 的正确期望值是 `0x3eff8000`，先前误写成了 `2.0f` 对应的结果。

## 本轮测试

- 构建与定向验证：
  - `timeout 1200s cmake --build build -j`
  - `timeout 180s tests/arm64/build_tests.sh`
  - `timeout 60s ./build/aarchvm -bin tests/arm64/out/fpsimd_fp_estimate.bin -load 0x0 -entry 0x0 -steps 400000`
- 裸机完整回归：
  - `timeout 2400s tests/arm64/run_all.sh`
- Linux 单核功能回归：
  - `timeout 1200s tests/linux/run_functional_suite.sh`
- Linux SMP 功能回归：
  - `timeout 1200s tests/linux/run_functional_suite_smp.sh`

## 当前结论

- 这轮补齐的是 `FRECPE/FRSQRTE` 的真实 ISA 语义，不是测试特判。
- 新增裸机单测以及完整 bare-metal、Linux UMP、Linux SMP 回归均已通过。
- 继续往下审时，当前仍值得优先关注的高置信度缺口主要还有：
  - reciprocal / rsqrt step 家族：`FRECPS/FRSQRTS`
  - `FPCR` 更细的 `DN/FZ/AH` 影响
  - 更系统的 IEEE 例外位更新，尤其普通 FP 算术对 `IXC/UFC/OFC` 的一致性

# 修改日志 2026-03-19 19:52

## 本轮修改

- 继续按“审阅 -> 修复 -> 测试”流程补齐一组真实可观测的 AdvSIMD FP 行为缺口：
  - [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp)
  - [tests/arm64/fpsimd_fcvtxn_roundodd.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/fpsimd_fcvtxn_roundodd.S)
  - [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh)
  - [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)
- 补上 `FCVTXN/FCVTXN2` 的 round-to-odd 窄化语义：
  - 标量 `FCVTXN Sd, Dn`
  - 向量 `FCVTXN Vd.2S, Vn.2D`
  - 向量 `FCVTXN2 Vd.4S, Vn.2D`
- 新增 `fp64_to_fp32_bits_round_to_odd(...)`，按手册实现 round-to-odd：
  - 仅在结果不精确时强制结果尾数最低位为 `1`；
  - sNaN 输入会 quiet 并置 `FPSR.IOC`；
  - 溢出按 `FCVTXN*` 语义饱和到最大有限值，而不是 `Inf`；
  - `FCVTXN2` 保留目标寄存器低半区，只覆盖高半区。
- 新增裸机单测 [tests/arm64/fpsimd_fcvtxn_roundodd.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/fpsimd_fcvtxn_roundodd.S)：
  - 覆盖标量与向量路径；
  - 覆盖正负 inexact、最大有限值饱和、sNaN quieting、`FPSR.IOC/IXC`；
  - 覆盖 `FCVTXN2` 的高半区写回语义。

## 本轮测试

- 构建与定向验证：
  - `timeout 1200s cmake --build build -j`
  - `timeout 180s tests/arm64/build_tests.sh`
  - `timeout 60s ./build/aarchvm -bin tests/arm64/out/fpsimd_fcvtxn_roundodd.bin -load 0x0 -entry 0x0 -steps 400000`
- 裸机完整回归：
  - `timeout 2400s tests/arm64/run_all.sh`
- Linux 单核功能回归：
  - `timeout 1200s tests/linux/run_functional_suite.sh`
- Linux SMP 功能回归：
  - `timeout 1200s tests/linux/run_functional_suite_smp.sh`

## 当前结论

- 这轮修掉的是 `FCVTXN/FCVTXN2` 的真实 ISA 语义缺口，不是测试特判。
- 新增裸机单测以及完整 bare-metal、Linux UMP、Linux SMP 回归均已通过。
- 继续往下审时，当前仍值得优先关注的高置信度缺口主要还有：
  - reciprocal / rsqrt estimate 家族：`FRECPE/FRECPS/FRSQRTE/FRSQRTS`
  - 更完整的 `FPCR` 行为细节，尤其 `DN/FZ/AH` 对边界输入的影响
  - 继续系统排查尚未覆盖的 AdvSIMD reduction / conversion 角落语义

# 修改日志 2026-03-19 19:29

## 本轮修改

- 继续按“审阅 -> 修复 -> 测试”流程补齐一组真实可观测的 AdvSIMD FP 行为缺口：
  - [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp)
  - [tests/arm64/fp_scalar_pairwise.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/fp_scalar_pairwise.S)
  - [tests/arm64/fpsimd_fp_pairwise.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/fpsimd_fp_pairwise.S)
  - [tests/arm64/fpsimd_fp_convert_long_narrow.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/fpsimd_fp_convert_long_narrow.S)
  - [tests/arm64/fpsimd_fp_reducev.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/fpsimd_fp_reducev.S)
  - [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh)
  - [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)
- 补上 pairwise FP 家族：
  - 标量 `FADDP/FMAXP/FMINP/FMAXNMP/FMINNMP`
  - 向量 `FADDP/FMAXP/FMINP/FMAXNMP/FMINNMP`
  - 最小值/最大值的 qNaN/sNaN、`FPSR.IOC` 和带符号零规则统一复用现有 `fp_minmax_result_bits(...)`。
- 补上向量浮点窄化/加宽转换：
  - `FCVTL/FCVTL2 Vd.2D, Vn.2S/.4S`
  - `FCVTN/FCVTN2 Vd.2S/.4S, Vn.2D`
  - `FCVTN` 会清高半区，`FCVTN2` 保留低半区；
  - `FCVTN*` 按 `FPCR.RMode` 做舍入，并正确累积 `FPSR.IXC/IOC`；
  - `FCVTL*` 对 sNaN 做 quieting，并更新 `FPSR.IOC`。
- 补上向量 reduction-to-scalar 家族：
  - `FMAXV/FMINV/FMAXNMV/FMINNMV`
  - 这组实现按手册的 `FPReduce(...)` 递归 pairwise 语义执行，而不是线性 fold，避免后续在 NaN 传播细节上留下偏差。
- 新增四组裸机单测：
  - [tests/arm64/fp_scalar_pairwise.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/fp_scalar_pairwise.S)
  - [tests/arm64/fpsimd_fp_pairwise.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/fpsimd_fp_pairwise.S)
  - [tests/arm64/fpsimd_fp_convert_long_narrow.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/fpsimd_fp_convert_long_narrow.S)
  - [tests/arm64/fpsimd_fp_reducev.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/fpsimd_fp_reducev.S)
  - 覆盖普通值、不同 rounding mode、qNaN/sNaN、`FPSR` 标志、带符号零以及 `FCVTN2` 的高半区写回语义。

## 本轮测试

- 构建与定向验证：
  - `timeout 1200s cmake --build build -j`
  - `timeout 180s tests/arm64/build_tests.sh`
  - `timeout 60s ./build/aarchvm -bin tests/arm64/out/fpsimd_fp_convert_long_narrow.bin -load 0x0 -entry 0x0 -steps 400000`
  - `timeout 60s ./build/aarchvm -bin tests/arm64/out/fpsimd_fp_reducev.bin -load 0x0 -entry 0x0 -steps 400000`
- 裸机完整回归：
  - `timeout 2400s tests/arm64/run_all.sh`
- Linux 单核功能回归：
  - `timeout 1200s tests/linux/run_functional_suite.sh`
- Linux SMP 功能回归：
  - `timeout 1200s tests/linux/run_functional_suite_smp.sh`

## 当前结论

- 这轮修掉的是三组真实 ISA 行为缺口，不是测试特判：
  - pairwise FP 家族；
  - `FCVTL/FCVTL2/FCVTN/FCVTN2`；
  - `FMAXV/FMINV/FMAXNMV/FMINNMV`。
- 新增裸机单测以及完整 bare-metal、Linux UMP、Linux SMP 回归均已通过。
- 继续往下审时，当前仍值得优先关注的高置信度缺口主要还有：
  - `FCVTXN/FCVTXN2`
  - reciprocal / rsqrt estimate 家族：`FRECPE/FRECPS/FRSQRTE/FRSQRTS`
  - 更完整的 `FPCR` 行为细节，尤其 `DN/FZ/AH` 对边界输入的影响

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

# 修改日志 2026-03-19 19:02

## 本轮修改

- 继续按“审阅 -> 修复 -> 测试”流程补齐两组此前真实缺失的 FP/AdvSIMD pairwise 指令族：
  - [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp)
  - [tests/arm64/fpsimd_fp_pairwise.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/fpsimd_fp_pairwise.S)
  - [tests/arm64/fp_scalar_pairwise.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/fp_scalar_pairwise.S)
  - [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh)
  - [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)
- 在 `cpu.cpp` 中新增向量 pairwise 浮点执行路径：
  - `FADDP`
  - `FMAXP/FMINP`
  - `FMAXNMP/FMINNMP`
- 这组向量 pairwise 实现覆盖：
  - `.2s/.4s/.2d` 三种布局；
  - 标准 NaN 传播与 `NM` 数值优先语义；
  - `sNaN` 触发 `FPSR.IOC`，并按现有 `fp_minmax_result_bits()` 统一处理 quiet NaN。
- 修正了这组向量 pairwise 的 decode 根因问题：
  - 最初 case 常量不完整；
  - 更关键的是原先 `switch` 掩码错误地保留了 `Rm` 的一位，导致 opcode 会随寄存器编号漂移；
  - 现已改成稳定掩码 `0xFFE0FC00`，避免落入旧的逐 lane `FADD/FMAXNM/...` 分支。
- 新增裸机单测 [tests/arm64/fpsimd_fp_pairwise.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/fpsimd_fp_pairwise.S)，覆盖：
  - `FADDP .2s/.2d`
  - `FMAXP .4s`
  - `FMINP .2s`
  - `FMAXNMP .2d`
  - `FMINNMP .4s`
  - qNaN / sNaN / `FPSR.IOC` 行为。
- 在 `cpu.cpp` 中新增标量 pairwise 浮点执行路径：
  - `FADDP Sd, Vn.2S`
  - `FADDP Dd, Vn.2D`
  - `FMAXP/FMINP`
  - `FMAXNMP/FMINNMP`
- 新增裸机单测 [tests/arm64/fp_scalar_pairwise.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/fp_scalar_pairwise.S)，覆盖：
  - 标量 `FADDP` 的单精/双精形式；
  - 标量 `FMAXP/FMINP`；
  - 标量 `FMAXNMP/FMINNMP`；
  - qNaN / sNaN / `FPSR.IOC`。

## 本轮测试

- 定向验证：
  - `timeout 1200s cmake --build build -j`
  - `timeout 120s tests/arm64/build_tests.sh`
  - `timeout 60s ./build/aarchvm -bin tests/arm64/out/fpsimd_fp_pairwise.bin -load 0x0 -entry 0x0 -steps 400000`
  - `timeout 60s ./build/aarchvm -bin tests/arm64/out/fp_scalar_pairwise.bin -load 0x0 -entry 0x0 -steps 300000`
- 裸机完整回归：
  - `timeout 2400s tests/arm64/run_all.sh`
- Linux 单核功能回归：
  - `timeout 1200s tests/linux/run_functional_suite.sh`
- Linux SMP 功能回归：
  - `timeout 1200s tests/linux/run_functional_suite_smp.sh`

## 当前结论

- 这轮补的是 pairwise 浮点家族的真实 ISA 缺口，而不是测试特判。
- 向量与标量 pairwise 浮点加法、最值、数值优先最值现在都已形成“实现 + 裸机单测 + 裸机全回归 + Linux UMP/SMP 回归”的闭环。
- 我仍不认为当前已经“完整实现 Armv8-A ISA 的全部强制行为”。继续审时，仍值得优先关注：
  - reciprocal / rsqrt estimate 与 step 家族，如 `FRECPE/FRECPS/FRSQRTE/FRSQRTS`；
  - 更高阶的精度转换族，尤其 `FCVTN/FCVTN2/FCVTL/FCVTL2/FCVTXN*` 对 `FPCR` 舍入模式与异常位的完整语义；
  - 更细粒度的 `FPCR` 行为，如 `AH/FZ/FZ16/DN` 与 NaN / subnormal / flush-to-zero 的交互。

# 修改日志 2026-03-20 12:25

## 本轮修改

- 修正标量 SIMD&FP compare 结果类指令漏置 `FPSR.IOC` 的程序可见语义缺口：
  - [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp)
- 具体覆盖的指令族：
  - 标量寄存器 compare：`FCMEQ/FCMGE/FCMGT`
  - 标量绝对值 compare：`FACGE/FACGT`
  - 标量零比较：`FCMEQ/FCMGE/FCMGT/FCMLE/FCMLT ..., #0.0`
- 修复内容：
  - 当输入含 `sNaN` 时，这些路径现在会像向量 compare 路径一样正确置 `FPSR.IOC`；
  - 当输入仅含 `qNaN` 时，仍保持 quiet compare 语义，不误置 `IOC`；
  - 结果寄存器的 all-ones / zero compare 掩码行为保持不变，只修正异常状态寄存器的程序可见结果。
- 新增专门的裸机单测 [tests/arm64/fp_scalar_compare_flags.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/fp_scalar_compare_flags.S)，并接入：
  - [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh)
  - [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)
- 新单测覆盖：
  - `FCMEQ` 标量寄存器 compare 遇到 `qNaN` 时结果为假且 `FPSR` 保持清零；
  - `FCMGE` 标量寄存器 compare 遇到 `sNaN` 时结果为假且 `FPSR.IOC=1`；
  - `FACGT` 标量绝对值 compare 遇到 `sNaN` 时结果为假且 `FPSR.IOC=1`；
  - `FCMGT ..., #0.0` 标量零 compare 遇到 `sNaN` 时结果为假且 `FPSR.IOC=1`。

## 本轮测试

- 定向构建：
  - `timeout 1200s cmake --build build -j`
  - `timeout 300s tests/arm64/build_tests.sh`
- 定向验证：
  - `timeout 30s ./build/aarchvm -bin tests/arm64/out/fp_scalar_compare_flags.bin -load 0x0 -entry 0x0 -steps 200000`
  - `timeout 30s ./build/aarchvm -bin tests/arm64/out/fpsimd_compare_flags.bin -load 0x0 -entry 0x0 -steps 300000`
- 裸机完整回归：
  - `timeout 2400s tests/arm64/run_all.sh`
- Linux 单核功能回归：
  - `timeout 1800s tests/linux/run_functional_suite.sh`
- Linux SMP 功能回归：
  - `timeout 2400s tests/linux/run_functional_suite_smp.sh`

## 当前结论

- 这轮补的是一个“结果寄存器对，但异常状态错”的真实程序可见缺口，不是测试绕过。
- 向量 compare 家族与标量 compare 家族在 `FPSR.IOC` 语义上现在已经对齐。
- `TODO` 中“浮点 / AdvSIMD 语义收尾”仍未完成；目前下一批仍值得优先审的点是：
  - `FPCR.DN/FZ/AH` 对 arithmetic / convert / sqrt / estimate / round-int helper 的真实影响；
  - `FPSR` 其余异常位在标量与向量 convert / sqrt / estimate 路径中的一致性；
  - `qNaN/sNaN/default-NaN/subnormal/±0` 在尚未系统验证的 FP helper 中的传播细节。

# 修改日志 2026-03-20 01:47

## 本轮修改

- 修正 `FMAX/FMIN/FMAXNM/FMINNM` 家族对 `FPCR.DN` 的程序可见语义缺口：
  - [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp)
- 具体覆盖的路径：
  - 标量 `FMAX/FMIN/FMAXNM/FMINNM`
  - 向量 `FMAX/FMIN/FMAXNM/FMINNM`
  - pairwise `FMAXP/FMINP/FMAXNMP/FMINNMP`
  - reduce `FMAXV/FMINV/FMAXNMV/FMINNMV`
- 修复内容：
  - `FMAX/FMIN` 在 `DN=1` 且任一输入为 NaN 时，现在返回 `Default NaN`；
  - `FMAXNM/FMINNM` 在 `DN=1` 时，现在仅在“任一输入为 `sNaN`”或“两边都为 NaN”时返回 `Default NaN`，而 `numeric + qNaN` 仍返回 numeric；
  - 所有相关调用点统一传入当前 `FPCR` 控制位，而不是仅按舍入模式处理。
- 新增专门的裸机单测 [tests/arm64/fp_minmax_dn.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/fp_minmax_dn.S)，并接入：
  - [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh)
  - [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)
- 新单测覆盖：
  - 标量 `FMAX` + `qNaN` 在 `DN=1` 下返回 `Default NaN`
  - 标量 `FMINNM` + `qNaN` 在 `DN=1` 下保留 numeric
  - 标量 `FMAXNM` 在双 `qNaN` 下返回 `Default NaN`
  - 标量 `FMIN` + `sNaN` 在 `DN=1` 下返回 `Default NaN` 且置 `FPSR.IOC`
  - pairwise / vector / reduce 路径下的 `DN=1` 传播语义

## 本轮测试

- 定向验证：
  - `timeout 60s ./build/aarchvm -bin tests/arm64/out/fp_minmax_dn.bin -load 0x0 -entry 0x0 -steps 400000`
  - `timeout 60s ./build/aarchvm -bin tests/arm64/out/fp_minmax_nan_flags.bin -load 0x0 -entry 0x0 -steps 300000`
  - `timeout 60s ./build/aarchvm -bin tests/arm64/out/fp_scalar_pairwise.bin -load 0x0 -entry 0x0 -steps 300000`
  - `timeout 60s ./build/aarchvm -bin tests/arm64/out/fpsimd_fp_reducev.bin -load 0x0 -entry 0x0 -steps 400000`
- 裸机完整回归：
  - `timeout 2400s tests/arm64/run_all.sh`
- Linux 单核功能回归：
  - `timeout 1800s tests/linux/run_functional_suite.sh`
- Linux SMP 功能回归：
  - `timeout 2400s tests/linux/run_functional_suite_smp.sh`

## 当前结论

- 这轮补的是 `FPCR.DN` 对 min/max 家族的真实 ISA 语义缺口，不是测试特判。
- 当前 `FMAX/FMIN/FMAXNM/FMINNM` 的标量、向量、pairwise、reduce 路径在 `DN=1` 下已经对齐到架构手册要求。
- “Armv8-A 程序可见正确性收尾计划”仍未结束；下一批仍值得优先审的点是：
  - `FPCR.FZ/FZ16/AH` 在 arithmetic / convert / sqrt / estimate helper 中的系统性影响；
  - `Default NaN` 与 `subnormal` 语义在其余 FP helper 中的一致性；
  - 尚未系统验证的 FP convert / estimate / misc 路径在 `DN/FZ` 组合下的边界行为。

# 修改日志 2026-03-20 02:39

## 本轮修改

- 修正 `FRECPS/FRSQRTS` 标量与向量路径对 `FPCR.RMode` 和 `FPSR.IXC` 的程序可见语义缺口：
  - [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp)
- 修复方式：
  - 保留原有 `NaN`、`Inf*0`、精确零结果等显式特判；
  - 将普通计算路径统一切到 `host_fp_eval(...)`，使其真正受当前 `FPCR.RMode` 控制；
  - 将宿主浮点环境抛出的异常映射回 `FPSR`，补齐此前缺失的 `IXC` 置位。
- 扩展裸机单测 [tests/arm64/fpsimd_fp_step.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/fpsimd_fp_step.S)：
  - 新增 `FRECPS` / `FRSQRTS` 的标量 `S/D` inexact case；
  - 新增向量 `4S` inexact case；
  - 对 `RNE` 与 `RM` 两种舍入模式分别校验结果 bit pattern；
  - 对上述 case 明确校验 `FPSR.IXC=1`。

## 本轮测试

- 定向构建：
  - `timeout 1200s cmake --build build -j`
  - `timeout 300s tests/arm64/build_tests.sh`
- 定向验证：
  - `timeout 60s ./build/aarchvm -bin tests/arm64/out/fpsimd_fp_step.bin -load 0x0 -entry 0x0 -steps 400000`
- 裸机完整回归：
  - `timeout 2400s tests/arm64/run_all.sh`
- Linux 单核功能回归：
  - `timeout 1800s tests/linux/run_functional_suite.sh`
- Linux SMP 功能回归：
  - `timeout 2400s tests/linux/run_functional_suite_smp.sh`

## 当前结论

- 这轮补的是 `FRECPS/FRSQRTS` 的真实程序可见语义缺口，不是针对特定测试点的绕过。
- 当前 `FRECPS/FRSQRTS` 已经不再固定使用宿主默认舍入模式；`RNE/RM` 在标量与向量路径都能观察到正确结果差异。
- 对 `FRECPS/FRSQRTS`，此前遗漏的 `FPSR.IXC` 现在已经能在 inexact case 中稳定置位。
- `TODO` 中“浮点 / AdvSIMD 语义收尾”仍未结束；下一批仍值得优先审的点是：
  - `FPCR.DN/FZ/AH` 在其余 arithmetic / convert / sqrt / estimate / round-int helper 中的系统性影响；
  - 其余 FP helper 的 `FPSR` 置位一致性；
  - 尚未系统差分过的 `qNaN/sNaN/default-NaN/subnormal/±0/Inf` 传播细节。

# 修改日志 2026-03-20 10:49

## 本轮修改

- 修正 `FRECPS/FRSQRTS` 标量与向量路径在 `FPCR.FZ=1` 下的程序可见语义缺口：
  - [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp)
- 修复方式：
  - 在 `NaN` 处理后，对输入统一执行 `fp_flush_input_denormal_bits(...)`；
  - 后续 `Inf/Zero` 特判与普通计算统一基于 flush 后的操作数；
  - 普通路径补齐宿主浮点异常到 `FPSR` 的映射；
  - 结果返回前统一执行 `fp_flush_output_denormal_bits(...)`，保证 `FZ` 输出语义一致。
- 扩展裸机单测 [tests/arm64/fpsimd_fp_step.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/fpsimd_fp_step.S)：
  - 新增 `FRECPS` / `FRSQRTS` 的标量 `S/D` `FZ=1` case；
  - 新增向量 `4S` `FZ=1` case；
  - 校验 subnormal 输入在 `FZ=1` 下分别得到 `2.0` / `1.5`；
  - 校验 `FPSR.IDC=1`。
- 同时修正该测试文件中的访存偏移超范围问题，避免 `str q..., [xn,#imm]` 超编码范围导致的组装失败。

## 本轮测试

- 定向构建：
  - `timeout 1200s cmake --build build -j`
  - `timeout 300s tests/arm64/build_tests.sh`
- 定向验证：
  - `timeout 60s ./build/aarchvm -bin tests/arm64/out/fpsimd_fp_step.bin -load 0x0 -entry 0x0 -steps 600000`
- 裸机完整回归：
  - `timeout 2400s tests/arm64/run_all.sh`
- Linux 单核功能回归：
  - `timeout 1800s tests/linux/run_functional_suite.sh`
- Linux SMP 功能回归：
  - `timeout 2400s tests/linux/run_functional_suite_smp.sh`

## 当前结论

- 这轮补的是 `FRECPS/FRSQRTS` 在 `FPCR.FZ` 下的真实 ISA 语义，不是利用特定程序路径绕过。
- 对 `FRECPS/FRSQRTS`，subnormal 输入现在会先按 `FZ` 规则视为零，再进入后续 special-case 与普通计算路径，因此标量和向量行为已经一致。
- `qemu-aarch64` 在这轮只被当作局部 FP ISA 语义探针，不能替代 system 级结论；system 级正确性仍以本模拟器裸机单测和 Linux 回归为准。
- “Armv8-A 程序可见正确性收尾计划”仍未结束；下一批仍值得优先审的点是：
  - 其余 FP/AdvSIMD helper 中 `FPCR.FZ/FZ16/AH` 的系统性一致性；
  - 其余 estimate / convert / misc 路径的 `FPSR` 置位完整性；
  - 尚未系统覆盖的 `default-NaN`、`subnormal`、`±0`、`Inf` 组合传播细节。

# 修改日志 2026-03-20 11:04

## 本轮修改

- 修正 `FCVTL/FCVTN/FCVTXN` 在 `FPCR.FZ=1` 下的程序可见语义缺口：
  - [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp)
- 修复方式：
  - `fp32_to_fp64_bits(...)` 在 NaN/Inf 处理后补上 `fp_flush_input_denormal_bits(...)`；
  - `fp64_to_fp32_bits(...)` 与 `fp64_to_fp32_bits_round_to_odd(...)` 同时补上 input flush；
  - narrowing 路径在生成结果后统一补上 `fp_flush_output_denormal_bits(...)`，使 `FZ` 下的 subnormal 输出按架构规则变成零并置 `UFC`。
- 扩展裸机单测 [tests/arm64/fp_fz_misc.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/fp_fz_misc.S)：
  - 新增 `FCVTXN` subnormal input case，校验结果为零且 `FPSR.IDC=1`；
  - 新增 `FCVTXN` tiny-normal-to-subnormal-output case，校验结果为零且 `FPSR.UFC=1`；
  - 新增 `FCVTL` subnormal input case；
  - 新增 `FCVTN` 的 input-flush 与 output-flush case。

## 本轮测试

- ISA 行为探针：
  - 使用 `qemu-aarch64` 对临时 userspace 探针确认局部 FP ISA 语义：
    - subnormal 输入被 `FZ` 吞掉时置 `FPSR.IDC`
    - 正常输入但 narrowing 结果落入 subnormal、再被 `FZ` 吞掉时置 `FPSR.UFC`
  - 注意：这里只把 `qemu-aarch64` 当局部 ISA 语义探针，不作为 system 级结论。
- 定向构建：
  - `timeout 1200s cmake --build build -j`
  - `timeout 300s tests/arm64/build_tests.sh`
- 定向验证：
  - `timeout 60s ./build/aarchvm -bin tests/arm64/out/fp_fz_misc.bin -load 0x0 -entry 0x0 -steps 500000`
  - `timeout 60s ./build/aarchvm -bin tests/arm64/out/fpsimd_fp_convert_long_narrow.bin -load 0x0 -entry 0x0 -steps 500000`
  - `timeout 60s ./build/aarchvm -bin tests/arm64/out/fpsimd_fcvtxn_roundodd.bin -load 0x0 -entry 0x0 -steps 500000`
- 裸机完整回归：
  - `timeout 2400s tests/arm64/run_all.sh`
- Linux 单核功能回归：
  - `timeout 1800s tests/linux/run_functional_suite.sh`
- Linux SMP 功能回归：
  - `timeout 2400s tests/linux/run_functional_suite_smp.sh`

## 当前结论

- 这轮补的是 conversion 家族在 `FPCR.FZ` 下的一组真实程序可见缺口，不是对某个具体测试点做特判。
- 当前 `FCVTL/FCVTN/FCVTXN` 已能区分两类 `FZ` 语义：
  - subnormal 输入先被 flush，结果为零并置 `IDC`
  - 正常输入但结果会成为 subnormal 时，输出被 flush，结果为零并置 `UFC`
- `TODO` 中“浮点 / AdvSIMD 语义收尾”这一大项仍未完成，因此本轮不勾选复选框；下一批仍值得优先审的点是：
  - 其余 conversion / misc / compare helper 中 `FZ` 一致性；
  - `FPSR` 各异常位在标量与向量路径中的对齐情况；
  - `AH` 相关语义是否仍有程序可见缺口。

# 修改日志 2026-03-20 11:28

## 本轮修改

- 修正 `FP -> Int` conversion 在 `FPCR.FZ=1` 下的程序可见语义缺口：
  - [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp)
- 修复范围：
  - 标量写回 GPR 的 `FCVTNS/NU/AS/AU/PS/PU/MS/MU/ZS/ZU`
  - 标量写回 FP/SIMD 寄存器的 `FCVTZS`
  - 向量 `FCVTNS/NU/AS/AU/PS/PU/MS/MU`
- 修复方式：
  - 为 `FP -> Int` 路径补上统一的 `FZ` 输入 flush 预处理；
  - 在 flush 阶段保留 `FPSR.IDC`，再与后续 conversion 阶段的 `IOC/IXC` 合并，而不是被后续 helper 清掉。
- 同时修正一处真实的向量解码缺口：
  - 向量 `FCVTZS/FCVTZU` 之前没有被单独识别，且会因为掩码过宽误落入别的 `FCVT*` 路径；
  - 现已为向量 `FCVTZS/FCVTZU`（`4S/2D`）补独立 decode/execute 路径。
- 新增专用裸机单测：
  - [tests/arm64/fp_fz_to_int.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/fp_fz_to_int.S)
  - 覆盖标量 GPR 结果、标量 FP-reg 结果、向量 `FCVTZS/FCVTZU` 的 `4S/2D` 路径；
  - 校验 subnormal 输入在 `FZ=1` 下得到零结果，并置 `FPSR.IDC=1`。
- 测试框架接线更新：
  - [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh)
  - [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)

## 本轮测试

- ISA 行为探针：
  - 使用 `qemu-aarch64` 对临时 userspace 探针确认局部 FP ISA 语义：
    - `FCVTNS/ZS/ZU` 的 scalar subnormal 输入在 `FZ=1` 下返回零并置 `FPSR.IDC`
    - 向量 `FCVTZS/FCVTZU` 的 `4S/2D` subnormal 输入在 `FZ=1` 下返回全零并置 `FPSR.IDC`
  - 注意：这里只把 `qemu-aarch64` 当局部 ISA 语义探针，不作为 system 级结论。
- 定向构建：
  - `timeout 1200s cmake --build build -j`
  - `timeout 300s tests/arm64/build_tests.sh`
- 定向验证：
  - `timeout 60s ./build/aarchvm -bin tests/arm64/out/fp_fz_to_int.bin -load 0x0 -entry 0x0 -steps 500000`
  - `timeout 60s ./build/aarchvm -bin tests/arm64/out/fp_fcvt_flags.bin -load 0x0 -entry 0x0 -steps 300000`
  - `timeout 60s ./build/aarchvm -bin tests/arm64/out/fp_fcvt_rounding_scalar.bin -load 0x0 -entry 0x0 -steps 300000`
  - `timeout 60s ./build/aarchvm -bin tests/arm64/out/fpsimd_fcvt_rounding.bin -load 0x0 -entry 0x0 -steps 500000`
- 裸机完整回归：
  - `timeout 2400s tests/arm64/run_all.sh`
- Linux 单核功能回归：
  - `timeout 1800s tests/linux/run_functional_suite.sh`
- Linux SMP 功能回归：
  - `timeout 2400s tests/linux/run_functional_suite_smp.sh`

## 当前结论

- 这轮补的是 `FP -> Int` conversion 家族在 `FPCR.FZ` 下的一组真实程序可见缺口，不是测试特判。
- 这轮还顺手补齐了一个真实 decode 洞：向量 `FCVTZS/FCVTZU` 现在不再误落进其它 `FCVT*` 路径。
- `TODO` 中“浮点 / AdvSIMD 语义收尾”这一大项仍未完成，因此本轮仍不勾选复选框；下一批仍值得优先审的点是：
  - 其余 compare / misc / estimate helper 中 `FZ` 的一致性；
  - `FPSR` 各异常位在剩余标量/向量路径中的对齐情况；
  - `AH` 与剩余 NaN/subnormal 传播细节。

# 修改日志 2026-03-20 11:41

## 本轮修改

- 修正 `FMAX/FMIN/FMAXNM/FMINNM` 及其共享 helper 在 `FPCR.FZ=1` 下的程序可见语义缺口：
  - [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp)
- 修复方式：
  - `fp_minmax_result_bits(...)` 入口统一补上 `fp_flush_input_denormal_bits(...)`；
  - 被 flush 的 denormal 输入现在会正确按架构语义变成带符号零，并置 `FPSR.IDC`；
  - 对 `FMAXNM/FMINNM` 路径，返回结果统一走 `finalize(...)`，保留 numeric 变体的后处理闭环。
- 新增专用裸机单测：
  - [tests/arm64/fp_fz_minmax.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/fp_fz_minmax.S)
  - 覆盖标量 `S/D` 与向量 `4S` 路径；
  - 覆盖 `FMAX/FMIN` 与 `FMAXNM/FMINNM`；
  - 校验正负 subnormal 在 `FZ=1` 下被视为 `+0/-0`；
  - 校验 `qNaN + subnormal` 的 numeric min/max 结果不再把原始 denormal 漏回结果；
  - 校验 `FPSR.IDC=1`。
- 测试框架接线更新：
  - [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh)
  - [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)

## 本轮测试

- 定向构建：
  - `timeout 1200s cmake --build build -j`
  - `timeout 300s tests/arm64/build_tests.sh`
- 定向验证：
  - `timeout 60s ./build/aarchvm -bin tests/arm64/out/fp_fz_minmax.bin -load 0x0 -entry 0x0 -steps 400000`
  - `timeout 60s ./build/aarchvm -bin tests/arm64/out/fp_minmax_nan_flags.bin -load 0x0 -entry 0x0 -steps 400000`
  - `timeout 60s ./build/aarchvm -bin tests/arm64/out/fp_minmax_dn.bin -load 0x0 -entry 0x0 -steps 500000`
- 裸机完整回归：
  - `timeout 2400s tests/arm64/run_all.sh`
- Linux 单核功能回归：
  - `timeout 1800s tests/linux/run_functional_suite.sh`
- Linux SMP 功能回归：
  - `timeout 2400s tests/linux/run_functional_suite_smp.sh`

## 当前结论

- 这轮补的是 `min/max` 家族在 `FPCR.FZ` 下的真实 ISA 语义，而不是对特定程序路径做绕过。
- 之前 `fp_minmax_result_bits(...)` 完全没有处理 `FZ`，因此：
  - denormal 输入会被错误地直接参与比较；
  - numeric min/max 在 `qNaN + denormal` 组合下会把原始 denormal 直接漏回结果；
  - `FPSR.IDC` 也不会正确置位。
- 现在 `FMAX/FMIN/FMAXNM/FMINNM` 的共享 helper 已经把这条语义链补齐；这也同时覆盖了使用同一 helper 的向量、pairwise、reduce 变体的核心结果选择逻辑。
- `TODO` 中“浮点 / AdvSIMD 语义收尾”这一大项仍未完成，因此本轮仍不勾选复选框；下一批仍值得优先审的点是：
  - 其余 FP helper 中 `FZ`/`DN`/异常位映射是否还存在边角不一致；
  - 尚未系统覆盖的 `FRINT*`、compare/misc 与 NaN payload/sign 传播细节；
  - 如果未来宣告 `FEAT_AFP`，则需要单独补 `FPCR.AH` 的真实语义，而不是沿用当前“等价于 AH=0”的行为。

# 修改日志 2026-03-20 12:11

## 本轮修改

- 扩充 `FRECPX` 标量裸机单测覆盖：
  - [tests/arm64/fp_scalar_frecpx.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/fp_scalar_frecpx.S)
- 新增覆盖的程序可见边界：
  - `qNaN` 在 `DN=0` 下保持 payload、`FPSR` 不置位；
  - `qNaN/sNaN` 在 `DN=1` 下返回 default NaN，且仅 `sNaN` 置 `FPSR.IOC`；
  - `FZ=1` 下负 subnormal 输入会被视为带符号零，结果保留符号并置 `FPSR.IDC`。
- 这轮没有修改模拟器执行语义；新增的是此前缺失的回归覆盖，用于把 `FRECPX` 的 `DN/FZ/NaN/符号` 边界纳入常规验证。

## 本轮测试

- 局部 ISA 语义探针：
  - 使用 `qemu-aarch64` 对 `FRECPX` 的 `qNaN/sNaN`、`DN`、`FZ`、`±0/±Inf/负 subnormal` 行为做 userspace 局部核对。
  - 注意：这里只把 `qemu-aarch64` 当单条 ISA 语义探针，不作为 system 级结论。
- 定向构建：
  - `timeout 300s tests/arm64/build_tests.sh`
- 定向验证：
  - `timeout 60s ./build/aarchvm -bin tests/arm64/out/fp_scalar_frecpx.bin -load 0x0 -entry 0x0 -steps 500000`
- 裸机完整回归：
  - `timeout 2400s tests/arm64/run_all.sh`
- Linux 单核功能回归：
  - `timeout 1800s tests/linux/run_functional_suite.sh`
- Linux SMP 功能回归：
  - `timeout 2400s tests/linux/run_functional_suite_smp.sh`

## 当前结论

- 这轮暂未发现 `FRECPX` 的真实实现错误；当前 helper 行为与这批新核对的边界语义一致。
- 但此前 `FRECPX` 的回归只覆盖了常规 normal/zero/subnormal/Inf/sNaN 路径，没有把 `DN` 和 `FZ` 下的若干关键边界固定住，因此仍存在未来回归时无报警退化的风险。
- 这轮补完后，`FRECPX` 在 `NaN payload/default-NaN`、`IOC/IDC`、以及符号保持这几条线上都有了稳定回归。

# 修改日志 2026-03-20 12:18

## 本轮修改

- 扩充 `FP estimate` 家族裸机单测覆盖：
  - [tests/arm64/fpsimd_fp_estimate.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/fpsimd_fp_estimate.S)
- 新增覆盖的程序可见边界：
  - `FPCR.DN=1` 下向量 `FRECPE` 对 `{qNaN, sNaN, +Inf, -0.0}` 的返回值与异常位；
  - `FPCR.DN=1` 下向量 `FRSQRTE` 对 `{qNaN, sNaN}` 的 default-NaN 行为与 `IOC`；
  - 标量 `FRECPE/FRSQRTE` 对 `-0.0` 的 `-Inf` 结果与 `DZC`。
- 这轮没有修改模拟器执行语义；新增的是此前缺失的 correctness coverage，用于把 `estimate` 家族在 `DN/NaN/负零` 这些边界固定进常规回归。

## 本轮测试

- 局部 ISA 语义探针：
  - 使用 `qemu-aarch64` 对上述 `FRECPE/FRSQRTE` 边界做 userspace 局部核对。
  - 注意：这里只把 `qemu-aarch64` 当单条 ISA 语义探针，不作为 system 级结论。
- 定向构建：
  - `timeout 300s tests/arm64/build_tests.sh`
- 定向验证：
  - `timeout 60s ./build/aarchvm -bin tests/arm64/out/fpsimd_fp_estimate.bin -load 0x0 -entry 0x0 -steps 500000`
- 裸机完整回归：
  - `timeout 2400s tests/arm64/run_all.sh`
- Linux 单核功能回归：
  - `timeout 1800s tests/linux/run_functional_suite.sh`
- Linux SMP 功能回归：
  - `timeout 2400s tests/linux/run_functional_suite_smp.sh`

## 当前结论

- 这轮暂未发现 `FRECPE/FRSQRTE estimate` helper 的真实实现错误；当前行为与这批新核对的边界一致。
- 真实收获是把原先未被回归固定住的 `DN/default-NaN/FPSR` 边界补齐，降低未来回归无告警退化的风险。

# 修改日志 2026-03-20 12:27

## 本轮修改

- 扩充 `FRINT* / round-int` 家族裸机单测覆盖：
  - [tests/arm64/fp_roundint_flags.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/fp_roundint_flags.S)
- 新增覆盖的程序可见边界：
  - `FPCR.DN=1` 下向量 `FRINTN` 对 `{qNaN, sNaN, normal, negative subnormal}` 的结果与 `FPSR.IOC`；
  - `FPCR.FZ=1` 下标量 `FRINTX` 对负 subnormal 的 signed-zero 结果与 `FPSR.IDC`；
  - `FPCR.FZ=1` 下向量 `FRINTI` 对正负 subnormal / signed zero 的逐 lane 结果与聚合 `FPSR.IDC`。
- 这轮没有修改模拟器执行语义；新增的是此前缺失的 correctness coverage，用于把 `round-int` 家族在 `DN/FZ/NaN/subnormal` 这些边界固定进常规回归。

## 本轮测试

- 局部 ISA 语义探针：
  - 使用 `qemu-aarch64` 对上述 `FRINTN/FRINTX/FRINTI` 边界做 userspace 局部核对。
  - 注意：这里只把 `qemu-aarch64` 当单条 ISA 语义探针，不作为 system 级结论。
- 定向构建：
  - `timeout 300s tests/arm64/build_tests.sh`
- 定向验证：
  - `timeout 60s ./build/aarchvm -bin tests/arm64/out/fp_roundint_flags.bin -load 0x0 -entry 0x0 -steps 500000`
- 裸机完整回归：
  - `timeout 2400s tests/arm64/run_all.sh`
- Linux 单核功能回归：
  - `timeout 1800s tests/linux/run_functional_suite.sh`
- Linux SMP 功能回归：
  - `timeout 2400s tests/linux/run_functional_suite_smp.sh`

## 当前结论

- 这轮暂未发现 `FRINT*` 执行 helper 的真实实现错误；当前行为与这批新核对的边界一致。
- 当前收获仍然是补齐 correctness net，把此前未被持续验证的 `DN/default-NaN` 与 `FZ/subnormal/signed-zero` 行为固定住。

# 修改日志 2026-03-20 12:35

## 本轮修改

- 扩充 `AdvSIMD pairwise/reduce` 家族裸机单测覆盖：
  - [tests/arm64/fpsimd_fp_pairwise.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/fpsimd_fp_pairwise.S)
  - [tests/arm64/fpsimd_fp_reducev.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/fpsimd_fp_reducev.S)
- 新增覆盖的程序可见边界：
  - `FMAXP/FMINP/FMAXV/FMINV/FMAXNMV/FMINNMV` 在 `qNaN + sNaN` 混合时的 NaN 选择顺序与 payload 保留；
  - `FPCR.DN=1` 下 pairwise / reduce 对 all-NaN 与 mixed-NaN+numeric 的结果与 `FPSR.IOC`；
  - `FPCR.FZ=1` 下 reduce numeric min/max 对 subnormal 刷零后的 signed-zero 结果与 `FPSR.IDC`。
- 这轮没有修改模拟器执行语义；新增的是此前缺失的 correctness coverage，用于把 `pairwise/reduce` 路径中最容易悄悄退化的 `DN/FZ/NaN payload/signed-zero` 边界固定进常规回归。

## 本轮测试

- 局部 ISA 语义探针：
  - 使用 `qemu-aarch64` 对 `FMAXP/FMINP/FMAXV/FMINV/FMAXNMV/FMINNMV` 的 NaN 顺序、`DN`、`FZ` 边界做 userspace 局部核对。
  - 注意：这里只把 `qemu-aarch64` 当单条 ISA 语义探针，不作为 system 级结论。
- 定向构建：
  - `timeout 300s tests/arm64/build_tests.sh`
- 定向验证：
  - `timeout 60s ./build/aarchvm -bin tests/arm64/out/fpsimd_fp_pairwise.bin -load 0x0 -entry 0x0 -steps 500000`
  - `timeout 60s ./build/aarchvm -bin tests/arm64/out/fpsimd_fp_reducev.bin -load 0x0 -entry 0x0 -steps 500000`
- 裸机完整回归：
  - `timeout 2400s tests/arm64/run_all.sh`
- Linux 单核功能回归：
  - `timeout 1800s tests/linux/run_functional_suite.sh`
- Linux SMP 功能回归：
  - `timeout 2400s tests/linux/run_functional_suite_smp.sh`

## 当前结论

- 这轮暂未发现 `pairwise/reduce` helper 的真实执行语义错误；当前行为与这批新核对的边界一致。
- 真实收获仍然是补网：把此前没有被系统固定住的 `sNaN 优先级 / payload 传播 / DN default-NaN / FZ signed-zero` 规则纳入常规回归。

# 修改日志 2026-03-20 15:14

## 本轮修改

- 修正 cache maintenance / address-translation 类 system instruction fault 的 syndrome 编码：
  - [include/aarchvm/cpu.hpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/include/aarchvm/cpu.hpp)
  - [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp)
  - [tests/arm64/mmu_cache_maint_fault.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/mmu_cache_maint_fault.S)
- 现在 `IC IVAU`、`DC CVAC/CVAU/CIVAC`、`DC ZVA`、`AT S1E*` fault 不再复用普通 data abort ISS，而是按 Armv8-A 程序可见要求上报：
  - `CM=1`
  - `WnR=1`
- 补齐 `PAR_EL1` 成功态的 guest 可见字段编码：
  - [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp)
  - [tests/arm64/mmu_at_tlb_observe.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/mmu_at_tlb_observe.S)
  - [tests/arm64/mmu_at_el0_permissions.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/mmu_at_el0_permissions.S)
  - [tests/arm64/mmu_tcr_ips_mair_decode.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/mmu_tcr_ips_mair_decode.S)
  - [tests/arm64/mmu_tlb_cache.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/mmu_tlb_cache.S)
  - [tests/arm64/mmu_af_fault.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/mmu_af_fault.S)
- `AT` 成功后现在会对 guest 填充：
  - `PA[55:12]`
  - `bit11 = RES1`
  - `SH`
  - `ATTR`
- 当前仍维持高性能导向的最小模型，不追求安全态 / RME / `PAR_EL1` 非当前实现范围位段的精确建模。

## 本轮测试

- 定向构建：
  - `timeout 1200s cmake --build build -j`
  - `timeout 300s tests/arm64/build_tests.sh`
- 定向 ISA / MMU 验证：
  - `timeout 60s ./build/aarchvm -bin tests/arm64/out/mmu_cache_maint_fault.bin -load 0x0 -entry 0x0 -steps 4000000`
  - `timeout 60s ./build/aarchvm -bin tests/arm64/out/el0_cache_ops_privilege.bin -load 0x0 -entry 0x0 -steps 800000`
  - `timeout 60s ./build/aarchvm -bin tests/arm64/out/mmu_tlb_cache.bin -load 0x0 -entry 0x0 -steps 5000000`
  - `timeout 60s ./build/aarchvm -bin tests/arm64/out/mmu_at_tlb_observe.bin -load 0x0 -entry 0x0 -steps 4000000`
  - `timeout 60s ./build/aarchvm -bin tests/arm64/out/mmu_at_el0_permissions.bin -load 0x0 -entry 0x0 -steps 4000000`
  - `timeout 60s ./build/aarchvm -bin tests/arm64/out/mmu_tcr_ips_mair_decode.bin -load 0x0 -entry 0x0 -steps 4000000`
  - `timeout 60s ./build/aarchvm -bin tests/arm64/out/mmu_af_fault.bin -load 0x0 -entry 0x0 -steps 4000000`
- 裸机完整回归：
  - `timeout 2400s tests/arm64/run_all.sh`
- Linux 单核功能回归：
  - `timeout 2400s tests/linux/run_functional_suite.sh`
- Linux SMP 功能回归：
  - `timeout 2400s tests/linux/run_functional_suite_smp.sh`

## 当前结论

- 这轮是真正的 guest 可见语义修正，不只是补 coverage。
- 当前 `ESR_EL1.ISS` 与 `PAR_EL1` 的这两处程序可见缺口已被补上，并已经纳入常规裸机与 Linux 回归。
- 上一轮 Linux UMP 失败是并行执行 `tests/linux/run_functional_suite.sh` 与 `tests/linux/run_functional_suite_smp.sh` 共享 `initramfs` staging 目录导致的脚本级假失败；串行执行后回归通过。

# 修改日志 2026-03-20 15:27

## 本轮修改

- 补齐 `ERET` 非法返回与 `PSTATE.IL` 的程序可见最小闭环：
  - [include/aarchvm/system_registers.hpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/include/aarchvm/system_registers.hpp)
  - [src/system_registers.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/system_registers.cpp)
  - [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp)
- 现在当 `SPSR_EL1` 指定非法返回状态时，`ERET` 不会直接当场报错，而是按 Armv8-A 程序可见语义：
  - 保持当前 EL 不变；
  - 把 `PSTATE.IL` 置 1；
  - 分支到 `ELR_EL1`；
  - 由下一条指令尝试触发 `Illegal State` 异常，`ESR_EL1.EC = 0x0E`。
- 同时补齐了“合法返回但 `SPSR_EL1.IL=1`”的路径，确保首次目标指令同样触发 `Illegal State`。
- `PSTATE.IL` 现在参与：
  - `PSTATE/SPSR_EL1` 编解码；
  - 异常入口保存与清除；
  - 快照保存与恢复。
- 快照版本从 `14` 升到 `15`：
  - [src/soc.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/soc.cpp)
  - 仍兼容加载旧版本快照，旧快照恢复时 `IL` 默认按 `0` 处理。
- 新增裸机单测，覆盖：
  - 非法 `ERET`；
  - 合法返回但 `SPSR.IL=1`；
  - 异常前目标指令不得被执行；
  - `ELR_EL1/SPSR_EL1/EC` 的可见值。
  - [tests/arm64/illegal_state_return.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/illegal_state_return.S)
- 回归脚本与构建脚本已纳入该测试：
  - [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh)
  - [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)

## 本轮测试

- 定向构建：
  - `timeout 1200s cmake --build build -j`
  - `timeout 300s tests/arm64/build_tests.sh`
- 新增语义定向验证：
  - `timeout 60s ./build/aarchvm -bin tests/arm64/out/illegal_state_return.bin -load 0x0 -entry 0x0 -steps 600000`
- 裸机完整回归：
  - `timeout 2400s tests/arm64/run_all.sh`
- Linux 单核功能回归：
  - `timeout 2400s tests/linux/run_functional_suite.sh`
- Linux SMP 功能回归：
  - `timeout 2400s tests/linux/run_functional_suite_smp.sh`

## 当前结论

- 这轮补上的是异常返回路径里一个此前确实缺失的 guest 可见行为，而不是单纯测试补网。
- 目前对 `illegal ERET` 的实现仍是“面向当前模型的最小正确性闭环”：
  - 只覆盖当前声明支持的 `AArch64 EL0/EL1`；
  - 不扩展到 `AArch32/EL2/EL3`。

# 修改日志 2026-03-20 15:36

## 本轮修改

- 修正 `ESR_EL1.IL` 的 guest 可见编码：
  - [src/system_registers.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/system_registers.cpp)
- 之前同步异常路径在写 `ESR_EL1` 时基本漏掉了 `IL` 位，导致 AArch64 guest 看到的大量 syndrome 都错误地报告为 `IL=0`。
- 现在对当前模型中的 AArch64 同步异常统一报告 `IL=1`，覆盖：
  - `SVC`
  - system register / system instruction trap
  - instruction abort / data abort
  - illegal state
  - 以及其它通过 `enter_sync_exception()` 进入的 AArch64 同步异常路径
- 同时补了一处兼容性细节：
  - 旧版本快照恢复时，若快照中不存在 `PSTATE.IL` 字段，明确按 `0` 恢复，而不是依赖对象当前内存状态。
- 扩充裸机单测，把 `IL=1` 固定进回归：
  - [tests/arm64/svc_sysreg_minimal.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/svc_sysreg_minimal.S)
  - [tests/arm64/sync_exception_regs.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/sync_exception_regs.S)
  - [tests/arm64/el0_sysreg_privilege.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/el0_sysreg_privilege.S)
  - [tests/arm64/illegal_state_return.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/illegal_state_return.S)

## 本轮测试

- 定向构建：
  - `timeout 1200s cmake --build build -j`
  - `timeout 300s tests/arm64/build_tests.sh`
- 定向异常验证：
  - `timeout 60s ./build/aarchvm -bin tests/arm64/out/svc_sysreg_minimal.bin -load 0x0 -entry 0x0 -steps 300000`
  - `timeout 60s ./build/aarchvm -bin tests/arm64/out/sync_exception_regs.bin -load 0x0 -entry 0x0 -steps 2000000`
  - `timeout 60s ./build/aarchvm -bin tests/arm64/out/el0_sysreg_privilege.bin -load 0x0 -entry 0x0 -steps 400000`
  - `timeout 60s ./build/aarchvm -bin tests/arm64/out/illegal_state_return.bin -load 0x0 -entry 0x0 -steps 600000`
- 裸机完整回归：
  - `timeout 2400s tests/arm64/run_all.sh`
- Linux 单核功能回归：
  - `timeout 2400s tests/linux/run_functional_suite.sh`
- Linux SMP 功能回归：
  - `timeout 2400s tests/linux/run_functional_suite_smp.sh`

## 当前结论

- 这轮修的是异常 syndrome 编码本身，不是测试层面的补网。
- 在当前 AArch64-only 模型下，把同步异常统一报告为 `IL=1` 是符合架构伪代码的程序可见最小实现。

# 修改日志 2026-03-21 02:24

## 本轮修改

- 修正 `FEAT_GCS` 缺失时一组 system-encoding 指令的异常分类：
  - [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp)
- 当前模型未实现 `FEAT_GCS`，因此以下指令在所有异常级别都应直接 `UNDEFINED`，而不是在 EL0 误报成 `EC=0x18` 的 system access trap：
  - `GCSPUSHM`
  - `GCSSS1`
  - `GCSSS2`
  - `GCSPOPM`
  - `GCSPUSHX`
  - `GCSPOPX`
  - `GCSPOPCX`
- 在 `exec_system()` 中为上述编码增加了 absent-feature 的提前判定，避免它们落入通用 `MRS/MSR sysreg` 路径。
- 新增裸机单测，覆盖这组 GCS system-encoding 指令在 EL1/EL0 下的 guest 可见行为：
  - [tests/arm64/gcs_system_absent_undef.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/gcs_system_absent_undef.S)
- 单测校验点包括：
  - `ESR_EL1.EC=0`
  - `ESR_EL1.IL=1`
  - `ESR_EL1.ISS=0`
  - `FAR_EL1=0`
  - `ELR_EL1` 指向 faulting instruction
- 已把该测试接入构建与完整裸机回归：
  - [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh)
  - [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)

## 本轮测试

- 定向构建：
  - `timeout 1200s cmake --build build -j`
  - `timeout 300s tests/arm64/build_tests.sh`
- 新增语义定向验证：
  - `timeout 60s ./build/aarchvm -bin tests/arm64/out/gcs_system_absent_undef.bin -load 0x0 -entry 0x0 -steps 1500000`
- 裸机完整回归：
  - `timeout 5400s bash -lc 'tests/arm64/run_all.sh > out/arm64_run_all_gcs_system_absent.log 2>&1'`
- Linux 单核功能回归：
  - `timeout 5400s bash -lc 'tests/linux/run_functional_suite.sh > out/linux_functional_gcs_system_absent_ump.log 2>&1'`
- Linux SMP 功能回归：
  - `timeout 5400s bash -lc 'tests/linux/run_functional_suite_smp.sh > out/linux_functional_gcs_system_absent_smp.log 2>&1'`

## 当前结论

- 这轮修复和前一轮 `SB/WFxT/SPECRES` 是同类问题：absent extension 的 system-encoding 指令如果不提前建模为 `UNDEFINED`，就会在 EL0 被通用 system-register 路径错误分类。
- 当前这组 `FEAT_GCS` 指令的 absent-feature guest 可见行为已经被单测和完整回归固定住。

# 修改日志 2026-03-21 02:33

## 本轮修改

- 修正 `RNDR/RNDRRS` 在当前模型中的 present/absent 判定：
  - [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp)
- ARM ARM 规定：当 `FEAT_RNG` 和 `FEAT_RNG_TRAP` 都未实现时，`MRS RNDR` 与 `MRS RNDRRS` 的 direct access 必须 `UNDEFINED`。
- 当前模型的 `ID_AA64ISAR0_EL1.RNDR` 已经报告 absent，但 `sysreg_present()` 之前没有把 `RNDR/RNDRRS` 标成 absent，导致它们在 EL0 会误走 system-register trap 路径。
- 现在已将以下 sysreg key 归为 absent：
  - `RNDR`
  - `RNDRRS`
- 新增裸机单测，覆盖 EL1/EL0 下 `RNDR/RNDRRS` 的 absent-feature guest 可见行为：
  - [tests/arm64/rng_sysreg_absent.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/rng_sysreg_absent.S)
- 单测校验点包括：
  - 目的寄存器保持原值
  - `ESR_EL1.EC=0`
  - `ESR_EL1.IL=1`
  - `ESR_EL1.ISS=0`
  - `FAR_EL1=0`
  - `ELR_EL1` 指向 faulting instruction
- 已把该测试接入构建与完整裸机回归：
  - [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh)
  - [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)

## 本轮测试

- 定向构建：
  - `timeout 1200s cmake --build build -j`
  - `timeout 300s tests/arm64/build_tests.sh`
- 新增语义定向验证：
  - `timeout 60s ./build/aarchvm -bin tests/arm64/out/rng_sysreg_absent.bin -load 0x0 -entry 0x0 -steps 400000`
- 裸机完整回归：
  - `timeout 5400s bash -lc 'tests/arm64/run_all.sh > out/arm64_run_all_rng_absent.log 2>&1'`
- Linux 单核功能回归：
  - `timeout 5400s bash -lc 'tests/linux/run_functional_suite.sh > out/linux_functional_rng_absent_ump.log 2>&1'`
- Linux SMP 功能回归：
  - `timeout 5400s bash -lc 'tests/linux/run_functional_suite_smp.sh > out/linux_functional_rng_absent_smp.log 2>&1'`

## 当前结论

- 这也是同一类“ID 寄存器报告 absent，但解码层仍把访问当作 present”问题。
- 对当前模型而言，把 `RNDR/RNDRRS` 标为 absent sysreg 是程序可见最小正确实现，因为模型既不声明也不实现 `FEAT_RNG` / `FEAT_RNG_TRAP`。

# 修改日志 2026-03-21 02:46

## 本轮修改

- 修正另外 4 个当前模型未实现、但此前也没有被标成 absent 的 AArch64 sysreg：
  - `SCXTNUM_EL0`
  - `SCXTNUM_EL1`
  - `POR_EL0`
  - `TRFCR_EL1`
- 修改位置：
  - [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp)
- 这些寄存器在 ARM ARM 中分别依赖：
  - `SCXTNUM_EL0/EL1`: `FEAT_CSV2_1p2` 或 `FEAT_CSV2_2`
  - `POR_EL0`: `FEAT_S1POE`
  - `TRFCR_EL1`: `FEAT_TRF`
- 当前模型没有实现这些特性，因此 direct access 应为 `UNDEFINED`。修复前它们会在 EL0 误走通用 `MRS/MSR sysreg` 路径，被错误分类成 `EC=0x18` system access trap。
- 现在已把上述 4 个 key 纳入 `sysreg_present()==false` 的 absent 集合，令其在当前模型中表现为架构要求的 `UNDEFINED`。
- 新增裸机单测，覆盖 EL1/EL0 下的 `MRS/MSR`：
  - [tests/arm64/sysreg_optional_absent_more.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/sysreg_optional_absent_more.S)
- 单测校验点包括：
  - 目的寄存器保持原值
  - `ESR_EL1.EC=0`
  - `ESR_EL1.IL=1`
  - `ESR_EL1.ISS=0`
  - `FAR_EL1=0`
  - `ELR_EL1` 指向 faulting instruction
- 已把该测试接入构建与完整裸机回归：
  - [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh)
  - [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)

## 本轮测试

- 定向构建：
  - `timeout 300s tests/arm64/build_tests.sh`
  - `timeout 1200s cmake --build build -j`
- 新增语义定向验证：
  - `timeout 60s ./build/aarchvm -bin tests/arm64/out/sysreg_optional_absent_more.bin -load 0x0 -entry 0x0 -steps 1200000`
- 裸机完整回归：
  - `timeout 5400s bash -lc 'tests/arm64/run_all.sh > out/arm64_run_all_sysreg_absent_more.log 2>&1'`
- Linux 单核功能回归：
  - `timeout 5400s bash -lc 'tests/linux/run_functional_suite.sh > out/linux_functional_sysreg_absent_more_ump.log 2>&1'`
- Linux SMP 功能回归：
  - `timeout 5400s bash -lc 'tests/linux/run_functional_suite_smp.sh > out/linux_functional_sysreg_absent_more_smp.log 2>&1'`

## 当前结论

- 这一轮与前两轮本质相同，都是在收敛“optional/absent feature 的 sysreg 或 system instruction 在 EL0 被误当成 system-access trap”的缺口。
- 目前这一类高概率漏网项里，`SCXTNUM_EL0/EL1`、`POR_EL0`、`TRFCR_EL1` 已经被补齐到当前模型的程序可见最小正确性。

# 修改日志 2026-03-21 02:53

## 本轮修改

- 继续收敛 SME 相关 absent sysreg 的 guest 可见行为：
  - `SVCR`
  - `SMCR_EL1`
- 修改位置：
  - [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp)
- ARM ARM 明确规定：
  - `SVCR` 仅在 `FEAT_SME` 存在时 present
  - `SMCR_EL1` 仅在 `FEAT_SME` 存在时 present
  - 否则 direct access 为 `UNDEFINED`
- 当前模型未实现 SME，之前这两个 sysreg 没被标成 absent，会在 EL0 下误走通用 system-register 路径。
- 现在已把这两个 key 纳入 `sysreg_present()==false` 的 absent 集合。
- 新增裸机单测，覆盖 EL1/EL0 下 `SVCR/SMCR_EL1` 的 `MRS/MSR`：
  - [tests/arm64/sme_sysreg_absent.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/sme_sysreg_absent.S)
- 单测校验点包括：
  - 目的寄存器保持原值
  - `ESR_EL1.EC=0`
  - `ESR_EL1.IL=1`
  - `ESR_EL1.ISS=0`
  - `FAR_EL1=0`
  - `ELR_EL1` 指向 faulting instruction
- 已把该测试接入构建与完整裸机回归：
  - [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh)
  - [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)

## 本轮测试

- 定向构建：
  - `timeout 300s tests/arm64/build_tests.sh`
  - `timeout 1200s cmake --build build -j`
- 新增语义定向验证：
  - `timeout 60s ./build/aarchvm -bin tests/arm64/out/sme_sysreg_absent.bin -load 0x0 -entry 0x0 -steps 800000`
- 裸机完整回归：
  - `timeout 5400s bash -lc 'tests/arm64/run_all.sh > out/arm64_run_all_sme_sysreg_absent.log 2>&1'`
- Linux 单核功能回归：
  - `timeout 5400s bash -lc 'tests/linux/run_functional_suite.sh > out/linux_functional_sme_sysreg_absent_ump.log 2>&1'`
- Linux SMP 功能回归：
  - `timeout 5400s bash -lc 'tests/linux/run_functional_suite_smp.sh > out/linux_functional_sme_sysreg_absent_smp.log 2>&1'`

## 当前结论

- 到这一轮为止，我已经连续补齐了 3 批同类缺口：
  - absent system-encoding instruction
  - absent optional sysreg
  - absent SME sysreg
- 这条线下剩余的高概率候选，主要还包括：
  - `SMCR_EL2/EL3`
  - `SVCRSM/SVCRZA/SVCRSMZA`
  - `RCWMASK_EL1`
  - `CHKFEAT`

# 修改日志 2026-03-21 03:23

## 本轮修改

- 继续收敛 absent sysreg 在 EL0 下被误分类为 system-access trap 的剩余缺口。
- 修改位置：
  - [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp)
- 将下列当前模型 absent 的 system register 纳入 `sysreg_present()==false`：
  - `RCWMASK_EL1`
  - `RCWSMASK_EL1`
  - `SMCR_EL2`
  - `SMCR_EL3`
- 根因是这几项虽然在 EL1 下会因为 `sysregs_.read/write` 不支持而落回 `UNDEFINED`，但在 EL0 下会先被通用权限路径误判成 `EC=0x18` system register trap。
- 现在它们会和前几轮补过的 absent sysreg 一样，在 decode/dispatch 早期直接归类为 `UNDEFINED`。

## 本轮测试

- 先用最小裸机探针复现错误，确认修改前 EL0 下：
  - `RCWMASK_EL1`
  - `RCWSMASK_EL1`
  - `SMCR_EL2`
  - `SMCR_EL3`
  都会失败。
- 扩展已有单测，覆盖这些 EL0 `MRS/MSR` 路径：
  - [tests/arm64/sysreg_optional_absent_more.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/sysreg_optional_absent_more.S)
  - [tests/arm64/sme_sysreg_absent.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/sme_sysreg_absent.S)
- 已完成：
  - `timeout 300s tests/arm64/build_tests.sh`
  - `timeout 1200s cmake --build build -j`
  - `timeout 60s ./build/aarchvm -bin tests/arm64/out/sysreg_optional_absent_more.bin -load 0x0 -entry 0x0 -steps 1500000`
  - `timeout 60s ./build/aarchvm -bin tests/arm64/out/sme_sysreg_absent.bin -load 0x0 -entry 0x0 -steps 1000000`
  - `timeout 5400s bash -lc 'tests/arm64/run_all.sh > out/arm64_run_all_rcwmask_smcr_fix.log 2>&1'`
  - `timeout 5400s bash -lc 'tests/linux/run_functional_suite.sh > out/linux_functional_rcwmask_smcr_fix_ump.log 2>&1'`
  - `timeout 5400s bash -lc 'tests/linux/run_functional_suite_smp.sh > out/linux_functional_rcwmask_smcr_fix_smp.log 2>&1'`

## 当前结论

- 这轮补的是同一类问题的下一批漏网项：当前模型 absent 的 sysreg 在 EL1 下看似“没问题”，但 EL0 下会被更早的权限路径错误改写成 `system register trap`。
- 修复后，`RCWMASK_EL1` / `RCWSMASK_EL1` / `SMCR_EL2` / `SMCR_EL3` 在 EL0 下都已回到架构要求的 `UNDEFINED`，并已被裸机单测与 Linux UMP/SMP 回归覆盖。

# 修改日志 2026-03-21 03:49

## 本轮修改

- 继续收敛 `MSR (immediate)` 到 PSTATE 字段这一类的 absent-feature / reserved 边界。
- 修改位置：
  - [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp)
- 新增了一个按 `op1/op2/CRm` 解码的 `MSR (immediate)` 分发，统一处理：
  - `SPSel`
  - `PAN`
  - `DAIFSet`
  - `DAIFClr`
  - 以及当前模型 absent 的 `UAO / ALLINT / PM / SSBS / DIT / TCO / SVCRSM / SVCRZA / SVCRSMZA`
- 修复前，这类 immediate-form 指令里未显式实现的编码会在 EL0 下误落到通用 `MSR sysreg` 路径，被错误报成 `EC=0x18` system register trap。
- 修复后，这些当前模型 absent 的 immediate-form PSTATE 指令都会按 ARM ARM 要求直接表现为 `UNDEFINED`。
- 新增裸机单测：
  - [tests/arm64/msr_imm_absent_features_undef.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/msr_imm_absent_features_undef.S)
- 该测试覆盖 EL0 下的：
  - `MSR UAO, #1`
  - `MSR DIT, #1`
  - `MSR SSBS, #1`
  - `MSR TCO, #1`
  - `MSR ALLINT, #1`
  - `MSR PM, #1`
  - `SMSTART SM`
  - `SMSTART ZA`
  - `SMSTART`
- 已将该测试接入：
  - [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh)
  - [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)

## 本轮测试

- 定向构建：
  - `timeout 300s tests/arm64/build_tests.sh`
  - `timeout 1200s cmake --build build -j`
- 定向语义验证：
  - `timeout 60s ./build/aarchvm -bin tests/arm64/out/msr_imm_absent_features_undef.bin -load 0x0 -entry 0x0 -steps 1200000`
  - `timeout 60s ./build/aarchvm -bin tests/arm64/out/sysreg_optional_absent_more.bin -load 0x0 -entry 0x0 -steps 1500000`
  - `timeout 60s ./build/aarchvm -bin tests/arm64/out/sme_sysreg_absent.bin -load 0x0 -entry 0x0 -steps 1000000`
- 裸机完整回归：
  - `timeout 5400s bash -lc 'tests/arm64/run_all.sh > out/arm64_run_all_msr_imm_absent_fix.log 2>&1'`
- Linux 单核功能回归：
  - `timeout 5400s bash -lc 'tests/linux/run_functional_suite.sh > out/linux_functional_msr_imm_absent_fix_ump.log 2>&1'`
- Linux SMP 功能回归：
  - `timeout 5400s bash -lc 'tests/linux/run_functional_suite_smp.sh > out/linux_functional_msr_imm_absent_fix_smp.log 2>&1'`

## 当前结论

- `SVCRSM/SVCRZA/SVCRSMZA` 不是孤立问题，根因是整类 `MSR (immediate)` 编码此前缺少正确分发。
- 这轮修复后，当前模型里 absent 的 immediate-form PSTATE 指令已经不会在 EL0 下被误分类成 `system register trap`。

# 修改日志 2026-03-21 04:04

## 本轮修改

- 继续收敛 `absent sysreg` 在 EL0 下被误分类的问题，这一轮针对的是当前模型未实现 PMU 时的一整组 PMU System register。
- 修改位置：
  - [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp)
- 在 `sysreg_present()` 中把当前模型 absent 的 PMU System register 明确标记为不存在，从而让 `MRS/MSR` 直接回到 `UNDEFINED`，而不是在 EL0 下先被通用权限检查误报为 `EC=0x18` system register trap。
- 本轮补上的寄存器包括：
  - `PMCCFILTR_EL0`
  - `PMCCNTR_EL0`
  - `PMCEID0_EL0`
  - `PMCEID1_EL0`
  - `PMCNTENCLR_EL0`
  - `PMCNTENSET_EL0`
  - `PMCR_EL0`
  - `PMINTENCLR_EL1`
  - `PMINTENSET_EL1`
  - `PMMIR_EL1`
  - `PMOVSCLR_EL0`
  - `PMOVSSET_EL0`
  - `PMSELR_EL0`
  - `PMSWINC_EL0`
  - `PMXEVCNTR_EL0`
  - `PMXEVTYPER_EL0`
- 新增裸机单测：
  - [tests/arm64/pmu_sysreg_absent.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/pmu_sysreg_absent.S)
- 该测试覆盖：
  - EL1 下 `PMCR_EL0` 的 `MRS/MSR`
  - EL0 下代表性的 PMU `MRS/MSR` 路径
  - 并显式检查异常确实是 `EC=0` 的 `UNDEFINED`
- 已将该测试接入：
  - [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh)
  - [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)

## 本轮测试

- 定向构建：
  - `timeout 300s tests/arm64/build_tests.sh`
  - `timeout 1200s cmake --build build -j`
- 定向语义验证：
  - `timeout 60s ./build/aarchvm -bin tests/arm64/out/pmu_sysreg_absent.bin -load 0x0 -entry 0x0 -steps 1400000`
  - `timeout 60s ./build/aarchvm -bin tests/arm64/out/msr_imm_absent_features_undef.bin -load 0x0 -entry 0x0 -steps 1200000`
  - `timeout 60s ./build/aarchvm -bin tests/arm64/out/sysreg_optional_absent_more.bin -load 0x0 -entry 0x0 -steps 1500000`
  - `timeout 60s ./build/aarchvm -bin tests/arm64/out/sme_sysreg_absent.bin -load 0x0 -entry 0x0 -steps 1000000`
- 裸机完整回归：
  - `timeout 5400s bash -lc 'tests/arm64/run_all.sh > out/arm64_run_all_pmu_absent_fix.log 2>&1'`
- Linux 单核功能回归：
  - `timeout 5400s bash -lc 'tests/linux/run_functional_suite.sh > out/linux_functional_pmu_absent_fix_ump.log 2>&1'`
- Linux SMP 功能回归：
  - `timeout 5400s bash -lc 'tests/linux/run_functional_suite_smp.sh > out/linux_functional_pmu_absent_fix_smp.log 2>&1'`

## 当前结论

- 这轮确认了前面一度混淆的点：对于 AArch64 System register 访问，当前模型 PMU absent 时，这批寄存器应表现为 `UNDEFINED`；`RES0` 是 external register 访问的语义，不适用于这里。
- 修复后，这组 PMU sysreg 在 EL0 下不再被误分类成 `system register trap`，裸机与 Linux UMP/SMP 回归均已通过。

# 修改日志 2026-03-21 04:14

## 本轮修改

- 继续沿 PMU `absent sysreg` 这条线补第二批漏网项，覆盖 `PMUv3_SS`、`PMUv3_ICNTR`、`PMUv3p9` 相关但当前模型未实现的寄存器。
- 修改位置：
  - [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp)
- 在 `sysreg_present()` 中新增以下 absent PMU sysreg：
  - `PMCCNTSVR_EL1`
  - `PMICFILTR_EL0`
  - `PMICNTR_EL0`
  - `PMICNTSVR_EL1`
  - `PMUACR_EL1`
  - `PMZR_EL0`
- 这些寄存器在当前模型里对应的特性均未实现：
  - `PMCCNTSVR_EL1` 依赖 `FEAT_PMUv3_SS`
  - `PMICFILTR_EL0` / `PMICNTR_EL0` 依赖 `FEAT_PMUv3_ICNTR`
  - `PMICNTSVR_EL1` 依赖 `FEAT_PMUv3_ICNTR + FEAT_PMUv3_SS`
  - `PMUACR_EL1` / `PMZR_EL0` 依赖 `FEAT_PMUv3p9`
- 修复前，这些寄存器会在 EL0 下被错误落成 `EC=0x18` system register trap，或者依赖通用权限路径给出错误分类。
- 修复后，它们会统一按架构要求表现为 `UNDEFINED`。
- 新增裸机单测：
  - [tests/arm64/pmu_sysreg_absent_more.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/pmu_sysreg_absent_more.S)
- 该测试覆盖：
  - EL1 下 `PMCCNTSVR_EL1`、`PMICFILTR_EL0`、`PMICNTR_EL0`、`PMICNTSVR_EL1`、`PMUACR_EL1`、`PMZR_EL0`
  - EL0 下同组寄存器的代表性访问
  - 并显式检查异常仍是 `EC=0` 的 `UNDEFINED`
- 已将该测试接入：
  - [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh)
  - [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)

## 本轮测试

- 定向构建：
  - `timeout 300s tests/arm64/build_tests.sh`
  - `timeout 1200s cmake --build build -j`
- 定向语义验证：
  - `timeout 60s ./build/aarchvm -bin tests/arm64/out/pmu_sysreg_absent_more.bin -load 0x0 -entry 0x0 -steps 1600000`
  - `timeout 60s ./build/aarchvm -bin tests/arm64/out/pmu_sysreg_absent.bin -load 0x0 -entry 0x0 -steps 1400000`
- 裸机完整回归：
  - `timeout 5400s bash -lc 'tests/arm64/run_all.sh > out/arm64_run_all_pmu_absent_more_fix.log 2>&1'`
- Linux 单核功能回归：
  - `timeout 5400s bash -lc 'tests/linux/run_functional_suite.sh > out/linux_functional_pmu_absent_more_fix_ump.log 2>&1'`
- Linux SMP 功能回归：
  - `timeout 5400s bash -lc 'tests/linux/run_functional_suite_smp.sh > out/linux_functional_pmu_absent_more_fix_smp.log 2>&1'`

## 当前结论

- PMU 这组 `absent feature -> direct AArch64 system register access is UNDEFINED` 的漏项又补掉了一层。
- 当前还值得继续追的同类缺口，优先级较高的会是更后面的 PMU/SPE 相关寄存器族，例如 `PMECR_EL1`、`PMIAR_EL1`、`PMS*` 采样寄存器等，它们大概率也需要同样的 absent-feature 分类收口。

# 修改日志 2026-03-21 17:49

## 本轮修改

- 继续沿 “absent optional feature 的 AArch64 system register direct access 应为 `UNDEFINED`” 这条线收口 SPE/PMU profiling 相关寄存器：
  - `PMECR_EL1`
  - `PMIAR_EL1`
  - `PMSCR_EL1`
  - `PMSCR_EL2`
  - `PMSDSFR_EL1`
  - `PMSEVFR_EL1`
  - `PMSFCR_EL1`
  - `PMSICR_EL1`
  - `PMSIDR_EL1`
  - `PMSIRR_EL1`
  - `PMSLATFR_EL1`
  - `PMSNEVFR_EL1`
- 修改位置：
  - [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp)
  - [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh)
  - [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)
- 新增裸机单测：
  - [tests/arm64/spe_sysreg_absent.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/spe_sysreg_absent.S)
- 修复前：这组 sysreg 访问会落到通用 system register trap/权限路径，导致软件观测到与架构不一致的异常分类。
- 修复后：在当前模型未实现 SPE/相关 PMU profiling 特性时，上述寄存器在 EL0/EL1 下的 direct access 统一表现为 `UNDEFINED`。

## 本轮测试

- 定向构建：
  - `timeout 300s tests/arm64/build_tests.sh`
  - `timeout 1200s cmake --build build -j`
- 定向语义验证：
  - `timeout 60s ./build/aarchvm -bin tests/arm64/out/spe_sysreg_absent.bin -load 0x0 -entry 0x0 -steps 2200000`
- 裸机完整回归：
  - `timeout 5400s bash -lc 'tests/arm64/run_all.sh > out/arm64_run_all_spe_absent_fix.log 2>&1'`
- Linux 单核功能回归：
  - `timeout 5400s bash -lc 'tests/linux/run_functional_suite.sh > out/linux_functional_spe_absent_fix_ump.log 2>&1'`
- Linux SMP 功能回归：
  - `timeout 5400s bash -lc 'tests/linux/run_functional_suite_smp.sh > out/linux_functional_spe_absent_fix_smp.log 2>&1'`

## 当前结论

- SPE/PMU profiling 这组寄存器的 absent-feature 语义已按架构要求收口为 `UNDEFINED`，并由裸机单测覆盖。
- 这条线下一步若继续推进，优先级高的会是其它 “可选系统寄存器族” 的 absent 行为收口，避免在 EL0 下被误分类成 `EC=0x18` system register trap（尤其是 profiling/debug 相关寄存器）。

# 修改日志 2026-03-21 23:58

## 本轮修改

- 修正了上一轮新加测试中的一个覆盖错误：
  - [tests/arm64/spe_sysreg_absent.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/spe_sysreg_absent.S) 原先把 `PMSEVFR_EL1` 和 `PMSNEVFR_EL1` 的编码写混，导致测试通过但没有真正覆盖 `PMSEVFR_EL1`。
  - 现已更正 `PMSEVFR_EL1` 的 `MRS/MSR` 编码为 `0xD53899A0 / 0xD51899A0`。
- 继续沿 SPE absent sysreg 这条线补 Profiling Buffer 管理寄存器族：
  - `PMBIDR_EL1`
  - `PMBLIMITR_EL1`
  - `PMBMAR_EL1`
  - `PMBPTR_EL1`
  - `PMBSR_EL1`
  - `PMBSR_EL2`
  - `PMBSR_EL3`
- 修改位置：
  - [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp)
  - [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh)
  - [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)
- 新增裸机单测：
  - [tests/arm64/spe_pmb_sysreg_absent.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/spe_pmb_sysreg_absent.S)
- 修复后，在当前模型未实现 `FEAT_SPE / FEAT_SPE_nVM / FEAT_SPE_EXC` 时，这组 PMB sysreg 的 direct access 会稳定表现为 `UNDEFINED`，不再误走通用 trap 路径。

## 本轮测试

- 定向构建：
  - `timeout 300s tests/arm64/build_tests.sh`
  - `timeout 1200s cmake --build build -j`
- 定向语义验证：
  - `timeout 60s ./build/aarchvm -bin tests/arm64/out/spe_sysreg_absent.bin -load 0x0 -entry 0x0 -steps 2200000`
  - `timeout 60s ./build/aarchvm -bin tests/arm64/out/spe_pmb_sysreg_absent.bin -load 0x0 -entry 0x0 -steps 1800000`
- 裸机完整回归：
  - `timeout 5400s bash -lc 'tests/arm64/run_all.sh > out/arm64_run_all_spe_pmb_absent_fix.log 2>&1'`
- Linux 单核功能回归：
  - `timeout 5400s bash -lc 'tests/linux/run_functional_suite.sh > out/linux_functional_spe_pmb_absent_fix_ump.log 2>&1'`
- Linux SMP 功能回归：
  - `timeout 5400s bash -lc 'tests/linux/run_functional_suite_smp.sh > out/linux_functional_spe_pmb_absent_fix_smp.log 2>&1'`

## 当前结论

- 这轮继续收紧了 “profile / buffer / profiling exception” 这条 sysreg 缺口，减少了 EL0/EL1 下将 absent feature 误判成 `EC=0x18` trap 的空间。
- 同类高优先级剩余项，仍主要是：
  - AMU 相关寄存器族
  - 其它 trace / debug / profiling 相关的可选 sysreg
  - 与 ID 寄存器报告 absent feature 对应的系统指令/系统寄存器边界

# 修改日志 2026-03-22 00:13

## 本轮修改

- 补齐了 AMU / AMUv1p1 缺失时应直接表现为 `UNDEFINED` 的 AArch64 system register 族判定，避免误走通用 `EC=0x18` system register trap 路径。
- 本轮纳入 absent-feature 收口的寄存器族包括：
  - `AMCFGR_EL0`
  - `AMCGCR_EL0`
  - `AMCR_EL0`
  - `AMCNTENCLR0_EL0`
  - `AMCNTENSET0_EL0`
  - `AMCNTENCLR1_EL0`
  - `AMCNTENSET1_EL0`
  - `AMEVCNTR0<m>_EL0`
  - `AMEVTYPER0<m>_EL0`
  - `AMEVCNTR1<m>_EL0`
  - `AMEVTYPER1<m>_EL0`
  - `AMCG1IDR_EL0`
  - `AMEVCNTVOFF0<m>_EL2`
  - `AMEVCNTVOFF1<m>_EL2`
- 修改位置：
  - [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp)
  - [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh)
  - [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)
- 新增裸机单测：
  - [tests/arm64/amu_sysreg_absent.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/amu_sysreg_absent.S)
- 修复前：在当前模型 `ID_AA64PFR0_EL1.AMU=0` 的配置下，部分 AMU 相关 direct access 仍可能被解释为普通 system register 访问，再落入 trap/权限路径，软件看到的异常分类不符合架构要求。
- 修复后：这组 AMU / AMUv1p1 寄存器在 EL0/EL1 下的 direct access 会统一表现为 `UNDEFINED`，与当前 feature ID 报告一致。

## 本轮测试

- 定向构建：
  - `timeout 300s tests/arm64/build_tests.sh`
  - `timeout 1200s cmake --build build -j`
- 定向语义验证：
  - `timeout 60s ./build/aarchvm -bin tests/arm64/out/amu_sysreg_absent.bin -load 0x0 -entry 0x0 -steps 2200000`
- 裸机完整回归：
  - `timeout 5400s bash -lc 'tests/arm64/run_all.sh > out/arm64_run_all_amu_absent.log 2>&1'`
- Linux 单核功能回归：
  - `timeout 5400s bash -lc 'tests/linux/run_functional_suite.sh > out/linux_functional_amu_absent_ump.log 2>&1'`
- Linux SMP 功能回归：
  - `timeout 5400s bash -lc 'tests/linux/run_functional_suite_smp.sh > out/linux_functional_amu_absent_smp.log 2>&1'`

## 当前结论

- 这轮把 AMU 缺失特性的主要 AArch64 system register 空间收口到了当前 ID 配置一致的行为。
- 继续沿这条线往下审时，优先级更高的剩余项会偏向：
  - 其它由 ID 寄存器明确报告 absent、但还未系统化归类的 debug / trace / profiling sysreg
  - trap / undef / no-op 边界里仍未细分的 debug/system 指令族

# 修改日志 2026-03-22 17:02

## 本轮修改

- 修正了普通 load/store 路径中一个会污染异常语义的边界问题：
  - [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp) 的 `exec_load_store()` 现在会在对齐检查 helper 已经取同步异常后停止同一条指令的后续 sub-access，避免继续访存或再补一次 `data_abort()` 覆盖掉原异常。
- 修正了 `MSR SPSel` 的程序可见切换语义：
  - [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp) 现在在切换 `PSTATE.SP` 前后显式保存/恢复当前 `SP` bank，不再只改 `PSTATE` 而不切换实际 `SP_EL0/SP_EL1`。
- 修正了普通 `SIMD&FP` 整寄存器 `LDR/STR` 的对齐 fault 语义：
  - [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp) 将普通 `LDR/STR <Q>` 与 structured load/store 分开处理。
  - 修复前：`LDR/STR Qt` 被内部拆成两个 8-byte 子访问，`SCTLR_EL1.A=1` 时地址若是“8-byte 对齐但 16-byte 不对齐”，会被错误放行。
  - 修复后：普通 `LDR/STR Qt` 会按 16-byte access size 进行对齐判定；structured load/store 继续按 element-size 语义处理，不被误伤。
- 新增/接入的裸机单测：
  - [tests/arm64/sp_alignment_fault.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/sp_alignment_fault.S)
  - [tests/arm64/data_alignment_fault.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/data_alignment_fault.S)
  - [tests/arm64/atomic_alignment_fault.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/atomic_alignment_fault.S)
  - [tests/arm64/fpsimd_q_alignment_fault.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/fpsimd_q_alignment_fault.S)
- 更新了测试接线：
  - [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh)
  - [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)

## 本轮测试

- 定向构建：
  - `timeout 300s tests/arm64/build_tests.sh`
  - `timeout 1200s cmake --build build -j`
- 定向语义验证：
  - `timeout 120s bash -lc 'AARCHVM_BRK_MODE=halt ./build/aarchvm -bin tests/arm64/out/sp_alignment_fault.bin -load 0x0 -entry 0x0 -steps 500000 2>/dev/null | tr -d "\r\n"'`
  - `timeout 120s bash -lc 'AARCHVM_BRK_MODE=halt ./build/aarchvm -bin tests/arm64/out/data_alignment_fault.bin -load 0x0 -entry 0x0 -steps 500000 2>/dev/null | tr -d "\r\n"'`
  - `timeout 120s bash -lc 'AARCHVM_BRK_MODE=halt ./build/aarchvm -bin tests/arm64/out/atomic_alignment_fault.bin -load 0x0 -entry 0x0 -steps 500000 2>/dev/null | tr -d "\r\n"'`
  - `timeout 120s bash -lc 'AARCHVM_BRK_MODE=halt ./build/aarchvm -bin tests/arm64/out/fpsimd_q_alignment_fault.bin -load 0x0 -entry 0x0 -steps 600000 2>/dev/null | tr -d "\r\n"'`
- 裸机完整回归：
  - `timeout 5400s bash -lc 'tests/arm64/run_all.sh > out/arm64_run_all_fpsimd_q_align.log 2>&1'`
- Linux 单核功能回归：
  - `timeout 5400s bash -lc 'tests/linux/run_functional_suite.sh > out/linux_functional_fpsimd_q_align_ump.log 2>&1'`
- Linux SMP 功能回归：
  - `timeout 5400s bash -lc 'tests/linux/run_functional_suite_smp.sh > out/linux_functional_fpsimd_q_align_smp.log 2>&1'`

## 当前结论

- 这轮把对齐 fault 这条高优先级程序可见语义继续往前收了一步，尤其是此前容易被内部“拆成多个子访问”掩盖掉的 `LDR/STR Qt` 情况。
- 目前已被单测直接覆盖的对齐 fault 高优先级路径包括：
  - `SP` alignment fault。
  - 标量 misaligned load / pair load fault。
  - `LDAR/STLR/LDXR/LSE atomic` misaligned fault。
  - 普通 `SIMD&FP` `LDR/STR Q` misaligned fault。

# 修改日志 2026-03-29 18:34

## 本轮修改

- 继续按“审阅 -> 修复 -> 测试”流程推进“Armv8-A 程序可见正确性收尾计划”，这轮收的是一组彼此相关的 `SP` / `SPSel` / special sysreg 程序可见语义缺口：
  - [src/system_registers.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/system_registers.cpp) 现在按当前 `EL` 与 `PSTATE.SP` 收紧 direct `MRS/MSR SP_EL0`、`MRS/MSR SP_EL1` 的可见性。
  - [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp) 修正了 direct `SP_EL0/SP_EL1` 的 special-case 语义：
    - `SP_EL0` 在 `EL1t` 下不再被错误当作当前 `SP` 读写。
    - `SP_EL1` 在当前仅实现 `EL0/EL1` 的模型里不再被错误宽放行。
  - [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp) 补齐了 register-form `MSR SPSel, Xt` 的 bank 切换语义，避免它和 immediate-form `MSR SPSel, #imm` 行为不一致。
  - [include/aarchvm/cpu.hpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/include/aarchvm/cpu.hpp) 把通用 datapath 对 `SP` 的写回统一改为走 `set_sp()`，使普通 `mov/add/sub/... -> SP` 与当前活跃 `SP` bank 始终保持同步，避免 `regs_[31]` 和 `SP_EL0/SP_EL1` 脱节。
- 新增并接入了一个新的裸机回归：
  - [tests/arm64/sp_special_sysreg_access.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/sp_special_sysreg_access.S)
  - [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh)
  - [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)
  - 该回归显式覆盖：
    - `EL1h` 下 direct `MRS/MSR SP_EL0` 正常；
    - `EL1t` 下 direct `MRS/MSR SP_EL0` 必须 `UNDEFINED`；
    - `EL1` 下 direct `MRS/MSR SP_EL1` 在当前模型里必须 `UNDEFINED`；
    - `MSR SPSel, Xt` 切换前后的 `SP_EL0/SP_EL1/current SP` bank 同步。
- 修正了两处旧测试中“原先依赖模拟器宽放行、但不符合架构”的初始化方式：
  - [tests/arm64/sp_alignment_fault.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/sp_alignment_fault.S)
  - [tests/arm64/fpsimd_sp_alignment_fault.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/fpsimd_sp_alignment_fault.S)
  - 旧写法用 `MSR SP_EL1, Xt` 在 `EL1` 直接初始化 handler 栈；在本轮把 `SP_EL1` 语义收紧到架构要求后，这两处测试改为通过当前 `SP` 路径 `mov sp, ...` 初始化 `EL1h` 栈。

## 本轮测试

- 定向构建与验证：
  - `timeout 600s cmake --build build -j`
  - `timeout 300s ./tests/arm64/build_tests.sh`
  - `timeout 60s env AARCHVM_BRK_MODE=halt ./build/aarchvm -bin tests/arm64/out/sp_special_sysreg_access.bin -load 0x0 -entry 0x0 -steps 800000`
  - `timeout 60s env AARCHVM_BRK_MODE=halt ./build/aarchvm -bin tests/arm64/out/sp_alignment_fault.bin -load 0x0 -entry 0x0 -steps 300000`
  - `timeout 60s env AARCHVM_BRK_MODE=halt ./build/aarchvm -bin tests/arm64/out/fpsimd_sp_alignment_fault.bin -load 0x0 -entry 0x0 -steps 800000`
- 裸机完整回归：
  - `timeout 5400s ./tests/arm64/run_all.sh`
- Linux 单核功能回归：
  - `timeout 5400s ./tests/linux/run_functional_suite.sh`
- Linux SMP 功能回归：
  - `timeout 5400s ./tests/linux/run_functional_suite_smp.sh`

## 当前结论

- 这轮确认并收口了一类真实的程序可见问题：同一个架构状态转换如果存在多条编码入口，就必须共用同一套 `SP bank` / `PSTATE.SP` 同步逻辑，否则会出现“单条指令测试能过，但换一种编码或进异常后状态坏掉”的隐蔽错误。
- 目前这组 `SP_EL0/SP_EL1/SPSel/current SP` 相关 direct accessor 与 bank 切换路径已经被新的裸机回归和 Linux UMP/SMP 回归共同覆盖。
- 但这还不足以宣称“Armv8-A 程序可见最小集已完整收口”：
  - 仍需继续系统审查 `FP/AdvSIMD` 细节、SMP 同步原语与内存模型、MMU/TLB/fault 一致性，以及更系统的差分验证/长期 Linux 压力回归。

# 修改日志 2026-03-29 23:13

## 本轮修改

- 继续按“审阅 -> 修复 -> 测试”流程推进“Armv8-A 程序可见正确性收尾计划”，这轮收的是 AArch64 self-hosted debug 里 `OS Lock / OS Double Lock` 对 debug exception 生成的程序可见语义：
  - [include/aarchvm/system_registers.hpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/include/aarchvm/system_registers.hpp)
  - [src/system_registers.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/system_registers.cpp)
  - 现在 `hardware breakpoint`、`watchpoint`、`software step` 的异常生成都会同时受 `OSLAR_EL1` 与 `OSDLR_EL1`/`DBGPRCR_EL1.CORENPDRQ` 共同约束；
  - `BRK` instruction exception 仍保持不受 `OS Lock` / `OS Double Lock` 屏蔽，和 Arm ARM 的这条边界一致。
- 同轮顺手修复了一个真实但非架构语义的回归问题：
  - [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp)
  - `Cpu::load_state()` 中 `dcc_int_enable_` 的快照恢复条件此前写反，导致新版本快照在读到该字段后直接失败；现已改正。
- 新增并接入了专门的 debug lock 裸机回归：
  - [tests/arm64/debug_lock_exception_gating.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/debug_lock_exception_gating.S)
  - [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh)
  - [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)
  - 覆盖项包括：
    - `OS Lock` 屏蔽 EL1 hardware breakpoint；
    - `OS Double Lock` 屏蔽 EL1 watchpoint；
    - `CORENPDRQ=1` 关闭 `DoubleLockStatus()` 后 breakpoint 恢复生效；
    - `OS Lock` 屏蔽 software step；
    - `BRK` 不受上述 lock 影响。
- 修正了两处旧 debug 测试的前置条件，使它们不再依赖此前错误的“复位态 lock 不生效”行为：
  - [tests/arm64/debug_break_watch_basic.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/debug_break_watch_basic.S)
  - [tests/arm64/software_step_basic.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/software_step_basic.S)
  - 这两处现在都会在测试开始时显式清除 `OSLAR_EL1` / `OSDLR_EL1` / `DBGPRCR_EL1`，使测试只验证目标语义本身。

## 本轮测试

- 定向验证：
  - `timeout 60s ./build/aarchvm -bin tests/arm64/out/snapshot_resume.bin -load 0x0 -entry 0x0 -steps 2000 -snapshot-save out/tmp-test.snap`
  - `timeout 60s ./build/aarchvm -snapshot-load out/tmp-test.snap -steps 10000`
  - `timeout 60s env AARCHVM_BRK_MODE=trap ./build/aarchvm -bin tests/arm64/out/debug_lock_exception_gating.bin -load 0x0 -entry 0x0 -steps 1200000`
- 裸机完整回归：
  - `timeout 5400s ./tests/arm64/run_all.sh`
- Linux 单核功能回归：
  - `timeout 5400s ./tests/linux/run_functional_suite.sh`
- Linux SMP 功能回归：
  - `timeout 5400s ./tests/linux/run_functional_suite_smp.sh`

## 当前结论

- 这轮把 `debug / system / trap` 线里一个明确的程序可见缺口补上了：此前模型会在 `OS Lock` 或 `OS Double Lock` 生效时，仍错误生成 non-BRK 类 debug exception；现在这条行为已经被实现并被单测锁住。
- 目前还不能宣称“Armv8-A 程序可见最小集已完整收口”。在当前代码状态下，我认为剩余最值得继续审的仍然是：
  - `FP/AdvSIMD` 细节，尤其 `FPCR/FPSR` 与 NaN/subnormal/flags 传播的一致性尾差；
  - `SMP` 下同步原语、barrier、exclusive/LSE 与异常/fault 交错时的边界；
  - `MMU/TLB/fault` 在更复杂 Linux 压力和差分验证下是否还会暴露新的尾差。
# 修改日志 2026-03-30 21:15

- 继续按“审阅 -> 修复 -> 测试”流程推进“Armv8-A 程序可见正确性收尾计划”，这轮收的是 `MDSCR_EL1.TDA` 对 breakpoint/watchpoint debug sysreg 访问的最小程序可见语义。
- 修改：
  - 在 [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp) 的 `exec_system()` 中补上了 `software access debug event` 的最小建模：
    - 对本来会成功的 `DBGBVR/DBGBCR/DBGWVR/DBGWCR` 读写访问；
    - 当 `MDSCR_EL1.TDA=1` 且 `OS Lock` 已解锁时；
    - 不再静默成功，而是进入当前模型的 halting 路径。
  - 这里没有把它伪装成别的同步异常；当前模型仍未实现完整 halting debug state，因此采用现有 `halted_` 机制来表达“软件访问触发 halting debug event”这一程序可见结果。
- 新增并接入裸机回归：
  - [tests/arm64/debug_software_access_halt_read.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/debug_software_access_halt_read.S)
  - [tests/arm64/debug_software_access_halt_write.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/debug_software_access_halt_write.S)
  - [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh)
  - [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)
- 新测试覆盖：
  - `OS Lock` 保持锁定时，`TDA` 只作为 `EDSCR.TDA` 的 save/restore 影子位，不应影响直接 debug sysreg 访问；
  - `OS Lock` 解锁后，同样的 `DBGBCR/DBGWCR` 访问会触发 halting 行为；
  - 分别覆盖读路径与写路径。

# 修改日志 2026-04-01 11:38

## 本轮修改

- 继续按“审阅 -> 修复 -> 测试”流程推进“Armv8-A 程序可见正确性收尾计划”，这轮收的是 base cache maintenance by set/way 里一个真实的 guest 可见缺口：
  - [src/cpu.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/cpu.cpp)
  - 此前 `DC CSW, Xt` 没有被系统指令路径解码，客体在 `EL1` 执行它时会错误落到 `UNDEFINED`。
  - 现在已把 `DC CSW, Xt` 接入 `exec_system()`，与现有 `DC ISW/CISW` 一样按当前模型实现为：
    - `EL1` 下 side-effect-free 成功执行；
    - `EL0` 下保持 `UNDEFINED`。
- 新增并接入了专门的裸机回归：
  - [tests/arm64/dc_csw_privilege.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/dc_csw_privilege.S)
  - [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh)
  - [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)
  - 这条用例同时锁定：
    - `EL1` 执行 `DC CSW` 不应异常；
    - `EL0` 执行 `DC CSW` 仍应 `UNDEFINED`；
    - 异常时 `ESR_EL1.IL=1`、`ISS=0`、`FAR_EL1=0`；
    - `SPSR_EL1` 中保存的源 `NZCV/DAIF/PAN/M` 正确无尾差。

## 本轮测试

- 定向验证：
  - `timeout 1200s env AARCHVM_BRK_MODE=halt ./build/aarchvm -bin tests/arm64/out/dc_csw_privilege.bin -load 0x0 -entry 0x0 -steps 600000`
- 重新编译与测试产物：
  - `timeout 1200s cmake --build build -j`
  - `timeout 1200s ./tests/arm64/build_tests.sh`
- 裸机完整回归：
  - `timeout 5400s ./tests/arm64/run_all.sh`
- Linux 单核功能回归：
  - `timeout 5400s ./tests/linux/run_functional_suite.sh`
- Linux SMP 功能回归：
  - `timeout 5400s ./tests/linux/run_functional_suite_smp.sh`

## 当前结论

- 这轮又补掉了一条真实的 trap/undef/no-op 边界，不是“补文档式”的审计：之前 base cache op 家族里 `DC CSW` 这条指令确实缺失。
- 当前代码状态下，新增 `DC CSW` 语义与回归都已经通过，且没有破坏裸机、Linux UMP、Linux SMP 现有回归。
- 但我还不能宣称“Armv8-A 程序可见最小集已完整收口”。下一批仍最值得继续审的是：
  - `FP/AdvSIMD` 里 `DN/FZ/NaN/payload/flags` 的一致性尾差；
  - `ESR_EL1/FAR_EL1/PAR_EL1/ISS` 尚未逐类完全枚举覆盖的剩余异常；
  - `SMP` 下 barrier / exclusive / fault 交错时的程序可见边界。

# 修改日志 2026-04-01 16:15

## 本轮修改

- 为 SoC 新增了宿主一致的 `PL031` 风格 RTC 设备，并接入现有 MMIO / snapshot 框架：
  - [include/aarchvm/rtc_pl031.hpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/include/aarchvm/rtc_pl031.hpp)
  - [src/rtc_pl031.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/rtc_pl031.cpp)
  - [include/aarchvm/soc.hpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/include/aarchvm/soc.hpp)
  - [src/soc.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/soc.cpp)
- 当前 RTC 实现提供：
  - `DR/MR/LR/CR/IMSC/RIS/MIS/ICR`
  - PrimeCell `PeriphID/PCellID`
  - 宿主 `CLOCK_REALTIME` 驱动的秒级 wall-clock 语义
  - guest 写 `LR` 时通过 `offset_seconds` 维护 guest-visible RTC 时间
  - snapshot 保存 / 恢复 RTC 设备状态，SoC 快照版本升到 `23`
- 为 Linux DT 补充了 RTC 节点，使内核可直接加载 `rtc-pl031`：
  - [dts/aarchvm-current.dts](/media/luzeyu/Storage2/FOSS_src/aarchvm/dts/aarchvm-current.dts)
  - [dts/aarchvm-linux-min.dts](/media/luzeyu/Storage2/FOSS_src/aarchvm/dts/aarchvm-linux-min.dts)
  - [dts/aarchvm-linux-smp.dts](/media/luzeyu/Storage2/FOSS_src/aarchvm/dts/aarchvm-linux-smp.dts)
- 调整了 Linux snapshot 构建脚本，在所用 `.dts` 更新后自动重建对应 `.dtb`：
  - [tests/linux/build_linux_shell_snapshot.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/linux/build_linux_shell_snapshot.sh)
- 新增 Linux 用户态 RTC 冒烟程序，并并入统一 initramfs / 功能回归：
  - [tests/linux/rtc_smoke.c](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/linux/rtc_smoke.c)
  - [tests/linux/build_usertests_rootfs.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/linux/build_usertests_rootfs.sh)
  - [tests/linux/run_functional_suite.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/linux/run_functional_suite.sh)
  - [tests/linux/run_functional_suite_smp.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/linux/run_functional_suite_smp.sh)
- 同步更新了项目文档与 TODO：
  - [README.md](/media/luzeyu/Storage2/FOSS_src/aarchvm/README.md)
  - [doc/README.en.md](/media/luzeyu/Storage2/FOSS_src/aarchvm/doc/README.en.md)
  - [doc/README.zh.md](/media/luzeyu/Storage2/FOSS_src/aarchvm/doc/README.zh.md)
  - [TODO.md](/media/luzeyu/Storage2/FOSS_src/aarchvm/TODO.md)

## 本轮测试

- 编译与 rootfs：
  - `timeout 1800s cmake --build build -j`
  - `timeout 1800s ./tests/linux/build_usertests_rootfs.sh`
- Linux 单核冷启动 / snapshot 构建：
  - `timeout 1800s ./tests/linux/build_linux_shell_snapshot.sh`
- Linux 单核功能回归：
  - `timeout 5400s ./tests/linux/run_functional_suite.sh`
- 裸机完整回归：
  - `timeout 5400s ./tests/arm64/run_all.sh`
- Linux SMP 功能回归：
  - `timeout 5400s ./tests/linux/run_functional_suite_smp.sh`

## 当前结论

- Linux 已能稳定识别新的 RTC 设备，functional log 中可见：
  - `rtc-pl031 9030000.rtc: registered as rtc0`
  - `/sys/class/rtc/rtc0` 枚举成功
  - 用户态 `rtc_smoke` 已验证 `/dev/rtc0` 的读时、设时、恢复三条路径
- 这轮修改没有破坏裸机完整回归、Linux 单核功能回归、Linux SMP 功能回归。
- 当前 RTC 仍是第一阶段实现：
  - 已具备 Linux 可见的 wall-clock / set-time / snapshot 基础闭环；
  - 尚未接入 GIC alarm IRQ，也还没有 `frozen/mock` 等确定性时钟模式。

# 修改日志 2026-04-01 19:13

## 本轮修改

- 为架构 timer 增加了双模式支持：
  - 默认 `step` 模式保持原有基于 guest step 的确定性计时语义；
  - 新增 `host` 模式，使 `CNTVCT/CNTPCT` 跟随宿主机 monotonic 时钟推进。
- 相关实现已接入：
  - [include/aarchvm/generic_timer.hpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/include/aarchvm/generic_timer.hpp)
  - [src/generic_timer.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/generic_timer.cpp)
  - [include/aarchvm/soc.hpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/include/aarchvm/soc.hpp)
  - [src/soc.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/soc.cpp)
  - [src/main.cpp](/media/luzeyu/Storage2/FOSS_src/aarchvm/src/main.cpp)
- 命令行与环境控制新增：
  - `-arch-timer-mode <step|host>`
  - `AARCHVM_ARCH_TIMER_MODE=step|host`
- SoC 主循环在 `host` 模式下改为使用更短的同步窗口，避免 timer IRQ 在长 chunk 中被明显拖后，从而让交互式 Linux 的时间流速恢复正常。
- 交互脚本默认切到 `host` 模式，而自动化回归保持 `step` 模式不变：
  - [tests/linux/run_interactive.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/linux/run_interactive.sh)
  - [tests/linux/run_gui_tty1.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/linux/run_gui_tty1.sh)
  - [tests/linux/run_gui_tty1_from_snapshot.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/linux/run_gui_tty1_from_snapshot.sh)
- 新增 Linux 用户态时间流速冒烟程序，并并入统一 initramfs：
  - [tests/linux/time_rate_smoke.c](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/linux/time_rate_smoke.c)
  - [tests/linux/build_usertests_rootfs.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/linux/build_usertests_rootfs.sh)
- 同步更新文档：
  - [README.md](/media/luzeyu/Storage2/FOSS_src/aarchvm/README.md)
  - [doc/README.en.md](/media/luzeyu/Storage2/FOSS_src/aarchvm/doc/README.en.md)
  - [doc/README.zh.md](/media/luzeyu/Storage2/FOSS_src/aarchvm/doc/README.zh.md)
  - [TODO.md](/media/luzeyu/Storage2/FOSS_src/aarchvm/TODO.md)

## 本轮测试

- 编译：
  - `timeout 1800s cmake --build build -j`
- initramfs / snapshot：
  - `timeout 1800s ./tests/linux/build_usertests_rootfs.sh`
  - `timeout 1800s ./tests/linux/build_linux_shell_snapshot.sh`
- host 模式定向验证：
  - `printf '/bin/time_rate_smoke\n' | AARCHVM_BUS_FASTPATH=1 AARCHVM_TIMER_SCALE=1 timeout 180s ./build/aarchvm -snapshot-load out/linux-usertests-shell-v1.snap -arch-timer-mode host -fb-sdl off -steps 600000000 -stop-on-uart 'TIME-RATE-SMOKE PASS'`
- 裸机完整回归：
  - `timeout 5400s ./tests/arm64/run_all.sh`
- Linux 单核功能回归：
  - `timeout 5400s ./tests/linux/run_functional_suite.sh`
- Linux SMP 功能回归：
  - `timeout 5400s ./tests/linux/run_functional_suite_smp.sh`

## 当前结论

- 问题根因不是 RTC，而是 Linux 启动后实际持续使用的架构 timer 仍按 guest step 推进，因此系统时间会跟着模拟执行速度跑快/跑慢。
- 这轮已经把“可重复回归”与“交互式时间正确性”拆开：
  - 自动化与性能基线继续使用 `step` 模式；
  - 手工交互和 GUI 恢复默认改为 `host` 模式。
- `time_rate_smoke` 在 `host` 模式下已验证：
  - `CLOCK_REALTIME` 约 `2004ms`
  - `CLOCK_MONOTONIC` 约 `2004ms`
  - RTC 增量 `2s`
  - 三者已重新对齐到宿主机节奏。

# 修改日志 2026-04-01 21:05

## 本轮修改

- 继续沿 Armv8-A 程序可见异常语义做审计，确认 `Rt==31` 的 system-register 访问语义无需修正，当前实现已经正确把它当作 `XZR/WZR` 处理，而非 `SP`。
- 新增裸机回归 [tests/arm64/sysreg_xzr_semantics.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/sysreg_xzr_semantics.S)，把以下边界正式锁进回归：
  - `MRS XZR, <sysreg>` 必须丢弃结果且不能污染当前 `SP`
  - `MSR <sysreg>, XZR` 必须以零为源值而不是错误读取 `SP`
  - 覆盖 generic sysreg 路径 `TPIDR_EL0` 与 special sysreg 路径 `SP_EL0`
- 新增裸机回归 [tests/arm64/mmu_el0_uxn_fetch_abort.S](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/mmu_el0_uxn_fetch_abort.S)，补上此前缺失的 lower-EL instruction-abort 强断言覆盖：
  - `EL0` 下对 `UXN` 页取指必须上报 `EC=0x20`
  - `IL=1`、`WnR=0`、`IFSC=permission fault level 3`
  - `FAR_EL1` 与 `ELR_EL1` 都必须指向 faulting EL0 PC
- 两个新增回归都已接入：
  - [tests/arm64/build_tests.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/build_tests.sh)
  - [tests/arm64/run_all.sh](/media/luzeyu/Storage2/FOSS_src/aarchvm/tests/arm64/run_all.sh)
  - [TODO.md](/media/luzeyu/Storage2/FOSS_src/aarchvm/TODO.md)

## 本轮测试

- `timeout 1800s ./tests/arm64/build_tests.sh`
- `timeout 120s ./build/aarchvm -bin tests/arm64/out/sysreg_xzr_semantics.bin -load 0x0 -entry 0x0 -steps 400000`
- `timeout 120s ./build/aarchvm -bin tests/arm64/out/mmu_el0_uxn_fetch_abort.bin -load 0x0 -entry 0x0 -steps 4000000`
- `timeout 5400s ./tests/arm64/run_all.sh`
- `timeout 5400s ./tests/linux/run_functional_suite.sh`
- `timeout 5400s ./tests/linux/run_functional_suite_smp.sh`

## 当前结论

- 这轮仍未发现新的执行语义 bug，但又补齐了一块此前没有强断言锁住的异常面：`EL0` lower-EL instruction abort。
- 目前异常/系统寄存器/MMU 这条线的剩余风险已明显收缩到更细碎的 syndrome 角落、SMP 内存模型和更长时程 Linux 压力交互，而不再是明显缺失的基础异常分类。
