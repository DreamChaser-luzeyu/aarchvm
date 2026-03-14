# aarchvm

`aarchvm` is a C++ / CMake AArch64 full-system emulator using an interpreter execution model.
The current tree has already been validated for the following end-to-end loop: cold-boot U-Boot, load a Linux `Image`, enter a BusyBox `initramfs`, interact through a serial shell, restore from full-machine snapshots, render through an SDL framebuffer, and inject keyboard input through a PL050 KMI PS/2 path.

## Quick Start

If you just want to get the current tree running quickly, use this order:

1. Build the emulator

```bash
cmake -S . -B build
cmake --build build -j
```

2. Prepare the base BusyBox rootfs, then build the unified user-test initramfs

Note: `tests/linux/build_usertests_rootfs.sh` currently reuses an existing `out/initramfs-full-root`, so you need to prepare that BusyBox base rootfs once before the first run. The exact commands are listed in the Build section below. Once the base rootfs exists, run:

```bash
./tests/linux/build_usertests_rootfs.sh
```

3. Build the Linux shell snapshot

```bash
./tests/linux/build_linux_shell_snapshot.sh
```

4. Run the bare-metal regression suite

```bash
./tests/arm64/run_all.sh
```

5. Run the Linux functional suite

```bash
./tests/linux/run_functional_suite.sh
```

6. Run the Linux algorithm / perf suite

```bash
./tests/linux/run_algorithm_perf.sh
```

7. Manually validate the GUI path

```bash
./tests/linux/run_gui_tty1.sh
```

## Current Validated Status

The following paths are currently implemented and exercised by in-tree regression flows:
- Single-core AArch64 EL1 interpreter execution.
- Minimal SoC device set: RAM, PL011 UART, Generic Timer, and minimal GICv3.
- Minimal synchronous exception loop with `ESR_EL1`, `FAR_EL1`, `ELR_EL1`, and `SPSR_EL1`.
- Minimal MMU/TLB behavior required by early Linux page-table bring-up.
- U-Boot serial boot and Linux hand-off via `booti`.
- Linux serial output through both `earlycon=pl011,...` and normal `ttyAMA0`.
- Interactive BusyBox shell over serial.
- SDL framebuffer output visible to U-Boot and Linux `simpledrm` / `fbcon`.
- PL050 KMI keyboard device recognized by Linux through `CONFIG_SERIO_AMBAKMI` + `CONFIG_KEYBOARD_ATKBD`.
- Full-machine snapshot save / restore.
- In-tree bare-metal regression, Linux functional regression, and Linux algorithm/perf regression suites.

Automatic Linux regression runs intentionally use the serial-only path and explicitly disable SDL output for reproducibility and speed. GUI and PS/2 keyboard validation are kept as a separate manual path.

## Current Device Model

The current repository includes and uses the following device / platform pieces:
- RAM
- PL011 UART
- ARM Generic Timer system-register path
- Minimal GICv3 distributor / redistributor / CPU-interface path
- framebuffer RAM region described as `simple-framebuffer`
- SDL window backend for presenting framebuffer contents
- PL050 KMI keyboard controller
- basic block-device path and related state save/restore
- full-machine snapshot support

The Linux-facing DTs already contain the relevant nodes in:
- `dts/aarchvm-current.dts`
- `dts/aarchvm-linux-min.dts`

That includes:
- `simple-framebuffer`
- `arm,pl050` / `arm,primecell`

## Toolchain Version

The cross toolchain actually used and validated in this workspace is:

```text
aarch64-linux-gnu-gcc (Debian 14.2.0-19) 14.2.0
GNU ld (GNU Binutils for Debian) 2.44
GNU as (GNU Binutils for Debian) 2.44
```

You can confirm it with:

```bash
aarch64-linux-gnu-gcc --version | head -n 1
aarch64-linux-gnu-ld --version | head -n 1
aarch64-linux-gnu-as --version | head -n 1
```

## Layout and Key Artifacts

- Emulator sources: `src/`, `include/`
- Emulator build dir: `build/`
- U-Boot sources: `u-boot-2026.01/`
- U-Boot build dir: `u-boot-2026.01/build-qemu_arm64/`
- Linux sources: `linux-6.12.76/`
- Linux build dir: `linux-6.12.76/build-aarchvm/`
- Control DTB: `dts/aarchvm-current.dtb`
- Linux boot DTB: `dts/aarchvm-linux-min.dtb`
- Unified user-test initramfs: `out/initramfs-usertests.cpio`
- Linux shell snapshot: `out/linux-usertests-shell-v1.snap`

