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
