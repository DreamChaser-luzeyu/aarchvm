#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT_DIR"

tests/linux/build_usertests_rootfs.sh >/dev/null

IMAGE_PATH="${AARCHVM_DEBIAN_ROOTFS_IMAGE:-out/debian-arm64-bookworm.ext4}"
if [[ ! -f "$IMAGE_PATH" ]]; then
  tests/linux/build_debian_rootfs_image.sh >/dev/null
fi

LOG="${AARCHVM_BLOCK_MOUNT_LOG:-out/linux-block-mount-smoke.log}"
TIMEOUT_SEC="${AARCHVM_BLOCK_MOUNT_TIMEOUT:-240s}"
STEPS="${AARCHVM_BLOCK_MOUNT_STEPS:-1800000000}"
FASTPATH="${AARCHVM_BUS_FASTPATH:-1}"
TIMER_SCALE="${AARCHVM_TIMER_SCALE:-1}"
LINUX_DTB="${AARCHVM_LINUX_DTB:-dts/aarchvm-linux-min.dtb}"
INITRD="out/initramfs-usertests.cpio.gz"
DTB_ADDR=0x47f00000
STOP_PATTERN="BLOCK-MOUNT PASS"

ensure_linux_dtb() {
  local dtb="$1"
  local dts="${dtb%.dtb}.dts"
  local dtc_bin=""

  if [[ ! -f "$dtb" || ( -f "$dts" && "$dts" -nt "$dtb" ) ]]; then
    if [[ ! -f "$dts" ]]; then
      echo "missing DTS source for DTB: $dtb" >&2
      exit 1
    fi
    if command -v dtc >/dev/null 2>&1; then
      dtc_bin=dtc
    elif [[ -x linux-6.12.76/build-aarchvm/scripts/dtc/dtc ]]; then
      dtc_bin=linux-6.12.76/build-aarchvm/scripts/dtc/dtc
    else
      echo "missing dtc; cannot rebuild $dtb" >&2
      exit 1
    fi
    timeout 120s "$dtc_bin" -I dts -O dtb -o "$dtb" "$dts"
  fi
}

print_cmd() {
  local label="$1"
  shift
  printf '%s' "$label"
  printf ' %q' "$@"
  printf '\n'
}

mkdir -p "$(dirname "$LOG")"
ensure_linux_dtb "$LINUX_DTB"

INITRD_SIZE_HEX=$(printf '0x%x' "$(stat -c '%s' "$INITRD")")
UBOOT_CMDS=$(cat <<EOC
setenv bootargs console=ttyAMA0,115200 earlycon=pl011,0x09000000 rdinit=/init initramfs_async=0
booti 0x40400000 0x46000000:${INITRD_SIZE_HEX} ${DTB_ADDR}
EOC
)
SHELL_CMDS=$'/bin/busybox mkdir -p /mnt/debian\n/bin/busybox mount -t ext4 -o ro /dev/vda /mnt/debian\n/bin/busybox test -d /mnt/debian/bin && echo ROOTFS-BIN-OK\n/bin/busybox test -f /mnt/debian/etc/debian_version && echo ROOTFS-DEBIAN-VERSION-OK\n/bin/busybox cat /mnt/debian/etc/debian_version\necho BLOCK-MOUNT PASS\n'

AARCHVM_CMD=(
  ./build/aarchvm
  -bin u-boot-2026.01/build-qemu_arm64/u-boot.bin
  -load 0x0
  -entry 0x0
  -sp 0x47fff000
  -dtb "$LINUX_DTB"
  -dtb-addr "$DTB_ADDR"
  -segment linux-6.12.76/build-aarchvm/arch/arm64/boot/Image@0x40400000
  -segment "$INITRD"@0x46000000
  -drive "$IMAGE_PATH"
  -steps "$STEPS"
  -stop-on-uart "$STOP_PATTERN"
  -fb-sdl off
)

echo "-----------"
print_cmd 'AARCHVM block smoke command:' "${AARCHVM_CMD[@]}"
printf 'U-Boot scripted input:\n%s\n' "$UBOOT_CMDS"
printf 'Shell auto-reply:\n%s\n' "$SHELL_CMDS"
echo "-----------"

set -o pipefail
(
  sleep 3
  printf '\n\n\n'
  sleep 1
  printf '%s\n' "$UBOOT_CMDS"
) | \
env \
  AARCHVM_UART_TX_MATCH='~ # ' \
  AARCHVM_UART_TX_REPLY="$SHELL_CMDS" \
  AARCHVM_PRINT_SUMMARY=1 \
  AARCHVM_BUS_FASTPATH="$FASTPATH" \
  AARCHVM_TIMER_SCALE="$TIMER_SCALE" \
  timeout "$TIMEOUT_SEC" "${AARCHVM_CMD[@]}" > "$LOG" 2>&1

tr -d '\r\000' < "$LOG" > "${LOG%.log}.clean.log"
CLEAN_LOG="${LOG%.log}.clean.log"

check() {
  local pattern="$1"
  if ! grep -aFq "$pattern" "$CLEAN_LOG"; then
    echo "missing expected output: $pattern" >&2
    tail -n 160 "$CLEAN_LOG" >&2
    exit 1
  fi
}

check 'virtio_blk virtio0'
check '[vda]'
check 'ROOTFS-BIN-OK'
check 'ROOTFS-DEBIAN-VERSION-OK'
check 'BLOCK-MOUNT PASS'

printf 'linux block mount smoke passed\nlog: %s\nimage: %s\n' "$LOG" "$IMAGE_PATH"
