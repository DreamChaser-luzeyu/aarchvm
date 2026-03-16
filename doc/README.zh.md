# aarchvm

`aarchvm` 是一个使用 C++ 与 CMake 开发的 AArch64 全系统解释执行模拟器。
当前代码库已经验证可完成以下闭环：U-Boot 冷启动、加载 Linux `Image`、进入 BusyBox `initramfs`、串口交互 shell、整机快照恢复、SDL framebuffer 输出，以及通过 PL050 KMI 提供的 PS/2 键盘输入。

## Quick Start

如果你只想尽快跑通当前仓库，推荐按下面的顺序：

1. 构建模拟器

```bash
cmake -S . -B build
cmake --build build -j
```

2. 准备基础 BusyBox rootfs，并构建统一用户态测试 initramfs

说明：`tests/linux/build_usertests_rootfs.sh` 目前会复用已有的 `out/initramfs-full-root`，因此第一次使用前需要先准备 BusyBox 基础 rootfs。具体命令见下文“构建”章节。完成基础 rootfs 后执行：

```bash
./tests/linux/build_usertests_rootfs.sh
```

3. 构建 Linux shell 快照

```bash
./tests/linux/build_linux_shell_snapshot.sh
```

4. 运行裸机回归

```bash
./tests/arm64/run_all.sh
```

5. 运行 Linux 功能回归

```bash
./tests/linux/run_functional_suite.sh
```

6. 运行 Linux 算法/性能回归

```bash
./tests/linux/run_algorithm_perf.sh
```

7. 运行 Linux SMP 功能回归

```bash
./tests/linux/run_functional_suite_smp.sh
```

8. 手工验证 GUI 路径

```bash
./tests/linux/run_gui_tty1.sh
```

## 当前验证状态

当前仓库已经实际跑通并回归过的路径包括：
- 单核 AArch64 EL1 解释执行。
- 同线程 round-robin 的 SMP 执行路径，当前已验证 `-smp 2`。
- 最小 SoC 设备集合：RAM、PL011 UART、Generic Timer、最小 GICv3。
- 最小同步异常闭环：`ESR_EL1`、`FAR_EL1`、`ELR_EL1`、`SPSR_EL1`。
- Linux 早期页表所需的最小 MMU/TLB 路径。
- U-Boot 串口启动与 `booti` 引导 Linux。
- Linux `earlycon=pl011,...` 与正式 `ttyAMA0` 串口输出。
- BusyBox 交互式串口 shell。
- SDL framebuffer 输出路径，可显示 U-Boot 与 Linux `simpledrm` / `fbcon` 图像。
- PL050 KMI 键盘设备，Linux 侧可由 `CONFIG_SERIO_AMBAKMI` + `CONFIG_KEYBOARD_ATKBD` 驱动识别。
- 整机 snapshot 保存 / 恢复。
- 仓库内裸机回归、Linux 功能回归、Linux 算法/性能回归。
- Linux SMP 冒烟路径：通过 PSCI 启动次级核，进入 BusyBox shell，用户态 `/proc/cpuinfo` 可见 2 个 CPU。

当前 SMP 范围与限制：
- 当前已验证的 SMP 已覆盖裸机与 2 核 Linux 冒烟路径（`-smp 2 -smp-mode psci`）。
- 已覆盖的程序可见行为包括 `MPIDR_EL1`、PSCI `CPU_ON`、每核 GIC redistributor 发现、每核 Generic Timer 中断递送、跨核 `SEV/WFE`，以及跨核 exclusive monitor 失效与 `LDAXR`/`STLXR` 自旋锁。
- 当前实现仍采用同线程 round-robin 调度，目标是程序可感知正确性，而不是周期精确的 SMP 机器模型。
- 自动化 Linux 回归已经覆盖默认的单核串口路径，以及 2 核 SMP shell 路径。

