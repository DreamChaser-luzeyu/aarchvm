#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT_DIR"

IMAGE_PATH="${AARCHVM_DEBIAN_ROOTFS_IMAGE:-out/debian-arm64-bookworm-systemd.ext4}"
INITRD="${AARCHVM_DEBIAN_BOOT_INITRD:-out/initramfs-debian-systemd.cpio.gz}"
LINUX_DTB="${AARCHVM_LINUX_DTB:-dts/aarchvm-linux-min.dtb}"
LOG="${AARCHVM_DEBIAN_BOOT_LOG:-out/linux-debian-systemd.log}"
TIMEOUT_SEC="${AARCHVM_DEBIAN_BOOT_TIMEOUT:-5400s}"
STEPS="${AARCHVM_DEBIAN_BOOT_STEPS:--1}"
FASTPATH="${AARCHVM_BUS_FASTPATH:-1}"
TIMER_SCALE="${AARCHVM_TIMER_SCALE:-1}"
DTB_ADDR=0x47f00000
STOP_PATTERN="${AARCHVM_DEBIAN_BOOT_STOP_PATTERN:-AARCHVM-DEBIAN-SYSTEMD-READY}"
UBOOT_DELAY_SEC="${AARCHVM_UBOOT_COMMAND_DELAY_SEC:-3}"
PROMPT_DELAY_SEC="${AARCHVM_UBOOT_PROMPT_DELAY_SEC:-1}"

print_cmd() {
  local label="$1"
  shift
  printf '%s' "$label"
  printf ' %q' "$@"
  printf '\n'
}

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

if [[ ! -f "$IMAGE_PATH" ]]; then
  AARCHVM_DEBIAN_PROFILE=systemd tests/linux/build_debian_rootfs_image.sh
fi

if [[ ! -f "$INITRD" ]]; then
  tests/linux/build_debian_systemd_initramfs.sh
fi

ensure_linux_dtb "$LINUX_DTB"
mkdir -p "$(dirname "$LOG")"

INITRD_SIZE_HEX=$(printf '0x%x' "$(stat -c '%s' "$INITRD")")
UBOOT_BOOT_CMDS=$(cat <<EOC
setenv bootargs console=ttyAMA0,115200 earlycon=pl011,0x09000000 rdinit=/init root=/dev/vda rw rootfstype=ext4 initramfs_async=0 systemd.unit=multi-user.target systemd.log_level=info systemd.log_target=console
booti 0x40400000 0x46000000:${INITRD_SIZE_HEX} ${DTB_ADDR}
EOC
)

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
print_cmd 'AARCHVM Debian systemd command:' "${AARCHVM_CMD[@]}"
printf 'U-Boot scripted input:\n%s\n' "$UBOOT_BOOT_CMDS"
printf 'timeout: %s\n' "$TIMEOUT_SEC"
printf 'stop pattern: %s\n' "$STOP_PATTERN"
echo "-----------"

set -o pipefail
set +e
(
  sleep "$UBOOT_DELAY_SEC"
  printf '\n\n\n'
  sleep "$PROMPT_DELAY_SEC"
  printf '%s\n' "$UBOOT_BOOT_CMDS"
) | \
AARCHVM_PRINT_SUMMARY=1 \
AARCHVM_BUS_FASTPATH="$FASTPATH" \
AARCHVM_TIMER_SCALE="$TIMER_SCALE" \
timeout "$TIMEOUT_SEC" "${AARCHVM_CMD[@]}" \
  > "$LOG" 2>&1
RC=$?
set -e

if [[ "$RC" -ne 0 ]]; then
  echo "Debian systemd boot failed with exit code $RC" >&2
  tail -n 200 "$LOG" >&2
  exit 1
fi

tr -d '\r\000' < "$LOG" > "${LOG%.log}.clean.log"
CLEAN_LOG="${LOG%.log}.clean.log"

if ! grep -aFq "$STOP_PATTERN" "$CLEAN_LOG"; then
  echo "missing expected boot completion marker: $STOP_PATTERN" >&2
  tail -n 200 "$CLEAN_LOG" >&2
  exit 1
fi

if rg -n "Kernel panic|Unable to mount root fs|Failed to execute /init|No working init found|VFS: Cannot open root device|Requested init .* failed|switch_root:|AARCHVM-DEBIAN-SECOND-STAGE FAILED" "$CLEAN_LOG" >/dev/null; then
  echo 'Debian systemd boot detected guest failure' >&2
  tail -n 200 "$CLEAN_LOG" >&2
  exit 1
fi

printf 'Debian systemd boot passed\nlog: %s\nimage: %s\ninitrd: %s\n' "$LOG" "$IMAGE_PATH" "$INITRD"
