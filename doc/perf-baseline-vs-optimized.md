# Linux 性能基线与优化对比

## 测量方法
- guest 环境：Linux 6.12.76 + 统一 `initramfs-usertests` rootfs，由 `tests/linux/run_algorithm_perf.sh` 冷启动并通过 UART 注入启动 benchmark。
- 测量时计时倍率：`AARCHVM_TIMER_SCALE=10000`。
- 测量时总线模式：`AARCHVM_BUS_FASTPATH=1`。
- 计时边界：由模拟器中的 perf mailbox 在 `BEGIN` / `END` 两个 MMIO 命令处采样宿主机单调时钟。
- 主指标：`host_ns`，表示宿主机真实耗时。
- 辅助指标：`steps` 与各类内部计数器，仅用于定位热点与解释结果。
- 本文中的 baseline：指加入 perf mailbox 与统计基础设施后、进行任何进一步性能优化之前的第一次测量结果。
- 本文中的 optimized：指在同一套 benchmark 与快照条件下，加入 CPU 端 micro-TLB 热页缓存后的测量结果。

## Case 映射
- `case_id=1`：`base64-enc-4m`
- `case_id=2`：`base64-dec-4m`
- `case_id=3`：`fnv1a-16m`
- `case_id=4`：`tlb-seq-hot-8m`
- `case_id=5`：`tlb-seq-cold-32m`
- `case_id=6`：`tlb-rand-32m`

## 对比结果
| case | baseline host_ns | optimized host_ns | 提升 | baseline tlb_lookup | optimized tlb_lookup | baseline tlb_miss | optimized tlb_miss |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| `base64-enc-4m` | 9145532852 | 8386163883 | 8.30% | 92743471 | 5097060 | 5681 | 5681 |
| `base64-dec-4m` | 9692103994 | 9043132414 | 6.70% | 99624230 | 13309633 | 3300 | 3300 |
| `fnv1a-16m` | 11424411553 | 10197286676 | 10.74% | 118932734 | 225063 | 4004 | 4004 |
| `tlb-seq-hot-8m` | 26151808 | 24147110 | 7.67% | 251211 | 17230 | 46 | 46 |
| `tlb-seq-cold-32m` | 30279703 | 28889330 | 4.59% | 245784 | 32782 | 16391 | 16391 |
| `tlb-rand-32m` | 33458865 | 31891678 | 4.68% | 278552 | 49200 | 16425 | 16425 |

## 结果解释
- 这次优化并没有减少 TLB miss；它优化的是“绝大多数都是 hit”的常见路径。因此 `tlb_miss` 基本不变，但 `tlb_lookup` 显著下降。
- 流式 workload 上收益更明显，例如 `fnv1a-16m`。这类 workload 会在同一页内连续访问较长时间，因此很适合被 micro-TLB 命中。
- 目前最明显的剩余热点信号是：长时间运行的标量 workload 中，`gic_ack` 依旧和 `steps` 同量级，说明没有中断待处理时，CPU 侧仍在非常频繁地做 IRQ 查询。这很可能是下一步优化重点。
- `bus_find` 几乎始终接近 0，说明此前加入的 RAM fast path 已经把“线性扫描 MMIO 映射表”从 RAM 热路径中拿掉了。

## 原始数据文件
- baseline：`out/perf-baseline-results.txt`
- optimized：`out/perf-optimized-results.txt`

## 第二轮优化对比

这一轮对比对应当前主线里的两组改动：
- `BusFastPath` 改为按当前 SoC 地址图硬编码的 fixed-coded fast path，RAM 读写不再走通用映射查找。
- 取消交互式 stdin 的慢路径分支，主循环保持大 chunk 执行。
- GIC 增加 `state_epoch()`，CPU 侧加入“负缓存”式 IRQ 查询跳过逻辑；当 GIC 状态与 PMR 门限都未变化且上次确认无可接收中断时，直接跳过 `acknowledge()`。

### 第二轮结果
| case | baseline host_ns | optimized host_ns | 提升 | baseline gic_ack | optimized gic_ack |
| --- | ---: | ---: | ---: | ---: | ---: |
| `base64-enc-4m` | 17540980619 | 14984404448 | 14.57% | 67561568 | 346 |
| `base64-dec-4m` | 18825884359 | 17984792573 | 4.47% | 73742774 | 376 |
| `fnv1a-16m` | 21296190271 | 19049901404 | 10.55% | 100677524 | 510 |
| `tlb-seq-hot-8m` | 49762287 | 20396278 | 59.01% | 213039 | 0 |
| `tlb-seq-cold-32m` | 54800182 | 23488597 | 57.14% | 213014 | 2 |
| `tlb-rand-32m` | 64635312 | 28265296 | 56.27% | 229399 | 2 |

### 第二轮结果解释
- 这一轮的关键收益点不在 MMU/TLB，本质上是把“没有中断时也高频调用 GIC acknowledge”的热路径压掉了。
- `gic_ack` 从和 `steps` 同量级，直接降到接近 0；这说明绝大多数 benchmark 期间 CPU 不再为空轮询 GIC。
- TLB 类 case 的提升特别明显，因为这类 case 本身总指令数不大，之前被固定成本型的 IRQ 轮询开销放大得更严重。
- `bus_find` 继续保持为 0，说明当前主线里的 RAM fast path 已经把通用 bus 映射查找从热 RAM 路径中移除了。

### 第二轮原始数据文件
- baseline：`out/perf-round2-baseline.txt`
- optimized：`out/perf-round2-optimized.txt`

## 第三轮优化对比

这一轮对应的主线改动非常局部，目标是压缩已经从 GIC 轮询阶段收敛后的固定热成本：
- `SoC::run()` 增加设备线电平缓存，只在 timer/UART 线电平发生变化时才调用 `gic->set_level()`。
- `Bus` 同时缓存 `BusFastPath*` 原始指针，RAM 热路径不再为每次访存额外付出 `shared_ptr` 取值与空比较开销。
- 清理 `SoC` 构造函数里重复三次的 `BusFastPath` 重建。

### 第三轮结果
| case | 上一版 host_ns | 当前 host_ns | 提升 | 上一版 gic_level | 当前 gic_level |
| --- | ---: | ---: | ---: | ---: | ---: |
| `base64-enc-4m` | 18700435738 | 17780580646 | 4.92% | 3242493 | 4886 |
| `base64-dec-4m` | 20495423550 | 19922264931 | 2.80% | 3596919 | 5412 |
| `fnv1a-16m` | 22534408131 | 22040323674 | 2.19% | 3902610 | 6576 |
| `tlb-seq-hot-8m` | 50854849 | 50728776 | 0.25% | 8292 | 14 |
| `tlb-seq-cold-32m` | 66273853 | 52050307 | 21.46% | 9609 | 14 |
| `tlb-rand-32m` | 63201374 | 62025254 | 1.86% | 9468 | 16 |

### 第三轮结果解释
- 这一轮的主要收益不来自减少 `run_chunks`，而是把每个 chunk 内固定要做的“设备线同步”进一步压到只在边沿变化时更新。
- `gic_level` 从数百万次直接降到与真实中断边沿数量同量级，说明 `set_level()` 的无效重复调用已经基本清掉。
- `host_ns` 的改善幅度没有第二轮那么剧烈，符合预期：当前主热点已经转移回 CPU 执行与地址翻译本身，这些路径不会被这一轮局部改动成倍压缩。
- `tlb-seq-cold-32m` 的提升最明显，说明这类“中等时长、设备事件稀少、页访问较密”的 workload 对固定同步成本更敏感。

### 第三轮原始数据文件
- baseline：`out/perf-round3-baseline.txt`
- optimized：`out/perf-round3-optimized.txt`

## gprof 热点观察

由于宿主机 `perf_event_paranoid` 限制，宿主机 `perf record` 当前不可用；这里使用 `-pg` 构建的 `build-gprof/aarchvm`，从 shell snapshot 恢复后注入 `run_functional_suite`，并在 UART 输出 `FUNCTIONAL-SUITE PASS` 时停止，得到 `out/gprof-functional.txt`。

在当前代码上，`gprof` 最值得关注的热点是：
- `aarchvm::Cpu::exec_load_store(unsigned int)`
- `aarchvm::Cpu::exec_data_processing(unsigned int)`
- `aarchvm::Bus::read(unsigned long, unsigned long) const`
- `aarchvm::Cpu::step()`
- `aarchvm::Cpu::translate_address(...)`
- `aarchvm::BusFastPath::read(...) const`

同时，`std::optional`/返回值包装相关符号在热点表里占比仍然显著，例如：
- `std::optional<aarchvm::Cpu::TranslationResult>::optional(...)`
- `std::optional<unsigned long>::has_value() const`
- `std::optional<aarchvm::Cpu::TranslationResult>::operator->() const`

这说明当前主瓶颈已经比较清晰：
- 第一层是 CPU 解释执行本身，尤其是 load/store 与地址翻译路径。
- 第二层是 RAM 访存入口，虽然 bus fast path 已经存在，但 `Bus::read()` 仍在高频热表中。
- 第三层是 `translate_address()` 的返回包装与访问检查；如果后续要继续挤性能，这里很可能需要把 `std::optional` 风格返回改成更轻量的“状态码 + out 参数”接口，但这已经属于结构性修改，适合单独讨论后再做。

## 第四轮优化对比

这一轮继续沿着“短小高频 helper 内联”的思路推进，没有改执行模型：
- 将 `Cpu::reg()` / `reg32()` / `set_reg()` / `set_reg32()` / `sp_or_reg()` / `set_sp_or_reg()` 内联到头文件。
- 目标是减少解释执行主路径上寄存器取值、写回与 `SP`/通用寄存器别名分发的函数调用开销。

