# TODO

## Armv8-A 程序可见正确性收尾计划

目标：
- 不追求周期级、微架构级或实现细节级的精确模拟。
- 只追求 guest 程序可观察到的 ISA / 系统行为符合 Armv8-A 规范，以及与当前 ID 寄存器声明保持一致。
- 保持“高性能优先”的项目方向；任何修复都不应把热路径拖回到无必要的慢精确模型。

范围边界：
- [ ] 仅把“当前模型已声明存在的特性”纳入正确性闭环；对 ID 寄存器已声明 absent 的特性，不为追求覆盖率而实现。
- [ ] 不追求 EL2/EL3、AArch32、SVE/SME、MTE、PAC、BTI、PMU、完整 debug 架构等超出当前模型目标的全规范支持。
- [ ] 对 hint、cache、TLB、异常、timer、GIC 等路径，只要求程序可见结果正确，不要求内部实现方式或时序贴近真实硬件。

### 0. 文档审计状态（2026-04-01 21:30）

审计结论：
- 当前实现已经非常接近“Armv8-A 程序可见最小集”收口，但基于代码现状仍不能宣布完全收口。
- 这次审计仅更新文档与 TODO，不包含模拟器行为修改。

本轮确认的剩余高优先级缺口：
- [x] 已重新审查 `DMB/DSB/ISB` 在当前执行模型下的程序可见语义：`SoC::run()` 以单个 `cpu.step()` 为粒度交织各核，`mmu_write_value()/raw_mmu_write()` 同步提交访存效果，因此 guest 可见内存效果本身已经形成单一全序；在该模型下 barrier 继续实现为 no-op 不会放宽程序可见顺序。
- [x] 已重新审查多个 LSE 指令家族的 acquire/release/ordered 变体：由于当前模型不存在写缓冲、延迟提交或宿主侧乱序可见性，`CAS/CASP/SWP/LDADD/LDCLR/LDEOR/LDSET` 的顺序变体折叠到同一同步 RMW 结果不会放宽 guest 可见语义；`smp_dmb_message_passing`、`smp_lse_casa_publish`、`smp_lse_ldaddal_counter`、`smp_spinlock_ldaxr_stlxr` 与 Linux SMP functional suite 也已继续通过。
- [ ] `ESR_EL1/FAR_EL1/PAR_EL1/ISS` 目前已覆盖大量关键路径，但仍需按异常类别做“已实现路径逐类对账”，避免剩余角落遗漏。
- [ ] 差分验证当前以 `qemu-aarch64`（user-mode）为主；系统级路径还需补强与 `qemu-system-aarch64` 和 Linux SMP 压测的组合对账。

本轮复核（2026-04-01 22:34）：
- [x] 已复核 `TODO.md` 中 Armv8-A 收口相关复选框一致性：未发现“明确已完成但仍未打勾”的文档/计划项。
- [ ] 仍有多项处于“部分完成”状态（例如事件驱动与 SMP 语义闭环中的阶段性项）；这些条目暂不勾选，避免误报完成度。

本轮复核（2026-04-06 03:13）：
- [x] 已重新串联 `src/soc.cpp` 的 SMP 主循环与 `src/cpu.cpp` 的访存提交路径，确认当前模型对 guest 可见内存效果提供单一全序；`DMB/DSB/ISB` 的 no-op 实现与 acquire/release LSE 变体折叠在该模型下是保守且自洽的。
- [x] 已用现有正式回归再次验证上述结论：`tests/arm64/run_all.sh`、`tests/linux/run_qemu_user_diff.sh`、`tests/linux/run_functional_suite.sh`、`tests/linux/run_functional_suite_smp.sh` 均通过。
- [ ] 仍未完成的高优先级收口点主要收缩为：`ESR_EL1/FAR_EL1/PAR_EL1/ISS` 逐类对账、`MMU/TLB/fault` 细颗粒边界，以及 `qemu-system-aarch64` 层面的系统级差分。

### 1. 浮点 / AdvSIMD 程序可见语义收尾

目标：
- 收敛当前最可能“程序不一定立刻崩，但结果会悄悄错”的剩余语义尾差。

任务：
- [ ] 系统性梳理 `FPCR.DN/FZ/AH` 对普通 FP arithmetic、conversion、compare、sqrt、estimate、round-int 的影响。
- [ ] 统一检查 `FPSR` 的 `IOC/DZC/OFC/UFC/IXC` 在标量与向量路径中的置位一致性。
- [ ] 统一检查 `qNaN/sNaN/default-NaN/payload/subnormal/±0/Inf` 在各类 FP/AdvSIMD 指令中的传播行为。
- [ ] 检查 pairwise / reduce / widen / narrow / scalar-by-element / structured load-store 等已实现 AdvSIMD 路径的边界语义。
- [ ] 对拿不准的行为优先做工具链 / QEMU / 手册交叉验证，再实现，不凭猜测补语义。

预期收益：
- 降低 glibc、libm、编译器生成代码、数值程序和多媒体代码中“看似能跑、结果却悄悄错”的风险。

当前进展：
- [x] 已继续收口 `!FEAT_FP16` 与 `CPACR_EL1.FPEN` 交界处的一组真实尾差：`FMUL/FDIV/FMAX/FMINNM/FNMUL`、`FCVTAS` 以及 `FMUL/FDIV/FMAX/FMINNM V*.4H/8H` 这类 half data-processing 编码现在不会再被 `FPEN=0` 误吞成 `EC=0x07` trap；同时修正了 `FMAXNM/FMINNM V*.4H/8H` 被早前 `DUP (element)` 宽掩码误解码的 overlap，并补正式裸机回归同时锁定 direct-undef 与 `FPEN=0` 两条路径。
- [x] 已收口普通 `SIMD&FP` 整寄存器 `LDR/STR Q` 在 `SCTLR_EL1.A=1` 下的 16-byte 对齐 fault 语义，并与 structured load/store 的 element-size 对齐语义显式分离。
- [x] 已补裸机单测覆盖 misaligned `LDR/STR Q` 的 `A=0` 正常访问与 `A=1` fault、`WnR`、`FAR_EL1`、`ELR_EL1` 行为。
- [x] 已收口 `LDP/STP Q,Q` pair transfer 在 `SCTLR_EL1.A=1` 下按“每个 Q 元素 16-byte”而不是内部 8-byte 子访问做对齐检查的语义。
- [x] 已收口 structured `LD1/ST1 {Vt.8B/16B}` / `{Vt.8B/16B,Vt2.8B/16B}` 的 element-size 对齐语义，避免 `.16B` multiple-structures 形式被错误按 8-byte 对齐要求触发 alignment fault。
- [x] 已收口 `FP/AdvSIMD` memory 路径在 `Rn==SP` 时的 `CheckSPAlignment()` 语义，覆盖 `LDR/STR Q`、single-structure lane/replicate 与 whole-register structured load/store，并补裸机单测验证 fault 优先于访问与写回。
- [x] 已补 whole-register structured `LD1/ST1/LD2/ST2/LD3/ST3/LD4/ST4` 的 register post-index (`[Xn|SP], Xm`) 形式，并用裸机单测覆盖各家族的 load/store 与写回语义。
- [x] 已修正 `CPACR/CPTR` 下 `FP/AdvSIMD` trap 识别漏掉 whole-register structured register post-index 的问题，确保 `[Xn|SP], Xm` 形式也先走 `EC=0x07` trap，并补裸机单测验证 trap 优先于写回。
- [x] 已收口当前已实现的饱和 AdvSIMD 整数指令 `SQADD/UQADD/SQXTN/UQXTN` 的 `FPSR.QC` 程序可见语义，并把 `FPSR` 读写掩码收回到当前 AArch64-only 模型真正实现的 architected 位，避免 `QC/IDC/IOC..IXC` 之外的保留位被错误读回。
- [x] 已收口当前 AArch64-only 模型的 `FPCR` direct read/write 掩码：把 `!FEAT_AFP` 下的 `NEP/AH/FIZ`、`!FEAT_FP16` 下的 `FZ16`、`!FEAT_EBF16` 下的 `EBF`，以及当前未实现 trapped FP exception 时本应 `RAZ/WI` 的 `IDE/IXE/UFE/OFE/DZE/IOE` 从 guest 可见状态中移除，并新增裸机回归锁定这组可见位。
- [x] 已修正 `FCVTN/FCVTXN` 的 narrowing exception flags 尾差：`double -> float` overflow 现在会按程序可见语义置 `FPSR.OFC|IXC`，underflow/tiny inexact 现在会置 `FPSR.UFC|IXC`，并新增裸机回归分别锁定 regular `FCVTN` 与 round-to-odd `FCVTXN` 的 overflow / underflow / exact-subnormal 边界。
- [x] 已继续收口 `FPCR.FZ` 与 `AH==0` 下“先判断结果是否 tiny、再舍入”的普通 FP helper 语义：`ADD/SUB/MUL/DIV`、`FMULX`、`FMA/FMSUB/FNMADD/FNMSUB` 与 `FSQRT` 现在不会再先经宿主舍入把 borderline tiny 结果抬成最小正规数；在 `FZ=1` 时会按程序可见语义直接返回带符号零并置 `FPSR.UFC`，同时新增 `fp_fz_preround_arith` 裸机回归锁定 single/double 的 `FMUL/FMADD` 中点边界。
- [x] 已修正 `FRECPE` 对“倒数估计会溢出到 `Inf` 的极小 subnormal”只置 `FPSR.IXC` 的尾差；当前 scalar/vector `single/double` 共享 helper 会按程序可见语义置 `FPSR.OFC|IXC`，并新增裸机回归锁定 smallest-subnormal 的正/负 `single` 与 `double` 边界，同时把既有 `fpsimd_fp_estimate` 中 `tiny` case 的旧错误期望一并收正。
- [x] 已新增 `fp_fcvt_special_scalar` 正式裸机回归，锁定 scalar `FCVT*` 在 `qNaN/sNaN/±Inf`、`-0.6/-0.5` 近零负数，以及 `FPCR.FZ=1` 下 subnormal-to-int 的程序可见结果与 `FPSR`（`IOC/IXC/IDC`）组合，覆盖 signed/unsigned 与 `W/X` 目的寄存器边界。
- [x] 已补齐 `LDR (literal, SIMD&FP)` 的 `St/Dt/Qt` 三条程序可见路径，并把这一家族接入 `CPACR_EL1` 的 `EC=0x07` FP trap 识别；新增 `fp_literal_load` 与 `cpacr_fp_literal_trap` 裸机回归，覆盖标量 `S/D` literal load 后高位清零、`Q` literal load 正向执行，以及 EL1/EL0 下 trap 优先于执行的边界。
- [x] 已补齐 `LDP/STP (SIMD&FP)` scalar pair transfer 的 `St/Dt/Qt` 路径，覆盖 signed-offset / pre-index / post-index / no-allocate pair 形式，并把整个 pair 家族接入 `CPACR_EL1` 的 `EC=0x07` trap 识别；同时统一复用 `load_vec_whole/store_vec_whole`，修正 `Rn==SP` 时 pair transfer 漏掉 `CheckSPAlignment()` 的尾差，并新增 `fp_pair_scalar_ls`、`cpacr_fp_pair_trap` 与扩展后的 `fpsimd_sp_alignment_fault` 裸机回归锁定正常执行、trap 优先级、fault 优先于写回与内存更新的边界。

### 2. 异常 / 系统寄存器 / trap 语义收尾

目标：
- 让内核、libc、JIT、信号处理与自检程序能看到正确的异常与系统行为。

任务：
- [ ] 复查 `ESR_EL1/FAR_EL1/PAR_EL1/ISS` 的程序可见编码是否与当前已实现异常类型一致。
- [ ] 复查“同一条指令到底应 trap / undef / no-op / 正常执行”的判定，确保与当前 ID 寄存器声明一致。
- [ ] 复查 `ERET`、`SPSR_EL1`、`ELR_EL1`、`PSTATE` 相关可见行为，确保异常返回不留下状态尾差。
- [ ] 复查 EL0 对系统寄存器、cache/TLB/system 指令的可见行为是否与 Linux 用户态预期一致。
- [ ] 统一整理“当前模型已声明支持 / 已声明 absent / 保守 no-op”的系统指令语义边界。

预期收益：
- 减少用户态 `illegal instruction`、内核异常路径错判、信号恢复异常、JIT/运行时探测失败等问题。