当前自动回归默认走串口路径，并显式关闭 SDL 窗口，以保证可重复性与速度。图形界面和 PS/2 键盘路径使用单独的手工脚本验证。

## 当前外设模型

当前已经实现并在实际启动路径中使用的外设 / 机制包括：
- RAM
- PL011 UART
- ARM Generic Timer 系统寄存器路径
- 最小 GICv3 分发器 / redistributor / CPU 接口路径
- `simple-framebuffer` 对应的 framebuffer RAM 区域
- SDL 窗口后端，用于把 framebuffer 内容显示到宿主机窗口
- PL050 KMI 键盘控制器
- 基础块设备路径与相关状态保存
- 整机 snapshot

与 Linux GUI 路径相关的设备树节点已经在以下文件中提供：
- `dts/aarchvm-current.dts`
- `dts/aarchvm-linux-min.dts`

其中包括：
- `simple-framebuffer`
- `arm,pl050` / `arm,primecell`

## 工具链版本

当前仓库已实际验证过的交叉工具链版本为：

```text
aarch64-linux-gnu-gcc (Debian 14.2.0-19) 14.2.0
GNU ld (GNU Binutils for Debian) 2.44
GNU as (GNU Binutils for Debian) 2.44
```

可通过以下命令确认：

```bash
aarch64-linux-gnu-gcc --version | head -n 1
aarch64-linux-gnu-ld --version | head -n 1
aarch64-linux-gnu-as --version | head -n 1
```

## 目录与关键构件

- 模拟器源码：`src/`、`include/`
- 模拟器构建目录：`build/`
- U-Boot 源码：`u-boot-2026.01/`
- U-Boot 构建目录：`u-boot-2026.01/build-qemu_arm64/`
- Linux 源码：`linux-6.12.76/`
- Linux 构建目录：`linux-6.12.76/build-aarchvm/`
- 控制 DTB：`dts/aarchvm-current.dtb`
- Linux 启动 DTB：`dts/aarchvm-linux-min.dtb`、`dts/aarchvm-linux-smp.dtb`
- 用户态测试 initramfs：`out/initramfs-usertests.cpio`
- Linux shell 快照：`out/linux-usertests-shell-v1.snap`
- Linux SMP shell 快照：`out/linux-smp-shell-v1.snap`
- Linux SMP 功能回归脚本：`tests/linux/run_functional_suite_smp.sh`

## 构建

### 1. 构建模拟器

```bash
cmake -S . -B build
cmake --build build -j
```

### 2. 构建 U-Boot

当前验证使用 `qemu_arm64_defconfig`：

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

关键产物：
- `u-boot-2026.01/build-qemu_arm64/u-boot.bin`

### 3. 构建 Linux

推荐从 `defconfig` 开始，然后启用当前模拟器路径所需的最小选项。
当前工作流中，内核命令行由 U-Boot 脚本在运行时传入，不再依赖 `CONFIG_CMDLINE_FORCE=y`。

```bash
make -C linux-6.12.76 \
  O=build-aarchvm \
  ARCH=arm64 \
  CROSS_COMPILE=aarch64-linux-gnu- \
  defconfig
```

建议启用的关键配置项：

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

关键产物：
- `linux-6.12.76/build-aarchvm/arch/arm64/boot/Image`
- `linux-6.12.76/build-aarchvm/vmlinux`

### 4. 生成 DTB

如果系统没有全局安装 `dtc`，可直接使用 Linux 构建目录中的 `dtc`：

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

### 5. 准备 BusyBox 基础 rootfs

`tests/linux/build_usertests_rootfs.sh` 目前会复用一个已经存在的 `out/initramfs-full-root` 作为基础 rootfs，因此第一次使用前需要先准备它。

一种最直接的准备方式如下：

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

### 6. 构建统一用户态测试 initramfs

在基础 BusyBox rootfs 准备好之后，再执行：

```bash
./tests/linux/build_usertests_rootfs.sh
```

