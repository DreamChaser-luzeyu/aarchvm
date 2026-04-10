# Armv8-A 程序可见最小集审计（文档版）

更新时间：2026-04-10 22:57

## 目的与边界

- 本文档用于记录“当前代码状态”下，Armv8-A 程序可见最小实现的收口进度。
- 本轮继续做白盒审计，并修正了少量测试 / 回归侧问题；未修改模拟器源代码。
- 判定标准遵循项目目标：高性能优先，不追求微架构级精确，只要求 guest 程序可见行为正确。

## 当前判断

- 结论：**当前模型下 Armv8-A 程序可见最小集已经完成收口**。
- 仍保留的事项是额外置信度工作，而不是最小集阻塞项：`qemu-system-aarch64` 系统级差分与更长期 Linux SMP soak。现有 `qemu-aarch64` user-mode 差分入口已在当前环境重跑通过。

## 关键证据（只读代码审计）

1. 当前执行模型已提供单一全序内存可见性  
   `src/soc.cpp` 的 SMP 主循环按单个 `cpu.step()` 交织各核，`src/cpu.cpp` 的 `mmu_write_value()/raw_mmu_write()` 会同步提交访存效果；因此 guest 可见内存效果天然处在单一全序中，`DMB/DSB` 的 no-op 实现与 LSE acquire/release 变体折叠在该模型下不会放宽程序可见顺序。

2. 已实现同步异常家族的 syndrome / 保存态生成链路已白盒闭环  
   `src/cpu.cpp` 中的 `enter_sync_exception()`、`fault_status_code()`、`data_abort_iss()`、`set_par_el1_for_fault()`、`sysreg_trap_iss()` 与 `captured_pstate_for_sync_exception()`，再加上 `src/system_registers.cpp` 的 `exception_enter_sync()`，现在把当前已实现的 `UNDEFINED/WFx trap/FP trap/illegal execution/SVC/system-access trap/instruction abort/data abort/SP alignment/PC alignment/debug exceptions/BRK` 统一收敛到一套 `EC/IL/ISS/FAR/ELR/SPSR` 生成路径上；`debug_exception_regs`、`sync_exception_regs`、`svc_sysreg_minimal`、`software_step_basic`、`mmu_at_par_formats`、`mmu_at_par_write_fault_kinds` 等正式回归已把关键家族逐类锁定。

3. MMU/TLB/fault 与 fast-path / predecode 的交界已白盒闭环  
   `translate_data_address_fast()`、`translate_address()`、`translate_cache_maintenance_address()`、`walk_page_tables()` 与 `access_permitted()` 现在共用同一套权限 / translation 判定；`mmu_read_value()/mmu_write_value()` 在跨页路径按 byte 粒度推进并保存 faulting byte；`on_code_write()/notify_external_memory_write()/tlb_flush_*()/load_state()` 又把 CPU 自写、DMA、TLBI、IC IVAU、snapshot restore 的失效链统一起来。`aarchvm_unit_cpu_cache_consistency`、`predecode_pa_alias_codegen`、`mmu_tlbi_*`、`mmu_mair_write_flushes_tlb`、`mmu_sctlr_m_tlb_flush`、`mmu_el0_uxn_fetch_abort`、`mmu_el0_ap_fault`、`mmu_walk_ext_abort_*` 与 slow-decode 对账已把这些边界锁进正式回归。

4. 当前正式回归再次支撑上述判断  
   本轮已复跑并确认 `tests/arm64/run_all.sh`、`tests/linux/run_functional_suite.sh`、`tests/linux/run_algorithm_perf.sh`、`tests/linux/run_functional_suite_smp.sh`、`tests/linux/run_block_mount_smoke.sh`、`tests/linux/run_qemu_user_diff.sh` 全部通过；其中 `qemu-user` 差分还把 `fpsimd_selftest`、`fpint_selftest`、`mprotect_exec_stress`、`pthread_sync_stress` 与 `qemu-aarch64` 做了用户态对账。

## 非阻塞后续验证