当前进展：
- [x] 已收口一组“应为 `UNDEFINED` 却被误分类成 EL0 system access trap”的 system-encoding / sysreg 边界，包括若干 absent feature 指令族。
- [x] 已收口 direct `MRS/MSR <Xt>, ALLINT/PM` 在 `!FEAT_NMI/!FEAT_EBEP` 下的 absent-feature 语义，修正此前 `EL0` 访问被误分类成 `EC=0x18` system-access trap 的问题；当前 `EL1/EL0` 两级都稳定表现为同步 `UNDEFINED`，并新增 `allint_pm_sysreg_absent` 正式裸机回归锁定 `MRS/MSR`、`ESR_EL1.IL/ISS/FAR_EL1`、寄存器不变以及 `EL0` 保存下来的 `SPSR_EL1`。
- [x] 已把 `CPACR_EL1` 的 guest 可见位收回到当前模型真正声明支持的 `FPEN[21:20]`，避免把 `TTA/SMEN/ZEN/E0POE/TAM/TCPAC` 等 absent-feature 位错误读回为 1，并新增裸机回归锁定 direct `MRS/MSR` 可见值。
- [x] 已把 `VBAR_EL1` 的 `RES0` 低位语义收口到 bits `[10:0]==0`，并新增裸机回归同时锁定 direct read-back 与真实 `SVC` 异常向量落点，避免 misaligned `VBAR` 在 guest 侧继续可见。
- [x] 已收口 `EL0 WFE/WFI` 在 `SCTLR_EL1.nTWE/nTWI=0` 下的 trap 优先级，修正此前“先消费 event register / 先观察 pending IRQ、后决定 trap”的错误顺序，并补裸机回归覆盖 `event-register-set` 与 `pending-IRQ` 这两个此前未锁住的边界。
- [x] 已新增 `cpacr_fp_sysreg_trap` 裸机回归，覆盖 direct `MRS/MSR FPCR/FPSR` 在 `CPACR_EL1.FPEN=00/01/10/11` 下的 EL1/EL0 trap/allow 语义，确保 special-purpose `FPCR/FPSR` 访问稳定走 `EC=0x07` 的 `FP/ASIMD access trap` 语义，而不是被 generic sysreg 路径误分类。
- [x] 已新增 `sysreg_trap_iss_rt_fields` 裸机回归，把 `EC=0x18` system-access trap 的 `ISS` 细节从“只看 EC 对不对”收紧到显式锁定 `Rt`、read/write bit、`op0/op1/CRn/CRm/op2` 编码与 `FAR_EL1==0`，覆盖 `MRS CTR_EL0`、`MSR TCR_EL1`、`IC IVAU`、`DC ZVA` 这四类 EL0 trap。
- [x] 已把 `illegal_state_return` 裸机回归扩成三类 `ERET` 尾状态边界：保留的 AArch64 `SPSR.M`、合法返回但 `SPSR.IL=1`、以及返回到不支持的 AArch32 state；并显式锁定异常后保存下来的 `NZCV/DAIF/PAN/IL/M`，避免 `ERET/SPSR/PSTATE` 只“看起来能跑”但状态仍有尾差。
- [x] 已补 `PC alignment fault` 的最小程序可见语义：当前 CPU 主循环现在会在 AArch64 取指前真正检查 `PC[1:0]`，并保证它优先于 `Illegal Execution state`；新增裸机回归覆盖 `EL1 BR -> misaligned PC`，以及“`ERET` 返回到 `IL=1 + misaligned PC` 时仍应先报 `EC=0x22`”这条优先级边界。
- [x] 已把 `el0_eret_undef` 从“只看 EL0 执行 `ERET` 会不会进 `EC=0`”收紧到同时验证 `ESR_EL1.IL=1`、`ISS=0`、`FAR_EL1=0`，以及保存到 `SPSR_EL1` 里的 EL0 源 `NZCV/DAIF/PAN/M/IL`，补齐 `ERET` 家族另一条关键 trap 边界。
- [x] 已把 `sync_exception_regs`、`svc_sysreg_minimal`、`software_step_basic` 补成正式强断言回归，分别锁定 same-EL instruction abort、`SVC`、software-step/`BRK` 路径上的 `ESR_EL1/ELR_EL1/FAR_EL1`、live `DAIF`、以及 `SPSR_EL1` 保存的 `NZCV/DAIF/PAN/SS/IL/M`。
- [x] 已新增 `mmu_at_par_formats` 裸机回归，把 `AT -> PAR_EL1` 的关键程序可见格式固定进正式回归：成功态锁定 `bit11=RES1`、Device 与 Normal Non-cacheable 的 `SH=0b10`，fault 态锁定 `F`、`RES1 bit11`、`S/PTW=0` 与 `FST` 编码。
- [x] 已继续补齐 `AT -> PAR_EL1` 的 write-side fault 覆盖：新增 `mmu_at_par_write_fault_kinds`，把 `AT S1E1W/S1E0W` 下的 translation fault、access-flag fault 与 permission fault 统一锁进正式回归，显式校验 `PAR_EL1` fault 低 7 位 `F | (FST << 1)` 编码、`RES1 bit11`、`S/PTW=0` 与高位清零边界。
- [x] 已把 `el0_cache_ops_privilege` 与 `el0_tlbi_cache_undef` 从“只看会不会进异常”收紧到显式锁定 `EL0 cache/TLBI/AT` 这组边界里 `UNDEFINED` 与 `system-access trap` 的分类、`IL=1`、`ISS=0` 与 `FAR_EL1=0`，避免 `TLBI from EL0` 之类路径被错误宽放行或误报成 `EC=0x18`。
- [x] 已补 base cache maintenance by set/way 家族里此前漏掉的 `DC CSW, Xt` 解码，并新增 `dc_csw_privilege` 裸机回归锁定其程序可见边界：`EL1` 下正常执行、`EL0` 下保持 `UNDEFINED`，且异常时显式检查 `ESR_EL1.IL=1`、`ISS=0`、`FAR_EL1=0` 与保存到 `SPSR_EL1` 的源 `NZCV/DAIF/PAN/M`。
- [x] 已把 `brk_exception`、`hlt_undef`、`el0_hvc_smc_undef`、`el1_hvc_smc_undef` 这组 exception-generating 指令回归继续收紧，统一验证 `IL/ISS/FAR` 与 `SPSR_EL1` 中保存的源 `NZCV/DAIF/PAN/M`，把 `BRK/HLT/HVC/SMC` 从 smoke 提升为正式异常语义回归。
- [x] 已补 `ICC_HPPIR1_EL1/ICC_IAR1_EL1/ICC_EOIR1_EL1/ICC_DIR_EL1` 在“非 IRQ 异常上下文”下的最小程序可见语义：同步异常或普通 EL1 代码里现在也能手动查询最高优先级 pending IRQ、完成 acknowledge / priority drop / deactivate，`GIC` 侧会按优先级而不是纯 `INTID` 顺序选中断，并新增 `gic_sysreg_manual_ack` 裸机回归与 snapshot 状态持久化覆盖这一路径。
- [x] 已对齐 `ID_AA64PFR0_EL1.GIC` 与当前 `ICC_*` system register 可见性，避免对外声明与实现矛盾。
- [x] 已收口 debug sysreg 资源数量边界，使 `ID_AA64DFR0_EL1` 的资源声明与 `DBGBVR<n>/DBGWVR<n>` 等可见性一致。
- [x] 已按 AArch64-only 实现收口 `DBGBCR<n>_EL1.BAS[3:0]=RES1` 语义：当前 direct read 与 breakpoint 匹配都统一把 `BAS[3:0]` 视为 `0b1111`，并新增 `debug_break_bas_res1` 正式裸机回归锁定 `DBGBCR` 读回值、EL1 breakpoint 异常的 `ESR_EL1/ELR_EL1/FAR_EL1` 与“faulting instruction 不执行”边界；同时修正 `instr_legacy_each`、`debug_sysreg_resource_bounds`、`debug_software_access_halt_read` 里依赖旧 readback 假设的回归。
- [x] 已按 `OS Lock` / `OS Double Lock` / `CORENPDRQ` 收口 self-hosted debug 异常生成条件，确保除 `BRK` 外，hardware breakpoint、watchpoint、software step 在锁定时都被正确抑制，而 `BRK` 保持始终可见。
- [x] 已新增 `debug_lock_exception_gating` 裸机回归覆盖上述 debug lock 语义，并修正 `debug_break_watch_basic`、`software_step_basic` 的测试前置条件，使其显式解锁 debug lock，不再依赖旧的错误宽放行行为。
- [x] 已为 `MDSCR_EL1.TDA` 补上最小 software-access debug event 行为：对本来会成功的 `DBGBVR/DBGBCR/DBGWVR/DBGWCR` 访问，在 `OS Lock` 解锁且 `TDA=1` 时进入 halting 行为；并补 `debug_software_access_halt_read/write` 裸机回归覆盖“锁着不触发、解锁后 halt”的读写路径。
- [x] 已收口 `MDSCR_EL1[30:29] <-> EDSCR/MDCCSR_EL0` 这组 DCC full-flag 联动：`OS Lock` 锁定时通过 `MSR MDSCR_EL1` 的 save/restore 写入会同步更新 `TXfull/RXfull`，解锁后 `MRS MDSCR_EL1` 会只读反映 live 的 DCC 状态；并补 `debug_mdscr_dcc_flags` 裸机回归覆盖锁定态 save/restore 与解锁态 live 反映。
- [x] 已收口 `AT S1E1R/W` 与 `PSTATE.PAN` / `FEAT_PAN2` 的边界，避免 plain `AT` 错误受 PAN 影响，并通过 `at_pan2_absent_undef` 正式裸机回归锁定 `AT S1E1RP/WP` 在 `FEAT_PAN2` absent 时应表现为 `UNDEFINED`，且不应修改 `PAR_EL1` 或源寄存器。
- [x] 已新增 `at_s1e0_el0_undef` 正式裸机回归，把 `EL0` 执行 `AT S1E0R/W` 的程序可见负向语义锁进回归：当前模型下两者都应报 `UNDEFINED`，并显式检查 `ESR_EL1.EC=0`、`IL=1`、`ISS=0`、`FAR_EL1=0`，同时断言 `PAR_EL1` 与源寄存器保持不变，避免后续 system decode 调整把这组 `AT` 指令误宽放行或留下隐藏副作用。
- [x] 已新增 `wfxt_absent_undef` 正式裸机回归，把当前 `!FEAT_WFxT` 模型下 `WFET/WFIT` 的负向语义单独锁进回归：覆盖 EL1/EL0 两级的 `UNDEFINED` 分类、`ESR_EL1.EC=0`、`IL=1`、`ISS=0`、`FAR_EL1=0`，并断言超时寄存器参数不被修改，以及 EL0 异常保存的 `NZCV/DAIF/PAN/M/IL` 符合源 `PSTATE`。
- [x] 已新增 `mmu_el0_uxn_fetch_abort` 正式裸机回归，把此前缺失的“EL0 指令取指 permission fault” 锁进异常语义覆盖：显式检查 `ESR_EL1.EC=0x20`、`IL=1`、`WnR=0`、`IFSC=permission fault level 3`，以及 `FAR_EL1/ELR_EL1` 都指向 faulting EL0 PC，避免 lower-EL fetch abort 被误编码成 same-EL 或 data-abort 形态。
- [x] 已对齐 `ID_AA64MMFR0_EL1` 的 granule 声明与当前 `4KB`-only 页表实现，避免把未实现的 `16KB granule` 错误宣称为存在。
- [x] 已收口 `ID_AA64MMFR0_EL1.BigEnd/BigEndEL0` 与 `SCTLR_EL1.EE/E0E` 的固定值语义，避免在 mixed-endian absent 时仍把 `EE/E0E` 读回为可配置位。
- [x] 已收口 `SPSR_EL1` 在“AArch64-only + 当前 absent feature 集”下的 `RES0` / 固定位语义，避免 guest 通过 `MSR SPSR_EL1` 把 `UAO/DIT/TCO/SSBS/BTYPE/ALLINT/PM/PPEND/EXLOCK/PACM/UINJ` 等当前未实现位读回成 1。
- [x] 已收口 `SPSel` / `SP_EL0` / `SP_EL1` 这一组 special-purpose stack pointer accessor 的程序可见语义：
  - `MSR SPSel, #imm` 与 `MSR SPSel, Xt` 现在都会在 CPU 侧同步保存/恢复当前 `SP` bank；
  - 通用 datapath 对 `SP` 的写回现在会同步当前活跃 bank，避免 `regs_[31]` 与 `SP_EL0/SP_EL1` 脱节；
  - direct `MRS/MSR SP_EL0` 在 `EL1t` 下按架构表现为 `UNDEFINED`；
  - direct `MRS/MSR SP_EL1` 在当前仅实现 `EL0/EL1` 的模型里不再被错误宽放行；
  - 已补裸机回归覆盖这些边界，并把旧的 `SP` alignment 测试从架构上不成立的 `MSR SP_EL1, Xt` 初始化改回 `mov sp, ...`。
- [x] 已修正“同一条 load/store 指令在 helper 已经取同步异常后仍继续发起后续访存或再次 `data_abort()`”的问题，避免双重异常和错误的异常覆盖。
- [x] 已补 `DC IVAC/DC ZVA` 的 self-hosted watchpoint 语义：当前 `DC IVAC` 会按 store-like watchpoint 产生 `CM=1/WnR=1`，`DC ZVA` 会按 store-like watchpoint 产生 `CM=0/WnR=1`，并确保 `DC ZVA` 在 watchpoint 命中前不会提交 zero write；已新增 `debug_cache_maint_watchpoints` 正式裸机回归并接入 fast/slow 完整回归。
- [x] 已补 `DBM` + `HAFDBS` 缺口：当前模型公开 `ID_AA64MMFR1_EL1.HAFDBS=0` 时，不再错误地把 `DBM=1` 的 stage-1 只读页当成可写；已新增 `mmu_dbm_hafdbs_absent` 回归，同时锁定 `AT S1E1W -> PAR_EL1` 与后续写访问 `ESR_EL1/FAR_EL1/ELR_EL1` 的权限 fault 行为。
- [x] 已收口 `FEAT_PAuth absent` 下 hint-space 与 integer / direct `PAuth` encoding 的语义边界，确保前者为 `NOP`、后者为 `UNDEFINED`，并修正 direct integer encodings 被 generic integer decode 误吞的问题。
- [x] 已收口 `PACM` 在 `!FEAT_PAuth_LR` 下的语义，确保它作为 hint-space 指令表现为 `NOP` 而不是 `UNDEFINED`，并补裸机单测覆盖 EL1 / EL0 两种执行路径。
- [x] 已收口 `LDRAA/LDRAB` 在 `!FEAT_PAuth` 下的语义，确保它们不再被 generic X load/store post/pre-index decode 误吞，而是稳定表现为 `UNDEFINED`，并补裸机单测覆盖 offset/pre-index 与“无访存、无写回”边界。
- [x] 已收口 `FEAT_LS64 absent` 下 `LD64B/ST64B/ST64BV/ST64BV0` 的语义边界，修正它们落入 generic unscaled load/store decode 并被误吞成普通 `STUR` 类指令的问题；当前模型下它们现在稳定表现为 `UNDEFINED`，并已补裸机回归覆盖 EL1/EL0 的 `EC=0`、`IL=1`、`ISS=0`、`FAR_EL1=0` 与“无结果寄存器修改 / 无基址写回”边界。
- [x] 已收口 `FEAT_LRCPC absent` 下 `LDAPR W/X`、`LDAPRB W`、`LDAPRH W` 的语义边界，修正它们被 generic unscaled load/store decode 误吞成 `LDURSW/LDURSB/LDURSH` 的问题，并补裸机回归覆盖 EL1/EL0 下的 `EC=0`、`IL=1`、`ISS=0`、`FAR_EL1=0` 与“无寄存器修改 / 无写回”边界。
- [x] 已新增 `mops_absent_undef` 正式裸机回归，把当前 `ID_AA64ISAR2_EL1.MOPS=0` 模型下 `SETP/SETM/SETE` 与 `CPYP/CPYM/CPYE` 的 absent-feature 语义固定进回归，显式锁定 EL1/EL0 的 `EC=0`、`IL=1`、`ISS=0`、`FAR_EL1=0`，以及目的/源/长度寄存器都不应发生写回。
- [x] 已把 `BRAA/BLRAA/RETAA/ERETAA` 以及 `RETAASPPCR/RETABSPPCR` 等 `PAuth/PAuth_LR` 分支返回类 absent-feature 语义固化进正式裸机回归，防止后续解码调整把它们误吞成普通 `BR/BLR/RET/ERET`。
- [x] 已补 `RETAASPPC/RETABSPPC` immediate-return 形式的正式裸机回归，确认当前 `!FEAT_PAuth_LR` 模型下它们稳定表现为 `UNDEFINED`，且不会被其它分支类解码误吞。
- [x] 已补 `RMIF/SETF8/SETF16` 的正式裸机回归，确认当前 `!FEAT_FlagM` 模型下它们在 EL1/EL0 都稳定表现为 `UNDEFINED`，且 `FAR_EL1` 保持当前未定义指令异常形态。
- [x] 已补 `DGH/ESB/TSB/GCSB/CLRBHB/BTI/CHKFEAT/STSHH` 的 hint-space absent-feature 裸机回归，确认当前 feature 声明下它们稳定表现为 `NOP`，且 `CHKFEAT X16` 保持输入值不变。
- [x] 已补 `MDCCINT_EL1` 以及 `OSDTRRX_EL1 / OSDTRTX_EL1` 的最小程序可见语义：`MDCCINT_EL1` 现为 `RX/TX` 两个位的 `RES0`-masked `RW`，`OSDTR*` 现作为 side-effect-free 的 DCC save/restore 视图访问 `DTRRX/DTRTX`，并补裸机回归覆盖 EL1 读写、`TXfull/RXfull` 不被意外改变，以及 EL0 下这些 sysreg 为 `UNDEFINED` 而非 trap。
- [x] 已新增 `sysreg_xzr_semantics` 正式裸机回归，锁定 system register 指令里 `Rt==31` 的 `XZR/WZR` 语义：`MRS XZR, <sysreg>` 不应改当前 `SP`，`MSR <sysreg>, XZR` 应写入 0 而不是把当前 `SP` 当作源操作数，覆盖 generic `TPIDR_EL0` 与 special `SP_EL0` 两条路径。

### 3. SMP 内存模型与同步原语收尾

