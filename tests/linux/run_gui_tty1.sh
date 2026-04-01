#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT_DIR"

FASTPATH="${AARCHVM_BUS_FASTPATH:-1}"
TIMER_SCALE="${AARCHVM_TIMER_SCALE:-1}"
ARCH_TIMER_MODE="${AARCHVM_ARCH_TIMER_MODE:-host}"
STEPS="${AARCHVM_GUI_TTY1_STEPS:-300000000000}"
INITRD="${AARCHVM_GUI_TTY1_INITRD:-out/initramfs-usertests.cpio.gz}"
LINUX_DTB="${AARCHVM_LINUX_DTB:-dts/aarchvm-linux-smp.dtb}"
SNAPSHOT="${AARCHVM_USERTEST_SNAPSHOT_OUT:-out/my_test.snap}"
DEFAULT_DRIVE_IMAGE="${AARCHVM_DEBIAN_ROOTFS_IMAGE:-out/debian-arm64-bookworm.ext4}"
DRIVE_IMAGE="${AARCHVM_GUI_TTY1_DRIVE-$DEFAULT_DRIVE_IMAGE}"
DTB_ADDR=0x47f00000
INITRD_SIZE_HEX=$(printf '0x%x' "$(stat -c '%s' "$INITRD")")

print_cmd() {
  local label="$1"
  shift
  printf '%s' "$label"
  printf ' %q' "$@"
  printf '\n'
}

if [[ ! -f "$INITRD" ]]; then
  echo "missing initramfs: $INITRD" >&2
  echo "build it first with tests/linux/build_usertests_rootfs.sh" >&2
  exit 1
fi

if [[ -n "$DRIVE_IMAGE" && ! -f "$DRIVE_IMAGE" ]]; then
  if [[ "$DRIVE_IMAGE" == "$DEFAULT_DRIVE_IMAGE" ]]; then
    tests/linux/build_debian_rootfs_image.sh >/dev/null
  else
    echo "missing drive image: $DRIVE_IMAGE" >&2
    exit 1
  fi
fi

UBOOT_BOOT_CMDS=$(cat <<EOC
setenv bootargs console=ttyAMA0,115200 console=tty1 earlycon=pl011,0x09000000 rdinit=/init initramfs_async=0
booti 0x40400000 0x46000000:${INITRD_SIZE_HEX} ${DTB_ADDR}
EOC
)

UART_MATCH="${AARCHVM_GUI_TTY1_UART_MATCH:-=> }"
UART_REPLY="${UBOOT_BOOT_CMDS}"$'\n'
AARCHVM_CMD=(
  ./build/aarchvm
  -smp 2
  -smp-mode psci
  -bin u-boot-2026.01/build-qemu_arm64/u-boot.bin
  -load 0x0
  -entry 0x0
  -sp 0x47fff000
  -dtb "$LINUX_DTB"
  -dtb-addr "$DTB_ADDR"
  -segment linux-6.12.76/build-aarchvm/arch/arm64/boot/Image@0x40400000
  -segment "$INITRD"@0x46000000
  -fb-sdl on
  -arch-timer-mode "$ARCH_TIMER_MODE"
  -steps "$STEPS"
  -snapshot-save "$SNAPSHOT"
  -arch-timer-mode host
)

if [[ -n "$DRIVE_IMAGE" ]]; then
  AARCHVM_CMD+=(-drive "$DRIVE_IMAGE")
fi

echo "-----------"
print_cmd 'AARCHVM gui tty1 command:' "${AARCHVM_CMD[@]}"
printf 'arch timer mode: %s\n' "$ARCH_TIMER_MODE"
if [[ -n "$DRIVE_IMAGE" ]]; then
  printf 'drive image: %s\n' "$DRIVE_IMAGE"
else
  printf 'drive image: <disabled>\n'
fi
printf 'U-Boot auto-reply match:\n%s\n' "$UART_MATCH"
printf 'U-Boot auto-reply text:\n%s\n' "$UBOOT_BOOT_CMDS"
echo "-----------"

# AARCHVM_BUS_FASTPATH="$FASTPATH" \
# AARCHVM_TIMER_SCALE="$TIMER_SCALE" \
# AARCHVM_UART_TX_MATCH="$UART_MATCH" \
# AARCHVM_UART_TX_REPLY="$UART_REPLY" \
# "${AARCHVM_CMD[@]}"

(
  # sleep "$UBOOT_DELAY_SEC"
  printf '\n\n\n'
  # sleep "$PROMPT_DELAY_SEC"
  printf '%s\n' "$UBOOT_BOOT_CMDS"
) | \
AARCHVM_BUS_FASTPATH="$FASTPATH" \
AARCHVM_TIMER_SCALE="$TIMER_SCALE" \
"${AARCHVM_CMD[@]}"