1. `qemu-system-aarch64` 系统级差分  
   这会继续提高系统级置信度，但当前已不再阻塞“当前模型程序可见最小集”结论。

2. 更长期 Linux SMP soak / 压测  
   当前 UMP/SMP / block-mount / algorithm 路径都已通过，但更长时间的 soak 仍可继续增强信心。

3. `qemu-aarch64` user-mode 差分扩展  
   当前环境已补齐 `qemu-aarch64`，`tests/linux/run_qemu_user_diff.sh` 已通过；后续若要继续增强置信度，可把更多 deterministic 的用户态探针纳入这条入口。

## 本轮补充复核（2026-04-10 14:33）

- 重新核对了所有当前已实现的同步异常家族入口，确认 `ESR_EL1/FAR_EL1/PAR_EL1/ISS` 现在不再存在“家族已经实现、但 syndrome 仍未逐类对白盒闭环”的剩余缺口。
- 重新核对了 `MMU/TLB/fault` 与 fast-path / predecode 的共享 helper、失效链和跨页/faulting-byte 处理，确认此前担心的“细颗粒一致性”项已经通过共享路径与正式回归机械锁死。
- 复跑并确认以下回归通过：`tests/arm64/build_tests.sh`、`tests/arm64/run_all.sh`、`tests/linux/run_functional_suite.sh`、`tests/linux/run_algorithm_perf.sh`、`tests/linux/run_functional_suite_smp.sh`、`tests/linux/run_block_mount_smoke.sh`。
- 在 2026-04-10 14:33 这轮审计时，`tests/linux/run_qemu_user_diff.sh` 尚未重跑，原因是当时环境缺少 `qemu-aarch64`；这被记录为当时的额外验证缺口，而非当前最小集阻塞项。

## 本轮补充复核（2026-04-10 22:57）

- 已在当前容器补齐 `qemu-user/qemu-system` 工具，并重跑 `tests/linux/run_qemu_user_diff.sh`；当前 `fpsimd_selftest`、`fpint_selftest`、`mprotect_exec_stress`、`pthread_sync_stress` 的 `qemu-aarch64` user-mode 差分入口通过。
- 已把 [tests/linux/mprotect_exec_stress.c](/workspaces/aarchvm/tests/linux/mprotect_exec_stress.c) 的 `busybox` 路径解析改为优先 guest `/bin/busybox`、再回退到仓库内 `out/initramfs-usertests-root/bin/busybox`，修复 direct `qemu-aarch64 ./out/mprotect_exec_stress` 时误依赖宿主根文件系统的测试前提问题；这次修正只作用于测试入口，不改变模拟器行为。
- 已把 [tests/linux/run_algorithm_perf.sh](/workspaces/aarchvm/tests/linux/run_algorithm_perf.sh) 的 `INITRD_SIZE_HEX` 读取挪到按需重建 shell snapshot / initrd 之后，修复缺少 initrd 时脚本自身先天失败的前置条件问题。
- 已再次复跑并确认 `tests/arm64/run_all.sh`、`tests/linux/run_qemu_user_diff.sh`、`tests/linux/run_functional_suite.sh`、`tests/linux/run_functional_suite_smp.sh`、`tests/linux/run_algorithm_perf.sh`、`tests/linux/run_block_mount_smoke.sh` 全部通过。
- 基于此前已闭环的 `ESR_EL1/FAR_EL1/PAR_EL1/ISS`、`MMU/TLB/fault` 与 `SMP barrier/LSE` 白盒审计，再叠加当前补齐的 `qemu-aarch64` 差分与全回归结果，我现在可以自信地说“模拟器已经完整实现 ARM v8-A 要求的最小集合”。

## 与 TODO 的一致性

- 已同步更新 `TODO.md` 中“Armv8-A 程序可见正确性收尾计划”的审计状态：`异常 / 系统寄存器 / trap` 与 `MMU / fault / TLB / self-modifying code` 两个此前剩余的高优先级收口项现已勾选完成。