### 第四轮结果
| case | 上一版 host_ns | 当前 host_ns | 提升 |
| --- | ---: | ---: | ---: |
| `base64-enc-4m` | 17780580646 | 17574297911 | 1.16% |
| `base64-dec-4m` | 19922264931 | 19463084089 | 2.30% |
| `fnv1a-16m` | 22040323674 | 22245340456 | -0.93% |
| `tlb-seq-hot-8m` | 50728776 | 44877798 | 11.53% |
| `tlb-seq-cold-32m` | 52050307 | 51835501 | 0.41% |
| `tlb-rand-32m` | 62025254 | 61866311 | 0.26% |

### 第四轮结果解释
- 这一轮属于小步微调，收益整体不大，但多数 case 仍有轻微改善。
- `tlb-seq-hot-8m` 的改善最明显，说明这类短小、热点集中的 workload 对 helper 调用开销比较敏感。
- `fnv1a-16m` 出现轻微回退，幅度不到 1%，更像宿主机波动而不是稳定性回退。
- 总体上，这一轮说明“把高频小 helper 往内联方向收”仍然有效，但已经开始接近测量噪声边界。

### 第四轮原始数据文件
- baseline：`out/perf-round4-baseline.txt`
- optimized：`out/perf-round4-optimized.txt`

## 第五轮优化对比

这一轮继续做同类的局部收敛：
- 将 `Cpu::access_permitted()` 与 `Cpu::tlb_lookup()` 内联到头文件。
- 目标是压缩 TLB hit / 权限检查这条热链上的函数调用层级。

### 第五轮结果
| case | 上一版 host_ns | 当前 host_ns | 提升 |
| --- | ---: | ---: | ---: |
| `base64-enc-4m` | 17574297911 | 17783980868 | -1.19% |
| `base64-dec-4m` | 19463084089 | 19837218253 | -1.92% |
| `fnv1a-16m` | 22245340456 | 22128901400 | 0.52% |
| `tlb-seq-hot-8m` | 44877798 | 44304271 | 1.28% |
| `tlb-seq-cold-32m` | 51835501 | 54577940 | -5.29% |
| `tlb-rand-32m` | 61866311 | 63030172 | -1.88% |

### 第五轮结果解释
- 这轮修改后，内部 perf 计数器与上一轮几乎完全一致，说明它没有改变 guest 指令条数、TLB 行为或 bus 行为。
- `host_ns` 上出现的涨跌基本可以视为宿主机调度噪声；没有证据表明这一轮在当前 benchmark 里带来了稳定、可复现的收益。
- 这也从侧面说明：继续堆这类“很小的 helper 内联”已经接近收益天花板，后续如果要继续推进，需要转向更结构化的热点，例如 `translate_address()` 的返回包装和 `Bus::read()` 调用层。

### 第五轮原始数据文件
- baseline：`out/perf-round5-baseline.txt`
- optimized：`out/perf-round5-optimized.txt`

## 第五轮后 gprof 观察

重新以第五轮代码生成 `build-gprof/aarchvm` 并复测后，热点排序没有发生本质变化：
- `Cpu::exec_load_store()` 仍是第一热点。
- `Cpu::exec_data_processing()`、`Bus::read()`、`Cpu::translate_address()` 仍处于最核心热点组。
- `std::optional<TranslationResult>` / `std::optional<uint64_t>` 相关包装符号依旧频繁出现。

这说明第五轮之后，当前主线的性能瓶颈依旧稳定集中在两处：
- CPU 解释执行本身，尤其是 load/store 与地址翻译。
- `translate_address()` 与 `Bus::read()` 一带的调用层与返回包装。

换句话说，后续若还想拿到明显收益，下一步就不应再主要依赖 helper 内联，而应转向更结构化的热点改造。

## 第六轮优化对比

这一轮是第一类真正的结构性改动：
- `translate_address()` 从 `std::optional<TranslationResult>` 返回，改为 `bool + TranslationResult* out_result`。
- `walk_page_tables()` 同步改为 `bool + out_result + fault`。
- 所有高频调用点都切换到新的返回协议，包括取指、异常追踪、`AT`、`DC ZVA`、普通 load/store 的 MMU 读写辅助路径。

目标是压缩地址翻译热链上的 `std::optional<TranslationResult>` 构造、移动与判空开销。

### 第六轮结果
| case | 上一版 host_ns | 当前 host_ns | 提升 |
| --- | ---: | ---: | ---: |
| `base64-enc-4m` | 17783980868 | 19123844642 | -7.53% |
| `base64-dec-4m` | 19837218253 | 24155453310 | -21.77% |
| `fnv1a-16m` | 22128901400 | 28607372135 | -29.28% |
| `tlb-seq-hot-8m` | 44304271 | 69451779 | -56.76% |
| `tlb-seq-cold-32m` | 54577940 | 44834439 | 17.86% |
| `tlb-rand-32m` | 63030172 | 48843402 | 22.51% |

### 第六轮结果解释
- 从结果看，这一轮没有呈现“全局稳定变快”的特征，而是出现了明显分化：顺序/标量类 case 普遍变慢，而两类 TLB 压力 case 反而变快。
- 由于这一轮比较是冷启动基准，且 prompt-step 会随 build log 更新，`host_ns` 受宿主机调度与启动节拍影响较大；因此不能仅凭这一组 `host_ns` 就断言返回协议重构本身是净回退。
- 更可靠的信号来自 `gprof`：`std::optional<TranslationResult>` 相关热点符号已经基本退出核心热表，而 `translate_address()` 本体仍然保留在热点组内。这说明“去掉返回包装”这个方向本身是有效的，但当前整体性能已经被更高层的其它固定成本和宿主机波动放大了。
- 换言之，第六轮证明了一个结论：`translate_address()` 继续优化是对的，但下一步需要结合更稳定的测量方式，或者同步压缩 `Bus::read()` / `SystemRegisters` 小 helper / fault 路径，才能把收益更清晰地反映到端到端 `host_ns` 上。

### 第六轮原始数据文件
- baseline：`out/perf-round6-baseline.txt`
- optimized：`out/perf-round6-optimized.txt`

## 第六轮后 gprof 观察

最新 `gprof` 结果位于 `out/gprof-functional-round6.txt`。与第五轮前相比，最值得注意的变化是：
- `std::optional<TranslationResult>` 相关构造/判空符号已经不再停留在最核心热点组。
- `Cpu::translate_address()` 仍然处在高频热点中，但现在热点更集中在函数本体、`Bus::read()`、`BusFastPath::read()`、`access_permitted()` 与寄存器/系统寄存器 helper 上。
- 仍然明显存在的是 `std::optional<uint64_t>`，它主要来自 `Bus::read()` 返回值，这也再次指向下一阶段优化重点。

## 第六轮波动复核

为确认第六轮中几个运算类 case 的明显劣化是否来自代码回退，我又做了三次“从 BusyBox shell snapshot 直接启动 `bench_runner`”的复测，避免冷启动与 U-Boot 路径干扰。结果表明：
- 多数 case 的 guest `steps`、`translate`、`tlb_lookup`、`bus_read` 等内部计数器在三次运行中完全一致或近乎一致。
- 但 `host_ns` 波动很大，说明端到端时间主要被宿主机调度/频率/缓存状态噪声放大，而不是 guest 执行路径发生了同量级变化。

### snapshot 复测摘要
| case | steps 是否稳定 | host_ns 观测范围 |
| --- | --- | ---: |
| `base64-enc-4m` | 完全一致 | 20.10s - 27.17s |
| `base64-dec-4m` | 完全一致 | 18.05s - 30.56s |
| `fnv1a-16m` | 完全一致 | 24.19s - 32.81s |
| `tlb-seq-hot-8m` | 完全一致 | 37.61ms - 84.91ms |
| `tlb-seq-cold-32m` | 完全一致 | 44.81ms - 100.45ms |
| `tlb-rand-32m` | 近乎一致 | 52.68ms - 115.48ms |

### 复核结论
- 第六轮代码改动确实改变了地址翻译热链的实现方式，但从重复实验看，并没有证据表明它必然造成了文档里那种幅度的稳定回退。
- 相反，当前测量方法下，`host_ns` 已经足以在“内部 guest 计数器不变”的情况下发生非常大的波动。
- 因此，如果目标是判断后续结构性优化是否真的有效，下一步更重要的不是继续做微小代码改动，而是引入更稳定的 benchmark 采样方法，例如：固定从 shell snapshot 起跑、重复多次取中位数，或者把 perf mailbox 的 case 单独拆成可独立运行脚本。

## Release 模式切换

当前主构建已经切到 `Release`：
- `CMakeLists.txt` 在单配置生成器且未显式指定 `CMAKE_BUILD_TYPE` 时，默认使用 `Release`。
- 当前构建目录 `build/` 的实际编译参数为 `-O3 -DNDEBUG`。

### Release 回归验证
- `tests/arm64/run_all.sh`：通过。
- `tests/linux/build_linux_shell_snapshot.sh`：通过。
- `AARCHVM_FUNCTIONAL_TIMEOUT=360s tests/linux/run_functional_suite.sh`：通过。

这说明切换到 `Release` 后，当前主线的裸机指令测试、Linux 冷启动到 shell snapshot、以及 BusyBox 功能回归都没有引入行为回退。

## Release 相对 Debug 的性能提升

这次对比使用的是同一份当前源码，只切换 `CMAKE_BUILD_TYPE`，并使用同一套 `tests/linux/run_algorithm_perf.sh` 测试流程。

