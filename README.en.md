# aarchvm (Stage-0)

A minimal full-system AArch64 simulator written in C++ with CMake, using an interpreter execution model.

## Status

This project is still a bring-up prototype, but now includes:
- A single-core AArch64 interpreter.
- Basic EL1-oriented system register model.
- Minimal SoC model with RAM, PL011 UART, Generic Timer, and minimal GICv3.
- Minimal IRQ closed loop (timer -> GIC -> vector -> `ERET`).
- Minimal sync-exception closed loop with `ESR_EL1/FAR_EL1/ELR_EL1/SPSR_EL1`.
- Minimal MMU/TLB path for Stage-1 translation with `TTBR0_EL1/TTBR1_EL1` (4KB-granule style).
- Minimal GICv3 system-register CPU interface + virtual timer sysreg path (`ICC_*`, `CNTV_*`).

## Build

```bash
cmake -S . -B build
cmake --build build -j
```

## Run

Embedded demo:

```bash
./build/aarchvm
```

External raw binary:

```bash
./build/aarchvm -bin <program.bin> -load <load_addr> -entry <entry_pc> -steps <max_steps>
```

Common optional arguments:
- `-sp <addr>`: initialize startup stack pointer (optional)
- `-dtb <file> -dtb-addr <addr>`: load DTB and pass DTB pointer via `x0` (optional)

## Implemented Instruction Set (Current)

Control flow:
- `NOP`, `B`, `BL`, `BR`, `BLR`, `RET`, `B.cond`, `CBZ`, `CBNZ`, `TBZ`, `TBNZ`, `BRK`, `ERET`
- `WFI`, `WFE`, `SEV`, `SEVL`

Data processing:
- `ADR`, `ADRP`
- `MOVZ`, `MOVK`, `MOVN` (32/64-bit)
- `ADD/SUB/ADDS/SUBS` (immediate + shifted register, 32/64-bit)
- `CSEL` (32/64-bit)
- Bitfield immediate family (32/64-bit): `UBFM`, `SBFM`, `BFM`
  with common aliases (`LSR`, `LSL`, `UBFX`, `SBFX`, `BFXIL`)
- Extract/rotate immediate: `EXTR`, `ROR` (immediate alias)
- Conditional select family: `CSEL`, `CSINC` (thus `CSET` alias path)
- Integer divide: `UDIV`, `SDIV`
- Logical shifted-register family (32/64-bit):
  `AND`, `BIC`, `ORR`, `ORN`, `EOR`, `EON`, `ANDS`, `BICS`

Load/store:
- `LDR/STR Xt`, `LDR/STR Wt` (`[Xn, #imm12]`)
- `LDRSW` / `LDURSW` (unsigned + unscaled + pre/post-index forms used by tests)
- `LDRB/STRB`, `LDRH/STRH` (`[Xn, #imm12]`)
- `LDUR/STUR Xt`, `LDUR/STUR Wt` (`[Xn, #simm9]`)
- `LDP/STP` (forms used by tests)

System and maintenance:
- `MRS/MSR` subset for key EL1 registers
- `MSR DAIFSet/DAIFClr`, `MRS/MSR DAIF`
- `MRS/MSR SPSel`, `MSR SPSel, #imm`
- `DMB`, `DSB`, `ISB`, `CLREX`
- `TLBI VMALLE1`, `TLBI VAE1, Xt`
- `AT S1E1R`, `AT S1E1W`
- `IC IALLU`, `IC IVAU, Xt`
- `DC IVAC/CVAC/CIVAC, Xt`

## System Register Subset

- `SCTLR_EL1`, `TTBR0_EL1`, `TTBR1_EL1`, `TCR_EL1`, `MAIR_EL1`, `VBAR_EL1`
- `ELR_EL1`, `SPSR_EL1`, `ESR_EL1`, `FAR_EL1`, `SP_EL0`, `SP_EL1`, `SPSel`
- `DAIF`, `NZCV`, `CURRENTEL`, `PAR_EL1`
- `CNTFRQ_EL0`, `CNTPCT_EL0` (minimal alias), `CNTVCT_EL0`
- `CNTV_CTL_EL0`, `CNTV_CVAL_EL0`, `CNTV_TVAL_EL0`
- `ICC_IAR1_EL1`, `ICC_EOIR1_EL1`, `ICC_PMR_EL1`, `ICC_CTLR_EL1`, `ICC_SRE_EL1`

## MMU/TLB/Cache-Maintenance Model

Current MMU model is intentionally minimal:
- Stage-1 translation supports both `TTBR0_EL1` and `TTBR1_EL1`.
- Simplified 4-level walk model with 4KB page granularity assumptions.
- Supports 4KB-granule style start-level derivation from `TCR_EL1` (`T0SZ/T1SZ`).
- Supports table descriptors and block/page descriptors for early-boot mappings.
- Valid descriptors treated as table/page descriptors in a reduced form.
- Software TLB cache exists and is affected by `TLBI` instructions.
- `AT` updates `PAR_EL1` in a minimal way.
- `IC/DC` maintenance instructions are decoded and executed with minimal functional semantics.

This is not a full architectural MMU/cache implementation yet.

## Test Suite

Build all test binaries:

```bash
tests/arm64/build_tests.sh
```

Run full regression:

```bash
tests/arm64/run_all.sh
```

Key outputs (in order):
- `E` (`instr_legacy_each.bin`: previously implemented instructions checked one-by-one)
- `Q` (`mmu_tlb_cache.bin`: MMU/TLB/cache-maintenance path)
- `K` (`mmu_ttbr1_early.bin`: TTBR1 high-VA early mapping/fetch path)
- `4` (`mmu_tlbi_non_target.bin`: `TLBI VAE1` invalidates only the target VA)
- `5` (`mmu_l2_block_vmalle1.bin`: L2 block remap + `TLBI VMALLE1` visibility)
- `6` (`mmu_at_tlb_observe.bin`: `AT/PAR_EL1` behavior before/after TLB invalidation)
- `7` (`mmu_ttbr_asid_mask.bin`: `TTBR0_EL1` ASID bits + table-base masking behavior)
- `X` (`sync_exception_regs.bin`: sync exception + ESR/FAR/ELR/SPSR path)
- `G` (`gic_timer_sysreg.bin`: minimal GICv3 sysreg + CNTV sysreg IRQ path)
- `U` (`bitfield_basic.bin`: bitfield-immediate/shift aliases + CNTPCT path)
- `V` (`p1_core.bin`: EXTR/ROR + CSINC/CSET + UDIV/SDIV + LDRSW family)
- `ASM B C S IM T D W P L M R` (existing functional tests)

## Reference

- `DDI0487_M.a.a_a-profile_architecture_reference_manual.pdf`