## Build

### 1. Build the emulator

```bash
cmake -S . -B build
cmake --build build -j
```

### 2. Build U-Boot

The validated setup currently uses `qemu_arm64_defconfig`:

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

Key artifact:
- `u-boot-2026.01/build-qemu_arm64/u-boot.bin`

### 3. Build Linux

A good baseline is `defconfig`, followed by the minimum options required by the current emulator path.
In the current flow, kernel boot arguments are injected by U-Boot at runtime, so the setup no longer depends on `CONFIG_CMDLINE_FORCE=y`.

```bash
make -C linux-6.12.76 \
  O=build-aarchvm \
  ARCH=arm64 \
  CROSS_COMPILE=aarch64-linux-gnu- \
  defconfig
```

Suggested key options:

```bash
linux-6.12.76/scripts/config --file linux-6.12.76/build-aarchvm/.config \
  --enable PRINTK \
  --enable ARM_AMBA \
  --enable ARM_GIC_V3 \
  --enable ARM_ARCH_TIMER \
  --enable SERIAL_AMBA_PL011 \
  --enable SERIAL_AMBA_PL011_CONSOLE \
  --enable DEVTMPFS \
  --enable DEVTMPFS_MOUNT \
  --enable BLK_DEV_INITRD \
  --enable RD_GZIP \
  --enable INPUT_EVDEV \
  --enable SERIO \
  --enable SERIO_AMBAKMI \
  --enable KEYBOARD_ATKBD \
  --enable VT \
  --enable VT_CONSOLE \
  --enable FB \
  --enable DRM_SIMPLEDRM \
  --enable FRAMEBUFFER_CONSOLE

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

Key artifacts:
- `linux-6.12.76/build-aarchvm/arch/arm64/boot/Image`
- `linux-6.12.76/build-aarchvm/vmlinux`

### 4. Generate DTBs

If `dtc` is not installed globally, use the one built inside the Linux tree:

```bash
linux-6.12.76/build-aarchvm/scripts/dtc/dtc \
  -I dts -O dtb \
  -o dts/aarchvm-linux-min.dtb \
  dts/aarchvm-linux-min.dts

linux-6.12.76/build-aarchvm/scripts/dtc/dtc \
  -I dts -O dtb \
  -o dts/aarchvm-current.dtb \
  dts/aarchvm-current.dts
```

### 5. Prepare the base BusyBox rootfs

`tests/linux/build_usertests_rootfs.sh` currently reuses an existing `out/initramfs-full-root` tree as its base rootfs, so you need to prepare that once before the first run.

One straightforward way is:

```bash
make -C busybox-1.37.0 distclean
make -C busybox-1.37.0 defconfig
perl -0pi -e 's/^# CONFIG_STATIC is not set$/CONFIG_STATIC=y/m' busybox-1.37.0/.config
make -C busybox-1.37.0 oldconfig </dev/null
make -C busybox-1.37.0 -j"$(nproc)"
make -C busybox-1.37.0 install

rm -rf out/initramfs-full-root
mkdir -p out/initramfs-full-root
cp -a busybox-1.37.0/_install/. out/initramfs-full-root/
mkdir -p out/initramfs-full-root/{dev,proc,sys,tmp,run,root,mnt,etc}
```

### 6. Build the unified user-test initramfs

Once the base BusyBox rootfs exists, run:

```bash
./tests/linux/build_usertests_rootfs.sh
```

This script will:
- build the statically linked BusyBox-side test binaries
- reuse `out/initramfs-full-root` to create `out/initramfs-usertests-root`
- generate `out/initramfs-usertests.cpio`
- generate `out/initramfs-usertests.cpio.gz`
- install an `/init` script that selects shell, functional suite, or perf suite based on the kernel command line

## Run

### 1. Minimal built-in demo

```bash
./build/aarchvm
```

### 2. External raw binary

```bash
./build/aarchvm \
  -bin <program.bin> \
  -load <load_addr> \
  -entry <entry_pc> \
  -steps <max_steps>
