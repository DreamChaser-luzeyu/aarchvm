# Armv8-A 程序可见最小集审计（文档版）

更新时间：2026-04-06 03:13

## 目的与边界

- 本文档用于记录“当前代码状态”下，Armv8-A 程序可见最小实现的收口进度。
- 本轮为后台文档审计，未修改模拟器源代码、测试源代码、DTS 或构建逻辑。
- 判定标准遵循项目目标：高性能优先，不追求微架构级精确，只要求 guest 程序可见行为正确。

## 当前判断

- 结论：**接近收口，但暂不能宣布“已完整收口”**。

## 关键证据（只读代码审计）

1. 当前执行模型已提供单一全序内存可见性  
   `src/soc.cpp` 的 SMP 主循环按单个 `cpu.step()` 交织各核，`src/cpu.cpp` 的 `mmu_write_value()/raw_mmu_write()` 会同步提交访存效果；因此 guest 可见内存效果天然处在单一全序中，`DMB/DSB` 的 no-op 实现与 LSE acquire/release 变体折叠在该模型下不会放宽程序可见顺序。

2. 现有 SMP litmus 与 Linux SMP 回归继续支撑上述判断  
   `smp_dmb_message_passing`、`smp_lse_casa_publish`、`smp_lse_ldaddal_counter`、`smp_spinlock_ldaxr_stlxr` 与 `tests/linux/run_functional_suite_smp.sh` 这组路径在当前代码状态下继续通过，说明 barrier / exclusive / LSE 的核心程序可见约束未见回退。

3. 异常 syndrome 覆盖虽广但仍需逐类对账  
   `fault_status_code`、`data_abort_iss`、`set_par_el1_for_fault` 等 helper 已存在（`src/cpu.cpp` 约 `4629+`），但仍需按“当前已实现异常家族”做 `ESR_EL1/FAR_EL1/PAR_EL1/ISS` 的系统化闭环清单，避免遗漏角落。

## 剩余缺口（建议继续收口顺序）

1. MMU/TLB/fault 一致性  
   聚焦跨页、fault byte、TLBI/IC IVAU 与并发交错场景的组合测试，避免“单测通过但系统压力偶发异常”。

2. FP/AdvSIMD 尾差  
   继续清理 `FPCR/FPSR + NaN/subnormal/flags` 角落，特别是标量/向量路径一致性。

3. 差分与 Linux SMP 压测  
   在保留现有 `qemu-aarch64`（user-mode）差分基础上，补强 `qemu-system-aarch64` 参考路径和更长期 SMP 压测，提升系统级置信度。

## 本轮补充复核（2026-04-06 03:13）

- 重新串联了 `src/soc.cpp` 的 SMP 调度与 `src/cpu.cpp` 的访存提交路径，确认当前模型对 guest 可见内存效果提供单一全序，因此 barrier 与 LSE acquire/release 变体不再是阻止宣布“接近收口”的首要疑点。
- 复跑并确认以下回归通过：`tests/arm64/run_all.sh`、`tests/linux/run_qemu_user_diff.sh`、`tests/linux/run_functional_suite.sh`、`tests/linux/run_functional_suite_smp.sh`。
- 当前仍未闭环的重点已经收缩为：`ESR_EL1/FAR_EL1/PAR_EL1/ISS` 逐类对账、`MMU/TLB/fault` 与 fast-path / predecode 的细颗粒一致性、以及缺失的 `qemu-system-aarch64` 系统级差分入口。

## 与 TODO 的一致性

- 已同步更新 `TODO.md` 中“Armv8-A 程序可见正确性收尾计划”的审计状态与剩余高优先级缺口。
