#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT_DIR"

FASTPATH="${AARCHVM_BUS_FASTPATH:-1}"
# tty1/fbcon on SMP needs a slower virtual timer under the current instruction-count time model.
TIMER_SCALE="${AARCHVM_TIMER_SCALE:-10}"
STEPS="${AARCHVM_GUI_TTY1_STEPS:-300000000000}"
UBOOT_DELAY_SEC="${AARCHVM_UBOOT_COMMAND_DELAY_SEC:-3}"
PROMPT_DELAY_SEC="${AARCHVM_UBOOT_PROMPT_DELAY_SEC:-1}"
INITRD="${AARCHVM_GUI_TTY1_INITRD:-out/initramfs-usertests.cpio.gz}"
LINUX_DTB="${AARCHVM_LINUX_DTB:-dts/aarchvm-linux-smp.dtb}"
SNAPSHOT="${AARCHVM_USERTEST_SNAPSHOT_OUT:-out/my_test.snap}"
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

UBOOT_BOOT_CMDS=$(cat <<EOC


setenv bootargs console=ttyAMA0,115200 console=tty1 earlycon=pl011,0x09000000 rdinit=/init initramfs_async=0
booti 0x40400000 0x46000000:${INITRD_SIZE_HEX} ${DTB_ADDR}
EOC
)
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
  -steps "$STEPS"
  # -snapshot-save "$SNAPSHOT"
)
print_cmd 'AARCHVM gui tty1 command:' "${AARCHVM_CMD[@]}"
printf 'U-Boot scripted input:\n%s\n' "$UBOOT_BOOT_CMDS"

(
  sleep "$UBOOT_DELAY_SEC"
  printf '\n\n\n'
  sleep "$PROMPT_DELAY_SEC"
  printf '%s\n' "$UBOOT_BOOT_CMDS"
) | \
AARCHVM_BUS_FASTPATH="$FASTPATH" \
AARCHVM_TIMER_SCALE="$TIMER_SCALE" \
"${AARCHVM_CMD[@]}"
