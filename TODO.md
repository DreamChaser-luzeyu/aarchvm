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
- 在 SoC 层引入统一 guest 时间基准，并让 timer、脚本注入、stop-on-pattern 等都基于它。
- 区分 guest 时间与 host 时间；SDL 刷新、stdin poll 等宿主机行为不应直接污染 guest 时间。
- 为新旧时间模型保留切换开关，便于回归与定位问题。

预期收益：
- 降低 `console=ttyAMA0` 与 `console=tty1` 对 `AARCHVM_TIMER_SCALE` 的异常敏感性。

### 2. 引入事件调度器

目标：
- 为设备和外部输入提供统一的事件接口，避免每轮全量 `sync_devices()`。

任务：
- 在 SoC 层实现最小事件队列，支持 `(deadline, event_type, target)` 的调度。
- 为设备定义统一接口，例如：
  - `next_deadline(guest_now)`
  - `process_until(guest_now)`
  - `on_external_input(...)`
  - `on_state_change(...)`
- 先让 timer 成为第一个真正基于 deadline 的设备。

预期收益：
- 将“轮询所有设备”改为“只处理已到期或状态变化的设备”。

### 3. 重构 SoC 主循环

目标：
- 把当前固定 chunk 驱动改为“运行到下一个原因”。

任务：
- 主循环改为：
  - 处理已到达的 host 输入事件；
  - 查询最近 guest deadline；
  - 查询最近 host poll deadline；
  - 计算当前 CPU 可安全运行的窗口；
  - 运行 CPU；
  - 处理到期 guest 事件；
  - 在无 runnable CPU 时直接等待事件或跳到最近 deadline。
- 保留旧的步数驱动路径作为 debug / fallback 模式。

预期收益：
- 降低空转和无意义设备同步开销。

### 4. 先把 Generic Timer deadline 化

目标：
- Timer 从“每轮同步一次的被动设备”变成“主动报告下一截止时间的设备”。

任务：
- 让 `GenericTimer` 直接基于 `guest_now` 计算当前 counter。
- 提供“下一个 IRQ 何时到来”的精确接口。
- 让 SoC 不再每轮调用 `sync_to_steps()`，而是在事件边界或寄存器读写时更新 timer 状态。

预期收益：
- Timer 不再成为所有路径都必须经过的高频同步点。

### 5. WFI/WFE 真正停车

目标：
- 等待态 CPU 不再继续参与每轮 `cpu.step()`。

任务：
- 把 CPU 运行态细分为 `runnable / waiting_irq / waiting_event / halted / powered_off`。
- `WFI` CPU 仅在 IRQ 到来时唤醒。
- `WFE` CPU 仅在事件或可中断异常到来时唤醒。
- 对单活跃 CPU 场景启用长突发执行，直到最近事件 deadline。

预期收益：
- 这是 SMP 性能提升最大的低风险步骤之一。

### 6. SMP 调度从“每核一步”升级为“量子 + deadline”

目标：
- 避免当前 SMP 路径的“一核一步、每轮时间只加一次”的低效和失真。

任务：
- 当只有一个 CPU runnable 时，复用单核长 chunk 逻辑。
- 多个 CPU runnable 时，引入可配置小量子，例如每核 32/64/128 条。
- 一个调度轮次结束后，再按该轮的 guest 时间窗口推进时间。
- 给调度器加入必要的公平性约束，避免某核长期饥饿。

预期收益：
- 提高 SMP 性能，并减少 guest 时间模型与真实执行量脱节的问题。

### 7. GIC 改为增量更新而非轮询扫描

目标：
- 减少 `has_pending()` / `acknowledge()` 的线性扫描开销，并让 IRQ 查询更接近事件驱动。

任务：
- 为每 CPU 维护 pending summary / candidate cache。
- 在 `set_level()`、enable、priority、PMR 变化时增量更新状态。
- 让 `has_pending()` 优先查 summary，而不是每次全扫 local + SPI。
- 让 `acknowledge()` 先命中缓存候选，再必要时精查。

预期收益：
- 降低 IRQ 热路径成本，改善 SMP 下大量中断查询的开销。

### 8. UART/KMI 改为状态变化驱动

目标：
- 避免 UART/KMI IRQ 状态被 SoC 每轮轮询。

任务：
- UART RX FIFO 从空变非空、mask 变化、使能变化时，立即更新 IRQ line。
- KMI RX 与控制位变化同理。
- 保持 guest 可见寄存器语义不变，但把 IRQ 推送改成状态变化触发。
- UART TX 的宿主机输出做缓冲/批量 flush，避免每字节 `fflush()`。

预期收益：
- 降低串口路径对整体执行节奏的污染。

### 9. Framebuffer / SDL 彻底与 guest 时间解耦

目标：
- GUI 刷新和键盘轮询不再影响 guest 时间推进。

任务：
- SDL present 只依赖 dirty 状态和 host 刷新节流，不再成为 SoC 高频同步点。
- SDL 键盘输入进入 host 事件队列，再转成 PS/2 注入事件。
- 允许 headless 路径完全绕开 framebuffer host 逻辑。

预期收益：
- 减少 GUI 与非 GUI 路径之间的时序相互干扰。

### 10. main 层事件化

目标：
- 让 `main.cpp` 不再以固定 `run_chunk=200000` 驱动整个系统。

任务：
- 将 UART/PS2 脚本注入改为真正的 guest 时间事件。
- stdin 注入按 host 事件和 FIFO 水位控制，不再靠固定 gap + chunk 对齐。
- 保留 `-steps` 作为最大执行预算，但内部执行逻辑改为事件边界优先。

预期收益：
- 自动化测试、快照构建、交互模式将共享更一致的执行模型。

### 11. 快照支持补强

目标：
- 事件驱动化后，快照仍能完整恢复到同一 guest 状态。

任务：
- 快照中保存：
  - `guest_now`
  - CPU 等待态
  - 设备内部 deadline 所需状态
  - 事件队列内容
- 恢复后重建 host 层非 guest 语义对象，例如 SDL 刷新时钟、stdout flush 状态。

预期收益：
- 事件驱动不会破坏当前已可用的 snapshot 工作流。

### 12. 测试与回归策略

目标：
- 每一阶段都能被单独验证，而不是一次性大改后集中排雷。

任务：
- 为 timer deadline、WFI/WFE 唤醒、SEV、PSCI CPU_ON、IPI/SGI、TLBI 后唤醒等路径补单测。
- 保持单核 Linux、SMP Linux、串口 shell、GUI tty1、snapshot restore 全回归。
- 对 `console=ttyAMA0` 与 `console=tty1` 分别保留回归，以观测事件驱动后时间模型是否收敛。

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
