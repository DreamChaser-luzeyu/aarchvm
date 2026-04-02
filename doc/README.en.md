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

Optional for the 2-core SMP / GUI path:

```bash
./tests/linux/build_linux_smp_shell_snapshot.sh
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

7. Run the Linux SMP functional suite

```bash
./tests/linux/run_functional_suite_smp.sh
```

8. Manually validate the GUI path

```bash
./tests/linux/run_gui_tty1.sh
```

## Current Validated Status

The following paths are currently implemented and exercised by in-tree regression flows:
- Single-core AArch64 EL1 interpreter execution.
- Same-thread round-robin SMP execution, currently validated for `-smp 2`.
- Current Linux-facing platform path: low boot RAM alias, 1 GiB SDRAM, PL011 UART, PL031 RTC, Generic Timer, minimal GICv3, SDL framebuffer, PL050 KMI keyboard, virtio-mmio block, and perf mailbox.
- Minimal synchronous exception loop with `ESR_EL1`, `FAR_EL1`, `ELR_EL1`, and `SPSR_EL1`.
- Minimal MMU/TLB behavior required by early Linux page-table bring-up.
- U-Boot serial boot and Linux hand-off via `booti`.
- Linux serial output through both `earlycon=pl011,...` and normal `ttyAMA0`.
- Interactive BusyBox shell over serial.
- SDL framebuffer output visible to U-Boot and Linux `simpledrm` / `fbcon`.
- PL050 KMI keyboard device recognized by Linux through `CONFIG_SERIO_AMBAKMI` + `CONFIG_KEYBOARD_ATKBD`.
- Host-backed PL031 RTC recognized by Linux through `rtc-pl031`, with `/sys/class/rtc/rtc0` enumeration and read/set smoke coverage in the Linux functional suites.
- Standard Linux `virtio-mmio + virtio-blk` raw disk path, validated through `/dev/vda` enumeration plus a read-only Debian ext4 mount smoke.
- Full-machine snapshot save / restore.
- In-tree bare-metal regression, Linux functional regression, and Linux algorithm/perf regression suites.
- Linux SMP smoke bring-up through PSCI secondary boot to BusyBox shell, with user space observing 2 CPUs in `/proc/cpuinfo`.

Current SMP scope and limits:
- The validated SMP path now includes both bare-metal tests and a 2-core Linux smoke path (`-smp 2 -smp-mode psci`).
- It covers `MPIDR_EL1`, PSCI `CPU_ON`, per-CPU GIC redistributor discovery, per-CPU generic timer delivery, cross-core `SEV/WFE`, and cross-core exclusive-monitor invalidation / `LDAXR`/`STLXR` locking.
- The current implementation still uses same-thread round-robin scheduling and remains a minimal program-visible model rather than a cycle-accurate SMP machine.
- Automatic Linux regression now covers both the default single-core serial path and the 2-core SMP shell path.
- The SoC outer scheduler now defaults to `AARCHVM_SCHED_MODE=event`; `legacy` is still available for debugging and A/B measurement.

Automatic Linux regression runs intentionally use the serial-only path and explicitly disable SDL output for reproducibility and speed. GUI and PS/2 keyboard validation are kept as a separate manual path.

## Current Device Model

The current repository includes and uses the following device / platform pieces:
- low boot RAM alias plus 1 GiB SDRAM
- PL011 UART
- PL031 RTC
- ARM Generic Timer system-register path
- Minimal GICv3 distributor / redistributor / CPU-interface path
- perf mailbox MMIO block used by the Linux perf / benchmark helpers
- framebuffer RAM region described as `simple-framebuffer`
- SDL window backend for presenting framebuffer contents
- PL050 KMI keyboard controller
- standard `virtio,mmio` transport with a `virtio-blk` device behind `-drive`
- full-machine snapshot support

The Linux-facing DTs already contain the relevant nodes in:
- `dts/aarchvm-current.dts`
- `dts/aarchvm-linux-min.dts`

That includes:
- `simple-framebuffer`
- `arm,pl050` / `arm,primecell`
- `virtio,mmio`

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
- Linux boot DTBs: `dts/aarchvm-linux-min.dtb`, `dts/aarchvm-linux-smp.dtb`
- Unified user-test initramfs: `out/initramfs-usertests.cpio`
- Linux shell snapshot: `out/linux-usertests-shell-v1.snap`
- Linux SMP shell snapshot: `out/linux-smp-shell-v1.snap`
- SMP shell snapshot build wrapper: `tests/linux/build_linux_smp_shell_snapshot.sh`
- Fast serial restore helper: `tests/linux/run_interactive.sh`
- GUI snapshot restore helper: `tests/linux/run_gui_tty1_from_snapshot.sh`
- Linux SMP functional suite: `tests/linux/run_functional_suite_smp.sh`

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
  --enable FRAMEBUFFER_CONSOLE \
  --enable VIRTIO \
  --enable VIRTIO_MMIO \
  --enable VIRTIO_BLK \
  --enable EXT4_FS

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

### 7. Build a Debian ext4 image for the block-device smoke

```bash
./tests/linux/build_debian_rootfs_image.sh
```

The script now runs directly on the host with native `debootstrap` only.
It no longer falls back to Docker or `--foreign` / second-stage flows.
On non-arm64 hosts, native arm64 bootstrapping requires `qemu-aarch64` binfmt support.
Run it as `root` or via `sudo`; the script will restore output ownership when invoked through `sudo`.

Useful knobs:
- `AARCHVM_DEBIAN_DEBOOTSTRAP_MODE=auto|native`
- `AARCHVM_DEBIAN_SUITE=<suite>`
- `AARCHVM_DEBIAN_ARCH=<arch>`
- `AARCHVM_DEBIAN_MIRROR=<mirror>`
- `AARCHVM_DEBIAN_VARIANT=<variant>`

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
  -smp <cpu_count> \
  -steps <max_steps>
```