目标：
- 收敛最容易导致“偶发卡死 / 偶发乱序 / 难复现”问题的多核程序可见错误。

任务：
- [ ] 系统审查 exclusive monitor、`LDXR/STXR/LDAXR/STLXR`、LSE 原子指令在多核下的可见语义。
- [x] 已复查 `DMB/DSB/ISB` 在当前模拟器模型下的保守实现是否足以满足程序可见顺序要求。
- [ ] 复查 `SEV/WFE/WFI`、IPI/SGI、timer PPI 的跨核唤醒可见语义。
- [ ] 复查 `TLBI`、`IC IVAU`、自修改代码、远端代码失效、`TTBR/TCR/SCTLR` 切换在多核下的传播行为。
- [ ] 复查 cache maintenance 在“程序能观察到的内存/执行结果”层面是否成立，而不是只看内部状态。

预期收益：
- 提高 SMP Linux、锁、futex 风格同步、自旋锁、并发用户态 workload 的稳定性。

当前进展：
- [x] 已新增 `smp_lse_ldaddal_counter` 裸机单测，覆盖 2 核 `LDADDAL` 原子累加与 `LDAR/STLR + SEV/WFE` 配合下的可见性。
- [x] 已新增 `smp_dmb_message_passing` 与 `smp_lse_casa_publish` 两条 SMP litmus，用正式强断言回归锁定 `STR/LDR + DMB/DSB` message passing，以及 `CASAL/CASA` 发布-获取路径下的 payload 可见性。
- [x] 已把 `smp_mpidr_boot`、`smp_sev_wfe`、`smp_ldxr_invalidate`、`smp_spinlock_ldaxr_stlxr`、`smp_tlbi_broadcast`、`smp_wfe_*`、`smp_gic_sgi`、`smp_timer_*`、`smp_dc_zva_invalidate` 这组高价值 SMP 裸机用例升级为正式强断言回归，不再只是“跑一下不检查结果”。
- [x] 已新增 `smp_ic_ivau_remote` 正式裸机回归，锁定 2 核下代码页更新后的 `DC CVAU + IC IVAU + DSB/ISB` 最小程序可见闭环；同时把 `IC IVAU` 在当前模型中保守扩展为跨核 decode 失效传播，避免显式 I-cache maintenance 后仍残留远端 stale decoded code。
- [x] 已新增 Linux 用户态 `pthread_sync_stress`，覆盖 `pthread + sched_setaffinity + C11 atomic` 下的双线程计数与 release/acquire 消息传递。
- [x] 已结合 `src/soc.cpp` 当前“一次只提交一个 `cpu.step()`、访存同步提交到全局内存顺序”的执行模型重新复核 barrier / acquire-release 语义，确认 `DMB/DSB` no-op 与 LSE 顺序变体折叠在当前模型下不会放宽程序可见内存顺序。

### 4. MMU / 地址翻译 / fault 边界收尾

目标：
- 保证内核与用户态看到的地址空间、权限和 fault 行为正确。

任务：
- [ ] 复查跨页访存、部分 fault、faulting byte、FAR 指向、pair load/store fault 行为。
- [ ] 复查 `TTBR0/TTBR1/TCR/MAIR/ASID/TLBI` 切换边界，以及与 TLB / decode cache 的一致性。
- [ ] 复查 PAN、XN/PXN/UXN、table AP 继承、AF 等已支持页表语义的程序可见行为。
- [ ] 复查“快路径 / 预解码 / TLB 优化”是否在 fault、权限、self-modifying code 场景下保持一致语义。

预期收益：
- 减少 Linux 启动、`execve`、动态链接、页权限切换、用户态 fault 测试中的边界错误。

当前进展：
- [x] 已补齐并回归验证一组高优先级对齐/fault 边界：`SP` alignment、标量 misaligned load/pair、`LDAR/STLR/LDXR/LSE atomic` misaligned fault、普通 `SIMD&FP` `LDR/STR Q` misaligned fault、`LDP/STP Q,Q` misaligned fault。
- [x] 已按 `AArch64.S1DisabledOutput()` 收口 `M=0` 时 stage-1 direct output 的程序可见语义：数据访问与 `AT` 数据翻译现在按 `Device-nGnRnE + OSH + PA=VA[55:0]` 生成 `PAR/翻译结果`，取指 direct output 维持 `Normal` 并受 `SCTLR_EL1.I` 控制；同时新增 `mmu_off_at_par_direct_data`、`mmu_off_dc_zva_align_fault`、`mmu_off_wxn_ignored_fetch` 正式回归，并把 `dc_zva`、`el0_cache_ops_privilege`、`smp_dc_zva_invalidate` 的成功路径改成在最小 Normal 映射下验证，避免继续依赖错误的 `M=0` 旧假设。
- [x] 已修正普通 load/store 在 `SCTLR_EL1.A=0` 时遗漏的“由 Device 内存类型决定的 alignment fault”语义缺口：当前 misaligned 访问即使关闭了常规 alignment check，只要 stage-1 输出是 Device memory 仍会报告 `FSC=0x21` 的 `Data Abort`，并新增 `mmu_device_unaligned_fault` 正式裸机回归同时锁定 load/store 两条路径的 `WnR/FAR_EL1` 与“fault 前内存不应被写坏”边界。
- [x] 已修正 translation fault 状态在 `AT` / cache-maintenance 翻译路径上的 stale `FAR` 污染问题：`translate_address()` 与 `translate_cache_maintenance_address()` 现在都会先清空旧的 `last_data_fault_va_`，避免前一条 data fault 的地址残留到后续 address-translation walk abort 或 cache-maintenance translation fault；同时新增白盒单测 `cache_maintenance_translation_fault_uses_requested_far` 与 `address_translation_walk_abort_uses_requested_far` 锁定这两条边界。
- [x] 已补裸机单测确认 structured `LD1/ST1` `.16B` multiple-structures 在 `SCTLR_EL1.A=1` 下仍按 byte element 对齐工作，不应错误 fault。
- [x] 已修正 whole-register structured `AdvSIMD` `LD1/ST1/LD2/ST2/LD3/ST3/LD4/ST4` 在跨页 fault 时的 `FAR_EL1` 报告，确保返回实际 faulting byte 而不是起始地址，并补裸机单测覆盖 sequential/interleaved、load/store 与 post-index fault 不写回。
- [x] 已修正 single-structure lane/replicate `AdvSIMD` load/store 在多字节元素跨页 fault 时的 `FAR_EL1` 报告，并补裸机单测覆盖 lane load、replicate load 与 post-index lane store 的 faulting byte 和 fault 时不写回。
- [x] 已补 `LDXP/LDAXP/STXP/STLXP` 32-bit pair 成功路径与 pair 对齐/fault 边界的裸机回归，并修正 `CASP` misaligned fault 的 `WnR` 断言为“atomic read 会先触发同一 fault 时 `WnR=0`”的架构语义。
- [x] 已补 pair-exclusive / `CASP` 在“对齐合法、地址翻译合法、但写权限 fault”场景下的裸机回归，确认 `STXP/CASP` fault 时内存不更新、`STXP` status 不写回，且 `FAR_EL1/DFSC/WnR` 保持正确。
- [x] 已收口 `TCR_EL1.TBI0/TBI1` 的最小程序可见语义：当前翻译、取指与 `PC` 规范化路径都会按 bit[55] 选择 `TBI0/TBI1`，在 `!FEAT_PAuth` 模型下对 instruction/data 一致忽略 top byte，并已用两条正式裸机回归分别锁定 `TTBR0/TBI0` 与 `TTBR1/TBI1` 的 tagged data access、`AT S1E1R`、`TLBI VAE1` 与 tagged `BLR` 后 `ADR` 看到的 canonical `PC` 行为。
- [x] 已修正 `TLBI VAE1/VALE1/VAAE1/VAALE1` 族的 `AA` / `LE` 语义位判定错误：此前代码把 bit[7] 误当成 `all ASIDs`，导致 `VAAE1` 被错误当成按 ASID 失效、`VALE1` 被错误当成全 ASID 失效；现已按 bit[6] 识别 `AA` 变体，并新增 `mmu_tlbi_vaae1_all_asids` 与 `mmu_tlbi_vale1_asid_scope` 两条正式裸机回归锁定 all-ASIDs 与 ASID-scoped 语义。
- [x] 已新增 `mmu_tcr_a1_ttbr1_asid_scope` 正式裸机回归，把 `TCR_EL1.A1=1` 时“TLB/translation ASID 必须来自 `TTBR1_EL1` 而不是 `TTBR0_EL1`”这条程序可见边界锁进 fast/slow 完整回归：仅切换 `TTBR0_EL1` 不应改变已缓存的低 VA 翻译，而切换 `TTBR1_EL1` 的 ASID 后必须触发新的 ASID 命中域并重新选中对应映射。
- [x] 已新增 `mmu_tcr_a1_aside1_scope` 正式裸机回归，把 `TCR_EL1.A1=1` 与 `TLBI ASIDE1` 的联动语义锁进 fast/slow 完整回归：低 VA 翻译虽然走 `TTBR0_EL1` 的页表，但当 `A1=1` 时，`ASIDE1` 仍必须按 `TTBR1_EL1` 的 ASID 作用在这组 TLB 项上，只刷新目标 ASID，不误伤其它低 VA ASID 域。
- [x] 已新增 Linux 用户态 `mprotect_exec_stress`，覆盖 `RW -> NONE -> RX` 权限切换、动态代码生成执行、`__builtin___clear_cache` 以及 `fork/execve` 路径。
- [x] 已补 `snapshot restore / TLBI tagged upper / IC IVAU tagged upper` 这组三类 predecode 一致性护栏：当前 `load_state()` 会显式清空 predecode cache，`TLBI VAE1*` 的 decode invalidation 会按 bit[55] 重建 canonical stage-1 VA 页，`IC IVAU` 的 decode invalidation 也统一走 `normalize_stage1_address()`；同时新增主机侧单测 `aarchvm_unit_cpu_cache_consistency` 与裸机回归 `mmu_tbi0_tagged_fault_far`，分别锁定内部 cache 失效一致性与 tagged translation fault 的 `FAR_EL1` 语义。
- [x] 已收口 `SCTLR_EL1.WXN` 对 stage-1 取指权限的程序可见语义：当前 `EL1` fetch 会把 `PXN || (WXN && privileged-writeable)` 作为 execute deny 条件，`EL0` fetch 会把 `UXN || (WXN && EL0-writeable)` 作为 execute deny 条件，并新增 `mmu_wxn_fetch_abort` 与 `mmu_el0_wxn_fetch_abort` 正式裸机回归，同时覆盖 fast/slow decode 路径。
- [x] 已修正 predecode cache 的 `PA` 侧失效索引错误：decode page 是按 `VA page` direct-mapped 入槽，旧实现却按 `PA page` 直接索引槽位，导致 `hash(VA)!=hash(PA)` 时 `on_code_write()` 的 `PA` 失效会漏掉真正缓存的 decoded page；现已改为按 `pa_page` 扫描全表失效，并新增白盒单测 `pa_invalidation_clears_decode_page_with_different_cache_index` 与正式裸机回归 `predecode_pa_alias_codegen` 锁定 alias self-modifying code 的程序可见行为。
- [x] 已修正外设/DMA 直接写 guest RAM 时绕过 CPU 失效链路的真实一致性缺口：`Bus` 现提供统一的 RAM buffer 写入口与 observer，`SoC` 会把外部 RAM 写入广播给所有 CPU 执行 `notify_external_memory_write()`，`BlockMmio/VirtioBlkMmio` 已改走该入口；新增白盒单测 `block_dma_write_notifies_cpu_decode_and_exclusive` 与 `virtio_dma_write_notifies_cpu_decode_and_exclusive`，锁定设备写入会同步清掉 stale predecode page 并清除 exclusive monitor。
- [x] 已把 `mmu_at_walk_ext_abort`、`mmu_walk_ext_abort_data`、`mmu_walk_ext_abort_fetch` 这组三条“页表 walk 上 synchronous External abort”正式纳入 `-decode slow` 一致性回归，避免此类 `ESR_EL1/FAR_EL1/CM/WnR/PAR_EL1 保持值` 边界只在 fast-path 下被验证。
- [x] 已收口 `!FEAT_MTE` 下 `ADDG/SUBG` 被 generic `ADD/SUB (immediate)` 误吞的 decode overlap：普通解释路径与 predecode/fast-path 现在都会把这两条编码稳定视为同步 `UNDEFINED`，并新增 `mte_absent_undef` 正式裸机回归覆盖 `ADDG/SUBG/IRG/GMI/STGP/STG/LDG` 的 `ESR_EL1.EC/IL/ISS`、`ELR_EL1`、`FAR_EL1` 与“无副作用”边界。
- [x] 已把一组 absent-feature decode overlap 高风险样例补进 `-decode slow` 一致性回归护栏：`flagm_integer_undef`、`pauth_absent_integer_undef`、`pauth_lr_absent_integer_undef`、`ldraa_ldrab_absent_undef`、`lrcpc_absent_undef`、`ls64_absent_undef` 现在都会随 `tests/arm64/run_all.sh` 同时验证 fast-path 与 slow-path，降低后续解码调整把缺特性指令误吞成已实现通路的风险。
- [x] 已新增 `mmu_cache_maint_el1_no_cmow_perm` 正式裸机回归，锁定当前 `ID_AA64MMFR1_EL1.CMOW=0` 模型下：`EL1` 上的 `DC CVAC/CVAU/CIVAC` 与 `IC IVAU` 不应仅因 stage-1 页是 privileged-only + read-only 就误报 permission fault；同时保持既有 `DC IVAC`“仍要求写权限”的程序可见边界不变。
- [x] 已修正 `TLBI VAE1/VALE1/VAAE1/VAALE1` 对 stage-1 block leaf 的失效范围：旧实现只会清掉命中 operand 的那个 4KB shadow TLB 项，导致同一 `L1/L2` block 内其它已缓存 page 仍可能继续命中旧翻译；现已按 leaf level 覆盖范围失效整块，并新增 `mmu_tlbi_block_vae1_scope` 正式裸机回归锁定 fast/slow decode 下“block remap 后仅以块内一个 VA 执行 TLBI，整块 shadow TLB 项都必须失效”的程序可见语义。

### 5. 正确性验证基础设施补强

目标：
- 让“程序可见正确性”可持续验证，而不是靠人工试命令。

任务：
- [ ] 持续补裸机单测，优先覆盖：FP 边界值、异常 syndrome、SMP 原子/屏障、跨页 fault、TLBI/IC IVAU。
- [ ] 持续补 Linux 用户态测试，优先覆盖：浮点/向量、线程同步、`mprotect`/`execve`/信号、长时间刷屏与内存压力。
- [ ] 对最容易拿不准的语义建立差分验证路径：当前模拟器 vs QEMU vs 工具链生成结果。
- [ ] 为“当前模型声明 absent 的特性”补负向测试，确保它们确实表现为 absent，而不是半实现状态。
- [x] 为当前 `ID_AA64ISAR0_EL1` 已声明存在的 `FEAT_CRC32` 补独立裸机单测，覆盖 `CRC32*` / `CRC32C*` 的 `b/h/w/x` 变体。

预期收益：
- 后续再做性能优化、预解码、事件驱动、JIT 时，不会轻易把程序可见语义重新弄坏。

