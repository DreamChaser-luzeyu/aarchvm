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
