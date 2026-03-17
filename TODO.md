# TODO

## 事件驱动化演进方案

目标：
- 将 SoC 外层调度、时间推进和设备同步从“按固定步数轮询”逐步改为“按事件和 deadline 驱动”。
- 保持 CPU 指令语义正确，不为了性能破坏 guest 可感知行为。
- 允许保留当前慢路径/兼容路径，并渐进切换到新的事件驱动主循环。

### 0. 设计约束

- CPU 内核短期内仍保持解释执行，不直接把“事件驱动”理解成跳过普通指令执行。
- 中断仍只在指令边界生效。
- SMP 下的共享内存可见性、SEV/WFE/WFI、TLBI、IC IVAU、PSCI CPU_ON 等跨核行为必须即时生效。
- 自修改代码、动态生成代码、TLB 切换、快照恢复不能因事件驱动化而被破坏。

### 1. 抽象出统一 guest 时间

目标：
- 用统一的 `guest_now` / `guest_ticks` 替代当前强依赖 `timer_steps_ * scale` 的时间推进模型。

任务：
- [x] 在 SoC 层引入统一 guest 时间基准，并让 timer、脚本注入、stop-on-pattern 等都基于它。
- [x] 区分 guest 时间与 host 时间；SDL 刷新、stdin poll 等宿主机行为不应直接污染 guest 时间。
- [x] 为新旧时间模型保留切换开关，便于回归与定位问题。

预期收益：
- 降低 `console=ttyAMA0` 与 `console=tty1` 对 `AARCHVM_TIMER_SCALE` 的异常敏感性。

### 2. 引入事件调度器

目标：
- 为设备和外部输入提供统一的事件接口，避免每轮全量 `sync_devices()`。

任务：
- [x] 在 SoC 层实现最小事件队列，支持 `(deadline, event_type, target)` 的调度。
- [ ] 为设备定义统一接口，例如：
  - `next_deadline(guest_now)`
  - `process_until(guest_now)`
  - `on_external_input(...)`
  - `on_state_change(...)`
- [x] 先让 timer 成为第一个真正基于 deadline 的设备。

预期收益：
- 将“轮询所有设备”改为“只处理已到期或状态变化的设备”。

### 3. 重构 SoC 主循环

目标：
- 把当前固定 chunk 驱动改为“运行到下一个原因”。

任务：
- [ ] 主循环改为：
  - 处理已到达的 host 输入事件；
  - 查询最近 guest deadline；
  - 查询最近 host poll deadline；
  - 计算当前 CPU 可安全运行的窗口；
  - 运行 CPU；
  - 处理到期 guest 事件；
  - 在无 runnable CPU 时直接等待事件或跳到最近 deadline。
- [x] 保留旧的步数驱动路径作为 debug / fallback 模式。

预期收益：
- 降低空转和无意义设备同步开销。

当前状态：
- 已完成“统一 guest 时间 + 最小 timer deadline 调度 + 旧步进路径 fallback”的第一阶段闭环。
- 当前默认主线继续使用 `event` 调度，以保证 SMP timer / Linux 路径的程序可见正确性。
- `AARCHVM_SCHED_MODE=legacy` 仍保留用于对照和定位，但它在 SMP 下会把近期限时器明显延后，不应视作等价语义模式。

### 4. 先把 Generic Timer deadline 化

目标：
- Timer 从“每轮同步一次的被动设备”变成“主动报告下一截止时间的设备”。

任务：
- [x] 让 `GenericTimer` 直接基于 `guest_now` 计算当前 counter。
- [x] 提供“下一个 IRQ 何时到来”的精确接口。
- [x] 让 SoC 不再每轮调用 `sync_to_steps()`，而是在事件边界或寄存器读写时更新 timer 状态。

预期收益：
- Timer 不再成为所有路径都必须经过的高频同步点。

### 5. WFI/WFE 真正停车

目标：
- 等待态 CPU 不再继续参与每轮 `cpu.step()`。

任务：
- [ ] 把 CPU 运行态细分为 `runnable / waiting_irq / waiting_event / halted / powered_off`。
- [ ] `WFI` CPU 仅在 IRQ 到来时唤醒。
- [ ] `WFE` CPU 仅在事件或可中断异常到来时唤醒。
- [ ] 对单活跃 CPU 场景启用长突发执行，直到最近事件 deadline。