当前进展：
- [x] 已修正 `CPACR_EL1` 的 `FP trap` 与 absent-feature 优先级边界：当前 `ID_AA64ISAR1_EL1.JSCVT/BF16=0`、`ID_AA64ISAR0_EL1.AES/DotProd=0`、`ID_AA64ISAR1_EL1.FCMA=0` 等模型下，`FJCVTZS/BFCVTN/BFCVTN2/FCADD/AESE/UDOT` 在 `FPEN` 关闭时不再被误报为 `EC=0x07` 的 FP 访问 trap，而会继续按架构要求落到 `EC=0` 的同步 `UNDEFINED`；并新增 `cpacr_fp_absent_undef` 正式回归把 `EL1/EL0` 下的 `ESR_EL1/FAR_EL1/目的寄存器不变` 一并锁定。
- [x] 已继续收口 `FEAT_FCMA` absent-feature 与 `CPACR_EL1.FPEN` 的交界边界：`FCMLA (by element)` 先前仍会在 `FPEN` 关闭时被误判成 `EC=0x07` 的 FP trap，现已在 `insn_uses_fp_asimd()` 中把 by-element half/single 两条 `FCMLA` 家族从 trap 集里排除，并扩展 `cpacr_fp_absent_more_undef` 正式回归锁定这两条编码在当前 `ID_AA64ISAR1_EL1.FCMA=0` 模型下保持 `EC=0/IL=1/ISS=0/FAR=0/目的寄存器不变`。
- [x] 已继续收口 `FEAT_FRINTTS` absent-feature 与 `CPACR_EL1.FPEN` 的交界边界：当前 `ID_AA64ISAR1_EL1.FRINTTS=0` 模型下，`FRINT32Z/FRINT32X/FRINT64Z/FRINT64X` 先前仍会被 `insn_uses_fp_asimd()` 误判成 `EC=0x07` 的 FP trap，现已把 scalar/vector 两组编码从 trap 集里精确排除，并扩展 `cpacr_fp_absent_more_undef` 与 `fpsimd_optional_absent_undef` 正式回归，锁定它们在 `FPEN=0` 与 `FPEN=开启` 两种情况下都保持 `EC=0/IL=1/ISS=0/FAR=0/目的寄存器不变` 的同步 `UNDEFINED` 语义。
- [x] 已继续扩展 `!FEAT_FP16` 负向回归：`fp16_absent_undef` 现在除原先的 half arithmetic/compare/FMOV/int-convert 外，又额外锁定了 `FSQRT/FRINTA/FRINTI/UCVTF/FCVTZU/FCSEL/FCCMP` 的 half 形式，确保它们在当前模型下保持 `EC=0/IL=1/ISS=0/FAR=0`，且目的寄存器或 `NZCV` 不被修改。
- [x] 已新增 `fpsimd_optional_absent_undef` 正式裸机回归，把当前模型声明 absent 的多类 optional SIMD&FP 特性在 `FPEN=开启` 时的直接执行语义固定进正式回归，覆盖 `FCMA/FHM/I8MM/DotProd/AES/PMUL/PMULL/SHA1/SHA2/SHA3/SM3/SM4/SHA512` 的代表性指令，显式锁定“直接执行仍为同步 `UNDEFINED`，而不是误执行或半实现状态”。
- [x] 已新增并修正一组 pair atomic 裸机回归，覆盖 `32-bit pair exclusive` 成功路径、`CASP` / pair-exclusive 对齐 fault，以及写权限 fault 下的“无部分提交 / status 不写回 / syndrome 正确”行为。
- [x] 已新增 `fp_ah_absent_ignored` 裸机单测，确认在当前 `ID_AA64ISAR1_EL1=0`、`!FEAT_AFP` 模型下，`FPCR.AH` 对 `FRECPE/FRECPS/FRSQRTE/FRSQRTS/FMAX` 的结果与 `FPSR` 都被正确忽略。
- [x] 已修正一处裸机回归覆盖空洞：`fpsimd_minimal` 现在显式断言当前模型下 `FPCR/FPSR` direct read/write 的掩码语义，`run_all.sh` 也不再只“运行看看”而会真正把这条失败记成回归失败。
- [x] 已新增 `sysreg_trap_iss_rt_fields`，把 `EC=0x18` system-access trap 的 syndrome 覆盖从 smoke 提升为强断言，显式锁定 `ISS` 的 `Rt/read-write/op fields` 与 `FAR_EL1`。
- [x] 已把 Linux UMP/SMP 功能回归接入 `mprotect_exec_stress`、`pthread_sync_stress` 与 `run_dmesg_stress_check` 的显式输出断言和无乱码检查。
- [x] 已新增 host 侧 `tests/linux/run_qemu_user_diff.sh`，把 `fpsimd_selftest`、`fpint_selftest`、`mprotect_exec_stress`、`pthread_sync_stress` 固化为 `qemu-aarch64` 差分验证入口。
- [x] 已新增主机侧白盒单测目标 `aarchvm_unit_cpu_cache_consistency`，直接覆盖 snapshot restore 后 predecode cache 清理、tagged upper `TLBI VAE1` / `IC IVAU` 对 canonical decode page 的失效行为，以及 `block/virtio` DMA 写 guest RAM 时对 predecode stale page 和 exclusive monitor 的统一失效，并把它接入 `tests/arm64/run_all.sh` 作为正式回归前置检查。
- [x] 已把 `fpsimd_debian_unimpl` 这条历史上用于覆盖 Debian/systemd 实际打到的 `FP/AdvSIMD` 指令族回归正式接入 `tests/arm64/run_all.sh`，避免它继续停留在“只构建不执行”的测试空洞状态。
- [x] 已把 `tests/arm64/build_tests.sh` 与 `tests/arm64/run_all.sh` 的构建/执行一致性固化为脚本自检：`run_all.sh` 现在会在开头检查所有已构建的 `.bin/.snap` 都被正式回归引用，后续若再出现“只构建不执行”的测试空洞会直接失败。
- [x] 已修正 `FCVTN/FCVTN2` 对 `BFCVTN/BFCVTN2` 的 decode overlap：当前 `ID_AA64ISAR1_EL1.BF16=0` 模型下，`BFCVTN/BFCVTN2` 不再误落到普通 `FCVTN/FCVTN2` 路径，而会按 absent-feature 语义同步 `UNDEFINED`；并新增 `bf16_absent_undef` 正式裸机回归，覆盖 `BFCVT/BFCVTN/BFCVTN2/BFMLALB/BFDOT/BFMMLA` 这几条 `BF16` 指令族在当前模型下的 `EC=0/IL=1/ISS=0/FAR=0/目的寄存器不变` 边界。
- [x] 已继续修正 `optional SIMD&FP absent helper` 的两处宽掩码 overlap：`BF16` 过滤现在额外检查 `ftype==2`，不会再把 mandatory `FCVTN/FCVTN2` 误判成 `BFCVTN/BFCVTN2`；`RDM` 过滤的 by-element vector/scalar 掩码也已收紧到真实 `SQRDMLAH/SQRDMLSH` 编码，不会再把 mandatory `SQRDMULH (by element)` 误判成 `!FEAT_RDM` 的同步 `UNDEFINED`。同时已扩展 `fpsimd_decode_overlap_regressions` 并以 `fpsimd_qdmulh_indexed_more / rdm_absent_undef / bf16_absent_undef / fp_fcvtn_flags` 连同 `tests/arm64/run_all.sh`、`tests/linux/run_functional_suite.sh`、`tests/linux/run_functional_suite_smp.sh` 完整回归锁定。
- [x] 已新增 `fjcvtzs_absent_undef` 正式裸机回归，把当前 `ID_AA64ISAR1_EL1.JSCVT=0`、`!FEAT_JSCVT` 模型下 `FJCVTZS` 的 absent-feature 语义固定进正式回归，显式锁定 `UNDEFINED` 分类、`ESR_EL1.EC=0`、`IL=1`、`ISS=0`、`FAR_EL1=0`，以及目的寄存器不被修改。
- [x] 已新增 `ls64_absent_undef` 裸机回归，把当前 `!FEAT_LS64` 模型下 `LD64B/ST64B/ST64BV/ST64BV0` 的 absent-feature 行为固定进正式回归，防止后续 generic load/store 解码调整再次把这组 64-byte single-copy atomic 指令误吞成普通 load/store。
- [x] 已新增 `lrcpc_absent_undef` 裸机回归，把当前 `!FEAT_LRCPC` 模型下 `LDAPR W/X`、`LDAPRB W`、`LDAPRH W` 的 absent-feature 语义固定进正式回归，避免后续 generic load/store 解码调整再次把它们误吞成已实现指令。
- [x] 已新增 `lrcpc2_absent_undef` / `lrcpc3_absent_undef` 裸机回归，把当前 `!FEAT_LRCPC2` / `!FEAT_LRCPC3` 模型下 `LDAPUR/STLUR`、`LDIAPP/STILP` 及 `LDAPUR/STLUR (SIMD&FP)` 的 absent-feature 语义固定进正式回归，显式锁定 `EC=0`、`IL=1`、`ISS=0`、`FAR_EL1=0`，以及“无真实访存 / 无写回 / 寄存器不被修改”的边界。
- [x] 已新增 `prfum_hint` 裸机回归，把 `PRFUM` 在当前模型下作为 prefetch hint 的 `no-op / 不产生同步异常 / 不发生 guest 可见访存` 语义固定进正式回归，防止后续 load/store 解码调整把它误吞成真实访存。
- [x] 已新增 `at_pan2_absent_undef` 裸机回归，把当前 `!FEAT_PAN2` 模型下 `AT S1E1RP/WP` 的 absent-feature 语义固定进正式回归，显式锁定 `EL1/EL0` 下的 `EC=0`、`IL=1`、`ISS=0`、`FAR_EL1=0`，以及 `PAR_EL1` 与源寄存器都不应发生修改。

### 6. 建议实施顺序

第一阶段：
- [ ] 浮点 / AdvSIMD 语义收尾
- [ ] 异常 / 系统寄存器 / trap 语义收尾

第二阶段：
- [ ] SMP 内存模型与同步原语收尾
- [ ] MMU / fault / TLB / self-modifying code 边界收尾

第三阶段：
- [ ] 建立更系统的差分与 Linux 用户态压力回归
- [ ] 在“程序可见正确性”稳定后，再继续做更激进的性能优化

### 7. 完成判据

当满足以下条件时，可认为“当前模型下 Armv8-A 强制要求的程序可见行为”已基本收敛：
- [ ] 当前 ID 寄存器声明存在的指令与系统行为，都有明确实现或明确 trap/undef 语义。
- [ ] 裸机单测不再持续发现新的高优先级 ISA / 异常 / SMP / MMU 语义缺口。
- [ ] Linux UMP / SMP 回归长期稳定，不再出现偶发 `illegal instruction`、莫名 panic、同步失效、刷屏乱码、卡死等问题。
- [ ] 进一步的性能优化已主要是结构性提速问题，而不是“边优化边暴露新语义 bug”。

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
- 已完成一轮 SoC 主循环骨架重构：
  - 用统一的 dispatch scan 一次性收集 `powered_on / active / runnable / first_runnable` 状态；
  - 单核、单 runnable SMP、多 runnable SMP 三条路径开始共用同一套运行窗口计算逻辑；
  - 去掉了单 runnable SMP 路径里重复的 `ready_to_run()` 热路径检查。
- 这一轮还没有完成“host 输入事件 / host poll deadline / guest deadline”三者统一驱动的完整主循环：
  - `maybe_sync_devices()` 与 `fast_forward_to_next_guest_event()` 仍然是过渡形态；
  - 性能测试显示 `sync_devices` / `run_chunks` 计数有所下降，但整体 `host_ns` 仍未形成稳定正收益，因此本节暂不勾选完成。
- 作为这一节的前置基础，第一阶段已经完成：
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

当前状态：
- 已完成这一步的最小闭环：
  - 当某些 CPU 处于 `WFI/WFE`，且系统里仍有其他 runnable CPU 时，等待态 CPU 已不再参与每轮 `cpu.step()`；
  - 当所有 CPU 都在等待，且存在明确的 guest timer deadline 时，SoC 已可直接把 guest 时间推进到该 deadline，再同步设备并唤醒相应 CPU。
- 仍未完全完成“真正停车”：
  - 当所有 CPU 都在等待、且当前没有 guest 侧 deadline 时，仍会保留旧的 polling fallback；
  - 这样做是为了兼容当前 `main.cpp` 基于 `soc.steps()` 的 UART/PS2 注入脚本，避免 shell/自动化输入在 guest 空闲时永远等不到注入时刻。
- 因此，后续要想把这一节彻底勾完，必须继续推进第 10 节的 `main` 层事件化，以及第 7 节的 GIC 增量唤醒路径。

### 6. SMP 调度从“每核一步”升级为“量子 + deadline”

目标：
- 避免当前 SMP 路径的“一核一步、每轮时间只加一次”的低效和失真。

任务：
- [x] 当只有一个 CPU runnable 时，复用单核长 chunk 逻辑。
- [ ] 多个 CPU runnable 时，引入可配置小量子，例如每核 32/64/128 条。
- [ ] 一个调度轮次结束后，再按该轮的 guest 时间窗口推进时间。
- [ ] 给调度器加入必要的公平性约束，避免某核长期饥饿。

预期收益：
- 提高 SMP 性能，并减少 guest 时间模型与真实执行量脱节的问题。

当前状态：
- 这一轮虽然还没有真正引入“小量子 + 公平性”调度器，但已经迈出第一步：
  - SMP 主循环现在会优先只调度 `ready_to_run()` 的 CPU，而不是无差别让等待态 CPU 一起走每轮 `step()`；
  - 这已经足以显著压缩“一个 CPU 在跑 workload、另一个 CPU 只是 idle/wait”的额外执行量。
- 当只有一个 CPU runnable 时，主线已经稳定复用长突发执行逻辑；这部分对应的性能收益已经进入当前默认实现。
- 做过一次“多个 CPU runnable 时的小量子 + 轮转公平”原型，但它会改变 `smp_timer_ppi` / `smp_timer_rate` 这类裸机 SMP timer 用例中的可观察执行顺序，因此已经回退，没有并入主线。
- 但当前实现仍未把 `WFI` CPU 变成真正的 line-driven wakeup，`GicV3::has_pending(...)` 依旧是主热点，因此这一节的大头工作仍在第 7 节。

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

## 性能优化与 JIT 路线

### 1. 基于当前代码形态的下一步性能优化方案

目标：
- 基于当前 `Cpu::step()` / `lookup_decoded()` / `translate_data_address_fast()` / `SoC::run()` 的实现形态，继续压缩真正还在热路径里的固定成本。
- 避免重复尝试已经证伪的方向，例如“单条取指侧 RAM 页缓存”这类局部指标变好、但端到端明显退化的方案。

现状判断：
- 从代码上看，当前热点已经不是单一的一层，而是三段串联：
  - `SoC::run()` 里的 runnable/waiting 判定、guest deadline 判定、设备同步边界；
  - `Cpu::step()` 中的取指后主分发与 `lookup_decoded()` 命中路径；
  - `translate_data_address_fast()` / `mmu_read_value()` / `mmu_write_value()` 的访存热链。
- 其中还有几处很具体的结构性信号：
  - `ready_to_run()` / `step()` 的等待态路径仍会直接调用 `gic_.has_pending()`；
  - `translate_data_address_fast()` 在 TLB hit 后仍会重建一个临时 `TranslationResult` 再走 `access_permitted()`；
  - `mmu_read_value()` / `mmu_write_value()` 的跨页路径仍是逐字节循环；
  - `lookup_decoded()` 现在已经是 direct-mapped + raw compare，但“命中探测”和“miss 填充”仍混在一个入口里。

