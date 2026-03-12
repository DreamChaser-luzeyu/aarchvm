# aarchvm

`aarchvm` is a C++ / CMake AArch64 full-system emulator prototype using an interpreter execution model.
The current tree is already able to boot U-Boot, hand off to a Linux `Image`, start a BusyBox `initramfs`, and enter an interactive shell through the emulated PL011 UART.

## Validated Status

The current repository has been validated for the following paths:
- Single-core AArch64 EL1 interpreter execution.
- Minimal SoC model: RAM, PL011 UART, Generic Timer, and minimal GICv3.
- Minimal IRQ loop: timer -> GIC -> EL1 vector -> `ERET`.
- Minimal synchronous exception loop with `ESR_EL1/FAR_EL1/ELR_EL1/SPSR_EL1`.
- Minimal MMU/TLB path needed by early Linux page-table bring-up.
- U-Boot boot and serial output inside the emulator.
- U-Boot `booti` hand-off to a Linux `Image`.
- Linux serial output through both `earlycon=pl011,...` and the normal `ttyAMA0` path.
- Interactive BusyBox `ash` shell input has been validated.
- A richer static BusyBox userland has also been validated, including at least `ps`, `awk`, `find`, and `free`.
- Full-machine snapshot save / restore is available for fast Linux repro loops.

Linux / BusyBox output already observed and validated on the emulated serial port includes:
- `Booting Linux on physical CPU ...`
- `Linux version 6.12.76 ...`
- `earlycon: pl11 at MMIO 0x0000000009000000`
- `Serial: AMBA PL011 UART driver`
- `Run /init as init process`
- `BusyBox v1.37.0 ... built-in shell (ash)`
- the `#` prompt, along with interactive commands such as `echo X` being echoed and executed

This means the project has moved beyond the earlier "kernel entered but no visible output" stage and into an interactive Linux user-space shell state.

## Layout and Artifacts

The repository currently uses the following artifacts:
- Emulator sources: `src/`, `include/`
- Emulator build directory: `build/`
- U-Boot sources: `u-boot-2026.01/`
- U-Boot build directory: `u-boot-2026.01/build-qemu_arm64/`
- Linux sources: `linux-6.12.76/`
- Linux build directory: `linux-6.12.76/build-aarchvm/`
- U-Boot control DTB: `dts/aarchvm-current.dtb`
- Linux boot DTB: `dts/aarchvm-linux-min.dtb`

## Toolchain Version

The cross toolchain actually used and validated in this workspace is:

```text
aarch64-linux-gnu-gcc (Debian 14.2.0-19) 14.2.0
GNU ld (GNU Binutils for Debian) 2.44
GNU as (GNU Binutils for Debian) 2.44
```

You can verify this on the host with:

```bash
aarch64-linux-gnu-gcc --version | head -n 1
aarch64-linux-gnu-ld --version | head -n 1
aarch64-linux-gnu-as --version | head -n 1
```

## Build

### 1. Build the Emulator

```bash
cmake -S . -B build
cmake --build build -j
```

### 2. Build U-Boot

The current tree uses the `qemu_arm64` U-Boot configuration and loads the generated `u-boot.bin` in the emulator:

```bash
make -C u-boot-2026.01 \
  O=build-qemu_arm64 \
  CROSS_COMPILE=aarch64-linux-gnu- \
  qemu_arm64_defconfig

make -C u-boot-2026.01 \
  O=build-qemu_arm64 \
  CROSS_COMPILE=aarch64-linux-gnu- \
  -j"$(nproc)"
```

Build artifact:
- `u-boot-2026.01/build-qemu_arm64/u-boot.bin`

### 3. Build Linux

A reproducible flow is to start from `defconfig` and then override the options needed by the current emulator path:

```bash
make -C linux-6.12.76 \
  O=build-aarchvm \
  ARCH=arm64 \
  CROSS_COMPILE=aarch64-linux-gnu- \
  defconfig
```

Then apply the key options used by the current repository:

