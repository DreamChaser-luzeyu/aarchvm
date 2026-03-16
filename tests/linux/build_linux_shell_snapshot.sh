#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT_DIR"

tests/linux/build_usertests_rootfs.sh

SNAPSHOT="${AARCHVM_USERTEST_SNAPSHOT_OUT:-out/linux-usertests-shell-v1.snap}"
LOG="${AARCHVM_USERTEST_SNAPSHOT_LOG:-out/linux-usertests-shell-v1-build.log}"
VERIFY_LOG="${AARCHVM_USERTEST_SNAPSHOT_VERIFY_LOG:-out/linux-usertests-shell-v1-verify.log}"
STEPS="${AARCHVM_USERTEST_SNAPSHOT_STEPS:-1800000000}"
VERIFY_STEPS="${AARCHVM_USERTEST_SNAPSHOT_VERIFY_STEPS:-5000000}"
TIMEOUT_SEC="${AARCHVM_USERTEST_SNAPSHOT_TIMEOUT:-240s}"
VERIFY_TIMEOUT_SEC="${AARCHVM_USERTEST_SNAPSHOT_VERIFY_TIMEOUT:-20s}"
TIMER_SCALE="${AARCHVM_TIMER_SCALE:-100}"
VERIFY_TIMER_SCALE="${AARCHVM_VERIFY_TIMER_SCALE:-1}"
FASTPATH="${AARCHVM_BUS_FASTPATH:-1}"
STOP_PATTERN="${AARCHVM_STOP_ON_UART_PATTERN:-~ # }"
UBOOT_DELAY_SEC="${AARCHVM_UBOOT_COMMAND_DELAY_SEC:-3}"
PROMPT_DELAY_SEC="${AARCHVM_UBOOT_PROMPT_DELAY_SEC:-1}"
LINUX_DTB="${AARCHVM_LINUX_DTB:-dts/aarchvm-linux-min.dtb}"
DTB_ADDR=0x47f00000
AARCHVM_ARGS_RAW="${AARCHVM_ARGS:-}"
AARCHVM_EXTRA_ARGS=()
if [[ -n "$AARCHVM_ARGS_RAW" ]]; then
  read -r -a AARCHVM_EXTRA_ARGS <<< "$AARCHVM_ARGS_RAW"
fi
INITRD="out/initramfs-usertests.cpio.gz"
INITRD_SIZE_HEX=$(printf '0x%x' "$(stat -c '%s' "$INITRD")")

print_cmd() {
  local label="$1"
  shift
  printf '%s' "$label"
  printf ' %q' "$@"
  printf '\n'
}

mkdir -p "$(dirname "$SNAPSHOT")" "$(dirname "$LOG")" "$(dirname "$VERIFY_LOG")"
rm -f "$SNAPSHOT"
SNAPSHOT_OK=0
cleanup() {
  if [[ "$SNAPSHOT_OK" != 1 ]]; then
    rm -f "$SNAPSHOT"
  fi
}
trap cleanup EXIT

UBOOT_BOOT_CMDS=$(cat <<EOC
setenv bootargs console=ttyAMA0,115200 earlycon=pl011,0x09000000 rdinit=/init initramfs_async=0
booti 0x40400000 0x46000000:${INITRD_SIZE_HEX} ${DTB_ADDR}
EOC
)

AARCHVM_BUILD_CMD=(
  ./build/aarchvm
  "${AARCHVM_EXTRA_ARGS[@]}"
  -bin u-boot-2026.01/build-qemu_arm64/u-boot.bin
  -load 0x0
  -entry 0x0
  -sp 0x47fff000
  -dtb "$LINUX_DTB"
  -dtb-addr "$DTB_ADDR"
  -segment linux-6.12.76/build-aarchvm/arch/arm64/boot/Image@0x40400000
  -segment "$INITRD"@0x46000000
  -steps "$STEPS"
  -stop-on-uart "$STOP_PATTERN"
  -snapshot-save "$SNAPSHOT"
  -fb-sdl off
)