建议顺序：

- [ ] P0: 先把等待态 CPU 从 `gic_.has_pending()` 主动轮询里摘掉
  - 目标：`waiting_for_interrupt_` / `waiting_for_event_` CPU 不再在 `ready_to_run()` / `step()` 里高频主动问 GIC。
  - 方案：
    - 让 SoC 维护显式的 `runnable / waiting_irq / waiting_event / halted / powered_off` 位图或计数摘要；
    - CPU 状态改变时增量更新，而不是每轮重新扫再问 GIC；
    - `WFI` CPU 仅在明确的 IRQ 可达事件、`SEV`、`PSCI CPU_ON`、外部注入等场景下回到 runnable 集合。
  - 理由：这是当前代码里最容易继续吞掉 SMP 性能的路径。

- [ ] P1: 让 `SoC::run()` 的 dispatch state 与 device deadline 真正增量化
  - 目标：减少 `inspect_cpu_dispatch_state()`、`deadline_driven_window_needed()`、`next_device_event()` 这类在外层循环里反复全量计算的成本。
  - 方案：
    - 为 CPU 电源态 / halt / wait / runnable 维护 dirty bit 与摘要；
    - 为设备调度维护真正的 cached next-deadline 结构，而不是频繁把 `device_schedule_valid_` 整体打脏后重算；
    - 将 `sync_devices()` 继续收缩到“状态变化”和“deadline 到期”两类触发。
  - 预期收益：把事件驱动从“结构已成形”推进到“外层主循环固定成本真正下降”。

- [ ] P2: 压缩 TLB hit 权限检查的固定成本
  - 目标：让 `translate_data_address_fast()` / `translate_address()` 在 TLB hit 时更接近“几次整数判断 + 地址合成”。
  - 方案：
    - 在 `TlbEntry` 中预存更直接的权限/执行检查位，避免 hit 后临时拼装完整 `TranslationResult`；
    - 将 `access_permitted()` 的 hit 热路径拆成一个更扁平的内联 helper；
    - 保留 page walk / miss 路径上的完整 `TranslationResult`，只压缩 hit 路径。
  - 设计约束：不要为了这一点破坏异常信息完整性；fault 细节仍由 miss/slow path 负责。

- [ ] P3: 将 `lookup_decoded()` 拆成“纯命中探测”与“慢路径填充”
  - 目标：命中时尽量只做 tag/raw/valid 检查，不顺带背负 miss 填充逻辑。
  - 方案：
    - 新增类似 `probe_decoded(va, pa, raw)` 的纯命中接口；
    - miss 时再走单独的 `fill_decoded_slot(...)`；
    - 继续保留当前 direct-mapped page 设计，不重新引入已经失败过的顺序流预测。
  - 预期收益：减少 `Cpu::step()` 命中常见 case 时的分支和写流量。

- [ ] P4: 为 `mmu_read_value()` / `mmu_write_value()` 增加 2-page split 快路径
  - 目标：去掉当前跨页访存逐字节循环的高固定成本，尤其是 `8B` / `16B` 临界跨页情形。
  - 方案：
    - 保留单页 `1/2/4/8` RAM 快路径；
    - 跨页时优先识别“只跨两个页”的常见情况，拆成两段而不是逐字节；
    - 两段都落在 RAM 时，直接走两次 RAM fast read/write，而不是每字节 `bus_.read/write`。
  - 设计约束：要保留当前精确 fault 行为，不能把 faulting byte 的可观察语义改坏。

- [ ] P5: 在 P2/P3/P4 之后，再重新评估 predecode 覆盖面
  - 目标：避免在 `lookup_decoded()` / MMU hit 路径还偏重时，继续盲目扩大 decoded 覆盖率。
  - 推荐方向：
    - 优先补更多“窄而专”的 load/store hot forms；
    - 避免回到 generic decoded load/store 大分派；
    - 不重新启用已经验证收益不稳的“顺序页流缓存”试验。

- [ ] P6: 维持一套明确的“该做 / 暂不做”边界
  - 当前不优先：
    - 单条取指 RAM 旁路重试；
    - 为单个 benchmark 特化的地址或指令快路径；
    - 在事件驱动和等待态模型未稳定前提前上原生 JIT。
  - 当前应优先：
    - 等待态/IRQ 唤醒模型；
    - dispatch/deadline 增量化；
    - TLB hit / decoded hit / 跨页访存三条热链的固定成本压缩。

### 1.1 基于 2026-03-18 热点复核的即时优先级

现状：
- 本轮重新执行的性能结果见：
  - `out/perf-ump-analysis-20260318-results.txt`
  - `out/perf-smp-analysis-20260318-results.txt`
- 本轮热点报告见：
  - `out/perf-ump-current-20260318.report`
  - `out/perf-smp-current-20260318.report`
  - `out/gprof-smp-current-20260318.txt`
- 当前端到端基线：
  - UMP 总 `host_ns`：`5004455734`
  - SMP 总 `host_ns`：`4543807911`
- `perf` 与 `gprof` 的结论已经比较稳定：
  - `GicV3::has_pending(...)` 不再是当前主热点；
  - 热点重新集中在 `Cpu::step()`、`translate_address()`、`lookup_decoded()`、`mmu_write_value()`、`exec_load_store()` 和 `SoC::run()`。
- 因此，当前代码下一轮性能优化不应再优先回到 GIC 轮询治理，而应先压解释器/MMU/预解码固定成本。

下一步优化顺序：
- [ ] P0: 压缩 `translate_address()` / TLB hit 权限检查热路径
  - 目标：TLB hit 时尽量只做权限位检查与地址合成，不再临时构造完整 `TranslationResult` 再回到通用判定。
  - 方案：
    - 在 `TlbEntry` 中预存更直接的权限/执行检查位；
    - 为 hit case 拆一个更扁平的 `access_permitted` 快路径；
    - miss / page walk 路径继续保留当前完整 fault 信息生成。
- [ ] P1: 将 `lookup_decoded()` 拆成 probe/fill 两段
  - 目标：decoded hit 时只做 tag/raw/valid 检查，不顺带背 miss 填充分支和写流量。
  - 方案：
    - 新增纯命中接口，如 `probe_decoded(...)`；
    - 只有 miss 时才进入独立的 fill slow path。
- [ ] P2: 为 `mmu_read_value()` / `mmu_write_value()` 增加常见 2-page split 快路径
  - 目标：去掉 `8B/16B` 跨页访存逐字节循环的固定成本。
  - 方案：
    - 保留单页 RAM 快路径；
    - 跨两页时优先走“两段 RAM 访问 + 精确 fault 处理”；
    - 仍保持 faulting byte 的程序可见行为不变。
- [ ] P3: 在 P0/P1/P2 之后再回看 `SoC::run()` 的增量化空间
  - 目标：确认 `run()` / `next_device_event()` / dispatch scan 是否已降到第二梯队以下，再决定是否继续投时间在外层调度。
  - 方案：
    - 以新的 `perf/gprof` 结果为准，避免在热点顺序已经变化后继续沿旧结论优化。
- [ ] P4: 暂不把时间投入到新的 guest 特化快路径
  - 原因：
    - 当前最贵的路径已经是通用解释器/MMU/decoded hit 成本；
    - 针对某个 workload、某段日志或某个固定地址做特化，不会形成可复用收益。

### 2. 迈向 JIT 的后续改进方案

目标：
- 保留现有解释器与慢路径，沿“predecode -> block cache -> block executor -> selective native JIT”渐进推进。
- 先让 JIT 的前置条件稳定，再引入真正的本地代码生成，避免收益被调度/同步失真吞掉。

设计原则：
- [ ] JIT 不是下一步立刻落地的第一优先级；它建立在事件驱动、等待态停车、decode 失效、TLB/代码一致性都足够稳定之后。
- [ ] 必须同时保留：
  - 原解释器慢路径；
  - 当前单条 decoded 快路径；
  - 新的 block/JIT 路径。
- [ ] 自修改代码、`IC IVAU`、`TLBI`、`TTBR/TCR/SCTLR` 切换、snapshot restore、SMP 远端失效，必须先定义好统一失效机制，再谈 JIT。

#### 2.1 第一阶段：先做块级缓存，而不是直接做原生 JIT

目标：
- 先把“每条指令都重复取指/译码/分发”的解释器结构，升级成“按 basic block 执行”的结构。

任务：
- [ ] 新增 `BlockCache`，按 `(pc, decode_context_epoch, code_page_generation...)` 建块。
- [ ] block 边界先保守处理为：
  - 直接/间接控制流；
  - 异常返回；
  - 系统/同步原语；
  - 页边界或可疑自修改区域；
  - 最大指令数上限。
- [ ] block 内容第一版不生成原生代码，只保存：
  - 原始指令序列；
  - 已解码的紧凑 block op 数组；
  - 出口类型与下一个 PC。
- [ ] 运行时先走 `execute_block()`，只有 miss/不支持时才退回单条 `step()`。

理由：
- 这一步已经能把取指、单条 decode、主分发的固定成本显著搬出热循环；
- 同时还能复用当前 predecode 与失效逻辑，是最稳妥的“JIT 前置闭环”。

#### 2.2 第二阶段：把 block executor 做成更轻的中间层

目标：
- 在还不生成宿主机原生代码的前提下，尽量接近 JIT 的收益结构。

任务：
- [ ] 为 block op 设计比当前 `DecodedInsn` 更紧凑的内部表示，优先覆盖：
  - 热整数族；
  - 热 branch 族；
  - 热 `LDR/STR [Xn,#imm]`；
  - 常见系统指令边界。
- [ ] 让 block 内部直接使用专门化 helper，而不是再回到 `exec_data_processing()` / `exec_load_store()` 大分发。
- [ ] block 内增加显式 safepoint：
  - 中断检查；
  - 步数预算；
  - stop-on-pattern；
  - 设备 deadline 边界。
- [ ] 第一版 block chaining 只做 fall-through / 直接分支，不急着做 trace。

预期收益：
- 即使不做 native JIT，也可以先验证“块级执行模型”本身的正确性和收益。

#### 2.3 第三阶段：为 native JIT 准备统一失效与守卫模型

目标：
- 在真正生成宿主机代码前，把最容易出错的正确性基础先收束好。

任务：
- [ ] 引入更明确的代码页 generation/version 机制：
  - `on_code_write()` 增量 bump；
  - `IC IVAU` / `IC IALLU*` 触发相应失效；
  - `TLBI` / `TTBR/TCR/SCTLR` 改写触发上下文级失效。
- [ ] 为 block / JIT entry 定义统一 guard：
  - 起始 `pc`
  - `decode_context_epoch`
  - 涉及代码页 generation
  - 必要时的 ASID / MMU 配置摘要
- [ ] snapshot restore 时清空 block/JIT cache，只保留基础解释器状态。
- [ ] SMP 下保证远端可见的代码失效能传播到所有 CPU 的 block/JIT cache。

设计判断：
- 这一步做扎实后，后续无论是 block interpreter 还是 native JIT，都会安全得多。

#### 2.4 第四阶段：selective native JIT

目标：
- 只对最热、最稳定、最容易守卫的子集生成宿主机原生代码，不追求一步覆盖全 ISA。

任务：
- [ ] 第一批 native JIT 仅覆盖：
  - 常见整数 ALU；
  - 常见 branch；
  - 专门化 `LDR/STR [Xn,#imm]`；
  - 不含异常 side effect 的简单 flag 计算。
- [ ] 遇到以下情况立即 deopt / exit 回解释器：
  - 未覆盖指令；
  - 系统指令；
  - 复杂异常路径；
  - MMIO 或不稳定访存；
  - block guard 失效。
- [ ] 后端建议分两步：
  - 先做一个非常小的 host-code emitter，只支持固定模板；
  - 后续再考虑寄存器分配、更 aggressive 的块内优化。
- [ ] 先不碰：
  - FP/AdvSIMD native JIT；
  - SVE/SME；
  - 多线程并行执行。

#### 2.5 第五阶段：验证、调试与上线策略

目标：
- 让 block/JIT 路径可验证、可回退，而不是形成新的黑盒。

任务：
- [ ] 新增执行模式开关，例如：
  - `-exec interp`
  - `-exec block`
  - `-exec jit`
- [ ] 增加“抽样对拍”模式：
  - 同一 block 先跑 JIT，再周期性与解释器结果比对；
  - 出现不一致时自动回退并打印 block 信息。
- [ ] 新增专门测试：
  - 动态生成代码 / 自修改代码；
  - `IC IVAU` / `TLBI` / `FENCE.I` 类缓存失效；
  - 信号/异常返回；
  - SMP 下远端代码更新；
  - snapshot restore 后 block/JIT cache 失效。
- [ ] JIT 的默认上线顺序建议为：
  - 先默认 `interp`
  - 然后默认 `block`
  - 最后 `jit` 保持显式 opt-in，直到回归与性能都稳定。

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

## 宿主一致时钟外设设计

目标：
- 为 guest 提供一个与宿主系统 wall clock 保持一致的“时间-of-day”外设。
- 不改变当前 `GenericTimer/CNTVCT` 的 guest-time 语义，避免把 wall clock 与内核调度时钟混在一起。
- 保持现有外设风格一致：
  - SoC 固定 MMIO 窗口；
  - `Device` 子类；
  - 可选 DT 暴露；
  - 独立 `save_state/load_state`；
  - 需要时接 GIC 中断；
  - 首版不进 `BusFastPath`。

设计判断：
- [x] 首选做一个 `PL031` 风格的 RTC，而不是自定义奇特寄存器协议。
  - 原因：这和当前 `PL011/PL050/GICv3/simple-framebuffer` 的风格最一致；
  - Linux / U-Boot 都更容易直接复用已有驱动；
  - 设备职责清晰，就是“宿主一致的 wall clock / RTC”，不承担高频 timer tick 职责。
- [x] 明确把“宿主一致 wall clock”与“架构定时器 / guest 时间”分层：
  - `GenericTimer` 继续只表示 guest 可控、可回放的架构计数器；
  - 新 RTC 只负责日期/时间-of-day；
  - 不把 Linux scheduler、delay loop、clocksource 直接迁移到该 RTC。
- [ ] 明确该 RTC 的实现原则是“程序可见正确性优先于体系结构精确回放”：
  - RTC 对 guest 提供的是“当前时间”语义；
  - 默认不追求 snapshot 后 wall clock 严格可重放；
  - 如需确定性测试，应通过单独模式显式选择，而不是污染默认行为。

### 0. 总体架构原则

目标：
- 先把这个设备与现有时间体系、SoC 调度体系以及 snapshot 体系的边界定义清楚。

任务：
- [ ] 明确时间域分层：
  - `GenericTimer/CNTPCT/CNTVCT` 属于 guest-time / architectural timer 域；
  - 新 RTC 属于 host-derived wall-clock / time-of-day 域；
  - 两者共享同一套 MMIO/快照框架，但不共享时间推进语义。
- [ ] 明确 SoC 中的放置方式：
  - RTC 仍是普通 `Device`；
  - 由 `SoC` 统一创建、map、保存/恢复状态；
  - 不绕开当前 bus / GIC / snapshot 框架实现“特权旁路”。
- [ ] 明确首版不做的事：
  - 不替代 generic timer；
  - 不参与 TLB / decode hot path；
  - 不为了 RTC 引入新的 guest ABI；
  - 不把 wall clock 读数塞进 CPU system register。