```bash
linux-6.12.76/scripts/config --file linux-6.12.76/build-aarchvm/.config \
  --set-str CMDLINE "console=ttyAMA0,115200 earlycon=pl011,0x09000000 ignore_loglevel" \
  --enable CMDLINE_FORCE \
  --enable PRINTK \
  --enable ARM_AMBA \
  --enable SERIAL_AMBA_PL011 \
  --enable SERIAL_AMBA_PL011_CONSOLE \
  --enable ARM_GIC_V3 \
  --enable ARM_ARCH_TIMER \
  --enable DEVTMPFS \
  --enable DEVTMPFS_MOUNT \
  --enable VIRTIO \
  --enable VIRTIO_BLK \
  --enable BLK_DEV_INITRD \
  --enable RD_GZIP \
  --enable EXT4_FS \
  --enable TMPFS \
  --enable TMPFS_POSIX_ACL

make -C linux-6.12.76 \
  O=build-aarchvm \
  ARCH=arm64 \
  CROSS_COMPILE=aarch64-linux-gnu- \
  olddefconfig

make -C linux-6.12.76 \
  O=build-aarchvm \
  ARCH=arm64 \
  CROSS_COMPILE=aarch64-linux-gnu- \
  Image -j"$(nproc)"
```

Build artifacts:
- `linux-6.12.76/build-aarchvm/arch/arm64/boot/Image`
- `linux-6.12.76/build-aarchvm/vmlinux`

### 4. Generate DTBs

If `dtc` is not installed globally, you can directly use the one built inside the Linux tree:

```bash
linux-6.12.76/build-aarchvm/scripts/dtc/dtc \
  -I dts -O dtb \
  -o dts/aarchvm-linux-min.dtb \
  dts/aarchvm-linux-min.dts
```

Likewise, regenerate the U-Boot-facing DTB if needed:

```bash
linux-6.12.76/build-aarchvm/scripts/dtc/dtc \
  -I dts -O dtb \
  -o dts/aarchvm-current.dtb \
  dts/aarchvm-current.dts
```

### 5. Build BusyBox and the initramfs

The current tree uses a `defconfig`-derived static BusyBox build, with a few options disabled because they fail in the current host toolchain environment and are not needed by the current guest boot path.

```bash
make -C busybox-1.37.0 distclean
make -C busybox-1.37.0 defconfig
perl -0pi -e 's/^# CONFIG_STATIC is not set$/CONFIG_STATIC=y/m; s/^CONFIG_CROSS_COMPILER_PREFIX=.*$/CONFIG_CROSS_COMPILER_PREFIX="aarch64-linux-gnu-"/m; s/^CONFIG_TC=y$/# CONFIG_TC is not set/m; s/^CONFIG_FEATURE_TC_INGRESS=y$/# CONFIG_FEATURE_TC_INGRESS is not set/m; s/^CONFIG_SHA1_HWACCEL=y$/# CONFIG_SHA1_HWACCEL is not set/m; s/^CONFIG_SHA256_HWACCEL=y$/# CONFIG_SHA256_HWACCEL is not set/m' busybox-1.37.0/.config
make -C busybox-1.37.0 oldconfig </dev/null
make -C busybox-1.37.0 -j"$(nproc)"
make -C busybox-1.37.0 install

rm -rf out/initramfs-full-root
mkdir -p out/initramfs-full-root
cp -a busybox-1.37.0/_install/. out/initramfs-full-root/
mkdir -p out/initramfs-full-root/{dev,proc,sys,tmp,run,root,mnt,etc}
cp out/initramfs-root/init out/initramfs-full-root/init
cp out/initramfs-root/bin/guest_diag out/initramfs-full-root/bin/guest_diag
chmod 0755 out/initramfs-full-root/init out/initramfs-full-root/bin/guest_diag
( cd out/initramfs-full-root && find . -print0 | cpio --null -o -H newc > ../initramfs-full.cpio )
gzip -n -f -c out/initramfs-full.cpio > out/initramfs-full.cpio.gz
```

The regenerated `initramfs` sizes are currently:
- `out/initramfs-full.cpio`: `0x2cd000`
- `out/initramfs-full.cpio.gz`: `0x1662f8`

## Key Configuration Options in Use

