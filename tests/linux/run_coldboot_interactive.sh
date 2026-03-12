#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT_DIR"

if [[ ! -f out/initramfs-usertests.cpio ]]; then
  tests/linux/build_usertests_rootfs.sh >/dev/null
fi

KERNEL_IMAGE="${AARCHVM_LINUX_IMAGE:-linux-6.12.76/build-aarchvm/arch/arm64/boot/Image}"
UBOOT_BIN="${AARCHVM_UBOOT_BIN:-u-boot-2026.01/build-qemu_arm64/u-boot.bin}"
UBOOT_DTB="${AARCHVM_UBOOT_DTB:-dts/aarchvm-current.dtb}"
LINUX_DTB="${AARCHVM_LINUX_DTB:-dts/aarchvm-linux-min.dtb}"
INITRD="${AARCHVM_INITRD:-out/initramfs-usertests.cpio}"
FASTPATH="${AARCHVM_BUS_FASTPATH:-1}"
TIMER_SCALE="${AARCHVM_TIMER_SCALE:-10000}"
STEPS="${AARCHVM_COLD_STEPS:-3000000000}"
BOOT_DELAY_SEC="${AARCHVM_UBOOT_INPUT_DELAY_SEC:-3}"
BOOTARGS="${AARCHVM_BOOTARGS:-console=ttyAMA0,115200 earlycon=pl011,0x09000000 ignore_loglevel rdinit=/init}"
INITRD_ADDR="${AARCHVM_INITRD_ADDR:-0x46000000}"
KERNEL_ADDR="${AARCHVM_KERNEL_ADDR:-0x40400000}"
LINUX_DTB_ADDR="${AARCHVM_LINUX_DTB_ADDR:-0x47f00000}"
UBOOT_DTB_ADDR="${AARCHVM_UBOOT_DTB_ADDR:-0x40000000}"
INIT_SP="${AARCHVM_INIT_SP:-0x47fff000}"
LOAD_ADDR="${AARCHVM_LOAD_ADDR:-0x0}"
ENTRY_ADDR="${AARCHVM_ENTRY_ADDR:-0x0}"

for path in "$KERNEL_IMAGE" "$UBOOT_BIN" "$UBOOT_DTB" "$LINUX_DTB" "$INITRD"; do
  if [[ ! -f "$path" ]]; then
    echo "missing required file: $path" >&2
    exit 1
  fi
done

INITRD_SIZE_HEX=$(printf '0x%x' "$(stat -c '%s' "$INITRD")")

echo "cold boot interactive"
echo "  u-boot:   $UBOOT_BIN"
echo "  kernel:   $KERNEL_IMAGE"
echo "  initrd:   $INITRD ($INITRD_SIZE_HEX)"
echo "  dtb:      $LINUX_DTB"
echo "  fastpath: $FASTPATH"
echo "  timer:    $TIMER_SCALE"
echo "  steps:    $STEPS"
echo "  u-boot input delay: ${BOOT_DELAY_SEC}s"
echo

{
  # sleep "$BOOT_DELAY_SEC"
  printf 'setenv bootargs %s\n' "$BOOTARGS"
  printf 'booti %s %s:%s %s\n' "$KERNEL_ADDR" "$INITRD_ADDR" "$INITRD_SIZE_HEX" "$LINUX_DTB_ADDR"
} | \
AARCHVM_BUS_FASTPATH="$FASTPATH" \
AARCHVM_TIMER_SCALE="$TIMER_SCALE" \
./build/aarchvm \
  -bin "$UBOOT_BIN" \
  -load "$LOAD_ADDR" \
  -entry "$ENTRY_ADDR" \
  -sp "$INIT_SP" \
  -dtb "$UBOOT_DTB" \
  -dtb-addr "$UBOOT_DTB_ADDR" \
  -segment "$KERNEL_IMAGE@$KERNEL_ADDR" \
  -segment "$INITRD@$INITRD_ADDR" \
  -segment "$LINUX_DTB@$LINUX_DTB_ADDR" \
  -steps "$STEPS"