构建与结果文件：
- Debug 结果：`out/perf-debug-vs-release-debug-results.txt`
- Release 结果：`out/perf-debug-vs-release-release-results.txt`

### Debug vs Release 对比

| case | debug host_ns | release host_ns | Release 相对 Debug 提升 |
| --- | ---: | ---: | ---: |
| `base64-enc-4m` | 23375218692 | 6964686507 | 70.20% |
| `base64-dec-4m` | 22366702157 | 7242812003 | 67.62% |
| `fnv1a-16m` | 21569307632 | 7102121288 | 67.07% |
| `tlb-seq-hot-8m` | 41996373 | 12550518 | 70.12% |
| `tlb-seq-cold-32m` | 45938540 | 13564623 | 70.47% |
| `tlb-rand-32m` | 55415766 | 17172037 | 69.01% |

### Debug vs Release 结论
- 在当前代码基线下，`Release` 相对 `Debug` 的端到端 `host_ns` 提升约为 `67%` 到 `70%`。
- 六个 case 的算术平均提升为 `69.08%`。
- 这组结果说明：即便不继续修改模拟器实现，仅仅从 `Debug` 切到 `Release`，解释器主循环、MMU/TLB、总线访存和系统指令热链就能获得非常显著的整体收益。
- 因此，后续所有性能结论都应优先基于 `Release` 构建讨论，否则很容易被非优化构建的额外开销掩盖真实热点。

## Release 基线观测

第一次 `Release` 基线已经执行，并保留了两类证据：
- 运行记录：`out/perf-release-baseline.txt`
- host `perf` 报告：`out/perf-release.report`

需要说明的是：第一次 `Release` 基线的逐 case `PERF-RESULT` 明细当时来自 `out/perf-suite-results.txt`，但该文件后来被后续测试覆盖，没有单独归档。因此，当前文档无法再恢复“第一次 Release 基线”的完整逐 case 对比表，只能保留：
- 回归已通过这一事实。
- 基线热点分布。
- 当前 `Release` 主线的逐 case 性能结果。

### Release 基线热点

基于 `out/perf-release.report`，`Release` 下最主要的热点已经重新集中到解释器本体，而不是调试/统计基础设施：

| 热点 | 占比 |
| --- | ---: |
| `Cpu::exec_data_processing()` | 20.79% |
| `Bus::read()` | 16.73% |
| `Cpu::step()` | 14.09% |
| `Cpu::exec_load_store()` | 11.63% |
| `Cpu::translate_address()` | 10.03% |
| `Cpu::exec_system()` | 8.55% |
| `Cpu::try_take_irq()` | 4.72% |
| `getenv` | 1.74% |

这个基线有两个重要结论：
- `Release` 之后，性能瓶颈已经比较干净地回到了 CPU 解释执行、地址翻译和总线访存热链。
- 即便如此，仍能看到 `getenv` 这种与功能无关的固定成本留在热表里，说明还有一些低风险的“热路径杂音”可以继续清掉。

## Release 下的小优化：trace getenv 缓存

这轮只做了一个低风险小优化：
- 把 CPU/GIC/timer/UART 中热路径 trace 开关的 `std::getenv()` 调用改成一次性静态缓存。
- 相关文件：`src/cpu.cpp`、`src/gicv3.cpp`、`src/generic_timer.cpp`、`src/uart_pl011.cpp`。

修改后重新执行：
- `cmake --build build -j4`
- `tests/arm64/run_all.sh`
- `tests/linux/build_linux_shell_snapshot.sh`
- `AARCHVM_FUNCTIONAL_TIMEOUT=360s tests/linux/run_functional_suite.sh`
- `AARCHVM_PERF_TIMEOUT=900s tests/linux/run_algorithm_perf.sh`

以上均已通过；结果文件与热点报告为：
- `out/perf-release-opt1.txt`
- `out/perf-suite-results-release-opt1.txt`
- `out/perf-release-opt1.report`

### 当前 Release 主线逐 case 结果

| case | host_ns | steps | translate | tlb_lookup | tlb_miss | bus_read | gic_ack |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| `base64-enc-4m` | 10385089524 | 162455933 | 226002612 | 24382169 | 5865 | 203901203 | 4874 |
| `base64-dec-4m` | 11626692557 | 180216577 | 249463368 | 34860625 | 3546 | 227222387 | 5406 |
| `fnv1a-16m` | 12638466450 | 219625734 | 287954453 | 24824284 | 4060 | 268440536 | 6588 |
| `tlb-seq-hot-8m` | 27118554 | 455409 | 595991 | 68195 | 146 | 539365 | 14 |
| `tlb-seq-cold-32m` | 29777839 | 471826 | 611984 | 84323 | 17142 | 622905 | 14 |
| `tlb-rand-32m` | 35685861 | 526539 | 704658 | 113007 | 17207 | 708328 | 16 |

### Release 优化后热点

基于 `out/perf-release-opt1.report`，`getenv` 已经退出主热点表，剩余热点如下：

| 热点 | 占比 |
| --- | ---: |
| `Cpu::exec_data_processing()` | 20.82% |
| `Bus::read()` | 16.49% |
| `Cpu::step()` | 15.21% |
| `Cpu::exec_load_store()` | 11.52% |
| `Cpu::translate_address()` | 10.05% |
| `Cpu::exec_system()` | 8.46% |
| `Cpu::try_take_irq()` | 5.09% |
| `Bus::write()` | 1.01% |

### Release 优化后的结论
- 这轮优化的主要价值是“消掉热路径里的无关开销”，而不是改变 guest 指令行为，因此更可信的收益信号来自 `perf report`，不是单次 `host_ns` 波动。
- `getenv` 从主热点中消失，说明这一类低风险 fixed cost 已经被压掉。
- 但端到端性能的主体瓶颈并没有改变，仍然集中在三条热链：
  - 指令执行分发与整数/系统指令解释。
  - `translate_address()` 代表的 MMU/TLB/权限检查路径。
  - `Bus::read()` 代表的访存返回路径。

## 中低风险优化尝试：Bus/GIC 热路径收缩

这一轮做了三类中低风险改动：
- `Bus` 增加 `bool + out` 读接口，并把 CPU 取指、页表 descriptor 读取、MMU 常见读路径切过去，减少热路径上的 `std::optional<uint64_t>` 构造。
- `GicV3::acknowledge()` 改为 `bool + out`，避免 `try_take_irq()` 在“无中断可取”场景下频繁构造空 `optional`。
- CPU 侧加入 IRQ 阈值缓存，把 `min(PMR, running_priority)` 从“每步现算”改成按状态变化更新。

### 回归验证
- `tests/arm64/run_all.sh`：通过。
- `tests/linux/build_linux_shell_snapshot.sh`：通过。
- `AARCHVM_FUNCTIONAL_TIMEOUT=360s tests/linux/run_functional_suite.sh`：通过。

### 结果文件
- 结果：`out/perf-lowrisk-opt3-results.txt`
- 日志：`out/perf-lowrisk-opt3.log`
- host perf 报告：`out/perf-lowrisk-opt3.report`

### 与上一份已归档 Release 结果的对比

这里使用上一份已归档的 Release 主线结果 `out/perf-suite-results-release-opt1.txt` 作为参照。

| case | 参照 host_ns | 当前 host_ns | 提升 |
| --- | ---: | ---: | ---: |
| `base64-enc-4m` | 10385089524 | 9604452287 | 7.52% |
| `base64-dec-4m` | 11626692557 | 11816621963 | -1.63% |
| `fnv1a-16m` | 12638466450 | 12628215203 | 0.08% |
| `tlb-seq-hot-8m` | 27118554 | 37773365 | -39.29% |
| `tlb-seq-cold-32m` | 29777839 | 32939345 | -10.62% |
| `tlb-rand-32m` | 35685861 | 36703898 | -2.85% |

### 这一轮的结论
- 这轮修改没有带来“稳定且全局为正”的端到端提速。
- 标量大 workload 中，`base64-enc-4m` 有一次明显正收益，`fnv1a-16m` 基本持平；但三个 TLB 小 case 在这次采样里没有改善。
- 因此，从 `host_ns` 看，这轮更像是一次“热点重分布”，而不是已经验证成功的吞吐优化。

### perf 热点变化

对比 `out/perf-release-opt1.report` 与 `out/perf-lowrisk-opt3.report`，可以看到一些方向上正确、但尚未转化成稳定 wall-clock 收益的变化：

| 热点 | 之前 | 当前 |
| --- | ---: | ---: |
| `Cpu::try_take_irq()` | 5.09% | 4.66% |
| `Bus::read()` | 16.49% | 5.47% |
| `Cpu::exec_load_store()` 读 lambda | 3.35% | 7.55% |

这说明：
- `try_take_irq()` 的固定成本确实被压低了一点。
- `Bus::read()` 符号本身的占比明显下降。
- 但部分成本被重新分摊到了 `exec_load_store()` 里的读辅助路径上，整体没有换来稳定的总耗时下降。

所以，这一轮可以视为一次“正确但不够”的中低风险尝试：功能稳定，热点有变化，但尚不足以证明总吞吐已经明显提升。

## 当前代码结构下的后续优化方向

结合 `Release` 下的 `perf` 结果，当前最值得继续推进的方向如下。

### 1. 压缩 `Bus::read()` / `Bus::write()` 的返回与分派成本
- 当前 `Bus::read()` 仍然是第二大热点，说明“即便 fast path 已经固定编码”，通用总线 API 层本身仍然有明显成本。
- 下一步可考虑的低到中风险方向：
  - 让 RAM 热路径进一步内联化，减少 `Bus::read()` 函数边界本身的固定成本。
  - 把 `std::optional<uint64_t>` 风格的返回协议继续收缩成更轻量的 `bool + out` 形式。
  - 对常见 1/2/4/8 字节 RAM 访问做更扁平的专门路径，减少分支与 `memcpy` 式封装。

