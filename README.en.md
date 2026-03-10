# aarchvm (Stage-0)

A minimal full-system AArch64 simulator written in C++ with CMake, using an interpreter execution model.

## Status

This project is still a bring-up prototype, but now includes:
- A single-core AArch64 interpreter.
- Basic EL1-oriented system register model.
- Minimal SoC model with RAM, PL011 UART, Generic Timer, and minimal GICv3.
- Minimal IRQ closed loop (timer -> GIC -> vector -> `ERET`).
- Minimal sync-exception closed loop with `ESR_EL1/FAR_EL1/ELR_EL1/SPSR_EL1`.
- Linux-oriented minimal MMU/TLB path for Stage-1 translation with `TTBR0_EL1/TTBR1_EL1` (4KB granule), including leaf permission/AF/XN checks, table-attribute inheritance, and `TCR.IPS` PA-size checks.
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
- `TLBI VMALLE1/VMALLE1IS`
- `TLBI VAE1/VAE1IS/VALE1/VALE1IS, Xt`
- `TLBI ASIDE1/ASIDE1IS, Xt`
- `AT S1E1R`, `AT S1E1W`
- `IC IALLU`, `IC IVAU, Xt`
- `DC IVAC/CVAC/CIVAC, Xt`

## System Register Subset

- `SCTLR_EL1`, `TTBR0_EL1`, `TTBR1_EL1`, `TCR_EL1`, `MAIR_EL1`, `VBAR_EL1`
- `ELR_EL1`, `SPSR_EL1`, `ESR_EL1`, `FAR_EL1`, `SP_EL0`, `SP_EL1`, `SPSel`
- `DAIF`, `NZCV`, `CURRENTEL`, `PAR_EL1`
- `ID_AA64MMFR0_EL1`, `ID_AA64MMFR1_EL1`, `ID_AA64MMFR2_EL1`
- `CNTFRQ_EL0`, `CNTPCT_EL0` (minimal alias), `CNTVCT_EL0`
- `CNTV_CTL_EL0`, `CNTV_CVAL_EL0`, `CNTV_TVAL_EL0`
- `ICC_IAR1_EL1`, `ICC_EOIR1_EL1`, `ICC_PMR_EL1`, `ICC_CTLR_EL1`, `ICC_SRE_EL1`

## MMU/TLB/Cache-Maintenance Model

Current MMU model is still intentionally scoped, but it now covers the Linux-relevant early pieces:
- Stage-1 translation supports both `TTBR0_EL1` and `TTBR1_EL1`.
- Simplified 4-level walk model with 4KB page granularity assumptions.
- Start-level derivation from `TCR_EL1` (`T0SZ/T1SZ`) plus `EPD0/EPD1` gating.
- `TCR.IPS` is decoded and used to reject output addresses or next-level table bases that exceed the configured PA size.
- Translation-walk attributes from `TCR_EL1` (`IRGNx/ORGNx/SHx`) are decoded and carried through translation results/TLB entries.
- Leaf memory attributes from `MAIR_EL1` (`AttrIndx -> MAIR byte`) are decoded and stored in the translation result/TLB entry.
- Table descriptors and block/page descriptors are supported for early-boot mappings.
- Table-attribute inheritance is modelled for `APTable[1]` (write-protect) and `PXNTable/UXNTable`.
- Leaf attribute checks cover `AF`, `AP[2]` (EL1 read-only), and execute-never handling.
- Software TLB entries cache translated PA plus effective permission/attribute state after inheritance.
- `TLBI` invalidation supports the common EL1 local/inner-shareable aliases used by early firmware/kernel code.
- `AT S1E1R/S1E1W` bypasses the software TLB, performs a fresh walk, and reports success/fault state through `PAR_EL1`.
- Translation/access-flag/permission/address-size faults are reflected into abort ISS low bits so ESR is useful during bring-up.
- `IC/DC` maintenance instructions are decoded and executed with minimal functional semantics.

