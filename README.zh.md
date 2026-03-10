# aarchvm（Stage-0）

这是一个使用 C++ + CMake 开发的 AArch64 全系统解释执行模拟器最小原型。

## 当前状态

项目仍处于启动阶段，但已具备：
- 单核 AArch64 解释器。
- 面向 EL1 的基础系统寄存器模型。
- 最小 SoC：RAM、PL011 UART、Generic Timer、最小 GICv3。
- 最小 IRQ 闭环（timer -> GIC -> 向量 -> `ERET` 返回）。
- 最小同步异常闭环（`ESR_EL1/FAR_EL1/ELR_EL1/SPSR_EL1`）。
- 面向 Linux 早期启动的最小 MMU/TLB 路径（支持 `TTBR0_EL1/TTBR1_EL1` 的 Stage-1 翻译、4KB 粒度、table 属性继承，以及 `TCR.IPS` PA 位宽检查）。
- 最小 GICv3 系统寄存器 CPU 接口 + 虚拟定时器 sysreg 路径（`ICC_*`、`CNTV_*`）。

## 构建

```bash
cmake -S . -B build
cmake --build build -j
```

## 运行

运行内置示例：

```bash
./build/aarchvm
```

运行外部二进制：

```bash
./build/aarchvm -bin <program.bin> -load <load_addr> -entry <entry_pc> -steps <max_steps>
```

常用可选参数：
- `-sp <addr>`：初始化启动栈指针（可选）
- `-dtb <file> -dtb-addr <addr>`：加载 DTB 并通过 `x0` 传递 DTB 地址（可选）

## 当前已实现指令

控制流：
- `NOP`, `B`, `BL`, `BR`, `BLR`, `RET`, `B.cond`, `CBZ`, `CBNZ`, `TBZ`, `TBNZ`, `BRK`, `ERET`
- `WFI`, `WFE`, `SEV`, `SEVL`

数据处理：
- `ADR`, `ADRP`
- `MOVZ`, `MOVK`, `MOVN`（32/64 位）
- `ADD/SUB/ADDS/SUBS`（立即数 + 寄存器移位，32/64 位）
- `CSEL`（32/64 位）
- 位域立即数家族（32/64 位）：`UBFM`, `SBFM`, `BFM`
  及常见别名（`LSR`, `LSL`, `UBFX`, `SBFX`, `BFXIL`）
- 提取/旋转立即数：`EXTR`, `ROR`（立即数别名）
- 条件选择家族：`CSEL`, `CSINC`（覆盖 `CSET` 别名路径）
- 整数除法：`UDIV`, `SDIV`
- 逻辑寄存器类（32/64 位）：
  `AND`, `BIC`, `ORR`, `ORN`, `EOR`, `EON`, `ANDS`, `BICS`

访存：
- `LDR/STR Xt`, `LDR/STR Wt`（`[Xn, #imm12]`）
- `LDRSW/LDURSW`（覆盖测试使用的 unsigned/unscaled/pre/post-index 形式）
- `LDRB/STRB`, `LDRH/STRH`（`[Xn, #imm12]`）
- `LDUR/STUR Xt`, `LDUR/STUR Wt`（`[Xn, #simm9]`）
- `LDP/STP`（覆盖当前测试所需形式）

系统与维护：
- 关键 EL1 寄存器的 `MRS/MSR` 子集
- `MSR DAIFSet/DAIFClr`、`MRS/MSR DAIF`
- `MRS/MSR SPSel`、`MSR SPSel, #imm`
- `DMB`, `DSB`, `ISB`, `CLREX`
- `TLBI VMALLE1/VMALLE1IS`
- `TLBI VAE1/VAE1IS/VALE1/VALE1IS, Xt`
- `TLBI ASIDE1/ASIDE1IS, Xt`
- `AT S1E1R`、`AT S1E1W`
- `IC IALLU`、`IC IVAU, Xt`
- `DC IVAC/CVAC/CIVAC, Xt`

## 系统寄存器子集