预期收益：
- 这是 SMP 性能提升最大的低风险步骤之一。

### 6. SMP 调度从“每核一步”升级为“量子 + deadline”

目标：
- 避免当前 SMP 路径的“一核一步、每轮时间只加一次”的低效和失真。

任务：
- [ ] 当只有一个 CPU runnable 时，复用单核长 chunk 逻辑。
- [ ] 多个 CPU runnable 时，引入可配置小量子，例如每核 32/64/128 条。
- [ ] 一个调度轮次结束后，再按该轮的 guest 时间窗口推进时间。
- [ ] 给调度器加入必要的公平性约束，避免某核长期饥饿。

预期收益：
- 提高 SMP 性能，并减少 guest 时间模型与真实执行量脱节的问题。

### 7. GIC 改为增量更新而非轮询扫描

目标：
- 减少 `has_pending()` / `acknowledge()` 的线性扫描开销，并让 IRQ 查询更接近事件驱动。

任务：
- [ ] 为每 CPU 维护 pending summary / candidate cache。
- [ ] 在 `set_level()`、enable、priority、PMR 变化时增量更新状态。
- [ ] 让 `has_pending()` 优先查 summary，而不是每次全扫 local + SPI。
- [ ] 让 `acknowledge()` 先命中缓存候选，再必要时精查。

预期收益：
- 降低 IRQ 热路径成本，改善 SMP 下大量中断查询的开销。

### 8. UART/KMI 改为状态变化驱动

目标：
- 避免 UART/KMI IRQ 状态被 SoC 每轮轮询。

任务：
- [ ] UART RX FIFO 从空变非空、mask 变化、使能变化时，立即更新 IRQ line。
- [ ] KMI RX 与控制位变化同理。
- [ ] 保持 guest 可见寄存器语义不变，但把 IRQ 推送改成状态变化触发。
- [ ] UART TX 的宿主机输出做缓冲/批量 flush，避免每字节 `fflush()`。

预期收益：
- 降低串口路径对整体执行节奏的污染。

### 9. Framebuffer / SDL 彻底与 guest 时间解耦

目标：
- GUI 刷新和键盘轮询不再影响 guest 时间推进。

任务：
- [ ] SDL present 只依赖 dirty 状态和 host 刷新节流，不再成为 SoC 高频同步点。
- [ ] SDL 键盘输入进入 host 事件队列，再转成 PS/2 注入事件。
- [ ] 允许 headless 路径完全绕开 framebuffer host 逻辑。

预期收益：
- 减少 GUI 与非 GUI 路径之间的时序相互干扰。

### 10. main 层事件化

目标：
- 让 `main.cpp` 不再以固定 `run_chunk=200000` 驱动整个系统。

任务：
- [ ] 将 UART/PS2 脚本注入改为真正的 guest 时间事件。
- [ ] stdin 注入按 host 事件和 FIFO 水位控制，不再靠固定 gap + chunk 对齐。
- [ ] 保留 `-steps` 作为最大执行预算，但内部执行逻辑改为事件边界优先。

预期收益：
- 自动化测试、快照构建、交互模式将共享更一致的执行模型。

### 11. 快照支持补强

目标：
- 事件驱动化后，快照仍能完整恢复到同一 guest 状态。

任务：
- [ ] 快照中保存：
  - `guest_now`
  - CPU 等待态
  - 设备内部 deadline 所需状态
  - 事件队列内容
- [ ] 恢复后重建 host 层非 guest 语义对象，例如 SDL 刷新时钟、stdout flush 状态。

预期收益：
- 事件驱动不会破坏当前已可用的 snapshot 工作流。

### 12. 测试与回归策略

目标：
- 每一阶段都能被单独验证，而不是一次性大改后集中排雷。

任务：
- [ ] 为 timer deadline、WFI/WFE 唤醒、SEV、PSCI CPU_ON、IPI/SGI、TLBI 后唤醒等路径补单测。
- [ ] 保持单核 Linux、SMP Linux、串口 shell、GUI tty1、snapshot restore 全回归。
- [ ] 对 `console=ttyAMA0` 与 `console=tty1` 分别保留回归，以观测事件驱动后时间模型是否收敛。

### 13. 建议实施顺序

第一阶段：
- 统一 guest 时间
- Timer deadline 化
- SoC 主循环支持“运行到下一个 deadline”

