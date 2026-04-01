# Armv8-A 程序可见最小集审计（文档版）

更新时间：2026-04-01 22:34

## 目的与边界

- 本文档用于记录“当前代码状态”下，Armv8-A 程序可见最小实现的收口进度。
- 本轮为后台文档审计，未修改模拟器源代码、测试源代码、DTS 或构建逻辑。
- 判定标准遵循项目目标：高性能优先，不追求微架构级精确，只要求 guest 程序可见行为正确。

## 当前判断

- 结论：**接近收口，但暂不能宣布“已完整收口”**。

## 关键证据（只读代码审计）

1. Barrier 路径仍需 SMP 语义闭环  
   `src/cpu.cpp` 中 `DMB/DSB/ISB` 当前为 decode 后直接 `return true`（见约 `4957-4969` 附近逻辑），尚未在实现层显式体现更强的跨核顺序约束模型。

2. LSE 顺序变体仍标注为简化语义  
   `src/cpu.cpp` 的 LSE 相关路径仍含“ordering variants ... single-core semantics for now / no-ops beyond RMW”等注释（约 `11531+`、`11611+` 附近），说明 acquire/release/ordered 变体在并发语义上仍需持续验证。

3. 异常 syndrome 覆盖虽广但仍需逐类对账  
   `fault_status_code`、`data_abort_iss`、`set_par_el1_for_fault` 等 helper 已存在（`src/cpu.cpp` 约 `4629+`），但仍需按“当前已实现异常家族”做 `ESR_EL1/FAR_EL1/PAR_EL1/ISS` 的系统化闭环清单，避免遗漏角落。

## 剩余缺口（建议继续收口顺序）

1. SMP 同步语义  
   优先补强 `barrier + exclusive/LSE` 的系统级差分与压力验证，确保程序可见顺序行为稳定。

2. MMU/TLB/fault 一致性  
   聚焦跨页、fault byte、TLBI/IC IVAU 与并发交错场景的组合测试，避免“单测通过但系统压力偶发异常”。

3. FP/AdvSIMD 尾差  
   继续清理 `FPCR/FPSR + NaN/subnormal/flags` 角落，特别是标量/向量路径一致性。

4. 差分与 Linux SMP 压测  
   在保留现有 `qemu-aarch64`（user-mode）差分基础上，补强 `qemu-system-aarch64` 参考路径和更长期 SMP 压测，提升系统级置信度。

## 本轮补充复核（2026-04-01 22:34）

- 复查了当前仓库内与差分相关的脚本入口：已存在 `tests/linux/run_qemu_user_diff.sh`（`qemu-aarch64` user-mode），未发现对应 `qemu-system-aarch64` 的系统级差分脚本入口。
- 复查了 `TODO.md` 中 Armv8-A 收口相关复选框，未发现“条目文字已经明确完成、但仍保持未勾选”的明显冲突项。
- 对于“部分完成、尚未形成端到端闭环”的条目（尤其 SMP/事件驱动相关），维持未勾选状态以避免虚高完成度。

## 与 TODO 的一致性

- 已同步更新 `TODO.md` 中“Armv8-A 程序可见正确性收尾计划”的审计状态与剩余高优先级缺口。
