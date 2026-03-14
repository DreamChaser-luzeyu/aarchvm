# aarchvm

`aarchvm` 是一个使用 C++ 和 CMake 开发的 AArch64 全系统解释执行模拟器原型。
当前仓库已经具备从 U-Boot 进入 Linux 内核、启动 BusyBox `initramfs`，并通过 PL011 串口进入可交互 shell 的能力。

## 当前验证状态

当前仓库中已经实际验证通过的路径包括：
- 单核 AArch64 EL1 解释执行。
- 最小 SoC 设备集合：RAM、PL011 UART、Generic Timer、最小 GICv3。
- 最小中断闭环：timer -> GIC -> EL1 vector -> `ERET`。
- 最小同步异常闭环：`ESR_EL1/FAR_EL1/ELR_EL1/SPSR_EL1`。
- Linux 早期页表所需的最小 MMU/TLB 路径。
- U-Boot 在模拟器中启动并输出串口日志。
- U-Boot 通过 `booti` 启动 Linux `Image`。
- Linux `earlycon=pl011,...` 和正式 `ttyAMA0` 串口路径均可输出日志。
- BusyBox `ash` 用户态 shell 已验证可交互输入。
- 更完整的 static BusyBox 用户态工具集已验证可运行，至少包括 `ps`、`awk`、`find`、`free`。
- 整机 snapshot 保存 / 恢复，可从 Linux shell 附近快速复现问题。

当前已经在串口上看到并验证的 Linux / BusyBox 输出包括：
- `Booting Linux on physical CPU ...`
- `Linux version 6.12.76 ...`
- `earlycon: pl11 at MMIO 0x0000000009000000`
- `Serial: AMBA PL011 UART driver`
- `Run /init as init process`
- `BusyBox v1.37.0 ... built-in shell (ash)`
- `#` shell 提示符，以及 `echo X` 这类交互命令的回显与执行结果

这说明当前仓库已经稳定跨过了“无输出、无法确认是否进入内核”的阶段，进入了“Linux 用户态 shell 可交互”的状态。

## 目录与构件

仓库内当前实际使用的关键构件如下：
- 模拟器源码：`src/`、`include/`
- 模拟器构建目录：`build/`
- U-Boot 源码：`u-boot-2026.01/`
- U-Boot 构建目录：`u-boot-2026.01/build-qemu_arm64/`
- Linux 源码：`linux-6.12.76/`
- Linux 构建目录：`linux-6.12.76/build-aarchvm/`
- U-Boot 使用的控制 DTB：`dts/aarchvm-current.dtb`
- Linux 启动使用的 DTB：`dts/aarchvm-linux-min.dtb`

## 工具链版本

当前仓库实际使用并验证过的交叉工具链版本为：

```text
aarch64-linux-gnu-gcc (Debian 14.2.0-19) 14.2.0
GNU ld (GNU Binutils for Debian) 2.44
GNU as (GNU Binutils for Debian) 2.44
```

可在本机通过以下命令确认：

```bash
aarch64-linux-gnu-gcc --version | head -n 1
aarch64-linux-gnu-ld --version | head -n 1
aarch64-linux-gnu-as --version | head -n 1
```

## 构建

### 1. 构建模拟器

```bash
cmake -S . -B build
cmake --build build -j
```

### 2. 构建 U-Boot

当前仓库实际使用的是 `qemu_arm64` 配置，并在模拟器中加载其 `u-boot.bin`：

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

构建产物：
- `u-boot-2026.01/build-qemu_arm64/u-boot.bin`

### 3. 构建 Linux

推荐以 `defconfig` 为基础，再覆盖当前模拟器启动链所需的关键选项：

```bash
make -C linux-6.12.76 \
  O=build-aarchvm \
  ARCH=arm64 \
  CROSS_COMPILE=aarch64-linux-gnu- \
  defconfig
```

然后设置与当前模拟器路径一致的关键配置：

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

构建产物：
- `linux-6.12.76/build-aarchvm/arch/arm64/boot/Image`
- `linux-6.12.76/build-aarchvm/vmlinux`