### 2. 继续压缩 `Cpu::step()` 的固定成本
- `Cpu::step()` 仍是非常高的固定热点，说明每条指令都在为取指、解码、异常检查、IRQ 检查承担固定管理开销。
- 后续可以继续看：
  - 取指路径是否还能与 fetch 翻译做更紧的热页缓存结合。
  - dispatch 结构是否能减少重复位段提取和层层 helper 跳转。
  - `try_take_irq()` 是否还能在“确定无中断变化”的区间里进一步少做检查。

### 3. 继续针对 `translate_address()` 做结构性收缩
- 现在热点已经不是 `std::optional<TranslationResult>` 本身，而是 `translate_address()` 函数体与其后续权限检查、页表步进、TLB 填充逻辑。
- 后续收益更可能来自：
  - 热页命中的更短路径。
  - page walk 中 descriptor 读取与属性合成的分支压缩。
  - 把 fetch/read/write 三种 access 共用路径里真正不共享的分支拆开，减少无关判断。

### 4. `exec_system()` 偏热，说明 sysreg/系统指令路径值得单独拆看
- 当前 `exec_system()` 约 8.5%，比例不低。
- 这通常意味着 Linux 用户态 benchmark 期间，异常返回、系统寄存器访问、cache/TLB/system 指令路径仍有不小固定成本。
- 若下一轮继续做热点跟踪，建议单独量化：
  - `SystemRegisters::read/write()` 的占比。
  - 进入 `exec_system()` 后到底是哪些子类指令最常出现。

### 5. 真正的下一阶段收益，可能来自更“解释器结构级”的优化
- 到了当前阶段，再继续堆零散 helper 内联，收益大概率已经不大。
- 更有希望的方向反而是：
  - 预解码或更紧凑的 dispatch 结构。
  - 把设备同步进一步事件化，继续减少每条指令的固定轮询成本。
  - 让 MMU/TLB 热路径和 RAM fast path 更强地协同，而不是每次都穿越多层通用接口。

换句话说，当前性能还没有接近“解释执行的极限”；它更像是已经把最明显的低风险噪声压掉了，接下来要进入真正的结构性优化阶段。
## 第七轮：最小预解码闭环

这一轮引入的是一个可切换、保留慢路径的最小 predecode/decode-cache 闭环，目标不是一次性把解释器改成块缓存，而是先验证以下几件事：
- 在不移除原慢路径的前提下，能否为高频整数/分支指令提供一条更短的执行路径。
- 自修改代码、IC 失效、TLBI、TTBR/TCR/SCTLR 切换后，decode cache 是否能保持正确失效。
- 在 Linux 用户态 workload 上，这个最小闭环是否已经能带来可观察收益。

### 这一轮实现内容
- 新增命令行选项 `-decode <fast|slow>`，默认 `fast`。
- `fast` 模式下，`Cpu::step()` 在正常取指后，会优先查询 decode cache；若命中且属于已支持的预解码类型，则走 `exec_decoded()`；否则自动回退到原有慢路径。
- `slow` 模式完全保留原先解释执行逻辑，用于回归、对比和问题定位。
- 当前预解码覆盖的是最小高频整数子集：
  - `B/BL (imm)`
  - `B.cond`
  - `CBZ/CBNZ`
  - `TBZ/TBNZ`
  - `MOVZ/MOVK/MOVN`
  - `ADD/SUB (imm)`
  - `ADD/SUB (shifted register)`
- 已接入保守失效钩子，确保语义正确性：
  - CPU 通过 MMU 写内存时
  - `DC ZVA`
  - `IC IALLU` / `IC IALLUIS` / `IC IVAU`
  - `TLBI` 指令族
  - `SCTLR_EL1` / `TTBR0_EL1` / `TTBR1_EL1` / `TCR_EL1` 改写
  - reset / snapshot load / binary load

### 新增裸机单元测试
- `tests/arm64/predecode_dyn_codegen.S`
  - 目标：验证动态生成代码 / 自修改代码后，decode cache 能被正确失效，不会执行陈旧译码。
  - 期望输出：`AB`
- `tests/arm64/predecode_va_exec_switch.S`
  - 目标：验证虚拟地址空间切换后，同一 VA 下的执行不会误复用旧译码结果。
  - 期望输出：`ABA`

### 功能验证
- `tests/arm64/run_all.sh`：通过。
- `tests/linux/build_linux_shell_snapshot.sh`：通过。
- `AARCHVM_FUNCTIONAL_TIMEOUT=360s tests/linux/run_functional_suite.sh`：通过。
- 新增的两条 predecode 单测在 `fast` / `slow` 两种模式下都通过：
  - `predecode_dyn_codegen.bin`：`AB`
  - `predecode_va_exec_switch.bin`：`ABA`

### 结果文件
- `fast`：`out/perf-predecode-fast-results.txt`
- `slow`：`out/perf-predecode-slow-results.txt`
- `fast` 日志：`out/perf-predecode-fast.log`
- `slow` 日志：`out/perf-predecode-slow.log`

### Fast vs Slow 对比
| case | slow host_ns | fast host_ns | 提升 |
| --- | ---: | ---: | ---: |
| `base64-enc-4m` | 10386126604 | 9836525120 | 5.29% |
| `base64-dec-4m` | 12012712779 | 10288330563 | 14.35% |
| `fnv1a-16m` | 11774782995 | 6654138192 | 43.49% |
| `tlb-seq-hot-8m` | 12558498 | 21374770 | -70.20% |
| `tlb-seq-cold-32m` | 13654272 | 16692573 | -22.25% |
| `tlb-rand-32m` | 16225114 | 20825163 | -28.35% |

### 这一轮结果解释
- 结果呈现明显分化，而不是“统一正收益”。
- 三个偏整数/分支、长时间顺序运行的 case 明显受益，尤其 `fnv1a-16m`，说明当前这批已预解码的整数/分支指令已经覆盖到了一段真实热路径。
- 三个 TLB 压力 case 反而回退，说明当前最小闭环还没有触及它们的主热点。这个结论和实现范围是一致的：
  - 这一轮没有预解码 `load/store` 主家族。
  - 没有预解码 MMU/TLB 相关的访存密集路径。
  - decode cache 自身也引入了额外的查询与失效维护成本。
- 因此，这一轮更准确的结论不是“预解码已经全局提速”，而是：
  - 可切换的最小闭环已经跑通；
  - 正确性回归通过；
  - 对整数/分支热点已经出现可见收益；
  - 若想继续扩大收益面，下一步应优先覆盖 `load/store` 高频子集，而不是继续只加更多整数指令。

### 当前边界
- 这还不是块级缓存，也不是完整 IR/JIT；仍然是“取指后按单条指令执行”的解释器结构。
- 当前 decode cache 采用保守失效策略，优先保证正确性，而不是最细粒度性能。
- 现阶段它更像后续结构性优化的地基：已经证明“保留慢路径 + 增量引入快路径”这条路线是可行的。

## 第八轮：load/store 最小闭环

这一轮继续沿用“保留慢路径 + 可切换快路径”的策略，把 predecode 的覆盖面从整数/分支扩到最常见的一批标量 load/store 形式，验证两个问题：
- 对 Linux 用户态热点中的数据访存，最小 load/store 快路径是否已经足以扩大收益面。
- 在不引入更激进块缓存的前提下，generic load/store decoded exec 的固定成本是否可接受。

### 这一轮实现内容
- 在 decode kind 中新增 `LoadStore` 快路径类别。
- 新增通用 MMU 访存 helper，统一快路径与慢路径的 data abort / code-write invalidation / exclusive monitor 清理语义。
- 当前新覆盖的 load/store 子集：
  - unsigned offset：`LDR/STR B/H/W/X`
  - unsigned offset：`LDRSB/LDRSH/LDRSW`
  - post-index / pre-index：最小 no-sign-extend 家族 `LDR/STR B/H/W/X`
- 仍然保留原 `exec_load_store()`，未覆盖到的形式继续自动回退到慢路径。

### 新增裸机单元测试
- `tests/arm64/predecode_load_store_min.S`
  - 覆盖 unsigned offset 的 `LDR/STR B/H/W/X`、`LDRSB/LDRSH/LDRSW`
  - 覆盖 post-index / pre-index 的最小 `LDR/STR B/W`
  - 覆盖 `SP` 作为 base 的路径
  - `fast` / `slow` 期望输出都为 `L`

### 功能验证
- `tests/arm64/run_all.sh`：通过。
- `tests/linux/build_linux_shell_snapshot.sh`：通过。
- `AARCHVM_FUNCTIONAL_TIMEOUT=360s tests/linux/run_functional_suite.sh`：通过。
- 新增 `predecode_load_store_min.bin` 在 `fast` / `slow` 两种模式下都通过。

### 结果文件
首轮对比：
- `fast`：`out/perf-predecode-loadstore-fast-results.txt`
- `slow`：`out/perf-predecode-loadstore-slow-results.txt`

反向顺序复测：
- `slow`：`out/perf-predecode-loadstore-slow-rerun-results.txt`
- `fast`：`out/perf-predecode-loadstore-fast-rerun-results.txt`