Common options:
- `-sp <addr>`: set the startup stack pointer
- `-smp <n>`: create `n` CPUs; `1` is the default, and the current validated SMP smoke path is `2`
- `-smp-mode <all|psci>`: choose how secondary CPUs start; `all` powers on every CPU at reset, `psci` keeps secondary CPUs off until PSCI `CPU_ON`
- `-dtb <file>`: load a DTB
- `-dtb-addr <addr>`: place the DTB at a given address and pass it in `x0`
- `-segment <file@addr>`: load an extra image segment
- `-snapshot-save <file>`: save a full-machine snapshot at the end of the run
- `-snapshot-load <file>`: resume from a snapshot
- `-drive <image.bin>`: attach a raw image to the standard `virtio-mmio + virtio-blk` device
- `-stop-on-uart <text>`: stop immediately when UART output matches a string
- `-decode <fast|slow>`: switch decode execution path, default is the fast path
- `-fb-sdl <on|off>`: explicitly enable or disable the SDL framebuffer window
- `-arch-timer-mode <step|host>`: choose the architectural timer source. `step` keeps the deterministic guest-step counter used by regression/perf flows; `host` makes `CNTVCT/CNTPCT` follow the host monotonic clock for interactive Linux timing

Behavior-control environment variables:
- `AARCHVM_BRK_MODE=trap|halt`: control how the emulator handles A64 `BRK`. The default is `trap`, which follows the architected Breakpoint Instruction exception model. `halt` keeps the historical bare-metal test behavior where `BRK` immediately stops the emulator; `tests/arm64/run_all.sh` exports this mode for legacy stop semantics.
- `AARCHVM_STOP_ON_UART=<text>`: environment-variable form of `-stop-on-uart`.
- `AARCHVM_DTB_PATH=<file>` / `AARCHVM_DTB_ADDR=<addr>`: environment-variable DTB injection path, used when the DTB is not passed explicitly on the command line.
- `AARCHVM_UART_TX_MATCH=<text>` + `AARCHVM_UART_TX_REPLY=<text>`: one-shot host-side UART prompt matcher / auto-reply pair. This is useful for scripted U-Boot hand-off without permanently replacing interactive stdin.
- `AARCHVM_FB_SDL=0|1`: default SDL framebuffer enable switch when `-fb-sdl` is not specified.
- `AARCHVM_ARCH_TIMER_MODE=step|host`: environment-variable form of `-arch-timer-mode`. Use `host` for interactive Linux sessions when you want guest `CLOCK_REALTIME/CLOCK_MONOTONIC` to run at host speed.

Interactive serial shortcut:
- when stdin is a TTY, press `Ctrl+A`, then `x` to stop the emulator immediately
- if `-snapshot-save <file>` is active, the final snapshot is still written during this exit path

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

For the 2-core SMP shell snapshot, use:

```bash
./tests/linux/build_linux_smp_shell_snapshot.sh
```

This wrapper reuses the same flow but adds `-smp 2 -smp-mode psci` and `dts/aarchvm-linux-smp.dtb`.

Automatic regression uses:
- `-fb-sdl off`
- `console=ttyAMA0,115200 earlycon=pl011,0x09000000 rdinit=/init initramfs_async=0`

This is the recommended reproducible path for regression and performance runs.

### 5. Restore directly into the interactive serial shell

```bash
./tests/linux/run_interactive.sh
```

This restores `out/linux-usertests-shell-v1.snap` and is the fastest way to get back into the BusyBox serial shell once the snapshot has already been created.
It defaults to `-arch-timer-mode host`, so the Linux shell sees a host-paced architectural timer instead of the deterministic regression timer model.

When running interactively on the host terminal, you can exit with the QEMU-style serial escape `Ctrl+A`, then `x`.

If you already saved a GUI snapshot through `run_gui_tty1.sh`, you can restore it with:

```bash
./tests/linux/run_gui_tty1_from_snapshot.sh
```

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

This path now cold-boots the Debian `systemd` block-image through the Debian handoff initramfs, with a command line of the form:

```text
console=ttyAMA0,115200 console=tty1 earlycon=pl011,0x09000000 rdinit=/init root=/dev/vda rw rootfstype=ext4 initramfs_async=0 systemd.unit=multi-user.target systemd.log_level=info systemd.log_target=console
```