### U-Boot Key Options

These options come from the currently used
`u-boot-2026.01/build-qemu_arm64/.config`:

```text
CONFIG_ARM=y
CONFIG_ARM64=y
CONFIG_DEFAULT_DEVICE_TREE="qemu-arm64"
CONFIG_SYS_LOAD_ADDR=0x40200000
CONFIG_NR_DRAM_BANKS=4
CONFIG_BOOTDELAY=2
CONFIG_CMD_BOOTI=y
CONFIG_CMD_FDT=y
CONFIG_FIT=y
CONFIG_LEGACY_IMAGE_FORMAT=y
CONFIG_OF_CONTROL=y
CONFIG_OF_BOARD=y
CONFIG_DM_SERIAL=y
CONFIG_BAUDRATE=115200
CONFIG_DM_KEYBOARD=y
CONFIG_USB_KEYBOARD=y
```

Why these matter in the current setup:
- `CONFIG_CMD_BOOTI=y` allows direct boot of an AArch64 Linux `Image`.
- `CONFIG_OF_CONTROL=y` and `CONFIG_OF_BOARD=y` let U-Boot consume the board DTB provided by the emulator.
- `CONFIG_DM_SERIAL=y` and `CONFIG_BAUDRATE=115200` match the current PL011 UART model.
- `CONFIG_DEFAULT_DEVICE_TREE="qemu-arm64"` is the current U-Boot base configuration.

### Linux Key Options

These options come from the currently used
`linux-6.12.76/build-aarchvm/.config`:

```text
CONFIG_64BIT=y
CONFIG_MMU=y
CONFIG_SMP=y
CONFIG_PRINTK=y
CONFIG_CMDLINE="console=ttyAMA0,115200 earlycon=pl011,0x09000000 ignore_loglevel"
CONFIG_CMDLINE_FORCE=y
CONFIG_OF=y
CONFIG_ARM_AMBA=y
CONFIG_ARM_GIC_V3=y
CONFIG_ARM_ARCH_TIMER=y
CONFIG_SERIAL_AMBA_PL011=y
CONFIG_SERIAL_AMBA_PL011_CONSOLE=y
CONFIG_DEVTMPFS=y
CONFIG_DEVTMPFS_MOUNT=y
CONFIG_PCI=y
CONFIG_VIRTIO=y
CONFIG_VIRTIO_BLK=y
CONFIG_BLK_DEV_INITRD=y
CONFIG_RD_GZIP=y
CONFIG_EXT4_FS=y
CONFIG_TMPFS=y
CONFIG_TMPFS_POSIX_ACL=y
```

Why these matter here:
- `CONFIG_CMDLINE` and `CONFIG_CMDLINE_FORCE` ensure the kernel uses the serial command line matching the emulator.
- `CONFIG_SERIAL_AMBA_PL011` and `CONFIG_SERIAL_AMBA_PL011_CONSOLE` enable the PL011 driver and console.
- `CONFIG_ARM_GIC_V3` and `CONFIG_ARM_ARCH_TIMER` match the current minimal GIC/timer bring-up path.
- `CONFIG_DEVTMPFS*`, `CONFIG_EXT4_FS`, `CONFIG_TMPFS`, `CONFIG_BLK_DEV_INITRD` are useful for later rootfs-oriented boot flows.
- `CONFIG_VIRTIO` and `CONFIG_VIRTIO_BLK` keep the configuration aligned with later block-device work.

## Run

### 1. Built-in Demo

```bash
./build/aarchvm
```

### 2. External Raw Binary

```bash
./build/aarchvm \
  -bin <program.bin> \
  -load <load_addr> \
  -entry <entry_pc> \
  -steps <max_steps>
```

Common optional arguments:
- `-sp <addr>`: initialize the startup stack pointer
- `-dtb <file>`: load a DTB
- `-dtb-addr <addr>`: load the DTB at a given address and pass it to the guest in `x0`
- `-segment <file@addr>`: load an extra raw segment at a specific address
- `-snapshot-save <file>`: save a full-machine snapshot at the end of the run
- `-snapshot-load <file>`: resume directly from a full-machine snapshot instead of cold booting