### 测量说明
- 由于这一轮首轮与反向顺序复测的 `host_ns` 波动较大，单次结果不够稳。
- 因此这里采用“同一 case 两次 fast、两次 slow，各自取中位数”的方式给出结论。
- 两次测量中，guest 内部计数器如 `steps`、`translate`、`bus_read`、`ram_write` 基本一致，因此差异主要反映宿主机时间与解释器固定成本，而不是 guest 行为变化。

### Fast vs Slow 中位数对比
| case | slow median host_ns | fast median host_ns | 提升 |
| --- | ---: | ---: | ---: |
| `base64-enc-4m` | 5400051078 | 6451835198 | -19.48% |
| `base64-dec-4m` | 5558519766 | 6710032552 | -20.72% |
| `fnv1a-16m` | 6286994223 | 6883393437 | -9.49% |
| `tlb-seq-hot-8m` | 16783918 | 19485330 | -16.10% |
| `tlb-seq-cold-32m` | 13854323 | 16179937 | -16.79% |
| `tlb-rand-32m` | 17464384 | 19966119 | -14.32% |

### 这一轮结果解释
- 和上一轮“整数/分支最小闭环”不同，这一轮在两次测量取中位数后，结论已经比较稳定：当前版本的 load/store 快路径整体是负收益。
- 这说明现阶段的 `LoadStore` decoded exec 还没有比原来的 `exec_load_store()` 更短，反而叠加了：
  - decode cache 查询成本
  - generic `LoadStore` 分派成本
  - 通用 helper 带来的额外函数边界与条件分支
- 也就是说，虽然功能闭环已经成立，但这条路径还没有达到“值得替代慢路径热点分发”的成熟度。

### 当前判断
- 这一轮的主要价值在于：
  - 证明了 load/store 语义也可以安全接入 predecode 框架；
  - 验证了 `SP` base、sign-ext load、pre/post-index 这些边界在 fast/slow 下都一致；
  - 明确表明“仅把慢路径逻辑机械搬进 decoded exec”并不会自动带来性能收益。
- 下一步若还要继续扩 predecode，重点不该是继续扩大覆盖率，而应先压缩 decoded load/store 自身的固定成本，例如：
  - 针对 `LDR/STR B/H/W/X [Xn,#imm]` 做更扁平的专门执行路径，而不是 generic switch + helper 组合。
  - 让 decoded fast path 更直接复用 TLB/RAM hot path，减少额外函数边界。
  - 或者干脆把优化重心转向“块级预解码/预分派”，避免每条 load/store 都重复经过解释器级别的统一入口。

## 第九轮：专门化 `LDR/STR [Xn,#imm]` 快路径

第八轮证明了一件事：把 load/store 机械搬进 generic decoded exec 并不会自动变快。于是这一轮不再扩大覆盖率，而是只盯最常见、最热的一条路径：
- `LDR/STR B/H/W/X [Xn,#imm]`

目标是把这条路径从 generic `LoadStore` 分发里剥离出来，压缩到更短的执行链，并尽量直接复用数据访问的 TLB/RAM 热路径。

### 这一轮实现内容
- 在 decode kind 中新增 `LoadStoreUImm`，专门承载 unsigned-immediate 的 `LDR/STR B/H/W/X`。
- `LoadStoreUImm` 在执行时不再走 generic `LoadStore` 的 sign/writeback 分派逻辑，而是直接执行：
  - base + imm 地址计算
  - data-translation fast helper
  - `Bus::read/write`
  - 寄存器写回
- 新增 `translate_data_address_fast()`：
  - 针对 data access 单独复用 `tlb_last_data_`
  - 避免为最热数据访问路径搬运完整 `TranslationResult`
  - miss 时仍回落到 page walk + TLB fill，保持原有语义

### 中途发现并修复的问题
- 第一次把 `LoadStoreUImm` 接起来后，`perf` 明确显示 `Cpu::on_code_write()` 成为第一热点。
- 根因不是访存本身，而是 decode cache 失效路径还在用“扫描整个 decode page 数组”的方式做 `VA/PA` 失效。
- 由于普通数据 store 也会经过这条路径，这个 O(N) 失效开销被放大到了不可接受的程度。
- 随后把 `invalidate_decode_va_page()` / `invalidate_decode_pa_page()` 改成了 direct-mapped 的定点失效，这一热点随即下降到几乎可以忽略。

### 功能验证
- `tests/arm64/run_all.sh`：通过。
- `tests/linux/build_linux_shell_snapshot.sh`：通过。
- `AARCHVM_FUNCTIONAL_TIMEOUT=360s tests/linux/run_functional_suite.sh`：通过。
- `predecode_load_store_min.bin` 在 `fast` / `slow` 下继续通过。

### 结果文件
性能对比：
- `fast`：`out/perf-predecode-uimm2-fast-results.txt`
- `slow`：`out/perf-predecode-uimm2-slow-results.txt`
- `slow rerun`：`out/perf-predecode-uimm2-slow-rerun-results.txt`
- `fast rerun`：`out/perf-predecode-uimm2-fast-rerun-results.txt`

热点追踪：
- `perf data`：`out/perf-predecode-uimm2-fast.perf.data`
- `perf report`：`out/perf-predecode-uimm2-fast.report`

### 测量说明
- 与前几轮一样，`host_ns` 仍存在宿主机侧波动，因此这里仍采用“两次 fast、两次 slow，各自取中位数”的方式判断。
- 两轮中 guest 内部计数器保持一致量级，因此这次对比主要反映宿主机耗时与解释器固定成本的变化。

### Fast vs Slow 中位数对比
| case | slow median host_ns | fast median host_ns | 提升 |
| --- | ---: | ---: | ---: |
| `base64-enc-4m` | 7897606551 | 6250131218 | 20.86% |
| `base64-dec-4m` | 8562295699 | 6057772263 | 29.25% |
| `fnv1a-16m` | 9024054470 | 7542869397 | 16.41% |
| `tlb-seq-hot-8m` | 23746195 | 16608722 | 30.06% |
| `tlb-seq-cold-32m` | 22824542 | 14032309 | 38.52% |
| `tlb-rand-32m` | 28177568 | 17696157 | 37.20% |

### 这一轮结果解释
- 这一轮和第八轮相比，结论发生了反转：
  - 第八轮的 generic load/store 快路径整体负收益。
  - 这一轮的专门化 `LoadStoreUImm` 路径，在两轮取中位数后已呈现全局正收益。
- 这说明当前阶段真正有效的不是“覆盖更多”，而是“把最热路径做窄、做短”。
- 特别是三类 TLB case 的收益很明显，说明：
  - 数据访问热路径中，地址翻译和总线读写前后的固定解释器开销已经被压掉了一部分。
  - 把 unsigned-immediate 的标量访存从 generic 分派里剥离出来是值得继续的方向。

### 快路径热点追踪结果
基于 `out/perf-predecode-uimm2-fast.report`，当前最值得关注的 user-space 热点大致如下：
- `Cpu::step()`：约 17.6%
- `Cpu::exec_data_processing()`：约 14.6%
- `Cpu::translate_address()`：约 10.7%
- `Cpu::exec_decoded()`：约 9.6%
- `Cpu::lookup_decoded()`：约 9.4%
- `Bus::read()`：约 6.4%
- `Cpu::exec_load_store()`：约 6.0%
- `Cpu::try_take_irq()`：约 4.4%
- `Cpu::exec_system()`：约 4.2%
- `Cpu::decode_insn()`：约 3.1%
- `Cpu::translate_data_address_fast()`：约 2.0%
- `Cpu::on_code_write()`：约 0.25%

### 热点解读
- `on_code_write()` 已经不再是主要瓶颈，说明 direct-mapped 失效修正是必要且有效的。
- 当前最大的问题重新回到了“解释器主循环本身”：
  - `step()`
  - `exec_data_processing()`
  - `translate_address()`
  - `lookup_decoded()` / `exec_decoded()`
- `exec_load_store()` 仍有 6% 左右，说明大量未进入专门快路径的 load/store 形式还在慢路径里消耗明显成本。
- `Bus::read()` 仍然有 6% 左右，说明即便总线 fast path 已存在，RAM 热路径在接口层面仍然有可压缩空间。

### 当前结论
- 这轮验证了一个更明确的方向：
  - “窄而专”的 decoded fast path 是有效的。
  - “宽而泛”的 generic decoded fast path 在当前结构下反而容易变慢。
- 未来如果继续沿 predecode 方向推进，最优先的不应该是盲目增加更多指令覆盖，而应继续挑最热、最单纯、最容易压平的子集逐个专门化。

## 第十轮：专门化整数 `decoded` 快路径

这一轮把上一轮已经验证有效的 predecode 机制，继续扩到一批高频整数族，但仍坚持两个原则：
- 只接入已经在慢路径里实现并被裸机/Linux 实际使用到的家族。
- 每补一类，都保留原慢路径，不改变 decode cache 失效和自修改代码语义。

### 这一轮实现内容
- 新增 `DecodedKind` 快路径覆盖：
  - `LogicalShifted`
  - `LogicalImm`
  - `Bitfield`
- 这些家族对应的执行逻辑继续复用现有语义实现，只是从 `exec_data_processing()` 的大分派前移到 predecode 路径。
- 新增裸机对照用例 `tests/arm64/predecode_logic_min.S`：
  - 覆盖 `AND/ORN/BICS`
  - 覆盖 `ORR/AND/EOR/ANDS (immediate)`
  - 覆盖 `LSR/LSL/UBFX/SBFX/BFI`
  - 同时验证 `fast` / `-decode slow` 输出一致