Meaning:
- keep serial logging for debugging
- make `tty1` the active Linux console so the Debian login prompt appears on the framebuffer
- boot the GUI path with the 2-core SMP DTB, matching the script
- let the initramfs mount `/dev/vda` and switch into the Debian rootfs
- deliver SDL window keyboard input through the PL050 PS/2 keyboard device

Default credentials:
- framebuffer `tty1`: log in as `root` with password `000000`
- serial `ttyAMA0`: the generated Debian test image still enables `root` autologin

`run_gui_tty1.sh` is the cold-boot GUI path and saves a snapshot at the end by default. `run_gui_tty1_from_snapshot.sh` is the matching fast-restore helper once that snapshot already exists.
Both GUI helpers default to `-arch-timer-mode host` for the same reason as `run_interactive.sh`: the framebuffer shell should see normal wall-clock progression during manual use.

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
- `tests/linux/build_linux_smp_shell_snapshot.sh`
- `tests/linux/build_debian_rootfs_image.sh`
- `tests/linux/run_interactive.sh`
- `tests/linux/run_functional_suite.sh`
- `tests/linux/run_algorithm_perf.sh`
- `tests/linux/run_functional_suite_smp.sh`
- `tests/linux/run_block_mount_smoke.sh`
- `tests/linux/run_gui_tty1.sh`
- `tests/linux/run_gui_tty1_from_snapshot.sh`

Where:
- `build_linux_shell_snapshot.sh`, `build_linux_smp_shell_snapshot.sh`, `run_functional_suite.sh`, `run_algorithm_perf.sh`, and `run_functional_suite_smp.sh` are automation-oriented and intentionally serial-only
- `build_debian_rootfs_image.sh` prepares a Debian ext4 image for the block-device smoke path
- `run_block_mount_smoke.sh` cold-boots Linux, mounts `/dev/vda`, and checks that the Debian rootfs is visible
- `run_interactive.sh` is the quickest manual serial-shell restore path
- `run_gui_tty1.sh` and `run_gui_tty1_from_snapshot.sh` are the manual GUI validation / restore paths

## Injection and Debugging Notes

The current implementation supports these commonly used mechanisms:
- `AARCHVM_UART_RX_SCRIPT`: inject bytes into UART at selected step counts for automated serial tests.
- `AARCHVM_PS2_RX_SCRIPT`: inject bytes into the PS/2 keyboard device at selected step counts for KMI / keyboard testing.
- `AARCHVM_BUS_FASTPATH=1`: enable the bus fast path.
- `AARCHVM_TIMER_SCALE=<n>`: scale the guest-step architectural timer progression used by deterministic regression/perf paths.
- `AARCHVM_ARCH_TIMER_MODE=step|host`: select between deterministic guest-step timer mode and host-monotonic timer mode.
- `AARCHVM_SCHED_MODE=event|legacy`: choose the SoC outer scheduler. `event` is the default and is the semantically correct mode for the current SMP/Linux timer paths. `legacy` keeps the older fixed-step fallback and is useful for debugging or A/B measurement, but it can delay near-term SMP timer delivery and should not be treated as behavior-equivalent.
- The current automation scripts keep `-arch-timer-mode step` and `AARCHVM_TIMER_SCALE=1` for reproducible regression/perf behavior. The manual restore helpers (`run_interactive.sh`, `run_gui_tty1.sh`, `run_gui_tty1_from_snapshot.sh`) switch to host timer mode by default.
- `AARCHVM_STDIN_RX_GAP=<steps>`: pace bytes from non-interactive stdin before they reach UART, useful for scripted serial sessions and bulk command injection.
- `AARCHVM_DEBUG_SLOW=1`: force a conservative debug execution mode. This disables instruction predecode, disables the SoC bus fast path, and disables the CPU RAM direct read/write fast path so regressions can be checked without those host-side shortcuts.
- `AARCHVM_PRINT_SUMMARY=1`: print the final global step count and, in SMP mode, a per-CPU summary. The snapshot build / verify scripts use this to derive prompt-step checkpoints.
- `AARCHVM_TRACE_WRITE_VA=<va>` / `AARCHVM_TRACE_WRITE_PA=<pa>`: log guest writes that touch a selected virtual or physical address. Useful for chasing memory corruption, aliasing, or unexpected page-crossing stores.
- `AARCHVM_TRACE_BRANCH_ZERO=1`: when `BR` / `BLR` / `RET` would branch to address zero, print the branch source, register state, and the current contents of the PLT/GOT target slot.

## Important Notes

- The current automated boot flow does not rely on `CONFIG_CMDLINE_FORCE=y`. Boot arguments are injected by the U-Boot-side scripts at runtime.
- The GUI path is intentionally separate from the automated regression path. Automation disables SDL to minimize timing perturbation and improve reproducibility.
- Framebuffer and PS/2 keyboard support are now modeled as Linux / U-Boot-compatible devices rather than by forwarding keyboard input into UART.
- The project is still not a full architectural implementation of AArch64. What is guaranteed today is the behavior covered by the in-tree regression suites.