第二阶段：
- WFI/WFE 停车
- 单活跃 CPU 长突发执行
- SMP 小量子调度

第三阶段：
- GIC summary / cached pending
- UART/KMI 状态变化驱动
- UART host 输出批量化

第四阶段：
- SDL / framebuffer host 逻辑完全剥离
- main 输入注入事件化
- 快照格式补强

### 14. 暂不优先做的事项

- 真正的多线程并行 SMP：这是长期方向，但不适合在当前轮次优先做。
- JIT：在外层时间和事件模型仍不稳定之前，JIT 的收益会被调度/同步失真抵消。
- 为单个 guest workload 做特化快路径：优先做通用事件框架，而不是面向某个命令或某段日志的特例优化。

### 15. 基于 2026-03-17 SMP 热点复测的近期优先级

现状：
- 当前 SMP 算法性能结果见：
  - `out/perf-smp-current-results.txt`
- 当前 SMP `perf` 热点见：
  - `out/perf-smp-current-aarchvm-only.report`
- 当前 SMP `gprof` 热点见：
  - `out/gprof-smp-current.txt`
- 两份热点报告给出的结论高度一致：
  - `GicV3::has_pending(...)` 是当前 SMP 路径的绝对第一热点；
  - `gprof` 中其自耗时约 `87.52%`；
  - `perf` 中其 cycles 占比约 `85.43%`；
  - 它已经远高于 `Cpu::step()`、`lookup_decoded()`、`translate_address()`、`exec_load_store()` 这些此前更常见的热点。

判断：
- 这说明当前 SMP 最大瓶颈不是 CPU 解释执行本身，而是“等待态/空闲态 CPU 仍在反复查询 GIC 是否有可接收中断”。
- 在把这条链压下去之前，继续投入更多时间到 decode/MMU/load-store 微优化，收益会被 GIC 轮询成本吞掉。

下一步优化顺序：
- [ ] P0: 先实现真正的 `WFI`/等待态停车
  - 目标：处于 `waiting_for_interrupt_` 的 CPU 不再在每轮 `step()` 中调用 `gic_.has_pending()` 自旋。
  - 方案：SoC 维护 runnable CPU 集；`WFI` CPU 从调度集合摘出，仅在 IRQ line 边沿、GIC pending summary 变化、SEV/PSCI/SGI 等明确唤醒事件到来时重新加入。
  - 预期收益：这是当前最可能直接砍掉 `has_pending()` 主热点的改动。
- [ ] P1: 为 GIC 增加每 CPU pending summary / best-candidate cache
  - 目标：即便需要查询 pending，也不再每次线性扫描本地中断和全部 SPI。
  - 方案：
    - 为 local/SPIs 维护 pending+enable+priority 变化驱动的 summary；
    - `has_pending(cpu, pmr)` 先查 summary/缓存候选；
    - `acknowledge()` 先命中缓存，再必要时回退精查。
  - 预期收益：压缩非等待态 CPU 上的 IRQ 查询固定成本。
- [ ] P2: 将 CPU 侧 IRQ 门控从“epoch + negative cache”推进到“line-driven wakeup”
  - 目标：CPU 不再频繁主动问 GIC“有没有中断”，而是更多依赖 SoC/GIC 在状态变化时推送“现在可能有中断”。
  - 方案：给每 CPU 挂一个轻量 `irq_maybe_pending` / `irq_wakeup_needed` 标记，由 `set_level()`、PMR/enable 改变时增量更新。
  - 预期收益：进一步削减 `step()` 里与 IRQ 相关的固定判断。
- [ ] P3: 在解决 GIC/WFI 热点之后，再回到第二梯队热点
  - `Cpu::lookup_decoded()`
  - `Cpu::translate_address()`
  - `Cpu::exec_load_store()` / `Bus::read()`
  - `SoC::next_device_event()`
  - 理由：这些路径当前仍是热点，但在 SMP 场景下都明显排在 `has_pending()` 之后。

执行建议：
- [ ] 下一轮性能优化应严格按以下顺序推进：
  - 先做 `WFI` 停车；
  - 再做 GIC pending summary；
  - 每做完一步都重新跑：
    - SMP perf suite
    - SMP `perf`
    - SMP `gprof`
  - 只有在 `has_pending()` 不再是压倒性热点后，才继续做 decode/MMU/load-store 的细化优化。