This is still not a full architectural MMU/cache implementation: shareability and memory type are decoded but not yet driving real memory ordering/device side effects, DBM/dirty-state updates are absent, table attribute coverage is partial, and ASID-tagged TLB behavior remains simplified.

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
- `1` (`mmu_tlb_vae1_scope.bin`: per-VA invalidation only changes the targeted page)
- `2` (`mmu_ttbr_switch.bin`: TTBR0 switch updates the active translation regime)
- `3` (`mmu_unmap_data_abort.bin`: unmap + `TLBI` produces a data abort)
- `4` (`mmu_tlbi_non_target.bin`: `TLBI VAE1` invalidates only the target VA)
- `5` (`mmu_l2_block_vmalle1.bin`: L2 block remap + `TLBI VMALLE1` visibility)
- `6` (`mmu_at_tlb_observe.bin`: `AT/PAR_EL1` observes fresh page walks while data access still sees stale TLB state before invalidation)
- `7` (`mmu_ttbr_asid_mask.bin`: `TTBR0_EL1` ASID bits + table-base masking behavior)
- `8` (`mmu_perm_ro_write_abort.bin`: EL1 read-only mapping rejects writes with a permission fault)
- `9` (`mmu_xn_fetch_abort.bin`: execute-never mapping blocks fetch until PTE update + `TLBI`)
- `H` (`mmu_table_ap_inherit.bin`: `APTable` write-protect is inherited by lower-level leaves)
- `Y` (`mmu_table_pxn_inherit.bin`: `PXNTable` blocks fetch until the upper table descriptor is fixed and invalidated)
- `Z` (`mmu_tcr_ips_mair_decode.bin`: `TCR.IPS` is enforced on translated PA size, while `AttrIndx/MAIR` paths are exercised)
- `0` (`mmu_af_fault.bin`: access-flag fault is reported until `AF` is set and `TLBI` is issued)
- `X` (`sync_exception_regs.bin`: sync exception + ESR/FAR/ELR/SPSR path)
- `G` (`gic_timer_sysreg.bin`: minimal GICv3 sysreg + CNTV sysreg IRQ path)
- `U` (`bitfield_basic.bin`: bitfield-immediate/shift aliases + CNTPCT path)
- `V` (`p1_core.bin`: EXTR/ROR + CSINC/CSET + UDIV/SDIV + LDRSW family)
- `ASM B C S IM T D W P L M R` (existing functional tests)

## What Still Blocks Linux Boot

The MMU/TLB path is now strong enough for early page-table bring-up, but Linux still needs several major pieces before a real kernel boot is realistic:
- A larger AArch64 ISA subset, especially atomics/exclusive sequences (`LDXR/STXR`, `LDAXR/STLXR`, CAS-style atomics), more system instructions, and additional load/store forms used deeper in kernel init.
- More complete exception-level and privilege handling: EL2/EL3 boot paths, `HCR_EL2`, `SCR_EL3`, `VBAR_EL2/EL3`, and realistic `ERET` flows when entering the kernel from different firmware paths.
- More complete system register coverage, especially ID registers, timer/control registers, and MMU-related sysregs that Linux probes during CPU feature detection.
- Richer MMU semantics: full MAIR memory-type behavior, more complete table attribute propagation, ASID-aware TLB behavior, dirty/DBM handling, break-before-make corner cases, and eventually EL0 permission semantics.
- A more complete GICv3 model, especially MMIO distributor/redistributor coverage beyond the current minimal sysreg CPU interface.
- PSCI/SMCCC support for the boot chain and for the kernel's power-management / CPU-interface expectations.
- More complete timer support beyond the current minimal virtual timer path.
- Additional device models needed after very early boot: storage, block/network, and often virtio if a practical Linux userspace boot is the goal.
- Multi-core/SMP support.
- Real cache and barrier side effects. Decoding maintenance instructions is no longer enough once Linux starts relying on stronger memory-model interactions.

## Reference

- `DDI0487_M.a.a_a-profile_architecture_reference_manual.pdf`