该脚本会：
- 编译静态 BusyBox 相关用户态程序
- 复用 `out/initramfs-full-root` 生成 `out/initramfs-usertests-root`
- 生成 `out/initramfs-usertests.cpio`
- 生成 `out/initramfs-usertests.cpio.gz`
- 在 `/init` 中根据命令行参数选择进入 shell、功能测试或性能测试

## 运行

### 1. 运行最小示例

```bash
./build/aarchvm
```

### 2. 运行外部裸二进制

```bash
./build/aarchvm \
  -bin <program.bin> \
  -load <load_addr> \
  -entry <entry_pc> \
  -smp <cpu_count> \
  -steps <max_steps>
```

常用参数：
- `-sp <addr>`：设置启动栈指针
- `-smp <n>`：创建 `n` 个 CPU，默认是 `1`；当前已验证的 SMP smoke 用法是 `-smp 2`
- `-smp-mode <all|psci>`：选择次级 CPU 的启动方式；`all` 表示复位时全部上电，`psci` 表示次级 CPU 需等待 PSCI `CPU_ON`
- `-dtb <file>`：装载 DTB
- `-dtb-addr <addr>`：指定 DTB 地址，并通过 `x0` 传给 guest
- `-segment <file@addr>`：装载额外镜像段
- `-snapshot-save <file>`：运行结束保存整机快照
- `-snapshot-load <file>`：从整机快照恢复
- `-stop-on-uart <text>`：UART 输出命中特定字符串时立即停止
- `-decode <fast|slow>`：切换解码执行路径，默认使用快路径
- `-fb-sdl <on|off>`：显式打开或关闭 SDL framebuffer 窗口

交互式串口快捷键：
- 当 stdin 是终端时，可按 `Ctrl+A`，再按 `x`，立即终止模拟器
- 如果同时启用了 `-snapshot-save <file>`，这条退出路径仍会在结束前写出快照

### 3. 运行 U-Boot

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

### 4. 自动构建 Linux shell 快照

```bash
./tests/linux/build_linux_shell_snapshot.sh
```

这个脚本会：
- 自动构建统一 `initramfs`
- 冷启动 U-Boot -> Linux -> BusyBox
- 在串口命中提示符 `~ # ` 时使用 `-stop-on-uart` 立即停机
- 保存快照到 `out/linux-usertests-shell-v1.snap`
- 立即做一次快照恢复 sanity check

自动回归默认会加：
- `-fb-sdl off`
- `console=ttyAMA0,115200 earlycon=pl011,0x09000000 rdinit=/init initramfs_async=0`

这是当前推荐的可重复测试路径。

### 5. 从快照恢复进入交互式串口 shell

```bash
./tests/linux/run_interactive.sh
```

该脚本会从 `out/linux-usertests-shell-v1.snap` 恢复，适合在已经生成快照后快速进入 BusyBox 串口 shell。

如果是在宿主机终端上直接交互，可使用 QEMU 风格的串口退出序列 `Ctrl+A`，再按 `x` 来退出。

### 6. Linux 功能回归

```bash
./tests/linux/run_functional_suite.sh
```

该脚本会自动：
- 确保 shell 快照/构建日志存在
- 根据串口提示符所在步数构造输入注入脚本
- 启动 Linux 并执行 `run_functional_suite`
- 校验 `uname`、`ps`、`ping -c 1 127.0.0.1`、挂载信息、FPSIMD/FP 整数测试等输出

### 7. Linux 算法/性能回归

```bash
./tests/linux/run_algorithm_perf.sh
```

该脚本会自动：
- 冷启动进入 shell
- 基于提示符步数注入 `bench_runner`
- 收集 `PERF-RESULT` 行
- 输出到 `out/perf-suite-results.txt`

### 8. GUI 路径：framebuffer + PS/2 键盘 + `tty1`

```bash
./tests/linux/run_gui_tty1.sh
```