## RISC-V RV64IMAC 支持方案

现状判断：
- 当前工程的 `Bus` / `Ram` / `UART` / `PL050 KMI` / `Framebuffer` / `Block MMIO` / `PerfMailbox` / `Snapshot IO` / SoC 外层事件调度，这些层大体是可复用的。
- 当前工程的 `Cpu` / `SystemRegisters` / `GicV3` / `GenericTimer` / `SoC` 中断接线 / `main.cpp` 的 boot ABI / Linux DTB 与 U-Boot 启动路径 / `tests/arm64`，基本都强绑定于 Armv8-A/AArch64。
- 因此，支持 RISC-V 的正确方向不是在现有 `Cpu` 里硬塞第二套解码，而是先做“架构无关外壳 + 架构专用核心”的拆层。
- 另外要注意：如果目标只是“裸机 RV64IMAC 程序能跑”，工作量明显小于“Linux on RISC-V 能跑”。后者除了 RV64IMAC 指令外，还至少需要特权架构、CSR、异常/中断、SBI、MMU(Sv39) 与常见平台设备模型。严格说，Linux 路径通常还需要 `Zicsr` / `Zifencei` 这类 today 实际不可缺的扩展。

### 0. 目标边界

目标：
- 明确“RV64IMAC 支持”分成裸机最小闭环与 Linux 闭环两阶段，避免一开始就把 scope 混在一起。

任务：
- [ ] 先把目标拆成两级：
  - `Level 1`: 裸机 RV64IMAC + trap + 定时器 + 串口输出。
  - `Level 2`: RISC-V Linux 所需最小平台，至少含 M/S/U、CSR、SBI、Sv39、PLIC/CLINT 或 ACLINT 类设备。
- [ ] 明确文档口径：`RV64IMAC` 指令支持不等于“可启动 Linux/RISC-V 用户态”。

### 1. 先做架构解耦

目标：
- 把当前 AArch64 强绑定的 `SoC + Cpu` 结构拆成“通用机器层 + 架构核心层”。

任务：
- [ ] 抽象统一的 CPU 核心接口，例如：
  - `reset(entry_pc)`
  - `step()`
  - `halted()/waiting_*()`
  - `irq line / event line` 注入
  - `save_state()/load_state()`
  - `perf_counters()`
- [ ] 抽象架构相关的 boot ABI：
  - Arm 当前是 `x0=dtb_addr`、U-Boot/DTB 路径。
  - RISC-V 后续需要 `a0=hartid`、`a1=dtb_addr`、OpenSBI 或直接 M-mode 裸机入口。
- [ ] 把当前 `SoC` 中和 Arm 强绑定的逻辑隔离出来：
  - PSCI/SMCCC
  - MPIDR affinity
  - GIC PPI/SPI 编号
  - CNTV/CNTP sysreg timer
- [ ] 让现有事件调度、快照、UART/FB/block/perf mailbox 仍能留在架构无关层。

预期收益：
- 后续新增 RISC-V 时，不需要复制一整份 SoC 调度、设备、snapshot、perf 基础设施。

### 2. 新建 RISC-V 机器骨架

目标：
- 在不破坏 Arm 现状的前提下，引入最小的 RISC-V 机器与 CPU 类型。

任务：
- [ ] 新增独立的 `Rv64Cpu`，不要在现有 `Cpu` 类中混写两套 ISA。
- [ ] 新增 `Rv64Machine` 或等价的架构装配层，负责：
  - hart 数量
  - reset PC
  - 中断线接线
  - timer / software interrupt / external interrupt 接线
- [ ] 为 `main.cpp` 增加架构选择入口，例如未来的 `-arch arm64|rv64`，默认保持当前 Arm64。
- [ ] 让 snapshot header 具备架构标识，避免 Arm snapshot 被错误当作 RISC-V snapshot 恢复。

### 2.1 建议的代码拆分与落点

目标：
- 把这件事设计成“可渐进重构”，而不是一上来大规模重命名整个工程。

任务：
- [ ] 建议先引入一个最小公共 CPU 接口，例如 `ICpuCore` / `CpuCoreBase`，只暴露：
  - `reset(entry_pc)`
  - `step()`
  - `halted()/waiting_*()`
  - `save_state()/load_state()`
  - `perf_counters()`
  - 外部中断/事件注入