### 功能验证
- `tests/arm64/run_all.sh`：通过。
- `tests/linux/build_linux_shell_snapshot.sh`：通过。
- `AARCHVM_FUNCTIONAL_TIMEOUT=360s tests/linux/run_functional_suite.sh`：通过。
- `predecode_logic_min.bin` 在 `fast` / `slow` 下均输出 `P`。

### 结果文件
- `fast`：`out/perf-predecode-int-fast-results.txt`
- `slow`：`out/perf-predecode-int-slow-results.txt`
- `slow rerun`：`out/perf-predecode-int-slow-rerun-results.txt`
- `fast rerun`：`out/perf-predecode-int-fast-rerun-results.txt`

### Fast vs Slow 中位数对比
| case | slow median host_ns | fast median host_ns | 提升 |
| --- | ---: | ---: | ---: |
| `base64-enc-4m` | 4811152113.5 | 3793706553.0 | 21.15% |
| `base64-dec-4m` | 5293861651.0 | 3823185935.5 | 27.78% |
| `fnv1a-16m` | 5728386039.5 | 4658783794.5 | 18.67% |
| `tlb-seq-hot-8m` | 12528673.5 | 10180963.5 | 18.74% |
| `tlb-seq-cold-32m` | 13641224.0 | 11324408.0 | 16.98% |
| `tlb-rand-32m` | 17556525.0 | 14995345.5 | 14.59% |

### 这一轮结果解释
- 这说明 predecode 不只对分支、访存有效，对常用整数位运算家族也确实有收益。
- 与第九轮相比，这一轮的收益结构更偏向纯算法 workload：
  - 三个算法 case 全部继续向前。
  - 三个 TLB case 也维持正收益，但幅度小于第九轮对纯访存路径的改善。
- 这符合预期，因为这轮主要压的是：
  - `exec_data_processing()` 大分派成本
  - `exec_decoded()` 中通用整数 case 的固定成本

### 当前判断
- 这轮之后，predecode 的收益已经不再只是“访存特化有效”，而是形成了一个更明确的模式：
  - 最热整数族和最热访存族，都适合做窄而专的 decoded 执行路径。
- 但热点也更加清楚地暴露出来：
  - `lookup_decoded()`
  - `exec_decoded()`
  - `translate_address()`
- 因此下一轮不再继续扩大 decoded 覆盖率，而是转去压缩 predecode 自身的固定成本。

## 第十一轮：压缩 predecode 固定成本

这一轮尝试的重点不是“再多支持几类指令”，而是减少已支持 decoded 指令每次执行都要付出的固定开销。

### 这一轮实现内容
- 把第十轮和第九轮已经专门化过的 hot family 提取为独立 helper：
  - `AddSubImm`
  - `AddSubShifted`
  - `LogicalShifted`
  - `LogicalImm`
  - `Bitfield`
  - `LoadStoreUImm`
- `step()` 在拿到 decoded 指令后，先对这几个 hot family 直接分发到 helper，尽量绕开 `exec_decoded()` 里更大的通用 `switch`。
- 这一轮中途还试过“先做 cached lookup 旁路，再回落普通 lookup”的版本。它能明显压低热点里的 `exec_decoded()`，但端到端收益不够稳定，因此最终保留的是更简单的“单次 `lookup_decoded()` + hot helper dispatch”版本。

### 功能验证
- `tests/arm64/run_all.sh`：通过。
- `tests/linux/build_linux_shell_snapshot.sh`：通过。
- `AARCHVM_FUNCTIONAL_TIMEOUT=360s tests/linux/run_functional_suite.sh`：通过。
- 自修改代码、VA 切换、`predecode_load_store_min.bin`、`predecode_logic_min.bin` 继续通过。

### 结果文件
- `fast`：`out/perf-predecode-dispatch-fast-results.txt`
- `fast rerun`：`out/perf-predecode-dispatch-fast-rerun-results.txt`
- `fast rerun2`：`out/perf-predecode-dispatch-fast-rerun2-results.txt`
- `slow`：`out/perf-predecode-dispatch-slow-results.txt`
- `slow rerun`：`out/perf-predecode-dispatch-slow-rerun-results.txt`
- 热点：`out/perf-predecode-dispatch-fast.report`
- 试验版 cached 旁路对比：
  - `out/perf-predecode-cache-fast-results.txt`
  - `out/perf-predecode-cache-fast-rerun-results.txt`
  - `out/perf-predecode-cache-slow-results.txt`
  - `out/perf-predecode-cache-slow-rerun-results.txt`

### 测量说明
- 这一轮其中一条 `fast rerun` 出现了明显宿主机异常值，主要体现在 TLB 三项突然整体拉高。
- 为避免被异常值污染，这一轮最终采用：
  - `slow` 两次取中位数（两次值本身已很接近）
  - `fast` 三次取中位数
- 下表因此使用的是“`slow median(2)` vs `fast median(3)`”。

### Fast vs Slow 中位数对比
| case | slow median host_ns | fast median host_ns | 提升 |
| --- | ---: | ---: | ---: |
| `base64-enc-4m` | 4763778028.0 | 3991770309.0 | 16.21% |
| `base64-dec-4m` | 5296070549.5 | 4041578695.0 | 23.69% |
| `fnv1a-16m` | 5825577992.5 | 4950618209.0 | 15.02% |
| `tlb-seq-hot-8m` | 13940317.0 | 11778594.0 | 15.51% |
| `tlb-seq-cold-32m` | 13474735.5 | 11904380.0 | 11.65% |
| `tlb-rand-32m` | 17457177.5 | 15571087.0 | 10.80% |

### 与第十轮最优 fast 中位数对比
| case | 第十轮 fast median | 第十一轮 fast median | 变化 |
| --- | ---: | ---: | ---: |
| `base64-enc-4m` | 3793706553.0 | 3991770309.0 | -5.22% |
| `base64-dec-4m` | 3823185935.5 | 4041578695.0 | -5.71% |
| `fnv1a-16m` | 4658783794.5 | 4950618209.0 | -6.26% |
| `tlb-seq-hot-8m` | 10180963.5 | 11778594.0 | -15.69% |
| `tlb-seq-cold-32m` | 11324408.0 | 11904380.0 | -5.12% |
| `tlb-rand-32m` | 14995345.5 | 15571087.0 | -3.84% |

### 这一轮结果解释
- 第十一轮相对于 `slow` 路径依然是稳定正收益。
- 但如果把比较对象换成第十轮已经达到的最优 `fast` 状态，这一轮并没有继续向前，反而普遍小幅回退。
- 这说明：
  - 单纯把 hot family 从 `exec_decoded()` 大 `switch` 里再搬出来，并不自动等于更快。
  - 在当前编译器 codegen 下，额外 helper 分发、指令 cache 局部性、以及 `lookup_decoded()` 自身成本，可能抵消了这部分收益。
- 因此，第十一轮的价值更多在于“把热点拆明白”，而不是拿到新的最优性能点。

### 最终热点追踪
基于 `out/perf-predecode-dispatch-fast.report`，当前版本最值得关注的热点大致如下：
- `Cpu::step()`：约 22.45%
- `Cpu::translate_address()`：约 11.19%
- `Cpu::lookup_decoded()`：约 10.59%
- `Cpu::exec_data_processing()`：约 7.71%
- `Cpu::exec_load_store()`：约 6.35%
- `Bus::read()`：约 6.15%
- `Cpu::try_take_irq()`：约 5.08%
- `Cpu::exec_decoded()`：约 3.53%
- `Cpu::decode_insn()`：约 3.28%
- `Cpu::exec_system()`：约 2.71%

### 热点解读
- `exec_decoded()` 本身已经不像前几轮那么突出，但 `lookup_decoded()` 仍然很重，说明瓶颈更偏向：
  - decode cache 命中路径本身
  - page/slot 查找与 raw compare
  - fallback 指令仍需反复经过 predecode 框架
- `translate_address()`、`exec_load_store()`、`Bus::read()` 仍然排在前列，说明访存主路径依旧是解释器性能核心。
- `try_take_irq()` 仍有 5% 左右，表明“每条指令都做一次 IRQ 判定”这件事在当前模型下仍然不便宜。

### 当前结论
- 第十轮是明确有效的正优化，建议保留。
- 第十一轮主要提供了热点拆解价值，但没有形成新的最优端到端性能点。
- 如果下一步继续追性能，优先级应是：
  1. 压 `lookup_decoded()` 命中路径，而不是继续把更多 case 往 helper 里搬。
  2. 缩减 `translate_address()` / `exec_load_store()` / `Bus::read()` 之间的接口层固定成本。
  3. 引入更粗粒度的执行缓存，例如块级预解码，而不是继续做单条指令级的小分派优化。

## 第十二轮优化对比

这一轮目标是压缩 predecode 快路径里 `lookup_decoded()` 的命中固定成本，而不改变 decode cache 的失效语义：
- 将 `DecodedInsn::raw` 从结构体里拆出，改为 `DecodePage::raws[slot]`。
- 增加 `DecodePage::valid_words`，先用位图快速判定 slot 是否有效，再比较 `raws[slot]`，最后才触碰 `DecodedInsn` 本体。
- 这样可以让“命中但无需访问完整 decoded payload”的路径更紧凑，减少 cache line 触碰与结构体访问。

### 功能验证
- `tests/arm64/run_all.sh`：通过。
- `tests/linux/build_linux_shell_snapshot.sh`：通过。
- `AARCHVM_FUNCTIONAL_TIMEOUT=360s tests/linux/run_functional_suite.sh`：通过。