- [ ] 在设计稿里显式区分三种模式：
  - 默认 `realtime`：对齐宿主当前 wall clock；
  - 可选 `frozen`：固定在 snapshot/启动时刻推进或保持不动，只用于测试；
  - 可选 `mock=<epoch>`：测试专用伪时钟源。

### 1. 外设模型与寄存器风格

目标：
- 让该设备在代码结构上与当前其它 MMIO 外设保持同一种实现模式。

任务：
- [ ] 新增 `HostRtcPl031 final : public Device`，接口只暴露标准 `read/write`。
- [x] SoC 为它分配固定 MMIO 窗口，建议复用当前空洞地址：
  - `base = 0x09030000`
  - `size = 0x1000`
- [x] 首版只走 bus 慢路径，不放进 `BusFastPath`：
  - RTC 访问频率低；
  - 读路径可能调用宿主机时间 API；
  - 没必要把这类低频、强外部依赖设备塞进热路径。
- [x] DT 风格优先采用标准节点：
  - `compatible = "arm,pl031", "arm,primecell"`
  - `reg = <...>`
  - `interrupts = <...>` 作为后续 alarm 扩展保留
  - PrimeCell ID / cell-id 也按 `PL031` 风格返回。
- [ ] 明确首版内部状态字段：
  - `offset_seconds`：guest 可见时间相对宿主 wall clock 的偏移；
  - `match_seconds`：alarm compare 值；
  - `imsc`：中断 mask；
  - `raw_pending`：原始 pending；
  - `lr_shadow`：最近一次写入 `LR` 的值；
  - `cr_shadow`：控制寄存器可见值；
  - 必要时额外保留 `last_sampled_seconds` 仅用于调试，不作为架构状态。

#### 1.1 PL031 兼容寄存器 ABI

目标：
- 把 guest 可见寄存器布局写成实现时几乎可直接照着落地的程度。

任务：
- [x] 约定首版寄存器布局如下：
  - `0x000 DR`：Data Register，`RO`，返回当前 guest-visible RTC 秒值的低 32 位。
  - `0x004 MR`：Match Register，`RW`，保存 alarm compare 秒值。
  - `0x008 LR`：Load Register，`RW`，写入时重设 guest-visible 当前秒值；读取返回最近一次写入值。
  - `0x00c CR`：Control Register，首版只保留 `bit0` 可见；默认上电为启用态。
  - `0x010 IMSC`：Interrupt Mask Set/Clear，`RW`。
  - `0x014 RIS`：Raw Interrupt Status，`RO`。
  - `0x018 MIS`：Masked Interrupt Status，`RO`。
  - `0x01c ICR`：Interrupt Clear Register，`WO`，写任意值清 `raw_pending`。
  - `0xfe0..0xffc`：`PeriphID[0..3]`、`PCellID[0..3]`，按 PL031 风格返回固定值。
- [ ] 明确首阶段对保留位采用 `RAZ/WI`：
  - 不支持的位读零；
  - 写入忽略；
  - 避免 guest 因未定义垃圾位读出而误判设备能力。
- [ ] 明确读写宽度策略：
  - 首版仅保证 32-bit aligned `read/write` 为主路径；
  - 非法宽度或跨寄存器访问按照当前设备风格返回 bus error / `RAZ/WI`，需要在实现阶段与现有 MMIO 约定统一。
- [ ] 明确 `CR` 首版语义：
  - 复位后 `bit0 = 1`；
  - 写 `1` 保持启用；
  - 写 `0` 首版可以 `WI`，前提是 Linux/U-Boot 不依赖停表行为；
  - 若后续发现驱动会实际停表，再补全 disable 语义。

#### 1.2 PrimeCell 与 DT 暴露

目标：
- 避免设备本体实现完后，才发现 Linux/U-Boot DTS 绑定方式不匹配。

任务：
- [ ] DTS 节点草案：
  - `rtc@9030000`
  - `compatible = "arm,pl031", "arm,primecell"`
  - `reg = <0x0 0x09030000 0x0 0x1000>`
  - `interrupt-parent = <&gic>`
  - `interrupts = <GIC_SPI ... IRQ_TYPE_LEVEL_HIGH>` 预留 alarm 用
  - 如 Linux `pl031` 驱动需要 `apb_pclk`，则再配一个最小 `fixed-clock` 节点，不在第一阶段强行加入。
- [ ] U-Boot / Linux 集成策略：
  - 默认不改当前通用 DTB；
  - 使用专用 DTS 或 overlay 启用；
  - 这样不会把 wall-clock 非确定性引入所有现有回归。

### 2. 时间语义

目标：
- 精确定义“与宿主系统保持一致”到底是什么意思，避免后续实现时歧义。

任务：
- [ ] 把该设备定义为“宿主 wall clock 设备”，默认对齐宿主 `CLOCK_REALTIME`。
- [ ] 设备返回的是“当前宿主时间 + guest 软件通过 RTC load/set-time 写入形成的偏移”，而不是 guest-time 推进值。
- [ ] 明确 guest 写 RTC 时间时的语义：
  - 不是停止或重置宿主时间；
  - 而是更新“guest-visible RTC 相对宿主时间的 offset”。
- [ ] 明确首版只保证“读取时间-of-day 与宿主一致”，不保证：
  - 周期级可重放；
  - 与 snapshot 时间冻结一致；
  - 高精度 monotonic 基准。

设计判断：
- [ ] 首版只承诺 `seconds` 级 wall clock 语义，符合 `PL031` 定位。
- [ ] 如果后续确实需要纳秒级 `realtime/monotonic raw`，再加第二个 paravirt host-time 设备，不污染 RTC ABI。

#### 2.1 时间计算公式

目标：
- 把读写 RTC 时的数值语义写成明确公式。

任务：
- [x] 默认 `realtime` 模式下定义：
  - `host_now_sec = floor(CLOCK_REALTIME_ns / 1_000_000_000)`
  - `guest_rtc_sec = host_now_sec + offset_seconds`
- [ ] `DR` 读取返回：
  - `guest_rtc_sec & 0xffffffff`
  - 内部仍建议使用有符号 64 位保存 `offset_seconds`，避免设定过去时间时溢出。
- [ ] `LR = new_sec` 写入语义：
  - 先采样当前 `host_now_sec`；
  - 再设置 `offset_seconds = (int64_t)new_sec - (int64_t)host_now_sec`；
  - `lr_shadow = new_sec`；
  - 因此写完后紧接一次 `DR` 读取应立即得到 `new_sec` 或最多相差 1 秒边界。
- [ ] `MR = match_sec` 写入只更新匹配目标，不改变当前时间。
- [ ] `CR` 首版不改变 wall clock 公式，只影响“设备是否认为 alarm 逻辑有效”。

#### 2.2 程序可见语义边界

目标：
- 避免把本不该由 RTC 保证的语义混进去。

任务：
- [ ] 明确 RTC 返回的是 UTC epoch 秒，不带时区概念：
  - 时区转换由 guest 用户态/内核处理；
  - 不在设备内部维护 timezone/DST。
- [ ] 明确不单独模拟 leap second：
  - 直接继承宿主 `CLOCK_REALTIME` 的表现；
  - 如果宿主做 leap smear 或时间步进，guest 看到的 RTC 也随之变化。
- [ ] 明确 `PL031` 是秒级设备：
  - 不承诺亚秒精度；
  - 不保证单条 guest 指令之间时间严格单调增长；
  - 同一秒内重复读取相同值是预期行为。
- [ ] 明确 32 位秒值外露的边界：
  - guest-visible ABI 保持 `PL031` 风格的 32 位；
  - 若未来需要跨 2106 或纳秒级高精度，使用独立新设备而不是修改 RTC ABI。

#### 2.3 宿主时间抽象

目标：
- 在不破坏默认“宿主一致 wall clock”语义的前提下，为单元测试和 deterministic 场景留出实现空间。

任务：
- [ ] 设计一个最小 host clock 抽象层，仅供 RTC 使用：
  - `now_realtime_ns()`
  - 可选 `now_monotonic_ns()` 仅供 alarm/调度辅助
- [ ] 首版实现形态规划：
  - 生产环境使用 `RealHostClock`
  - 单元测试使用 `MockHostClock`
  - snapshot/回归若需确定性，可引入 `FrozenHostClock`
- [ ] 明确这个抽象层只服务 RTC / host-deadline 设备：
  - 不反向改写 `GenericTimer`
  - 不把整个 SoC 的 guest 时间推进机制改成 wall clock 驱动

#### 2.4 交互式架构 timer host 模式

目标：
- 在不破坏当前回归/性能可重复性的前提下，让交互式 Linux 场景中的 `CLOCK_REALTIME/CLOCK_MONOTONIC` 以宿主机节奏推进。

任务：
- [x] 保留默认 `step` 架构 timer 模式，继续服务现有回归与性能基线。
- [x] 为 `GenericTimer` 增加可选 `host` 模式，使 `CNTVCT/CNTPCT` 改为跟随宿主机 monotonic 时钟推进。
- [x] 在 SoC 主循环里为 `host` 模式使用更短的设备同步窗口，避免本地 timer IRQ 因长 chunk 被明显拖后。
- [x] 为交互/GUI 恢复脚本默认启用 `host` 模式，同时保持自动化回归脚本继续使用 `step` 模式。
- [x] 增加 Linux 用户态 `time_rate_smoke`，用 RTC 对比 `CLOCK_REALTIME/CLOCK_MONOTONIC` 的流速，验证交互模式下时间不再明显跑飞。

### 3. 快照与恢复语义

目标：
- 让该设备的 host-time 特性与当前 snapshot 体系不冲突。

任务：
- [ ] 明确 snapshot 保存哪些状态：
  - `match/alarm`
  - `mask`
  - `pending`
  - `load-offset`
  - 控制寄存器
- [ ] 明确 snapshot 不保存哪些状态：
  - 宿主机绝对时间值本身
  - “保存快照时的 host realtime 秒数”
- [x] `load_state()` 时重新采样当前宿主时间，并按保存下来的 offset 重建 guest-visible RTC。
- [ ] 文档中显式声明：
  - 这是一个“非严格可重放”的外设；
  - 恢复旧快照后，RTC 读数会跳到“当前宿主时间 + 已保存 offset”，而不是回到保存快照那一刻。

设计判断：
- [ ] 默认接受这种“恢复后 wall clock 前跳”的行为，因为它更符合 RTC/实时时钟的直觉。
- [ ] 如后续测试需要严格复现，再额外提供 `frozen/mock` 模式，而不是把默认行为做成不可预期的折中态。

#### 3.1 快照字段清单

目标：
- 明确哪些字段属于设备状态，哪些属于运行环境，不要在实现时混淆。

任务：
- [ ] snapshot 保存以下 RTC 设备状态：
  - `offset_seconds`
  - `match_seconds`
  - `imsc`
  - `raw_pending`
  - `lr_shadow`
  - `cr_shadow`
- [ ] snapshot 不保存以下内容：
  - 宿主绝对 `CLOCK_REALTIME` 秒值；
  - 保存快照时的 wall-clock 文本时间；
  - 任何“为了让恢复后回到旧世界线”的隐式补偿值。
- [ ] 如 alarm 在保存时已 pending，恢复后应保留 `raw_pending`，避免 guest 丢中断状态。

#### 3.2 恢复语义

目标：
- 把“为什么恢复后会前跳”定义成正式语义，而不是实现副作用。

任务：
- [ ] `load_state()` 时流程定义为：
  - 读取保存的逻辑状态；
  - 重新采样当前宿主 `CLOCK_REALTIME`；
  - 使用保存的 `offset_seconds` 重建 `guest_rtc_sec`；
  - 如 alarm 已实现，再根据新时间与 `match_seconds` 重算 `raw_pending` 或立即置位。
- [ ] 文档中明确说明：
  - 恢复旧 snapshot 后，`DR` 返回的是“当前宿主 wall clock + 已保存偏移”；
  - 因此 wall clock 可见值通常会比保存快照时更大；
  - 这不是 bug，而是 RTC 设备语义本身。
- [ ] 若 future test 需要“恢复回旧时刻”，只通过 `frozen/mock` 模式解决，不改变默认 snapshot 语义。

### 4. 中断与事件驱动衔接

目标：
- 让该外设能自然接入现有 SoC/GIC/事件驱动风格，但不一次把复杂度全拉满。

任务：
- [x] 第一阶段先做“可读、可设时、可快照”的 RTC，本身不作为高频事件源。
- [ ] 第二阶段再补 `PL031` alarm / match / IRQ：
  - `MIS/RIS/ICR`
  - GIC line
  - 与 DT 中断声明对齐
- [ ] 若实现 alarm，需要把它接入“host deadline”而不是“guest tick deadline”：
  - RTC alarm 比较的是 host-consistent wall time；
  - 不能简单复用当前 `guest_now` deadline 逻辑。
- [ ] 为事件驱动化预留单独的 host-deadline 通道：
  - `next_host_deadline_ns()`
  - `poll_host_time(now_ns)`
  - 或统一塞进未来的 host/guest 双时间域调度器。

设计判断：
- [ ] 在 host-deadline 通道未设计好之前，不急着把 RTC alarm 做成完整中断设备。
- [ ] 首版先把“正确、简单、Linux 可见”放在第一位，不为一个低频外设提前扭曲主循环。

#### 4.1 Alarm 语义细化

目标：
- 提前把后续 alarm/IRQ 的行为定义清楚，避免第一阶段结束后返工。

任务：
- [ ] 定义 alarm 触发条件：
  - 当 `guest_rtc_sec >= match_seconds` 时，`raw_pending = 1`
  - 若 `imsc = 1` 且 `cr_shadow.bit0 = 1`，则向 GIC 拉高 RTC IRQ 线
- [ ] `RIS/MIS/ICR` 语义：
  - `RIS` 反映 `raw_pending`
  - `MIS = RIS & IMSC`
  - `ICR` 写任意值清 `raw_pending`
- [ ] 明确 alarm 为单次 compare，不做 periodic 模式：
  - `PL031` 本身是简单 RTC + match；
  - 周期 tick 应继续由 generic timer 或未来独立设备承担。
- [ ] 明确如果 guest 把 `MR` 设到过去：
  - 写后下一次状态刷新即可立刻 pending；
  - 不要求再等一秒。

#### 4.2 事件驱动接入方向

目标：
- 让 RTC 后续能自然接入当前 SoC 的事件驱动演进，而不是走回“每步轮询所有设备”。

任务：
- [ ] 为低频 host-time 设备预留独立调度接口：
  - `next_host_deadline_ns() -> optional<uint64_t>`
  - `poll_host_time(now_ns)`
- [ ] 设计 SoC 层的双时间域调度原则：
  - guest-time 设备继续用 `guest_tick` 驱动；
  - host-time 设备用 `host monotonic/realtime deadline` 驱动；
  - 主循环取两者中最近的一个事件作为下一次同步边界。
- [ ] 明确第一阶段即使未做 alarm，也不要把 RTC 特判塞进主循环：
  - 设备本体先只提供正确 MMIO 语义；
  - 等 host-deadline 框架成熟后再接 IRQ。

### 5. 与 Linux / U-Boot / 现有测试体系的关系

目标：
- 让新设备的接入方式和现有工程的使用方式保持一致，不引入无谓不确定性。