- [ ] 当前 `Cpu` 先保留实现不动，逻辑上把它视作 `ArmCpu`；等抽象层稳定后，再决定是否重命名文件和类型，避免一开始制造过大的机械改动。
- [ ] 将当前 `SoC` 里真正架构无关的部分沉到更通用的机器层：
  - `Bus/Ram/BusFastPath`
  - UART/KMI/Framebuffer/Block/PerfMailbox
  - guest 时间推进
  - 事件调度
  - snapshot 外壳
- [ ] 将当前 `SoC` 中 Arm 专用的部分收束为 `ArmPlatform` 或等价层：
  - GIC
  - Generic Timer
  - PSCI/SMCCC
  - MPIDR 拓扑与 secondary bring-up
- [ ] 为 RISC-V 新增对等的 `Rv64Platform`：
  - hart 拓扑
  - ACLINT/PLIC 接线
  - SBI/firmware handoff
  - RISC-V boot ABI
- [ ] snapshot 设计建议改成：
  - common header
  - arch tag
  - machine-common blob
  - per-hart cpu blob
  - per-device blob
  - 这样后续不会因为“Arm/RISC-V CPU 状态布局不同”把快照格式绑死。

设计约束：
- [ ] 不要在 `step()` 或 `Bus` 热路径上加入 `if (arch == arm64) ... else ...` 这种高频分支。
- [ ] `BusFastPath` 仍保持“按当前机器地址图固定编码生成”的思路；如果未来有 RISC-V fast path，就做成另一套机器专用 fast path，而不是在一套 fast path 里混两套地址判断。

### 2.2 建议的首个 RISC-V 机器模型

目标：
- 先选一个“足够接近现有生态、又能尽量复用本项目设备”的 RISC-V 平台模型。

任务：
- [ ] 建议首个 RISC-V 机器采用“`virt` 风格内存图 + 项目现有设备复用”的折中方案，而不是一开始就完全照搬 Arm 板级图。
- [ ] 建议的第一版地址图：
  - DRAM：`0x8000_0000`
  - UART：`0x1000_0000`
  - ACLINT/CLINT：`0x0200_0000`
  - PLIC：`0x0c00_0000`
  - 可选 framebuffer / block / perf mailbox：放到单独的高地址 MMIO 区
- [ ] 串口设备第一阶段优先复用现有 UART 实现，而不是为了“更像 QEMU virt”立刻再做一套 16550。
- [ ] 设备树第一阶段也优先围绕“本模拟器自己的最小 RISC-V 板级模型”来写，不追求与 QEMU 完全兼容，只追求 Linux/OpenSBI/裸机程序可理解。

设计判断：
- [ ] 这样做的好处是：
  - RAM/中断控制器/定时器地址布局更接近 RISC-V 生态习惯；
  - 串口/块设备/性能 mailbox/GUI 路径又能最大限度复用现有代码；
  - 未来若要追加 `ns16550a`，也不会推翻第一阶段的 CPU/CSR/MMU 工作。

### 3. 先打通裸机 RV64IMAC 最小闭环

目标：
- 不碰 Linux，先把最小裸机程序跑起来并形成严格单测闭环。

任务：
- [ ] 实现 RV64I 基础整数指令：
  - `LUI/AUIPC/JAL/JALR`
  - `BEQ/BNE/BLT/BGE/BLTU/BGEU`
  - `ADDI/SLTI/SLTIU/XORI/ORI/ANDI/SLLI/SRLI/SRAI`
  - `ADD/SUB/SLL/SLT/SLTU/XOR/SRL/SRA/OR/AND`
  - `LB/LH/LW/LD/LBU/LHU/LWU/SB/SH/SW/SD`
- [ ] 实现 `M` 扩展：
  - `MUL/MULH/MULHU/MULHSU/DIV/DIVU/REM/REMU` 及 32-bit `W` 变体。
- [ ] 实现 `A` 扩展：
  - `LR/SC`
  - `AMOSWAP/AMOADD/AMOXOR/AMOAND/AMOOR/AMOMIN/AMOMAX/AMOMINU/AMOMAXU`
- [ ] 实现 `C` 扩展：
  - 先覆盖 toolchain 最常生成的一小批高频 compressed 指令，再逐步补全。
