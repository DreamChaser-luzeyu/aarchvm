#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT_DIR"

tests/linux/build_usertests_rootfs.sh 

SNAPSHOT="${AARCHVM_USERTEST_SNAPSHOT_OUT:-out/linux-usertests-shell-v1.snap}"
LOG="${AARCHVM_USERTEST_SNAPSHOT_LOG:-out/linux-usertests-shell-v1-build.log}"
VERIFY_LOG="${AARCHVM_USERTEST_SNAPSHOT_VERIFY_LOG:-out/linux-usertests-shell-v1-verify.log}"
STEPS="${AARCHVM_USERTEST_SNAPSHOT_STEPS:-1800000000}"
VERIFY_STEPS="${AARCHVM_USERTEST_SNAPSHOT_VERIFY_STEPS:-5000}"
TIMEOUT_SEC="${AARCHVM_USERTEST_SNAPSHOT_TIMEOUT:-240s}"
VERIFY_TIMEOUT_SEC="${AARCHVM_USERTEST_SNAPSHOT_VERIFY_TIMEOUT:-10s}"
TIMER_SCALE="${AARCHVM_TIMER_SCALE:-100}"
VERIFY_TIMER_SCALE="${AARCHVM_VERIFY_TIMER_SCALE:-1}"
FASTPATH="${AARCHVM_BUS_FASTPATH:-1}"
STOP_PATTERN="${AARCHVM_STOP_ON_UART_PATTERN:-~ # }"
UBOOT_DELAY_SEC="${AARCHVM_UBOOT_COMMAND_DELAY_SEC:-3}"
PROMPT_DELAY_SEC="${AARCHVM_UBOOT_PROMPT_DELAY_SEC:-1}"
AARCHVM_ARGS_RAW="${AARCHVM_ARGS:-}"
AARCHVM_EXTRA_ARGS=()
if [[ -n "$AARCHVM_ARGS_RAW" ]]; then
  read -r -a AARCHVM_EXTRA_ARGS <<< "$AARCHVM_ARGS_RAW"
fi
INITRD="out/initramfs-usertests.cpio"
INITRD_SIZE_HEX=$(printf '0x%x' "$(stat -c '%s' "$INITRD")")

mkdir -p "$(dirname "$SNAPSHOT")" "$(dirname "$LOG")"
rm -f "$SNAPSHOT"

set -o pipefail
(
  sleep "$UBOOT_DELAY_SEC"
  printf '\n'
  sleep "$PROMPT_DELAY_SEC"
  printf 'setenv bootargs console=ttyAMA0,115200 earlycon=pl011,0x09000000 ignore_loglevel\n'
  printf 'booti 0x40400000 0x46000000:%s 0x47f00000\n' "$INITRD_SIZE_HEX"
) | \
AARCHVM_PRINT_SUMMARY=1 \
AARCHVM_BUS_FASTPATH="$FASTPATH" \
AARCHVM_TIMER_SCALE="$TIMER_SCALE" \
timeout "$TIMEOUT_SEC" ./build/aarchvm "${AARCHVM_EXTRA_ARGS[@]}" \
  -bin u-boot-2026.01/build-qemu_arm64/u-boot.bin \
  -load 0x0 \
  -entry 0x0 \
  -sp 0x47fff000 \
  -dtb dts/aarchvm-current.dtb \
  -dtb-addr 0x40000000 \
  -segment linux-6.12.76/build-aarchvm/arch/arm64/boot/Image@0x40400000 \
  -segment "$INITRD"@0x46000000 \
  -segment dts/aarchvm-linux-min.dtb@0x47f00000 \
  -steps "$STEPS" \
  -stop-on-uart "$STOP_PATTERN" \
  -snapshot-save "$SNAPSHOT" \
  > "$LOG" 2>&1

test -f "$SNAPSHOT" && echo "snapshot produced: $SNAPSHOT"  || { echo "snapshot not produced: $SNAPSHOT" >&2; exit 1; }
PROMPT_STEPS=$(tr -d '\r' < "$LOG" | sed -n 's/.*SUMMARY: steps=\([0-9][0-9]*\).*/\1/p' | tail -n 1)
[[ -n "$PROMPT_STEPS" ]] || { echo 'prompt step summary missing from build log' >&2; tail -n 120 "$LOG" >&2; exit 1; }

echo "running snapshot restore sanity check..."
AARCHVM_PRINT_SUMMARY=1 \
AARCHVM_BUS_FASTPATH="$FASTPATH" \
AARCHVM_TIMER_SCALE="$VERIFY_TIMER_SCALE" \
timeout "$VERIFY_TIMEOUT_SEC" ./build/aarchvm "${AARCHVM_EXTRA_ARGS[@]}" \
  -snapshot-load "$SNAPSHOT" \
  -steps "$VERIFY_STEPS" \
  </dev/null > "$VERIFY_LOG" 2>&1

echo "snapshot restore sanity check finished"

grep -Fq 'SUMMARY:' "$VERIFY_LOG" || { echo 'snapshot restore verification missing SUMMARY' >&2; tail -n 120 "$VERIFY_LOG" >&2; exit 1; }

printf 'linux shell snapshot ready\nsnapshot: %s\nbuild log: %s\nverify log: %s\nstop pattern: %s\nprompt steps: %s\nu-boot command delay: %ss\nu-boot prompt delay: %ss\n' "$SNAPSHOT" "$LOG" "$VERIFY_LOG" "$STOP_PATTERN" "$PROMPT_STEPS" "$UBOOT_DELAY_SEC" "$PROMPT_DELAY_SEC"