### 3. Run U-Boot

```bash
./build/aarchvm \
  -bin u-boot-2026.01/build-qemu_arm64/u-boot.bin \
  -load 0x0 \
  -entry 0x0 \
  -sp 0x47fff000 \
  -dtb dts/aarchvm-current.dtb \
  -dtb-addr 0x40000000 \
  -steps 50000000
```

### 4. Boot Linux / BusyBox via U-Boot

If you only want to validate the U-Boot -> Linux hand-off, you can feed the `booti` command through a one-shot pipe:

```bash
printf 'setenv bootargs console=ttyAMA0,115200 earlycon=pl011,0x09000000 ignore_loglevel rdinit=/init\nbooti 0x40400000 0x46000000:0x1662f8 0x47f00000\n' | \
AARCHVM_TIMER_SCALE=100000 \
./build/aarchvm \
  -bin u-boot-2026.01/build-qemu_arm64/u-boot.bin \
  -load 0x0 \
  -entry 0x0 \
  -sp 0x47fff000 \
  -dtb dts/aarchvm-current.dtb \
  -dtb-addr 0x40000000 \
  -segment linux-6.12.76/build-aarchvm/arch/arm64/boot/Image@0x40400000 \
  -segment out/initramfs-full.cpio.gz@0x46000000 \
  -segment dts/aarchvm-linux-min.dtb@0x47f00000 \
  -steps 1200000000
```

If you want a truly interactive BusyBox shell, do not consume stdin with `printf | ...`. Start the emulator directly and type the U-Boot commands manually at the prompt:

```bash
AARCHVM_TIMER_SCALE=100000 \
./build/aarchvm \
  -bin u-boot-2026.01/build-qemu_arm64/u-boot.bin \
  -load 0x0 \
  -entry 0x0 \
  -sp 0x47fff000 \
  -dtb dts/aarchvm-current.dtb \
  -dtb-addr 0x40000000 \
  -segment linux-6.12.76/build-aarchvm/arch/arm64/boot/Image@0x40400000 \
  -segment out/initramfs-full.cpio.gz@0x46000000 \
  -segment dts/aarchvm-linux-min.dtb@0x47f00000 \
  -steps 5000000000
```

Type in U-Boot:

```bash
setenv bootargs console=ttyAMA0,115200 earlycon=pl011,0x09000000 ignore_loglevel rdinit=/init
booti 0x40400000 0x46000000:0x1662f8 0x47f00000
```

Artifacts and addresses used by this boot chain:
- Main DTB: `dts/aarchvm-current.dtb`, used so that U-Boot recognizes the current emulated board layout.
- Linux DTB: `dts/aarchvm-linux-min.dtb`, passed as the third `booti` argument.
- Kernel `Image`: loaded at `0x40400000`.
- BusyBox `initramfs`: loaded at `0x46000000`, current size `0x1662f8`.
- Linux DTB: loaded at `0x47f00000`.
- Initial U-Boot `sp`: `0x47fff000`.
- `AARCHVM_TIMER_SCALE=100000`: used to accelerate Linux progress under the interpreter model.

### 5. Use Snapshots to Speed Up Linux Repro Loops

The emulator now supports full-machine snapshots. A typical workflow is to run up to the BusyBox shell and save a snapshot there:

```bash
printf 'setenv bootargs console=ttyAMA0,115200 earlycon=pl011,0x09000000 ignore_loglevel rdinit=/init\nbooti 0x40400000 0x46000000:0x1662f8 0x47f00000\n' | \
AARCHVM_TIMER_SCALE=100000 \
./build/aarchvm \
  -bin u-boot-2026.01/build-qemu_arm64/u-boot.bin \
  -load 0x0 \
  -entry 0x0 \
  -sp 0x47fff000 \
  -dtb dts/aarchvm-current.dtb \
  -dtb-addr 0x40000000 \
  -segment linux-6.12.76/build-aarchvm/arch/arm64/boot/Image@0x40400000 \
  -segment out/initramfs-full.cpio.gz@0x46000000 \
  -segment dts/aarchvm-linux-min.dtb@0x47f00000 \
  -steps 1200000000 \
  -snapshot-save out/linux-shell-v2.snap
```