- [ ] 先做最小 trap/异常框架：
  - illegal instruction
  - ecall
  - load/store/fetch fault
  - misaligned 行为先明确策略：仿真支持还是按规范 trap
- [ ] 先打通一个最小 UART 裸机 hello world。

预期收益：
- 在不引入特权级/MMU/Linux 复杂度的前提下，先验证解码器、寄存器模型、异常返回与访存语义是通的。

### 4. 实现 RISC-V CSR 与特权级最小集

目标：
- 为后续 timer/中断/SBI/MMU/Linux 做准备。

任务：
- [ ] 建立 RISC-V CSR 子系统，至少包含：
  - `mstatus/misa/mtvec/mepc/mcause/mtval/mip/mie/mscratch`
  - `sstatus/stvec/sepc/scause/stval/sip/sie/sscratch`
  - `satp/time/cycle/instret`
- [ ] 明确 `M/S/U` 三层执行模型与 trap 委托：
  - `medeleg/mideleg`
  - `mret/sret`
- [ ] 补齐 `Zicsr` 与 `Zifencei` 所需指令和行为。
- [ ] 单测覆盖：
  - CSR 读改写
  - trap 进入与返回
  - 委托到 S-mode
  - `fence.i` 对自修改代码的可见性

### 5. 实现 RISC-V timer / IPI / 外部中断平台

目标：
- 先构建一个 Linux 前也需要的最小平台中断模型。

任务：
- [ ] 在“尽量复用当前事件调度器”的前提下，为 RISC-V 新增 CLINT/ACLINT 类 timer/software interrupt 设备。
- [ ] 新增 PLIC 或等价最小外部中断控制器。
- [ ] 把 UART、块设备等外设挂到 PLIC。
- [ ] 支持多 hart 的：
  - software interrupt
  - machine/supervisor timer interrupt
  - external interrupt claim/complete
- [ ] 补单测：
  - timer interrupt
  - software IPI
  - UART interrupt
  - SMP 下 `LR/SC` + IPI 协同

### 6. 实现 Sv39 MMU

目标：
- 为用户态程序和 Linux 提供最关键的地址翻译能力。

任务：
- [ ] 实现 `satp` 驱动的 Sv39 页表遍历。
- [ ] 支持 `R/W/X/U/G/A/D` 等核心位语义。
- [ ] 明确并测试：
  - instruction/load/store page fault
  - access fault vs page fault 区分
  - `SUM/MXR`
  - TLB 与 `SFENCE.VMA`
- [ ] 复用现有 TLB/快路径/跨页访存测试思路，但不要把 Arm 的表格式假设直接照搬。

预期收益：
- 这是从“裸机可跑”过渡到“用户态/内核可跑”的最大门槛。

### 7. 做最小 SBI 与 Linux 启动闭环

目标：
- 在已有 CSR、timer、PLIC、Sv39 之上，让 RISC-V Linux 能到串口 shell。

任务：
- [ ] 先决定启动策略：
  - 方案 A：实现最小 OpenSBI 兼容接口，让 Linux 走标准 `fw_dynamic` / `fw_jump` 路径。
  - 方案 B：先写一个极小 M-mode shim，只提供 Linux 所需的最小 SBI 调用。
- [ ] 提供 Linux 所需的最小设备树与平台约定：
  - memory
  - cpus/harts
  - timer
  - interrupt-controller
  - serial
  - chosen/bootargs
- [ ] 先目标到：
  - earlycon
  - 串口日志
  - `initramfs`
  - BusyBox shell
- [ ] 如果只做 RV64IMAC 而不加 F/D，则需同步确认工具链、OpenSBI、Linux 配置均关闭浮点依赖。

### 7.1 Linux 路径的推荐启动策略

目标：
- 先选择一条最短可达 Linux shell 的路径，避免在 firmware/bootloader 上过度分叉。

任务：
- [ ] 推荐顺序不是“先做 U-Boot on RISC-V”，而是：
  - 第一步：裸机程序直启；
  - 第二步：最小 M-mode firmware / SBI shim；
  - 第三步：直接启动 Linux kernel + DTB + initramfs；
  - 最后才考虑是否需要 U-Boot。
- [ ] 第一版 Linux 路径建议优先实现“内建极小 M-mode shim”，而不是一开始就兼容完整 OpenSBI：
  - 把 hart0 置于 M-mode 固件入口；
  - 初始化委托、基本 CSR、timer/IPI 平台；
  - 再以 `a0=hartid`、`a1=dtb_addr` 跳转到 S-mode Linux。