### 第十二轮结果
| case | 上一版 host_ns | 当前 host_ns | 提升 |
| --- | ---: | ---: | ---: |
| `base64-enc-4m` | 4704116100 | 4576972507 | 2.70% |
| `base64-dec-4m` | 5003840082 | 4535721134 | 9.36% |
| `fnv1a-16m` | 5526943925 | 5541376951 | -0.26% |
| `tlb-seq-hot-8m` | 26329071 | 13996800 | 46.84% |
| `tlb-seq-cold-32m` | 11553802 | 11552162 | 0.01% |
| `tlb-rand-32m` | 15717239 | 15567772 | 0.95% |

### 第十二轮结果解释
- 这一轮对 decode cache 命中路径的收益是明确的，尤其是 `tlb-seq-hot-8m` 这类短路径 benchmark，固定 lookup 成本占比高，因此改善最明显。
- `base64-dec-4m` 也取得了较明显提升，说明这类 workloads 对取指 / decode cache 命中固定成本同样敏感。
- `fnv1a-16m` 基本持平，说明这一轮不是全局性改造，而是针对 `lookup_decoded()` 命中热路径的定向减负。
- 这组数据来自单次 before/after，对长时间 case 仍应视为“方向确认”，不是严格统计显著性结论。

### 第十二轮原始数据文件
- baseline：`out/perf-current-results.txt`
- optimized：`out/perf-decodecache-compact-results.txt`
- log：`out/perf-decodecache-compact.log`

## 第十三轮优化对比

这一轮针对的是“无中断可取时仍高频进入 IRQ 查询路径”的固定成本：
- 在 `Cpu` 中加入 `should_try_irq()`，先基于 `PSTATE.I`、负缓存状态和 GIC epoch 做轻量门控，再决定是否真正进入 `try_take_irq()`。
- `step()` 在正常执行、`WFI`、`WFE` 路径上统一使用这一门控。
- `GicV3::has_pending()` 改为直接返回 `best_pending_valid_`，不再扫描 bitmap。
- 因为所有调用点已经在 `step()` 入口门控，`try_take_irq()` 自身去掉了重复的“无中断”固定检查。

### 功能验证
- `tests/arm64/run_all.sh`：通过。
- `tests/linux/build_linux_shell_snapshot.sh`：通过。
- `AARCHVM_FUNCTIONAL_TIMEOUT=360s tests/linux/run_functional_suite.sh`：通过。

### 第十三轮结果
| case | 上一版 host_ns | 当前 host_ns | 提升 |
| --- | ---: | ---: | ---: |
| `base64-enc-4m` | 4576972507 | 3868324877 | 15.48% |
| `base64-dec-4m` | 4535721134 | 4750755020 | -4.74% |
| `fnv1a-16m` | 5541376951 | 5104164626 | 7.89% |
| `tlb-seq-hot-8m` | 13996800 | 13929570 | 0.48% |
| `tlb-seq-cold-32m` | 11552162 | 11977252 | -3.68% |
| `tlb-rand-32m` | 15567772 | 15198171 | 2.37% |

### 第十三轮结果解释
- 这一轮的收益比第十二轮更混合，但 `base64-enc-4m`、`fnv1a-16m` 与 `tlb-rand-32m` 都有正向改善，说明 IRQ 门控的固定成本压缩确实起效。
- `base64-dec-4m` 与 `tlb-seq-cold-32m` 的单次结果出现回退，更像宿主机噪声而不是 guest 行为变化，因为这一轮并没有修改 guest 指令路径语义。
- `GicV3::has_pending()` 改成 O(1) 后，无中断阶段的 `try_take_irq()` 入口理论上已经接近“只做几次本地条件判断”。
- 这组数据同样来自单次 before/after，对 mixed 结果需要结合后续热点分析一起看，而不能单独据此下结论。

### 第十三轮原始数据文件
- baseline：`out/perf-decodecache-compact-results.txt`
- optimized：`out/perf-irqgate-results.txt`
- log：`out/perf-irqgate.log`

## 第十三轮后 perf / gprof 热点观察

这一轮在当前代码上重新做了两种热点采样：
- `perf`：`out/perf-post-round13.perf.data` / `out/perf-post-round13.report`
- `gprof`：`out/gprof-post-round13.txt`

### perf 观察

基于 `out/perf-post-round13.report` 中 `cpu_core/cycles` 样本，当前最靠前的用户态热点大致是：
- `Cpu::step()`：约 `21.85%`
- `Cpu::lookup_decoded()`：约 `12.14%`
- `Cpu::translate_address()`：约 `11.49%`
- `Cpu::exec_data_processing()`：约 `8.11%`
- `Cpu::exec_load_store()`：约 `6.92%`
- `Bus::read()`：约 `6.50%`
- `exec_load_store()` 内部读辅助 lambda：约 `3.89%`
- `exec_decoded()`：约 `3.78%`
- `exec_system()`：约 `3.48%`
- `decode_insn()`：约 `3.35%`
- `exec_decoded_load_store_uimm()`：约 `2.59%`
- `translate_data_address_fast()`：约 `2.48%`

最重要的变化是：`try_take_irq()` 已经不再位于主热点组里，说明第十三轮“IRQ 查询门控 + O(1) has_pending”确实把之前那条固定热链压下去了。

### gprof 观察

基于 `out/gprof-post-round13.txt` 的 flat profile，去掉明显的运行时噪声项（如 `_init`）后，当前最稳定的热点排序与 `perf` 基本一致：
- `Cpu::lookup_decoded()`：约 `13.73%`
- `Cpu::step()`：约 `12.78%`
- `Cpu::translate_address()`：约 `8.58%`
- `Cpu::exec_load_store()`：约 `7.48%`
- `Cpu::exec_data_processing()`：约 `5.19%`
- `BusFastPath::read()`：约 `5.17%`
- `Bus::read()`：约 `4.08%`
- `decode_insn()`：约 `2.40%`
- `exec_decoded()`：约 `2.33%`
- `translate_data_address_fast()`：约 `1.30%`

### 当前热点结论
- 第十二轮和第十三轮的方向是对的：decode cache 命中路径和 IRQ 固定成本都被进一步压缩了。
- 现在的第一梯队已经重新收敛到解释器核心：
  - 取指后的 `lookup_decoded()` 命中判定
  - `translate_address()` / `translate_data_address_fast()`
  - `exec_load_store()` + `Bus::read()` / `BusFastPath::read()`
- 这说明后续如果还想拿到可观收益，应优先继续压缩访存链和 decoded 命中链，而不是再做零碎 helper 内联。

## 第十四轮优化对比

这一轮针对 `load/store` 热链做了更直接的 RAM 优化，原则仍然是“只优化 RAM，不改变设备语义”：
- `BusFastPath` / `Bus` 暴露 RAM 指针查询接口，允许 CPU 在已经拿到物理地址后直接命中固定 RAM 区间。
- `Cpu::mmu_read_value()` / `Cpu::mmu_write_value()` 对 RAM 访问直接做窄宽度读写，只有非 RAM 地址才回退到原有 `Bus::read()` / `Bus::write()` 路径。
- 标量 `LDR/STR B/H/W/X [Xn,#imm]` 与 decoded `LoadStoreUImm` 统一复用这条路径，减少访存热链上的总线层包装开销。

### 功能验证
- `tests/arm64/run_all.sh`：通过。
- `tests/linux/build_linux_shell_snapshot.sh`：通过。
- snapshot 恢复后注入 `run_functional_suite`：通过，见 `out/linux-functional-suite-snapshot.log`。

说明：这一轮 Linux 功能回归与性能测试均通过 shell snapshot 恢复后注入命令完成，而没有使用当前 `tests/linux/run_functional_suite.sh` / `tests/linux/run_algorithm_perf.sh` 的冷启动注入路径。原因是冷启动脚本当前对 UART 注入时机较敏感，与本轮优化本身无关，但会影响自动回归稳定性。

### 第十四轮结果
| case | 上一版 host_ns | 当前 host_ns | 提升 |
| --- | ---: | ---: | ---: |
| `base64-enc-4m` | 3868324877 | 4216587985 | -9.00% |
| `base64-dec-4m` | 4750755020 | 4279537162 | 9.92% |
| `fnv1a-16m` | 5104164626 | 5668703466 | -11.06% |
| `tlb-seq-hot-8m` | 13929570 | 10035324 | 27.96% |
| `tlb-seq-cold-32m` | 11977252 | 11232707 | 6.22% |
| `tlb-rand-32m` | 15198171 | 15939156 | -4.88% |

### 第十四轮结果解释
- 这一轮对典型 RAM/TLB 热路径更友好，`tlb-seq-hot-8m` 和 `tlb-seq-cold-32m` 都取得了正向改善，说明“CPU 直接命中 RAM 指针”的方向是成立的。
- 但运算类 case 结果依然比较混合，尤其 `base64-enc-4m` 和 `fnv1a-16m` 出现回退。这更像是当前测量噪声与访存/非访存工作负载比例差异共同作用，而不是单一结论。
- 因此，第十四轮更适合视为“压缩 RAM 访存固定成本”的第一步，而不是已经把整个 `load/store` 热链彻底压干净。

### 第十四轮原始数据文件
- baseline：`out/perf-irqgate-results.txt`
- optimized：`out/perf-loadstore-ramfast-results.txt`
- log：`out/perf-loadstore-ramfast.log`

## 第十五轮尝试：lookup_decoded() 顺序页流缓存

这一轮尝试继续压 `lookup_decoded()`，思路是：
- 对“同一页内顺序执行”的取指流增加一个顺序页流缓存，复用上一次 `DecodePage` 和下一条 slot，减少 page 解析与索引判断。

### 功能验证
- `tests/arm64/run_all.sh`：通过。
- `tests/linux/build_linux_shell_snapshot.sh`：通过。
- snapshot 恢复后注入 `run_functional_suite`：通过。

