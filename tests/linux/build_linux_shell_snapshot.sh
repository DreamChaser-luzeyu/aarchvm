#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT_DIR"

tests/linux/build_usertests_rootfs.sh >/dev/null

SNAPSHOT="${AARCHVM_USERTEST_SNAPSHOT_OUT:-out/linux-usertests-shell-v1.snap}"
LOG="${AARCHVM_USERTEST_SNAPSHOT_LOG:-out/linux-usertests-shell-v1-build.log}"
VERIFY_LOG="${AARCHVM_USERTEST_SNAPSHOT_VERIFY_LOG:-out/linux-usertests-shell-v1-verify.log}"
STEPS="${AARCHVM_USERTEST_SNAPSHOT_STEPS:-1800000000}"
VERIFY_STEPS="${AARCHVM_USERTEST_SNAPSHOT_VERIFY_STEPS:-120000000}"
TIMEOUT_SEC="${AARCHVM_USERTEST_SNAPSHOT_TIMEOUT:-240s}"
VERIFY_TIMEOUT_SEC="${AARCHVM_USERTEST_SNAPSHOT_VERIFY_TIMEOUT:-60s}"
TIMER_SCALE="${AARCHVM_TIMER_SCALE:-10000}"
VERIFY_TIMER_SCALE="${AARCHVM_VERIFY_TIMER_SCALE:-1}"
FASTPATH="${AARCHVM_BUS_FASTPATH:-1}"
STOP_PATTERN="${AARCHVM_STOP_ON_UART_PATTERN:-~ # }"
INITRD="out/initramfs-usertests.cpio"
INITRD_SIZE_HEX=$(printf '0x%x' "$(stat -c '%s' "$INITRD")")

mkdir -p "$(dirname "$SNAPSHOT")" "$(dirname "$LOG")"
rm -f "$SNAPSHOT"

set -o pipefail
printf 'setenv bootargs console=ttyAMA0,115200 earlycon=pl011,0x09000000 ignore_loglevel rdinit=/init\nbooti 0x40400000 0x46000000:%s 0x47f00000\n' "$INITRD_SIZE_HEX" | \
AARCHVM_BUS_FASTPATH="$FASTPATH" \
AARCHVM_TIMER_SCALE="$TIMER_SCALE" \
timeout "$TIMEOUT_SEC" ./build/aarchvm \
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
  -snapshot-save "$SNAPSHOT" 

test -f "$SNAPSHOT" || { echo "snapshot not produced: $SNAPSHOT" >&2; exit 1; }

set -o pipefail
(
  sleep 1
  printf 'echo USERTEST_SHELL_READY\n'
  sleep 1
) | \
AARCHVM_BUS_FASTPATH="$FASTPATH" \
AARCHVM_TIMER_SCALE="$VERIFY_TIMER_SCALE" \
timeout "$VERIFY_TIMEOUT_SEC" ./build/aarchvm \
  -snapshot-load "$SNAPSHOT" \
  -steps "$VERIFY_STEPS" \
  > "$VERIFY_LOG" 2>&1

tr -d '\r' < "$VERIFY_LOG" > "${VERIFY_LOG%.log}.clean.log"
CLEAN_VERIFY_LOG="${VERIFY_LOG%.log}.clean.log"
grep -Fq 'USERTEST_SHELL_READY' "$CLEAN_VERIFY_LOG" || { echo 'usertest shell verification marker missing' >&2; tail -n 120 "$CLEAN_VERIFY_LOG" >&2; exit 1; }

printf 'linux shell snapshot ready\nsnapshot: %s\nbuild log: %s\nverify log: %s\nstop pattern: %s\n' "$SNAPSHOT" "$LOG" "$VERIFY_LOG" "$STOP_PATTERN"