AARCHVM_VERIFY_CMD=(
  ./build/aarchvm
  "${AARCHVM_EXTRA_ARGS[@]}"
  -snapshot-load "$SNAPSHOT"
  -fb-sdl off
  -steps "$VERIFY_STEPS"
  -stop-on-uart "$STOP_PATTERN"
)

print_cmd 'AARCHVM build command:' "${AARCHVM_BUILD_CMD[@]}"
printf 'U-Boot scripted input:\n%s\n' "$UBOOT_BOOT_CMDS"

set -o pipefail
(
  sleep "$UBOOT_DELAY_SEC"
  printf '\n\n\n'
  sleep "$PROMPT_DELAY_SEC"
  printf '%s\n' "$UBOOT_BOOT_CMDS"
) | \
AARCHVM_PRINT_SUMMARY=1 \
AARCHVM_BUS_FASTPATH="$FASTPATH" \
AARCHVM_TIMER_SCALE="$TIMER_SCALE" \
timeout "$TIMEOUT_SEC" "${AARCHVM_BUILD_CMD[@]}" \
  > "$LOG" 2>&1

test -f "$SNAPSHOT" && echo "snapshot produced: $SNAPSHOT" || { echo "snapshot not produced: $SNAPSHOT" >&2; exit 1; }
grep -Fq "$STOP_PATTERN" "$LOG" || { echo 'shell prompt stop pattern not observed during snapshot build' >&2; tail -n 160 "$LOG" >&2; exit 1; }
if rg -n "Kernel panic|Unable to mount root fs|Failed to execute /init|No working init found|VFS: Cannot open root device|Requested init .* failed" "$LOG" >/dev/null; then
  echo 'snapshot build detected guest boot failure' >&2
  tail -n 200 "$LOG" >&2
  exit 1
fi
PROMPT_STEPS=$(tr -d '\r' < "$LOG" | sed -n 's/.*SUMMARY: steps=\([0-9][0-9]*\).*/\1/p' | tail -n 1)
[[ -n "$PROMPT_STEPS" ]] || { echo 'prompt step summary missing from build log' >&2; tail -n 120 "$LOG" >&2; exit 1; }

echo "running snapshot restore sanity check..."
print_cmd 'AARCHVM verify command:' "${AARCHVM_VERIFY_CMD[@]}"
set +e
set -o pipefail
printf '\n' | \
AARCHVM_PRINT_SUMMARY=1 \
AARCHVM_BUS_FASTPATH="$FASTPATH" \
AARCHVM_TIMER_SCALE="$VERIFY_TIMER_SCALE" \
timeout "$VERIFY_TIMEOUT_SEC" "${AARCHVM_VERIFY_CMD[@]}" \
  > "$VERIFY_LOG" 2>&1
VERIFY_RC=$?
set -e

if [[ "$VERIFY_RC" -ne 0 ]]; then
  echo "snapshot restore verification failed with exit code $VERIFY_RC" >&2
  tail -n 160 "$VERIFY_LOG" >&2
  exit 1
fi

echo "snapshot restore sanity check finished"
grep -Fq 'SUMMARY:' "$VERIFY_LOG" || { echo 'snapshot restore verification missing SUMMARY' >&2; tail -n 120 "$VERIFY_LOG" >&2; exit 1; }
if rg -n "Kernel panic|Unable to mount root fs|Failed to load snapshot|Failed to execute /init|No working init found|VFS: Cannot open root device|Requested init .* failed" "$VERIFY_LOG" >/dev/null; then
  echo 'snapshot restore verification detected guest failure' >&2
  tail -n 160 "$VERIFY_LOG" >&2
  exit 1
fi

SNAPSHOT_OK=1
printf 'linux shell snapshot ready\nsnapshot: %s\nbuild log: %s\nverify log: %s\nstop pattern: %s\nprompt steps: %s\nu-boot command delay: %ss\nu-boot prompt delay: %ss\n' "$SNAPSHOT" "$LOG" "$VERIFY_LOG" "$STOP_PATTERN" "$PROMPT_STEPS" "$UBOOT_DELAY_SEC" "$PROMPT_DELAY_SEC"