任务：
- [ ] 默认不把该 RTC 节点塞进当前所有通用测试 DTB。
- [ ] 采用“专用 DTS/overlay 或显式开关”方式启用：
  - 避免现有回归因为 wall clock 非确定性受影响；
  - 也避免 guest 自动把它选成默认时钟源后改变启动路径。
- [ ] 如果后续启用 Linux 支持，优先让它作为 RTC / time-of-day 来源，而不是 clocksource。
- [ ] 文档中说明：
  - 何时建议启用该设备；
  - 它和 `GenericTimer/CNTVCT` 的职责区别；
  - 它会让 snapshot 恢复后的 wall clock 体现当前宿主时间。

#### 5.1 启用策略

目标：
- 避免 RTC 一落地就破坏现有确定性测试流程。

任务：
- [ ] 默认行为设计为：
  - 不启用 RTC 节点；
  - 不额外改变 Linux cmdline；
  - 不让当前 functional/perf/snapshot 脚本自动引入 wall-clock 变数。
- [ ] 单独提供：
  - 专用 DTS 或 DT overlay
  - 专用 Linux 用户态测试脚本
  - 必要时单独的 initramfs case
- [ ] 若 Linux 自动把 `pl031` 当作 RTC 驱动加载，应检查但不强迫其成为 clocksource。

#### 5.2 Linux / U-Boot 验证项

目标：
- 提前写明接入成功时应该观察到哪些现象。

任务：
- [ ] U-Boot 侧验证：
  - 设备节点可枚举；
  - MMIO 读 `DR` 有合理秒值；
  - 如 U-Boot 已启用 RTC 命令，可额外验证 `date` / `rtc` 相关命令。
- [x] Linux 侧验证：
  - `/sys/class/rtc/rtc0/` 出现；
  - `cat /sys/class/rtc/rtc0/since_epoch` 或 `hwclock -r` 能读到接近宿主的秒值；
  - `date -u +%s` 与 host 对比误差在预期范围内。
- [ ] snapshot 恢复验证：
  - 保存快照前读取一次 RTC；
  - 恢复后再次读取；
  - 验证其前跳符合“当前宿主时间 + offset”的设计，而不是冻结在旧值。

### 6. 后续可选扩展

目标：
- 给未来更强的“宿主一致时间能力”留出生长空间，但不把第一版做复杂。

任务：
- [ ] 可选增加一个单独的高精度 host-time 设备：
  - `realtime_ns`
  - `monotonic_ns`
  - `boot_id / clock_id / validity`
- [ ] 可选增加 `AARCHVM_HOST_CLOCK_MODE`：
  - `realtime`
  - `frozen`
  - `mock=<epoch>`
- [ ] 可选为 snapshot / 自动化测试补一个“固定宿主时间源”的测试模式。

#### 6.1 推荐分阶段落地顺序

目标：
- 把这个设备拆成几个风险可控的最小闭环，便于后续真正实现时按阶段推进。

任务：
- [ ] 第一阶段：只做可读 wall clock
  - `DR/LR/CR` 最小闭环
  - `offset_seconds` 语义
  - `save_state/load_state`
  - 裸机 MMIO 单元测试
- [x] 第二阶段：补齐 PL031 基础寄存器可见面
  - `MR/IMSC/RIS/MIS/ICR`
  - PrimeCell ID
  - DTS 节点与 Linux 枚举
- [ ] 第三阶段：接入 alarm IRQ
  - host-deadline 调度接口
  - GIC 路由
  - Linux/U-Boot alarm 行为验证
- [ ] 第四阶段：补 deterministic/test 模式
  - `frozen/mock`
  - snapshot 精确测试
  - 回归脚本接入

#### 6.2 测试计划细化

目标：
- 在设计阶段就把后续需要覆盖的行为写成清单，防止实现后漏测。

任务：
- [ ] 裸机单元测试：
  - `DR` 读取非零且单调不减；
  - 写 `LR` 后 `DR` 立即贴近写入值；
  - 写过去/未来时间都能正确更新 `offset_seconds`；
  - snapshot 恢复后 `DR` 前跳但保持先前设定偏移。
- [x] Linux 用户态测试：
  - `hwclock -r` / `cat /sys/class/rtc/rtc0/since_epoch`
  - 与宿主 `date +%s` 做秒级容差比较
  - 写 RTC 后再次读取，验证 guest-visible offset 生效
- [ ] Alarm 测试：
  - 设 `MR` 到未来 1~2 秒，验证中断到达；
  - 设 `MR` 到过去，验证立即 pending；
  - `ICR` 清除后 `MIS` 恢复为 0。
- [ ] Snapshot 测试：
  - `realtime` 模式验证“恢复后前跳”；
  - `frozen/mock` 模式验证“恢复后可精确复现”。

## 隔离式 `.so` 外设扩展机制

目标：
- 让模拟器可以按配置加载一个外设扩展 `.so`，并把该外设放入独立子进程运行，以获得地址空间隔离。
- 主模拟器继续作为 guest 物理地址空间、bus 映射表、guest 时间与中断控制器的权威拥有者。
- 外设扩展通过稳定的 C ABI SDK 与主模拟器通信，SDK 放在仓库内 `sdk/` 目录，作为独立 CMake 项目维护，尽量避免 SDK 因内部重构而频繁变化。
- 插件产物以独立 `.so` 形式构建，由子进程通过 `dlopen()` 动态加载；主项目不通过 `-l` 在链接期把插件拉进 `aarchvm`。
- 当前方案明确不要求外设扩展支持 snapshot；只要启用了这类外设，`-snapshot-save/-snapshot-load` 应显式拒绝并给出清晰错误，而不是保留半套语义。
- 第一阶段优先保证正确性、可扩展性与隔离性；性能优化留给后续阶段，不为了早期性能破坏 API 稳定性。

当前进展：
- [x] 已完成 `sdk/` 独立 CMake 边界与最小 SDK：公共 ABI 头、`AarchvmPlugin.cmake`、`register_bank` 示例插件、ABI smoke 坏插件样例均已落地。
- [x] 已完成 MMIO-only MVP 主链路：`-plugin` 配置解析、`socketpair(AF_UNIX, SOCK_SEQPACKET)+fork()+dlopen()+dlsym()` 子进程装载、HELLO/RESET/MMIO/SHUTDOWN 基础协议，以及父进程 `ExternalDeviceProxy` MMIO 代理已接入主项目。
- [x] 已把“启用插件即拒绝 `-snapshot-load/-snapshot-save`”做成显式错误路径，避免留下半套 snapshot 语义。
- [x] 已补宿主侧插件单测覆盖 `subword MMIO`、`reset` 与非法 MMIO fault：`aarchvm_unit_external_plugin` 现在会验证 `ExternalDeviceProxy` 的字节/半字/字写入拼接、`reset()` 清零，以及插件返回 fault 后的代理故障状态与日志路径。
- [x] 已把外设 `RESET` 真正接入启动与 `SoC::reset()` 路径：宿主在 attach 后会先做一次显式 reset，后续 `SoC::reset()` 也会向所有外部插件广播 reset；新增 `runtime_reset` 测试插件与 `plugin_reset_on_boot` 裸机回归锁定这一点。
- [ ] IRQ、DMA、deadline/guest-time 同步当前仍只保留 ABI/设计占位，宿主回调统一返回 `unsupported`，`irq=` 配置也会显式拒绝。
- [x] 裸机 `plugin_mmio_register_bank` 与 `plugin_mmio_register_bank_subword` 已接入 `tests/arm64` 脚本，并已在 `workspace` 容器内随 `tests/arm64/run_all.sh` 实际通过完整回归，覆盖 64-bit roundtrip、subword 读写拼接、独立 scratch 寄存器与 hole 区读零语义。

### 0. 设计约束

目标：
- 先把边界和不变式写清楚，避免后续一边实现一边改 ABI。

任务：
- [x] 明确外设扩展的基本边界：
  - 主模拟器拥有 guest RAM、bus 映射表、GIC/IRQ 路由、guest 时间与设备调度总控；
  - 外设插件不直接持有主模拟器进程内的任何裸指针；
  - 插件只能通过 IPC 请求访问 guest 物理地址空间、发起中断、申请未来 deadline。
- [x] 明确与当前代码结构的接缝：
  - 当前 `Device` 抽象只有 `read/write`，第一版不把 deadline、reset、IRQ、故障状态一口气做成“所有设备都必须实现”的统一虚接口；
  - 第一版通过 `ExternalDeviceProxy + SoC::external_devices_` 单独管理外部设备的 deadline、IRQ 电平、健康状态与配置，避免无谓改动现有内建设备。
- [x] 明确第一阶段不追求的事情：
  - 不把插件 MMIO 纳入当前 `BusFastPath`；
  - 不给插件暴露主模拟器内部 C++ 类型；
  - 不允许插件依赖主模拟器内部头文件直接访问 `Bus`/`SoC`/`Device` 私有实现；
  - 不要求自动生成 Linux DTB 节点或一开始就支持 Linux 枚举；
  - 不支持 snapshot；
  - 不允许插件自建按 wall clock 运行的宿主线程式设备时钟。
- [ ] 明确故障模型：
  - 插件子进程崩溃、死循环、非法内存访问，不应直接破坏主模拟器地址空间；
  - 主模拟器检测到插件失联后，应把该设备标记为故障设备，并给出明确日志；
  - 后续是否把故障映射为 guest 外设超时、总线错误或直接终止模拟器，需要作为策略项单独定义；
  - 启用插件时如请求 `-snapshot-save/-snapshot-load`，应在宿主入口处尽早失败，而不是让快照路径默默忽略插件。

### 1. `sdk/` 目录与构建边界

目标：
- 先把 SDK 作为独立产物边界定清楚，保证插件确实是“独立 `.so` + `dlopen()`”而不是伪插件。

任务：
- [x] 明确 `sdk/` 是仓库内独立 CMake 项目：
  - 形如 `cmake -S sdk -B out/sdk-build` 可单独配置与构建；
  - 不要求主项目根 `CMakeLists.txt` 把 `sdk/` 直接 `add_subdirectory()` 进默认构建；
  - 主项目可后续提供脚本或 CI 任务去调用 `sdk/` 构建，但两者链接边界保持解耦。
- [ ] 明确推荐目录结构：
  - `sdk/CMakeLists.txt`
  - `sdk/include/aarchvm-plugin-sdk/*.h`
  - `sdk/cmake/AarchvmPlugin.cmake`
  - `sdk/examples/register_bank/`
  - `sdk/examples/timer_doorbell/`
  - `sdk/examples/dma_copy/`
  - `sdk/tests/abi_smoke/`
  - `sdk/README.md`
- [ ] 明确插件产物形态：
  - 示例与第三方插件都应以 `add_library(<name> MODULE ...)` 方式生成 `.so`；
  - 插件只包含 SDK 头文件，最多链接 `sdk/` 自己提供的小型 helper library；
  - 插件不能在构建期依赖主项目里的 `aarchvm` 可执行文件或内部 C++ 对象。
- [x] 明确宿主与 SDK 的边界：
  - 主项目负责运行时加载、IPC、MMIO 代理、GIC 路由、guest 时间与 DMA 仲裁；
  - `sdk/` 只负责 ABI 头文件、CMake 模板、示例插件和最小测试工具；
  - “示例插件能成功生成 `.so`”应成为后续测试闭环的一部分。

### 2. 进程模型与生命周期

目标：
- 用 `fork()` 创建外设子进程，并把插件逻辑限制在子进程内执行。

任务：
- [x] 设计外设配置入口，例如：
  - `-plugin /path/to/device.so,mmio=0x...,size=0x...,irq=...[,arg=...]`
  - 或配置文件中声明插件路径、实例名、地址窗口、IRQ 路由与自定义参数；
  - 允许重复传入多个 `-plugin`，每个实例对应独立子进程。
- [x] 设计“父进程不直接执行插件业务逻辑”的启动流程：
  - 父进程解析命令行与配置；
  - 父进程创建 `socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC)`；
  - 父进程 `fork()`；
  - 子进程关闭不需要的 fd，并以 `RTLD_NOW | RTLD_LOCAL` 执行 `dlopen()`；
  - 子进程 `dlsym()` 获取 `aarchvm_plugin_get_api_v1()` 并完成初始化；
  - 子进程把插件 manifest/能力集通过 IPC 发送给父进程；
  - 父进程校验 ABI 版本、MMIO 窗口、IRQ 声明后，再把一个 proxy device 映射到 bus。
- [ ] 明确为什么不在父进程里直接 `dlopen()+调用插件`：
  - 这样能避免插件初始化代码、全局构造、越界写等问题污染主进程；
  - 真正的设备业务逻辑只在子进程执行，更符合隔离目标；
  - 也能明确做到“插件 `.so` 是运行时可选项，而不是 `aarchvm` 的链接期依赖”。
- [ ] 为插件实例定义完整生命周期：
  - `spawn`
  - `handshake`
  - `reset`
  - `running`
  - `quiesce`
  - `shutdown`
  - `crashed/timeout`

设计判断：
- [x] 第一版优先采用“一个插件实例对应一个子进程”的模型，而不是多个插件共享一个 worker 进程。
- [x] 第一版优先采用 `fork()` 而不是 `fork()+exec()`：
  - 实现路径更短；
  - 但后续可以把 `exec()` 隔离与 seccomp 作为加强版安全选项加入。

### 3. IPC 传输层与基础协议

目标：
- 用一套稳定、可扩展、可调试的协议承载 MMIO、DMA、IRQ、时间同步与快照操作。

任务：
- [x] 第一版 IPC 传输层使用 `AF_UNIX + SOCK_SEQPACKET`：
  - 保证消息边界；
  - 便于调试；
  - 不需要自己处理流分帧。
- [x] 设计统一消息头：
  - magic
  - ABI major/minor
  - 消息类型
  - 请求序号
  - flags
  - instance id
  - `guest_now`
  - payload 长度
  - 可选状态码
  - 采用固定宽度整数与约定字节序，避免把宿主原生结构体直接跨进程裸发。
- [ ] 定义最小消息类型集合：
  - `HELLO`
  - `MMIO_READ_REQ/RESP`
  - `MMIO_WRITE_REQ/RESP`
  - `DMA_READ_REQ/RESP`
  - `DMA_WRITE_REQ/RESP`
  - `IRQ_SET`
  - `IRQ_PULSE`
  - `ADVANCE_TO`
  - `SET_DEADLINE`
  - `RESET`
  - `SHUTDOWN`
  - `LOG`
  - `ERROR`
- [ ] 所有请求都带 `guest_now` 或与之可对齐的时间戳，避免插件使用独立 wall clock 形成时间漂移。
- [ ] 为每类 IPC 明确同步/异步语义：
  - MMIO 访问默认同步请求-应答；
  - IRQ 通知允许异步单向消息；
  - 时间推进与 reset 可同步化，保证主模拟器始终掌控 guest 时间。
- [ ] 明确等待与超时实现：
  - 父进程对同步请求使用 `poll/ppoll` 等待响应；
  - 每类请求定义独立超时预算；
  - 超时后进入统一的故障处理分支，而不是让 `Bus::read/write` 永久阻塞。

设计判断：
- [ ] 第一阶段优先做“协议正确、可观测、易调试”的同步 IPC；
- [ ] 性能优化阶段再考虑：
  - 批量 MMIO
  - 共享内存环形队列
  - `memfd` + doorbell
  - 零拷贝 DMA window

### 4. 宿主侧接入与 MMIO 代理机制

目标：
- 让主模拟器像访问内建设备一样访问插件暴露的 MMIO 地址范围，但实际请求转发到子进程。