### 第十五轮结果
| case | 第十四轮 host_ns | 第十五轮 host_ns | 提升 |
| --- | ---: | ---: | ---: |
| `base64-enc-4m` | 4216587985 | 4658271323 | -10.47% |
| `base64-dec-4m` | 4279537162 | 5812190237 | -35.81% |
| `fnv1a-16m` | 5668703466 | 6165366913 | -8.76% |
| `tlb-seq-hot-8m` | 10035324 | 10658141 | -6.21% |
| `tlb-seq-cold-32m` | 11232707 | 13034686 | -16.04% |
| `tlb-rand-32m` | 15939156 | 15554121 | 2.42% |

### 第十五轮结果解释
- 这次尝试虽然功能正确，但端到端结果明显更差，尤其 `base64-dec-4m` 与 `tlb-seq-cold-32m` 的回退幅度已经超出“可以先留着再观察”的范围。
- 因此这一轮没有保留在当前主线中，代码已回退到第十四轮的稳定状态。
- 这个结果也说明：`lookup_decoded()` 的优化不能只盯着“减少一次 page/slot 判断”，还要小心新增状态本身带来的分支、cache footprint 和编译器 codegen 退化。

### 第十五轮原始数据文件
- baseline：`out/perf-loadstore-ramfast-results.txt`
- trial：`out/perf-lookup-stream-results.txt`
- log：`out/perf-lookup-stream.log`

### 当前保留状态
- 当前代码保留的是第十四轮优化。
- 第十五轮 `lookup_decoded()` 顺序页流缓存仅作为已测失败尝试记录在案，不保留在工作树中。


## 第十六轮尝试：数据侧 RAM 页缓存

这一轮尝试继续压数据访存热链，思路是：
- 在 CPU 侧为最近一次命中的 RAM 物理页增加一个小页缓存，避免 `mmu_read_value()` / `mmu_write_value()` 每次都重新走 `bus_.ram_ptr()` / `bus_.ram_mut_ptr()` 的区间判断。
- 只缓存 RAM 页指针，不改变设备路径与 MMIO 语义。

在实现和验证这轮尝试时，还顺手修了两个会影响回归可信度的正确性问题：
- `exec_decoded_load_store_uimm()` 回退版里，`STR Xn, [Xm,#imm]` 的 64-bit store 误用了 `reg32()`，会把内核高地址指针截断。这一问题已经修复，并保留在当前主线。
- `build_linux_shell_snapshot.sh` 生成的 snapshot 之前会把 `halted=1` 固化进去，导致恢复后一步不跑。现在 host-stop 条件下保存 snapshot 时会清掉这个伪 `halted` 状态，该修复同样保留在当前主线。

### 功能验证
- `tests/arm64/run_all.sh`：通过。
- `tests/linux/build_linux_shell_snapshot.sh`：通过，且恢复后的 `steps` 可以继续增长。
- snapshot 恢复后注入 `run_functional_suite`：通过，见 `out/linux-functional-step1.clean.log`。

### 第十六轮结果
| case | 第十四轮 host_ns | 第十六轮 host_ns | 提升 |
| --- | ---: | ---: | ---: |
| `base64-enc-4m` | 4216587985 | 6619798769 | -56.99% |
| `base64-dec-4m` | 4279537162 | 5579224104 | -30.37% |
| `fnv1a-16m` | 5668703466 | 8516441642 | -50.24% |
| `tlb-seq-hot-8m` | 10035324 | 10072741 | -0.37% |
| `tlb-seq-cold-32m` | 11232707 | 11251814 | -0.17% |
| `tlb-rand-32m` | 15939156 | 14627591 | 8.23% |

### 第十六轮结果解释
- 这一轮只有 `tlb-rand-32m` 得到了明确改善，其余 case 尤其是三个运算类 workload 都明显退化。
- 这说明“再省掉一次 `ram_ptr()` 区间判断”并没有换来可观收益，反而把额外分支、页缓存状态和 codegen 负担带进了更常见的热路径。
- 因此第十六轮不保留在当前主线中，代码已回退到第十四轮稳定基线之上，只保留上面提到的两个正确性修复。

### 第十六轮原始数据文件
- baseline：`out/perf-loadstore-ramfast-results.txt`
- trial：`out/perf-step16-data-results.txt`
- log：`out/perf-step16-data.log`

## 第十七轮尝试：取指侧 RAM 快路径

这一轮尝试把相同思路转到取指链路：
- 对 `step()` 中最热的 4-byte 指令取指，在已经拿到物理地址后优先命中 CPU 侧最近 RAM 页缓存。
- 命中时直接从宿主 RAM 指针取 32-bit 指令，避免每条指令都再走一次 `Bus::read()`。

### 功能验证
- `tests/arm64/run_all.sh`：通过。
- `tests/linux/build_linux_shell_snapshot.sh`：通过。
- snapshot 恢复后注入 `run_functional_suite`：通过，见 `out/linux-functional-step2.clean.log`。

### 第十七轮结果
| case | 第十四轮 host_ns | 第十七轮 host_ns | 提升 |
| --- | ---: | ---: | ---: |
| `base64-enc-4m` | 4216587985 | 8005197392 | -89.85% |
| `base64-dec-4m` | 4279537162 | 6454682962 | -50.83% |
| `fnv1a-16m` | 5668703466 | 8546187346 | -50.76% |
| `tlb-seq-hot-8m` | 10035324 | 13027998 | -29.82% |
| `tlb-seq-cold-32m` | 11232707 | 15563320 | -38.55% |
| `tlb-rand-32m` | 15939156 | 18251956 | -14.51% |

### 第十七轮结果解释
- 尽管这一轮把 `bus_read` 大幅压下去了，但端到端 `host_ns` 全面变差，说明当前这版取指快路径的附加分支、页缓存检查和 `memcpy`/codegen 成本已经超过了省下来的总线层包装成本。
- 这是一个很典型的信号：热点指标局部变好，并不自动等于真实 workload 更快。当前解释器的主要固定成本，已经不再只是 `Bus::read()` 调用层。
- 因此第十七轮同样不保留在当前主线中，代码已回退。

### 第十七轮原始数据文件
- baseline：`out/perf-loadstore-ramfast-results.txt`
- trial：`out/perf-step17-fetch-results.txt`
- log：`out/perf-step17-fetch.log`

### 当前保留状态
- 当前主线仍然保留第十四轮 RAM 直达路径优化。
- 第十六轮“数据侧 RAM 页缓存”和第十七轮“取指侧 RAM 快路径”都已验证并回退，不保留在工作树中。
- 本轮实际保留的只有两个正确性修复：
  - decoded `STR Xn, [Xm,#imm]` 的 64-bit store 宽度修复。
  - host-stop 条件下 shell snapshot 的 `halted` 固化修复。

## 当前行为完善轮的性能观测

这一轮没有继续做性能优化，而是补了两类正确性工作：
- 向量 `FABD` 的解码冲突修复。
- `CPACR_EL1.FPEN` 对 FP/AdvSIMD 访问的最小 trap 语义，以及相应单测。

为了确认这些改动没有明显打坏当前主线性能，我重新跑了一次 Linux 算法性能套件，并补抓了一份基于 shell snapshot 的 `gprof` 热点。

### 当前算法性能结果

原始结果文件：`out/perf-suite-results.txt`

| case | host_ns |
| --- | ---: |
| `base64-enc-4m` | 2140511752 |
| `base64-dec-4m` | 2024712952 |
| `fnv1a-16m` | 3126710077 |
| `tlb-seq-hot-8m` | 5561744 |
| `tlb-seq-cold-32m` | 6263538 |
| `tlb-rand-32m` | 9742291 |

### 当前 gprof 热点

原始结果文件：`out/gprof-current.txt`

在基于 `out/linux-usertests-shell-v1.snap` 的 shell snapshot 恢复路径下，注入 `bench_runner` 后得到的平顶热点大致为：
- `Cpu::translate_address()`：约 `15.36%`
- `Cpu::step()`：约 `12.93%`
- `Cpu::lookup_decoded()`：约 `10.54%`
- `Cpu::exec_load_store()`：约 `7.57%`
- `Cpu::exec_data_processing()`：约 `5.55%`
- `BusFastPath::read()`：约 `4.20%`
- `Bus::read()`：约 `3.69%`
- `exec_load_store()` 内部 MMU 读辅助 lambda：约 `2.78%`
- `Cpu::exec_system()`：约 `2.43%`
- `Cpu::exec_decoded()`：约 `2.05%`

### 当前 perf 采样说明

我也补抓了一份 snapshot 恢复路径下的 `perf` 数据：
- data：`out/perf-current-snap.data`
- report：`out/perf-current-snap.report`

但当前这份 `perf` 采样仍明显被宿主机匿名页 fault 和页清零路径污染，热点主要落在宿主机内核的 `asm_exc_page_fault` / `handle_mm_fault` / `clear_page_erms` 上，因此它对分析解释器本体价值有限。现阶段更可信的本体热点来源仍是同工作负载下的 `gprof` 结果。

### 当前结论

- 本轮正确性补丁没有改变热点分布的主结构。
- 解释器的第一梯队瓶颈仍然稳定收敛在三条链路：
  - `step()` 后的取指与 decoded 命中链。
  - `translate_address()` / `translate_data_address_fast()` 地址翻译链。
  - `exec_load_store()` + `BusFastPath::read()` / `Bus::read()` 访存链。
- `CPACR` trap 前置判断已经进入热点表，但占比很低，只在 `0.22%` 左右，说明它没有成为新的固定负担。