- `SCTLR_EL1`, `TTBR0_EL1`, `TTBR1_EL1`, `TCR_EL1`, `MAIR_EL1`, `VBAR_EL1`
- `ELR_EL1`, `SPSR_EL1`, `ESR_EL1`, `FAR_EL1`, `SP_EL0`, `SP_EL1`, `SPSel`
- `DAIF`, `NZCV`, `CURRENTEL`, `PAR_EL1`
- `ID_AA64MMFR0_EL1`, `ID_AA64MMFR1_EL1`, `ID_AA64MMFR2_EL1`
- `CNTFRQ_EL0`, `CNTPCT_EL0`（最小别名）, `CNTVCT_EL0`
- `CNTV_CTL_EL0`, `CNTV_CVAL_EL0`, `CNTV_TVAL_EL0`
- `ICC_IAR1_EL1`, `ICC_EOIR1_EL1`, `ICC_PMR_EL1`, `ICC_CTLR_EL1`, `ICC_SRE_EL1`

## MMU/TLB/缓存维护模型说明

当前实现仍是“可验证闭环”的最小模型，但已经覆盖 Linux 早期页表最关键的部分：
- 支持 `TTBR0_EL1` 与 `TTBR1_EL1` 的 Stage-1 翻译。
- 简化的 4 级页表遍历（按 4KB 页粒度假设）。
- 按 `TCR_EL1` 的 `T0SZ/T1SZ` 推导起始遍历级别，并支持 `EPD0/EPD1` 对应的禁用路径。
- `TCR.IPS` 会被解码，并用于限制输出 PA 与下一级页表基址不能超出配置的物理地址位宽。
- `TCR_EL1` 中的 `IRGNx/ORGNx/SHx` walk 属性会被解码，并随翻译结果/TLB 项一起保存。
- 叶子项的 `AttrIndx` 会通过 `MAIR_EL1` 解码到对应的 MAIR byte，并记录到翻译结果/TLB 项中。
- 支持 table 描述符与 block/page 描述符（用于早期映射场景）。
- 支持 `APTable[1]`、`PXNTable/UXNTable` 的上级表属性继承。
- 对叶子项执行 `AF`、`AP[2]`（EL1 只读）以及执行禁止检查。
- 内建软件 TLB，缓存的不只是 PA，也包含继承后的有效权限与属性信息；`TLBI` 可观察到刷新效果。
- `TLBI` 已支持常见 EL1 本地/inner-shareable 变体，足以覆盖早期 firmware/kernel 常见用法。
- `AT S1E1R/S1E1W` 会旁路软件 TLB，执行 fresh walk，并通过 `PAR_EL1` 反映成功或 fault 状态。
- Translation fault / Access-flag fault / Permission fault / Address-size fault 会编码到 abort 的 ISS 低位，便于读取 `ESR_EL1` 做调试。
- `IC/DC` 维护指令已支持解码与执行（最小功能语义）。

该实现仍不是完整的架构级 MMU/Cache 模型：虽然 shareability 和 memory type 已能解码，但它们还没有驱动真实的内存次序/设备副作用；DBM/dirty 状态更新尚未支持；table 属性继承目前只覆盖了最关键子集；按 ASID 标记的真实 TLB 行为也仍做了简化。

## 测试

构建全部测试二进制：

```bash
tests/arm64/build_tests.sh
```

运行全量回归：

```bash
tests/arm64/run_all.sh
```