### 4. 生成 DTB

如果系统没有单独安装 `dtc`，可以直接使用 Linux 构建目录中的 `dtc`：

```bash
linux-6.12.76/build-aarchvm/scripts/dtc/dtc \
  -I dts -O dtb \
  -o dts/aarchvm-linux-min.dtb \
  dts/aarchvm-linux-min.dts
```

同理，必要时也可以重新生成：

```bash
linux-6.12.76/build-aarchvm/scripts/dtc/dtc \
  -I dts -O dtb \
  -o dts/aarchvm-current.dtb \
  dts/aarchvm-current.dts
```

### 5. 构建 BusyBox 与 initramfs

当前仓库实际使用的是基于 `defconfig` 的 static BusyBox，并额外关闭了少数会在当前宿主工具链环境下导致构建失败、且当前 guest 路径并不需要的选项。

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

当前重新生成后的 `initramfs` 产物大小为：
- `out/initramfs-full.cpio`：`0x2cd000`
- `out/initramfs-full.cpio.gz`：`0x1662f8`

## 当前实际使用的关键配置项

### U-Boot 关键配置项

以下选项来自当前仓库实际使用的
`u-boot-2026.01/build-qemu_arm64/.config`：

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

这些选项与当前启动路径的关系如下：
- `CONFIG_CMD_BOOTI=y`：允许在 U-Boot 中直接启动 AArch64 Linux `Image`。
- `CONFIG_OF_CONTROL=y` 和 `CONFIG_OF_BOARD=y`：允许 U-Boot 使用由模拟器传入的板级 DTB。
- `CONFIG_DM_SERIAL=y` 和 `CONFIG_BAUDRATE=115200`：与模拟器中的 PL011 串口参数保持一致。
- `CONFIG_DEFAULT_DEVICE_TREE="qemu-arm64"`：当前 U-Boot 基础配置来源。

### Linux 关键配置项

以下选项来自当前仓库实际使用的
`linux-6.12.76/build-aarchvm/.config`：

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

这些选项的用途如下：
- `CONFIG_CMDLINE` 和 `CONFIG_CMDLINE_FORCE`：强制使用与当前模拟器一致的串口命令行参数。
- `CONFIG_SERIAL_AMBA_PL011` 和 `CONFIG_SERIAL_AMBA_PL011_CONSOLE`：启用 Linux 的 PL011 驱动和控制台。
- `CONFIG_ARM_GIC_V3`、`CONFIG_ARM_ARCH_TIMER`：与当前最小 GICv3 / timer 路径匹配。
- `CONFIG_DEVTMPFS*`、`CONFIG_EXT4_FS`、`CONFIG_TMPFS`、`CONFIG_BLK_DEV_INITRD`：面向后续 rootfs 和更完整启动路径。
- `CONFIG_VIRTIO`、`CONFIG_VIRTIO_BLK`：为后续块设备路径预留。

## 运行

### 1. 运行内置最小示例

```bash
./build/aarchvm
```

### 2. 运行外部二进制

```bash
./build/aarchvm \
  -bin <program.bin> \
  -load <load_addr> \
  -entry <entry_pc> \
  -steps <max_steps>
```

常用可选参数：
- `-sp <addr>`：初始化栈指针
- `-dtb <file>`：装载 DTB
- `-dtb-addr <addr>`：指定 DTB 载入地址，并通过 `x0` 传入 guest
- `-segment <file@addr>`：把额外二进制段载入到指定地址
- `-snapshot-save <file>`：在本次运行结束时保存整机快照
- `-snapshot-load <file>`：直接从整机快照恢复运行，不再重新冷启动

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

### 4. 通过 U-Boot 启动 Linux / BusyBox

如果只是验证从 U-Boot 进入 Linux，可以使用一次性管道把 `booti` 命令喂给 U-Boot：

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

如果需要真正进入 BusyBox 交互式 shell，不要把 stdin 用在 `printf | ...` 管道上，而是先启动模拟器，再在 U-Boot 提示符中手工输入：

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

U-Boot 中输入：