```

Common options:
- `-sp <addr>`: set the startup stack pointer
- `-dtb <file>`: load a DTB
- `-dtb-addr <addr>`: place the DTB at a given address and pass it in `x0`
- `-segment <file@addr>`: load an extra image segment
- `-snapshot-save <file>`: save a full-machine snapshot at the end of the run
- `-snapshot-load <file>`: resume from a snapshot
- `-stop-on-uart <text>`: stop immediately when UART output matches a string
- `-decode <fast|slow>`: switch decode execution path, default is the fast path
- `-fb-sdl <on|off>`: explicitly enable or disable the SDL framebuffer window

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

### 4. Build the Linux shell snapshot automatically

```bash
./tests/linux/build_linux_shell_snapshot.sh
```

This script will:
- build the unified initramfs if needed
- cold-boot U-Boot -> Linux -> BusyBox
- stop immediately when the serial prompt `~ # ` is observed via `-stop-on-uart`
- save the snapshot to `out/linux-usertests-shell-v1.snap`
- immediately run a snapshot-restore sanity check

Automatic regression uses:
- `-fb-sdl off`
- `console=ttyAMA0,115200 earlycon=pl011,0x09000000 rdinit=/init initramfs_async=0`

This is the recommended reproducible path for regression and performance runs.

### 5. Restore directly into the interactive serial shell

```bash
./tests/linux/run_interactive.sh
```

This restores `out/linux-usertests-shell-v1.snap` and is the fastest way to get back into the BusyBox serial shell once the snapshot has already been created.

### 6. Linux functional regression

```bash
./tests/linux/run_functional_suite.sh
```

This script automatically:
- ensures the shell snapshot / build log exists
- derives the shell prompt step count from the build log
- injects `run_functional_suite` through scripted UART input
- validates outputs from `uname`, `ps`, `ping -c 1 127.0.0.1`, mounts, FPSIMD tests, and FP/integer tests

### 7. Linux algorithm / perf regression

```bash
./tests/linux/run_algorithm_perf.sh
```

This script automatically:
- cold-boots into the shell
- injects `bench_runner` based on the recorded prompt step
- collects `PERF-RESULT` lines
- writes them to `out/perf-suite-results.txt`

### 8. GUI path: framebuffer + PS/2 keyboard + `tty1`

```bash
./tests/linux/run_gui_tty1.sh
```

This path boots with a command line of the form:

```text
console=tty0 console=ttyAMA0,115200 earlycon=pl011,0x09000000 rdinit=/init initramfs_async=0 aarchvm_shell=tty1
```

Meaning:
- keep serial logging for debugging
- also bind the Linux console to `tty0`
- let `/init` switch the BusyBox shell to `/dev/tty1` when `aarchvm_shell=tty1` is present
- deliver SDL window keyboard input through the PL050 PS/2 keyboard device

## Test Entry Points

### Bare-metal regression

```bash
./tests/arm64/build_tests.sh
./tests/arm64/run_all.sh
```

This suite covers and validates multiple integer, sysreg, interrupt, and newly added KMI / PS2-related paths.

### Linux regression

The current recommended Linux-side scripts are:
- `tests/linux/build_usertests_rootfs.sh`
- `tests/linux/build_linux_shell_snapshot.sh`
- `tests/linux/run_functional_suite.sh`
- `tests/linux/run_algorithm_perf.sh`
- `tests/linux/run_gui_tty1.sh`

Where:
- the first four are automation-oriented and intentionally serial-only
- `run_gui_tty1.sh` is the manual GUI validation path

## Injection and Debugging Notes

The current implementation supports these commonly used mechanisms:
- `AARCHVM_UART_RX_SCRIPT`: inject bytes into UART at selected step counts for automated serial tests.
- `AARCHVM_PS2_RX_SCRIPT`: inject bytes into the PS/2 keyboard device at selected step counts for KMI / keyboard testing.
- `AARCHVM_BUS_FASTPATH=1`: enable the bus fast path.
- `AARCHVM_TIMER_SCALE=<n>`: scale virtual timer progression to accelerate Linux boot and regression runs.

## Important Notes

- The current automated boot flow does not rely on `CONFIG_CMDLINE_FORCE=y`. Boot arguments are injected by the U-Boot-side scripts at runtime.
- The GUI path is intentionally separate from the automated regression path. Automation disables SDL to minimize timing perturbation and improve reproducibility.
- Framebuffer and PS/2 keyboard support are now modeled as Linux / U-Boot-compatible devices rather than by forwarding keyboard input into UART.
- The project is still not a full architectural implementation of AArch64. What is guaranteed today is the behavior covered by the in-tree regression suites.