- [ ] 第一版 SBI 建议最小覆盖：
  - `set_timer`
  - `send_ipi`
  - `remote_fence_i`
  - `remote_sfence_vma` / `remote_sfence_vma_asid`
  - `hart_start/hart_stop/hart_status`
- [ ] `console_putchar`/`console_getchar` 可以放在较后位置，不要为了兼容旧 SBI console 而影响主路径设计。

设计判断：
- [ ] 对当前项目而言，最短路径是“自带极小 firmware + 直接进 Linux”，而不是“OpenSBI + U-Boot + Linux”三级全做。
- [ ] 等 Linux shell 跑稳后，再决定是否值得把外部 OpenSBI / U-Boot 纳入支持矩阵。

### 8. 测试策略

目标：
- RISC-V 支持从一开始就有独立而严格的测试闭环，而不是复用 Arm 测试后期补洞。

任务：
- [ ] 新建 `tests/rv64`，分为：
  - `unit-baremetal`
  - `smp-baremetal`
  - `linux-smoke`
- [ ] 先保证每补一类指令就有对应裸机单测，不依赖 Linux 才发现问题。
- [ ] 为 C 扩展补“反汇编/编译器生成”双路径测试，避免只测手写 case。
- [ ] 为 CSR / trap / `SFENCE.VMA` / `fence.i` / `LR/SC` 补严格的多核测试。
- [ ] Linux 闭环阶段再新增：
  - shell snapshot
  - BusyBox 功能回归
  - 算法性能回归

### 8.1 需要优先保证正确的语义点

目标：
- 优先补“程序最容易感知、且最容易在 SMP/Linux 场景踩炸”的语义，而不是只追求 ISA 覆盖率。

任务：
- [ ] `LR/SC` reservation 语义必须从第一版就按多 hart 语义设计：
  - reservation 为每 hart 独立持有；
  - 其它 hart 对 reservation 集合内地址的 store/AMO 会使其失效；
  - `SC` 成功/失败返回值与写入可见性要单测。
- [ ] `FENCE` / `FENCE.I` 不要偷懒混为一类：
  - `FENCE.I` 至少必须正确失效取指/预解码相关缓存；
  - `FENCE` 在当前模拟器模型下可先实现为保守全栈屏障，但要把语义写清楚。
- [ ] misaligned access 要先定策略并保持一致：
  - 第一阶段建议优先做 precise trap；
  - 不要一部分指令 silently fixup、一部分指令 trap。
- [ ] precise trap 信息要尽量完整：
  - `mcause/scause`
  - `mtval/stval`
  - faulting pc / return pc
- [ ] 需要专门准备一批多核测试，而不是只靠 Linux 压力回归来碰运气：
  - `LR/SC` 竞争
  - AMO 与普通 store 竞争
  - timer IPI + `WFI`
  - `SFENCE.VMA` 后另一 hart 的页表可见性
  - `remote_fence_i` 后另一 hart 执行新代码

### 9. 性能与代码结构约束

目标：
- 即使新增第二套 ISA，也不把当前高性能路径彻底拖慢。

任务：
- [ ] 维持“每种 ISA 一套独立的 decode/execute 热路径”，避免在单条指令执行里高频分支判断 `arch == ...`。
- [ ] 让架构无关层只保留调度、设备、snapshot、host IO 这些本就不在每条指令热路径上的逻辑。
- [ ] 为 RV64 单独设计 decode cache，不与当前 AArch64 decode cache 共享结构体。
- [ ] 明确 RISC-V 第一阶段不做的事情：
  - 向量扩展
  - 浮点扩展
  - JIT
  - 真正多线程 SMP

### 10. 建议实施顺序

第一阶段：
- 架构解耦
- `Rv64Cpu` 骨架
- 裸机 RV64I + UART hello

第二阶段：
- `M/A/C`
- trap/CSR/Zicsr/Zifencei
- 裸机 SMP 与 timer/IPI

第三阶段：
- PLIC/CLINT(或 ACLINT)
- Sv39 + TLB + `SFENCE.VMA`
- 最小 SBI

第四阶段：
- Linux DTB
- Linux + initramfs + BusyBox shell
- snapshot / 回归 / 性能测试接入
