# Linux 性能基线与优化对比

## 测量方法
- guest 环境：Linux 6.12.76 + BusyBox initramfs，基于快照 `out/linux-perf-shell-v1.snap` 启动 benchmark。
- 测量时计时倍率：`AARCHVM_TIMER_SCALE=1`。
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