Then resume directly near the shell with:

```bash
./build/aarchvm -snapshot-load out/linux-shell-v2.snap -steps 500000000
```

This is especially useful when debugging serial input, GIC, timer, syscall, or user-space shell issues.

## Current DT Conventions

### Linux DTB: `dts/aarchvm-linux-min.dts`

The current Linux boot DTB uses:
- RAM at `0x40000000`, size `128 MiB`
- GICv3 at `0x08000000`
- PL011 UART at `0x09000000`
- architected timer DT node with PPIs `<13, 14, 11, 10>`
- serial command line `console=ttyAMA0,115200 earlycon=pl011,0x09000000 ignore_loglevel`

### U-Boot DTB: `dts/aarchvm-current.dts`

This DTB is currently used to:
- let U-Boot understand the RAM / UART / GIC layout of the emulator
- keep U-Boot serial output aligned with the PL011 model
- preserve a consistent physical-address layout across the U-Boot -> Linux chain

## MMU / Interrupt / Serial Summary

The current implementation already covers and validates:
- `TTBR0_EL1/TTBR1_EL1` Stage-1 translation
- 4KB-granule page-table walks
- table-attribute inheritance
- software TLB behavior and `TLBI`
- synchronous exception handling via `ESR_EL1/FAR_EL1/ELR_EL1/SPSR_EL1`
- minimal GICv3 system-register CPU interface
- minimal Generic Timer sysreg path
- PL011 earlycon, normal console output, and BusyBox shell input
- `WFI` wakeup semantics plus nested IRQ delivery while already in EL1 exception context
- a growing set of atomics, sysregs, cache-maintenance instructions, and load/store forms encountered during Linux bring-up
- full-machine snapshot save / restore

The shell-input fix depended on two concrete behavioral corrections in the CPU model:
- `WFI/WFE` now leave the wait state as soon as an interrupt becomes pending, even if `PSTATE.I` still delays immediate delivery.
- IRQ entry is now permitted while Linux is already running inside EL1 exception context, which is required by the kernel idle/console path.

The `busybox ps` illegal-instruction failure was traced to a small cluster of AdvSIMD semantic gaps:
- `BIC (vector, immediate)` previously fell through a generic logic-immediate path; it now implements the correct `operand AND NOT(imm)` behavior.
- `DUP (element)` was missing and is now implemented.
- The vector-logic decoder was too broad, so `EOR` could be misrouted into the `BIT/BIF/BSL` path; that decode has been tightened.

With these fixes in place, BusyBox `ps` now prints the process list correctly in the guest and returns `0`.

## Tests

Build all test binaries:

```bash
tests/arm64/build_tests.sh
```

Run the full regression suite:

```bash
tests/arm64/run_all.sh
```

The current regression suite covers:
- per-instruction bring-up tests for previously implemented ISA subsets
- MMU/TLB/cache-maintenance tests
- synchronous exception register tests
- GICv3 + timer sysreg IRQ tests
- `irq_nested_el1_wfi` for `WFI` wakeup plus nested EL1 IRQ delivery
- `snapshot_resume` for full-machine snapshot save / restore
- atomic / exclusive-access tests
- additional instruction samples added during Linux bring-up
- `fpsimd_bic_imm` for `BIC (vector, immediate)`
- `fpsimd_dup_elem` for `DUP (element)`
- expanded `fpsimd_logic_more` coverage for `EOR/BIT/BIF/BSL` separation

## Current Limitations and Next Steps

Even though the current tree can already enter a BusyBox shell and handle basic interactive input, several platform-level pieces still matter for a more complete Linux system:
- more complete GICv3 distributor / redistributor MMIO behavior
- more complete architected timer and power-management behavior
- storage / block-device models required by rootfs mounting
- more complete PSCI / SMCCC / power-management flows
- continued ISA and sysreg expansion for deeper Linux init paths

## Reference

- `DDI0487_M.a.a_a-profile_architecture_reference_manual.pdf`