关键输出（按顺序）：
- `E`（`instr_legacy_each.bin`：先前已实现指令逐项验证）
- `Q`（`mmu_tlb_cache.bin`：MMU/TLB/缓存维护路径验证）
- `K`（`mmu_ttbr1_early.bin`：TTBR1 高地址早期映射与取指路径验证）
- `1`（`mmu_tlb_vae1_scope.bin`：按 VA 失效只影响目标页，不影响邻近页）
- `2`（`mmu_ttbr_switch.bin`：切换 `TTBR0_EL1` 后，活动翻译上下文随之切换）
- `3`（`mmu_unmap_data_abort.bin`：取消映射并 `TLBI` 后触发 data abort）
- `4`（`mmu_tlbi_non_target.bin`：`TLBI VAE1` 仅失效目标 VA）
- `5`（`mmu_l2_block_vmalle1.bin`：L2 block 重映射 + `TLBI VMALLE1` 生效）
- `6`（`mmu_at_tlb_observe.bin`：`AT/PAR_EL1` 在 `TLBI` 前可看到 fresh walk，而普通访存仍命中旧 TLB）
- `7`（`mmu_ttbr_asid_mask.bin`：`TTBR0_EL1` ASID 位与基址掩码处理验证）
- `8`（`mmu_perm_ro_write_abort.bin`：EL1 只读页写入触发 permission fault）
- `9`（`mmu_xn_fetch_abort.bin`：XN 页禁止取指，更新 PTE 并 `TLBI` 后才可执行）
- `H`（`mmu_table_ap_inherit.bin`：`APTable` 写保护会继承到下级叶子项）
- `Y`（`mmu_table_pxn_inherit.bin`：`PXNTable` 会禁止下级取指，修正上级表并 `TLBI` 后恢复）
- `Z`（`mmu_tcr_ips_mair_decode.bin`：`TCR.IPS` 对输出 PA 位宽生效，同时覆盖 `AttrIndx/MAIR` 路径）
- `0`（`mmu_af_fault.bin`：`AF=0` 时触发 access-flag fault，置位后恢复访问）
- `X`（`sync_exception_regs.bin`：同步异常与 ESR/FAR/ELR/SPSR 路径验证）
- `G`（`gic_timer_sysreg.bin`：最小 GICv3 sysreg 与 CNTV sysreg 中断路径验证）
- `U`（`bitfield_basic.bin`：位域立即数/移位别名与 CNTPCT 路径验证）
- `V`（`p1_core.bin`：EXTR/ROR + CSINC/CSET + UDIV/SDIV + LDRSW 家族）
- `ASM B C S IM T D W P L M R`（原有功能测试）

## 距离运行 Linux 还缺哪些关键特性

现在的 MMU/TLB 路径已经足以支撑更真实的 early page table bring-up，但距离真正启动 Linux 仍有几类关键缺口：
- 更完整的 AArch64 指令子集，尤其是原子/排他访存（`LDXR/STXR`、`LDAXR/STLXR`、CAS 类原子指令）、更多系统指令，以及更深内核初始化路径会用到的访存形式。
- 更完整的异常级与特权模型：EL2/EL3 启动路径、`HCR_EL2`、`SCR_EL3`、`VBAR_EL2/EL3`、以及从不同固件路径切入内核时的 `ERET` 行为。
- 更完整的系统寄存器覆盖，尤其是 Linux 在 CPU feature detect 阶段会探测的一批 ID 寄存器、timer/control 寄存器，以及更多 MMU 相关 sysreg。
- 更完整的 MMU 语义：真实的 MAIR memory type 行为、更完整的 table attribute 继承、ASID 感知的 TLB、dirty/DBM 行为、break-before-make 边界情况，以及最终需要的 EL0 权限语义。
- 更完整的 GICv3 模型，尤其是当前最小 sysreg CPU 接口之外的 distributor/redistributor MMIO 路径。
- PSCI/SMCCC 支持，用于 boot chain 以及 Linux 的电源管理/CPU 接口调用。
- 更完整的 timer 支持，不能只停留在当前最小 virtual timer 路径。
- 早期启动之后需要的更多设备模型：存储、块设备、网络；如果要有实用的 Linux 用户态，通常还需要 virtio 一类设备。
- 多核/SMP 支持。
- 真实 cache 与 barrier 副作用。仅仅能解码维护指令还不够，Linux 继续往后走会依赖更强的内存模型语义。

## 参考手册

- `DDI0487_M.a.a_a-profile_architecture_reference_manual.pdf`
