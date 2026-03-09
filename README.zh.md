# aarchvm（Stage-0）

这是一个使用 C++ + CMake 开发的 AArch64 全系统解释执行模拟器最小原型。

## 当前状态

项目仍处于启动阶段，但已具备：
- 单核 AArch64 解释器。
- 面向 EL1 的基础系统寄存器模型。
- 最小 SoC：RAM、PL011 UART、Generic Timer、最小 GICv3。
- 最小 IRQ 闭环（timer -> GIC -> 向量 -> `ERET` 返回）。
- 最小同步异常闭环（`ESR_EL1/FAR_EL1/ELR_EL1/SPSR_EL1`）。
- 最小 MMU/TLB 路径（支持 `TTBR0_EL1/TTBR1_EL1` 的 Stage-1 翻译，4KB 粒度风格）。
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
- `TLBI VMALLE1`、`TLBI VAE1, Xt`
- `AT S1E1R`、`AT S1E1W`
- `IC IALLU`、`IC IVAU, Xt`
- `DC IVAC/CVAC/CIVAC, Xt`

## 系统寄存器子集

- `SCTLR_EL1`, `TTBR0_EL1`, `TTBR1_EL1`, `TCR_EL1`, `MAIR_EL1`, `VBAR_EL1`
- `ELR_EL1`, `SPSR_EL1`, `ESR_EL1`, `FAR_EL1`, `SP_EL0`, `SP_EL1`, `SPSel`
- `DAIF`, `NZCV`, `CURRENTEL`, `PAR_EL1`
- `CNTFRQ_EL0`, `CNTPCT_EL0`（最小别名）, `CNTVCT_EL0`
- `CNTV_CTL_EL0`, `CNTV_CVAL_EL0`, `CNTV_TVAL_EL0`
- `ICC_IAR1_EL1`, `ICC_EOIR1_EL1`, `ICC_PMR_EL1`, `ICC_CTLR_EL1`, `ICC_SRE_EL1`

## MMU/TLB/缓存维护模型说明

当前实现是“可验证闭环”的最小模型：
- 支持 `TTBR0_EL1` 与 `TTBR1_EL1` 的 Stage-1 翻译。
- 简化的 4 级页表遍历（按 4KB 页粒度假设）。
- 按 `TCR_EL1` 的 `T0SZ/T1SZ` 推导起始遍历级别（最小实现）。
- 支持 table 描述符与 block/page 描述符（用于早期映射场景）。
- 描述符判定与属性解析为最小化实现。
- 内建软件 TLB，`TLBI` 可观察到刷新效果。
- `AT` 指令会以最小语义更新 `PAR_EL1`。
- `IC/DC` 维护指令已支持解码与执行（最小功能语义）。

该实现仍不是完整的架构级 MMU/Cache 模型。

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
- `4`（`mmu_tlbi_non_target.bin`：`TLBI VAE1` 仅失效目标 VA）
- `5`（`mmu_l2_block_vmalle1.bin`：L2 block 重映射 + `TLBI VMALLE1` 生效）
- `6`（`mmu_at_tlb_observe.bin`：`AT/PAR_EL1` 与 TLB 刷新前后行为验证）
- `7`（`mmu_ttbr_asid_mask.bin`：`TTBR0_EL1` ASID 位与基址掩码处理验证）
- `X`（`sync_exception_regs.bin`：同步异常与 ESR/FAR/ELR/SPSR 路径验证）
- `G`（`gic_timer_sysreg.bin`：最小 GICv3 sysreg 与 CNTV sysreg 中断路径验证）
- `U`（`bitfield_basic.bin`：位域立即数/移位别名与 CNTPCT 路径验证）
- `V`（`p1_core.bin`：EXTR/ROR + CSINC/CSET + UDIV/SDIV + LDRSW 家族）
- `ASM B C S IM T D W P L M R`（原有功能测试）

## 参考手册

- `DDI0487_M.a.a_a-profile_architecture_reference_manual.pdf`
