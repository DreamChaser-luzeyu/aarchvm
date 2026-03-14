#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT_DIR"

FASTPATH="${AARCHVM_BUS_FASTPATH:-1}"
TIMER_SCALE="${AARCHVM_TIMER_SCALE:-100}"
STEPS="${AARCHVM_GUI_TTY1_STEPS:-300000000000}"
UBOOT_DELAY_SEC="${AARCHVM_UBOOT_COMMAND_DELAY_SEC:-3}"
PROMPT_DELAY_SEC="${AARCHVM_UBOOT_PROMPT_DELAY_SEC:-1}"
INITRD="${AARCHVM_GUI_TTY1_INITRD:-out/initramfs-usertests.cpio}"
INITRD_SIZE_HEX=$(printf '0x%x' "$(stat -c '%s' "$INITRD")")

if [[ ! -f "$INITRD" ]]; then
  echo "missing initramfs: $INITRD" >&2
  echo "build it first with tests/linux/build_usertests_rootfs.sh" >&2
  exit 1
fi

# (
#   sleep "$UBOOT_DELAY_SEC"
#   printf '\n\n\n'
#   sleep "$PROMPT_DELAY_SEC"
#   printf 'setenv bootargs console=tty0 console=ttyAMA0,115200 earlycon=pl011,0x09000000 rdinit=/init initramfs_async=0 aarchvm_shell=tty1\n'
#   printf 'booti 0x40400000 0x46000000:%s 0x47f00000\n' "$INITRD_SIZE_HEX"
# ) | \
(
  sleep "$UBOOT_DELAY_SEC"
  printf '\n\n\n'
  sleep "$PROMPT_DELAY_SEC"
  printf 'setenv bootargs console=tty1 earlycon=pl011,0x09000000 rdinit=/init initramfs_async=0\n'
  printf 'booti 0x40400000 0x46000000:%s 0x47f00000\n' "$INITRD_SIZE_HEX"
) | \
AARCHVM_BUS_FASTPATH="$FASTPATH" \
AARCHVM_TIMER_SCALE="$TIMER_SCALE" \
./build/aarchvm \
  -bin u-boot-2026.01/build-qemu_arm64/u-boot.bin \
  -load 0x0 \
  -entry 0x0 \
  -sp 0x47fff000 \
  -dtb dts/aarchvm-current.dtb \
  -dtb-addr 0x40000000 \
  -segment linux-6.12.76/build-aarchvm/arch/arm64/boot/Image@0x40400000 \
  -segment "$INITRD"@0x46000000 \
  -segment dts/aarchvm-linux-min.dtb@0x47f00000 \
  -fb-sdl on \
  -steps "$STEPS"