该脚本会使用如下命令行风格启动：

```text
console=ttyAMA0,115200 console=tty1 earlycon=pl011,0x09000000 rdinit=/init initramfs_async=0
```

含义是：
- 保留串口日志，便于排错
- 让 `tty1` 成为当前 Linux 控制台，使 shell 直接显示到 framebuffer
- GUI 路径使用与脚本一致的 2 核 SMP 设备树
- SDL 窗口中的键盘输入通过 PL050 PS/2 键盘送入 Linux

## 测试入口

### 裸机回归

```bash
./tests/arm64/build_tests.sh
./tests/arm64/run_all.sh
```

当前已覆盖并验证多类基本整数、系统寄存器、中断以及新加入的 KMI/PS2 相关测试。

### Linux 回归

当前建议保留并使用这几类脚本：
- `tests/linux/build_usertests_rootfs.sh`
- `tests/linux/build_linux_shell_snapshot.sh`
- `tests/linux/run_functional_suite.sh`
- `tests/linux/run_algorithm_perf.sh`
- `tests/linux/run_functional_suite_smp.sh`
- `tests/linux/run_gui_tty1.sh`

其中：
- 前五者面向自动化、默认串口路径
- `run_gui_tty1.sh` 面向手工图形验证

## 调试与注入说明

当前实现支持以下几种常用注入机制：
- `AARCHVM_UART_RX_SCRIPT`：按步数向 UART 注入输入字节，供自动化串口测试使用。
- `AARCHVM_PS2_RX_SCRIPT`：按步数向 PS/2 键盘设备注入输入字节，供 KMI/键盘路径测试使用。
- `AARCHVM_BUS_FASTPATH=1`：启用总线快路径。
- `AARCHVM_TIMER_SCALE=<n>`：调整虚拟计时推进比例，加速 Linux 启动与测试。
- 在当前“按指令数推进虚拟时间”的模型下，2 核 Linux 串口 shell 路径如果保持 `console=ttyAMA0,115200`，应使用 `AARCHVM_TIMER_SCALE=1`。像 `10` 这样的更大倍率仍适合 `tty1` / framebuffer GUI 路径，但会让 SMP 串口启动在 guest 视角中过快前进，可能在到达 shell 前就触发 Linux 的 RCU / `stop_machine` 看门狗路径。
- `AARCHVM_STDIN_RX_GAP=<steps>`：对来自非交互式 stdin 的 UART 输入做步数节流，适合脚本化串口会话与批量命令注入。
- `AARCHVM_DEBUG_SLOW=1`：强制启用保守的调试慢路径，关闭指令预解码、SoC 总线 fast path，以及 CPU 对 RAM 的直读直写快路径，便于在不依赖这些宿主机侧优化捷径的情况下复查回归。
- `AARCHVM_TRACE_WRITE_VA=<va>` / `AARCHVM_TRACE_WRITE_PA=<pa>`：记录所有命中指定虚拟地址或物理地址的 guest 写操作，适合定位内存破坏、地址别名或跨页写入问题。
- `AARCHVM_TRACE_BRANCH_ZERO=1`：当 `BR` / `BLR` / `RET` 即将跳到零地址时，打印分支来源、关键寄存器，以及当前 PLT/GOT 目标槽位的内容。

## 重要说明

- 当前自动测试路径不依赖 `CONFIG_CMDLINE_FORCE=y`。`bootargs` 由 U-Boot 脚本在运行时注入。
- 当前 GUI 路径与自动回归路径是刻意分开的。自动回归默认关闭 SDL，以减少时序扰动并提高可重复性。
- framebuffer 和 PS/2 键盘已经是 Linux / U-Boot 树内兼容风格的设备树建模，不再通过“把键盘强行塞给 UART”实现 GUI 输入。
- 项目仍然不是“完整 AArch64 架构实现”，当前保证的是仓库内测试覆盖到的行为闭环。