```bash
setenv bootargs console=ttyAMA0,115200 earlycon=pl011,0x09000000 ignore_loglevel rdinit=/init
booti 0x40400000 0x46000000:0x1662f8 0x47f00000
```

该启动链中使用的关键工件与地址：
- 主 DTB：`dts/aarchvm-current.dtb`，用于让 U-Boot 识别当前模拟器平台。
- Linux DTB：`dts/aarchvm-linux-min.dtb`，作为 `booti` 的第三个参数传给 Linux。
- Kernel `Image`：加载到 `0x40400000`。
- BusyBox `initramfs`：加载到 `0x46000000`，当前大小 `0x1662f8`。
- Linux DTB：加载到 `0x47f00000`。
- U-Boot 初始 `sp`：`0x47fff000`。
- `AARCHVM_TIMER_SCALE=100000`：用于加快当前解释执行模式下的 Linux 启动推进速度。

### 5. 使用 snapshot 加速 Linux 复测

当前模拟器已经支持整机 snapshot。一次典型用法是先跑到 BusyBox shell，再保存快照：

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

之后可直接从 shell 附近恢复：

```bash
./build/aarchvm -snapshot-load out/linux-shell-v2.snap -steps 500000000
```

这对于排查串口、GIC、timer、系统调用、用户态 shell 输入等问题非常有用。

## 当前 DT 约定

### Linux DTB：`dts/aarchvm-linux-min.dts`

当前 Linux 启动 DTB 中使用的关键地址和设备为：
- RAM：`0x40000000`，大小 `128 MiB`
- GICv3：`0x08000000`
- PL011 UART：`0x09000000`
- architected timer DT 节点：使用 PPI `<13, 14, 11, 10>`
- 串口命令行：`console=ttyAMA0,115200 earlycon=pl011,0x09000000 ignore_loglevel`

### U-Boot DTB：`dts/aarchvm-current.dts`

该 DTB 当前主要用于：
- 让 U-Boot 正确识别 RAM / UART / GIC 基本布局
- 让 U-Boot 的串口输出与模拟器 PL011 保持一致
- 为 U-Boot -> Linux 的 boot chain 提供一致的物理地址布局

## MMU / 中断 / 串口现状摘要

当前实现已经覆盖并验证的关键架构路径包括：
- `TTBR0_EL1/TTBR1_EL1` Stage-1 翻译
- 4KB 粒度页表 walk
- table attribute 继承
- 软件 TLB 与 `TLBI` 观察行为
- `ESR_EL1/FAR_EL1/ELR_EL1/SPSR_EL1` 同步异常路径
- 最小 GICv3 sysreg CPU 接口
- 最小 Generic Timer sysreg 路径
- `CNTKCTL_EL1` 对 EL0 `CNTVCT/CNTPCT/CNTV_*/CNTP_*` 访问控制与 trap 闭环
- PL011 `earlycon`、正式控制台、以及 BusyBox shell 输入路径
- `WFI` 唤醒语义与 EL1 异常上下文中的嵌套 IRQ 接收
- Linux 启动中实际使用到的一批原子、system register、cache maintenance 和 load/store 指令
- 整机 snapshot 保存 / 恢复

BusyBox shell 输入问题最终依赖两个 CPU 语义修正：
- `WFI/WFE` 在“中断已 pending 但暂时还不能立即投递”时也会先退出等待态，避免 Linux idle 卡死。
- 允许 Linux 在已经处于 EL1 异常上下文时继续接收 IRQ，这对内核 idle/console 路径是必要条件。

`busybox ps` 的非法指令问题则定位为一组 AdvSIMD 语义缺口：
- `BIC (vector, immediate)` 先前错误地按普通逻辑立即数路径执行，现已改为真正的 `operand AND NOT(imm)`。
- `DUP (element)` 先前缺失，现已补齐。
- 向量逻辑类解码过宽，导致 `EOR` 曾被误判到 `BIT/BIF/BSL` 路径，现已收紧区分。

这些修复之后，BusyBox `ps` 已能在 guest 中正常列出进程并返回 `0`。

## 测试

构建全部测试二进制：