任务：
- [x] 明确推荐新增的宿主侧模块：
  - `plugin_config.*`：解析 `-plugin` 配置与参数；
  - `plugin_protocol.*`：消息头、序列化/反序列化、状态码；
  - `plugin_child_runtime.*`：子进程装载器与事件循环；
  - `external_device_proxy.*`：父进程里的 `Device` 代理对象。
- [x] 在父进程侧设计 `ExternalDeviceProxy`：
  - 继承当前统一的 `Device` 抽象，仅承担 MMIO `read/write` 入口；
  - 被 `Bus::map()` 当作普通设备挂载；
  - 内部持有 socket、子进程 pid、健康状态、逻辑 IRQ 状态缓存、下一个 deadline 缓存与 manifest 元数据。
- [ ] 定义插件 manifest 中的 MMIO 描述：
  - 支持一个或多个窗口；
  - 每个窗口含 `base/size/name/flags`；
  - flags 可声明是否支持未对齐访问、是否有 side-effect、是否适合后续 fast path。
- [ ] 明确与当前 `Bus/SoC` 的接缝：
  - 对多窗口插件，可把同一个 `ExternalDeviceProxy` 重复传给 `Bus::map()`，不强行重做 `Bus` 映射模型；
  - `main.cpp` 负责解析 `-plugin` 并把配置交给 `SoC`；
  - `SoC` 在现有内建设备之外维护 `external_devices_`，并在 `sync_devices()` 中处理外部 IRQ 电平同步，在 `next_device_event()` 中并入插件 deadline；
  - 当前硬编码 `BusFastPath` 保持不变，插件 MMIO 统一走慢路径。
- [ ] 明确 MMIO 语义：
  - 父进程发起读写时带上访问宽度、偏移、访存属性、当前 guest 时间；
  - 子进程必须返回确定结果，不能直接阻塞整个模拟器无限等待。
- [ ] 为超时策略留接口：
  - 每次 MMIO 最长等待多久；
  - 超时后是返回总线错误、设备故障还是终止模拟器。

设计判断：
- [x] 第一版插件 MMIO 一律走 bus 慢路径，不试图并入当前硬编码 `BusFastPath`。
- [ ] 等协议与行为稳定后，再单独设计“插件设备的 fast-path/offload 能力声明”，避免把热路径和 ABI 一起绑死。

### 5. 插件对 guest 物理地址空间的访问 API

目标：
- 让插件能发起 DMA/总线主设备访问，但仍由主模拟器掌控最终的物理内存与设备可见性。

任务：
- [ ] SDK 只暴露“guest physical address”访问 API，不暴露主模拟器 RAM 裸指针。
- [ ] 第一版提供同步 API：
  - `dma_read(pa, size, dst)`
  - `dma_write(pa, size, src)`
  - `dma_memset(pa, value, size)` 可选
- [ ] 明确 DMA 的语义入口：
  - 默认经由父进程统一的物理地址访问通路；
  - 第一版只允许访问 RAM，不开放“插件作为 bus master 去访问其他 MMIO 设备”；
  - 对未映射区域、设备 MMIO 或权限不允许的访问，父进程返回明确错误码。
- [ ] 明确一致性要求：
  - 插件 `dma_write` 必须复用主模拟器当前“外部设备写 RAM”通知链路；
  - 尤其要复用现在 `block/virtio` 已用到的 CPU decode / exclusive monitor 失效逻辑，不能让插件 DMA 绕过一致性路径。
- [ ] 为后续性能扩展预留能力位：
  - 只读 RAM window 映射
  - 分页映射缓存
  - 批量 scatter/gather DMA
  - 脏页回写通知

设计判断：
- [ ] 第一阶段优先保守：插件 DMA 通过父进程代理完成，不共享主进程 RAM 地址。
- [ ] 如果后续确实需要更高吞吐，再考虑：
  - 父进程把特定 RAM 页以只读或读写共享内存方式映射给插件；
  - 但该优化必须建立在明确的失效/一致性协议之上，不能破坏 guest 可感知行为。

### 6. 中断、deadline 与 guest 时间同步

目标：
- 让插件能以稳定方式向主模拟器报告中断状态变化与未来 deadline，同时保证 guest 时间仍只以父进程为准。

任务：
- [ ] 在 manifest 中声明插件需要的 IRQ 输出数量与类型：
  - 允许声明 `0..N` 条 IRQ 输出，MVP 可先从 `0` 条起步；
  - 电平触发 line
  - 脉冲触发 line
  - 后续如有 MSI/doorbell，再单独扩展；
  - 在真正开始做 IRQ 路由后，配置入口可先聚焦“每实例一个 IRQ 路由”，manifest 继续保留多 line 扩展位。
- [ ] SDK 暴露最小 IRQ API：
  - `irq_set(line, level)`
  - `irq_pulse(line)`
  - `irq_clear(line)` 或 `irq_set(..., 0)`
- [ ] 父进程负责把插件逻辑 line 映射到 SoC/GIC 的真实输入；
  - 插件不感知 GICv3、SPI、PPI 等具体细节；
  - 这样后续平台变化时可以保持插件 ABI 稳定。
- [ ] 为中断去抖与幂等行为定规则：
  - 重复 `irq_set(1)` 不应造成额外副作用；
  - line 状态以父进程记录为准；
  - 发生子进程崩溃或 timeout 后，父进程要能清晰定义该 line 的收敛状态。
- [ ] 明确“guest 时间只以父进程为准”：
  - 子进程不得自行读取宿主机 wall clock 作为设备时间；
  - 所有设备时间推进都由父进程通过 `guest_now` 或 `ADVANCE_TO` 驱动。
- [ ] 把插件分成两类：
  - 简单设备：仅在 MMIO/DMA/IRQ 边界上惰性推进；
  - 复杂设备：可以声明 `needs_deadline_sync`，并返回下一个需要被唤醒的 guest deadline。
- [ ] 设计基本时间 API：
  - `advance_to(guest_now)`
  - `set_next_deadline(guest_tick)`
  - `get_guest_now()`
- [ ] 明确同步机制：
  - 在每次主模拟器与插件发生可见交互前，父进程确保插件至少被推进到相同的 `guest_now`；
  - 当插件声明未来 deadline 时，父进程把它纳入 `SoC::next_device_event()` 的统一事件调度器；
  - 如果插件没有 deadline，则视为纯惰性设备。
- [ ] 为“防漂移”增加可选校验：
  - 父进程可周期性要求插件回报其内部 `device_now`；
  - 若插件报告时间回退、超前或持续滞后，视为插件逻辑错误并给出日志。

设计判断：
- [ ] 整体设计保留外部 IRQ 能力，但 MVP 第一阶段允许插件完全不声明 IRQ line，只先打通 MMIO 与子进程装载。
- [ ] 第一版不允许插件自建独立 host thread 按 wall clock 跑设备时钟。
- [ ] 第一版优先做确定性、可重放的 guest-time 驱动模型；需要更复杂异步设备时，再在此基础上扩展。

### 7. SDK / ABI 稳定性策略

目标：
- 让外设开发者只依赖一套尽量稳定的 C API，而不被主模拟器内部 C++ 重构频繁打断。

任务：
- [x] SDK 使用纯 C ABI：
  - 头文件仅暴露 `extern "C"` 接口；
  - 结构体带 `size` 字段；
  - 句柄一律使用 opaque pointer / opaque integer。
- [x] 插件导出统一入口，例如：
  - `aarchvm_plugin_get_api_v1()`
  - 返回只读的 ops 表与 capability 描述。
- [x] 插件 ops 至少包含：
  - `create`
  - `destroy`
  - `reset`
  - `mmio_read`
  - `mmio_write`
  - `advance_to`
  - `on_shutdown`
- [x] 宿主机回调表至少包含：
  - `dma_read`
  - `dma_write`
  - `irq_set/pulse`
  - `set_deadline`
  - `get_guest_now`
  - `log`
  - `abort_device`
- [x] 版本策略：
  - 采用 `ABI major/minor`；
  - `major` 不兼容变化时拒绝加载；
  - `minor` 只允许追加 capability，不破坏旧字段布局。
- [x] SDK 应包含：
  - 公共头文件
  - 最小构建脚本模板
  - 一个示例设备
  - 一个独立 `cmake -S sdk -B ...` 的构建示例
  - 协议与生命周期文档

设计判断：
- [x] SDK 稳定性的关键是“协议与回调表稳定”，而不是把内部类对外公开。
- [ ] 插件应只感知抽象能力，不感知：
  - `BusFastPath`
  - `SoC` 内部调度实现
  - GIC 具体内部状态结构

### 8. 复位、snapshot 限制与错误恢复

目标：
- 不为插件引入 snapshot 负担，同时把 reset、故障与“不支持 snapshot”的行为定义清楚。

任务：
- [ ] 定义 reset 语义：
  - system reset 时插件收到 `reset(kind)`；
  - 必须清理内部 DMA 状态、IRQ level、deadline 与暂存事务。
- [x] 明确 snapshot 不支持策略：
  - 只要命令行配置了外部插件，`-snapshot-load` 启动路径就直接拒绝；
  - 只要运行时存在外部插件，`-snapshot-save` 请求就直接失败并打印明确错误；
  - snapshot 文件格式不记录插件路径、配置或状态，避免留下“似乎能恢复、实际恢复不全”的假语义。
- [ ] 定义故障恢复策略：
  - 子进程崩溃时主进程打印明确日志；
  - 可选策略包括：
    - 终止整个模拟器；
    - 将设备标记为永久故障；
    - 尝试按 reset 级别重启子进程。
- [ ] 明确重启与 reset 的关系：
  - 如果后续允许“故障后重启插件”，其语义应等价于“重新 `fork()+dlopen()` 一个全新实例，再走一次 `reset + handshake`”；
  - 不允许伪造类似 snapshot 的热恢复语义。

### 9. 安全与隔离加强项

目标：
- 在“先实现可用机制”的基础上，逐步增强隔离。

任务：
- [ ] 第一版先依靠 `fork()` 带来的地址空间隔离。
- [ ] 后续可选加强：
  - 子进程 `prctl(PR_SET_PDEATHSIG, ...)`
  - `seccomp`
  - `setrlimit`
  - 独立工作目录
  - 只读插件目录
  - 禁止插件任意打开文件/网络
  - 关闭不必要的继承 fd
- [ ] 为调试保留开关：
  - 输出 IPC trace
  - 输出插件日志前缀
  - 输出每次 MMIO/DMA/IRQ 的统计信息

### 10. 测试与落地顺序

目标：
- 先做最小可用闭环，再逐步增加 DMA、IRQ、deadline 与健壮性；snapshot 不在范围内。

任务：
- [x] 阶段 0：SDK 构建与 ABI 烟测
  - `cmake -S sdk -B out/sdk-build` 能独立配置成功；
  - 示例插件能单独生成 `.so`，且不依赖把主项目可执行文件链接进来；
  - 宿主侧白盒测试覆盖：缺失导出符号、ABI major 不匹配、manifest 非法、MMIO 窗口非法时必须拒绝加载。
- [x] 阶段 1：最小 MMIO 闭环
  - 一个独立子进程插件；
  - 一个 MMIO 窗口；
  - 同步 MMIO read/write；
  - 无 IRQ；
  - 无 DMA；
  - 无 deadline；
  - 无 snapshot；
  - 用最简单的“纯寄存器型”示例设备打通。
- [ ] 阶段 1 测试：
  - 单元测试：协议头编码/解码、配置解析、请求序号匹配、超时、断连、状态机；
  - 集成测试：裸机程序读写插件寄存器并验证寄存器语义；
  - 负向测试：重复 MMIO 窗口、子进程初始化崩溃、子进程无响应；
  - 限制性测试：启用插件时 `-snapshot-save/-snapshot-load` 必须显式失败。
- [ ] 阶段 2：IRQ + deadline
  - 引入 `irq_set/irq_pulse`；
  - 增加 `advance_to` / `set_deadline`；
  - 让插件进入统一事件调度；
  - 把插件逻辑 line 正式接到 SoC/GIC。
- [ ] 阶段 2 测试：
  - 集成测试：裸机程序验证 IRQ 拉高/清除、脉冲中断与 deadline 唤醒；
  - 调度测试：`next_device_event()` 能正确纳入插件 deadline，避免纯 busy poll；
  - 负向测试：非法 IRQ 路由、重复 `irq_set()`、子进程故障后的 line 收敛。
- [ ] 阶段 3：DMA
  - 引入 `dma_read/dma_write`；
  - 让插件能完成基本 bus-master RAM 访问。
- [ ] 阶段 3 测试：
  - 集成测试：插件通过 DMA 改写 guest RAM，guest 代码能看到结果；
  - 一致性测试：插件 DMA 与当前 `block/virtio` 一样，必须触发 CPU decode / exclusive monitor 失效链路；
  - 组合测试：DMA 完成后可选择再配合阶段 2 的 IRQ 能力发完成中断。
- [ ] 阶段 4：reset / restart / 健壮性
  - reset/restart；
  - 错误恢复与日志体系；
  - 故障策略收敛。
- [ ] 阶段 4 测试：
  - 压力测试：插件死循环、崩溃、超时、异常退出；
  - 故障注入：在初始化、MMIO、DMA、`advance_to` 各阶段杀死子进程；
  - 行为断言：父进程日志、IRQ 收敛状态、后续 MMIO 返回值与退出码要符合既定策略。
- [ ] 阶段 5：协议性能优化
  - 批量 MMIO / 批量 DMA；
  - 共享内存优化；
  - 多窗口、多中断。
- [ ] 阶段 5 测试：
  - 保留前述功能回归；
  - 新增 perf smoke，先统计 `plugin_mmio/plugin_dma/plugin_irq` 基本计数，再决定是否值得做共享内存优化。
- [ ] 回归策略：
  - “无插件”场景下，现有 `tests/arm64/run_all.sh`、Linux functional suite、snapshot 流程应保持不变；
  - “有插件”场景优先从裸机回归开始，不把 Linux DTB/驱动枚举强绑进第一阶段；
  - 如后续要让 Linux 使用插件设备，再单独补 DTB 与 guest 驱动测试，不要把这一步混进最小闭环。

### 11. 建议的首个示例插件

目标：
- 用一组从简到繁的设备作为 SDK 示例，先覆盖 MMIO，再覆盖 IRQ / 时间推进 / DMA。

任务：
- [x] 首个示例建议实现 `sdk/examples/register_bank`：
  - 几个只读 ID / version 寄存器；
  - 一个或多个可读写 scratch 寄存器；
  - 不依赖 IRQ、DMA、deadline；
  - 专门用于阶段 0/1 的 SDK 构建与 MMIO 烟测。
- [ ] 第二个示例再实现 `sdk/examples/timer_doorbell`：
  - 少量控制寄存器；
  - 一个计数器；
  - 一个比较寄存器；
  - 一个 pending/status 寄存器；
  - 一个 ACK/clear 寄存器；
  - 到期后拉高 IRQ；
  - 同时可额外保留一个“写入即脉冲”的 doorbell 寄存器，方便验证 `irq_pulse()`。
- [ ] 第三个示例再考虑“简化 DMA 设备”：
  - 通过 `dma_read/dma_write` 搬运内存；
  - 验证 DMA API、时间推进与中断完成语义。

设计判断：
- [ ] 首个示例不应从块设备、网卡这类复杂设备开始。
- [ ] 先把 ABI、生命周期和调度模型做稳，再扩展到高吞吐外设。