```bash
tests/arm64/build_tests.sh
```

运行全量回归：

```bash
tests/arm64/run_all.sh
```

当前全量回归会覆盖：
- 先前已实现指令逐项测试
- MMU/TLB/缓存维护测试
- 同步异常寄存器测试
- `cntkctl_el0_timer_access`：验证 `CNTKCTL_EL1` 对 EL0 timer sysreg 的 trap / 放行
- GICv3 + timer sysreg 中断测试
- `irq_nested_el1_wfi`：验证 `WFI` 唤醒与 EL1 异常上下文中的嵌套 IRQ
- `snapshot_resume`：验证整机 snapshot 保存与恢复
- 原子/排他访存测试
- 多组 Linux bring-up 过程中补入的指令样例
- `fpsimd_bic_imm`：验证 `BIC (vector, immediate)`
- `fpsimd_dup_elem`：验证 `DUP (element)`
- 扩展后的 `fpsimd_logic_more`：覆盖 `EOR/BIT/BIF/BSL` 区分
- `fpsimd_arith_shift_perm`：验证 `ADD/SUB/MUL (vector)`、`USHR/SSHR`、`ZIP1/UZP1/TRN1` 以及 `MOV (vector)` 别名路径
- `fp_scalar_ls`：覆盖标量 FP `S/D` 的基本 load/store 与 `post-index` / `pre-index` 形式
- `predecode_logic_min`：验证整数 `decoded` 快路径与 `-decode slow` 的一致性

Linux shell snapshot 构建与提示符步数捕获：

```bash
tests/linux/build_linux_shell_snapshot.sh
```

该脚本会：
- 构建统一 `initramfs-usertests` rootfs
- 通过 U-Boot 冷启动 Linux
- 使用 `-stop-on-uart` 在 UART 输出命中指定序列时立即停止
- 保存统一 shell snapshot
- 验证该 snapshot 可以恢复并继续运行
- 在 `out/linux-usertests-shell-v1-build.log` 中通过 `SUMMARY: steps=...` 记录 BusyBox 提示符对应的 guest 步数

Linux 功能与指令完整性回归：

```bash
tests/linux/run_functional_suite.sh
```

该脚本会冷启动 Linux，复用 `build_linux_shell_snapshot.sh` 测得的提示符步数，并通过 `AARCHVM_UART_RX_SCRIPT` 在确定的 guest-step 偏移处注入 `run_functional_suite`。

Linux 算法性能测试：

```bash
tests/linux/run_algorithm_perf.sh
```

该脚本使用同样的“提示符步数 + UART 注入”路径，在冷启动后启动 `bench_runner`，并把 `PERF-RESULT` 行提取到 `out/perf-suite-results.txt`。

当前 Linux 用户态自检覆盖并验证的关键族包括：
- 向量逻辑：`AND/EOR/BIT/BIF/BSL/ORR(MOV alias)`
- 向量重排：`EXT/ZIP1/UZP1/TRN1`
- 向量整数算术：`ADD/SUB/MUL`
- 向量移位/扩展：`USHR/SSHR/SHRN/UXTL/SXTL`
- 标量 FP：`FADD/FSUB/FMUL/FDIV/FMADD/FMSUB/FNMADD/FNMSUB/FABS/FNEG/SCVTF/UCVTF/FCVTZS/FCVTZU/FCSEL`
- 标量 FP 访存：`STR/LDR S/D` 及 `post-index` / `pre-index` 子集

## 当前局限与下一步

虽然当前仓库已经能够稳定进入 BusyBox shell 并完成基本交互，但仍有若干平台级工作会继续影响“更完整 Linux 系统”的推进，包括：
- 更完整的 GICv3 distributor / redistributor MMIO 模型
- 更完整的 architected timer / power-management 行为
- 块设备 / rootfs 所需设备模型
- 更完整的 PSCI / SMCCC / 电源管理路径
- 进一步扩展 Linux 深层初始化阶段用到的指令与 sysreg

## 参考手册

- `DDI0487_M.a.a_a-profile_architecture_reference_manual.pdf`
