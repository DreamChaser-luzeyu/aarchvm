#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT_DIR"

LOG="${AARCHVM_PERF_LOG:-out/perf-suite.log}"
RESULTS="${AARCHVM_PERF_RESULTS:-out/perf-suite-results.txt}"
STEPS="${AARCHVM_PERF_STEPS:-4000000000}"
TIMEOUT_SEC="${AARCHVM_PERF_TIMEOUT:-300s}"
TIMER_SCALE="${AARCHVM_TIMER_SCALE:-100}"
FASTPATH="${AARCHVM_BUS_FASTPATH:-1}"
UBOOT_DELAY_SEC="${AARCHVM_UBOOT_COMMAND_DELAY_SEC:-3}"
PROMPT_DELAY_SEC="${AARCHVM_UBOOT_PROMPT_DELAY_SEC:-1}"
COMMAND_STEP_DELTA="${AARCHVM_PERF_COMMAND_STEP_DELTA:-50000}"
COMMAND_STEP_GAP="${AARCHVM_PERF_COMMAND_STEP_GAP:-2000}"
BUILD_LOG="${AARCHVM_USERTEST_SNAPSHOT_LOG:-out/linux-usertests-shell-v1-build.log}"
AARCHVM_ARGS_RAW="${AARCHVM_ARGS:-}"
AARCHVM_EXTRA_ARGS=()
if [[ -n "$AARCHVM_ARGS_RAW" ]]; then
  read -r -a AARCHVM_EXTRA_ARGS <<< "$AARCHVM_ARGS_RAW"
fi
INITRD="out/initramfs-usertests.cpio"
INITRD_SIZE_HEX=$(printf '0x%x' "$(stat -c '%s' "$INITRD")")
CMD="${AARCHVM_PERF_COMMAND:-bench_runner}"

build_uart_rx_script() {
  local text="$1"
  local step="$2"
  local gap="$3"
  local out=""
  local byte
  for byte in $(printf '%s' "$text" | LC_ALL=C od -An -t u1 -v); do
    out+="${step}:0x$(printf '%02x' "$byte"),"
    step=$((step + gap))
  done
  out+="${step}:0x0a"
  printf '%s' "$out"
}

if [[ ! -f "$BUILD_LOG" || ! -f "$INITRD" || tests/linux/build_usertests_rootfs.sh -nt "$BUILD_LOG" || tests/linux/build_linux_shell_snapshot.sh -nt "$BUILD_LOG" || "$INITRD" -nt "$BUILD_LOG" ]]; then
  tests/linux/build_linux_shell_snapshot.sh >/dev/null
fi

PROMPT_STEPS=$(tr -d '\r' < "$BUILD_LOG" | sed -n 's/.*SUMMARY: steps=\([0-9][0-9]*\).*/\1/p' | tail -n 1)
[[ -n "$PROMPT_STEPS" ]] || { echo 'prompt step summary missing from snapshot build log' >&2; tail -n 120 "$BUILD_LOG" >&2; exit 1; }
RX_SCRIPT=$(build_uart_rx_script "$CMD" "$((PROMPT_STEPS + COMMAND_STEP_DELTA))" "$COMMAND_STEP_GAP")

mkdir -p "$(dirname "$LOG")"
set -o pipefail
(
  sleep "$UBOOT_DELAY_SEC"
  printf '\n'
  sleep "$PROMPT_DELAY_SEC"
  printf 'setenv bootargs console=ttyAMA0,115200 earlycon=pl011,0x09000000 rdinit=/init initramfs_async=0\n'
  printf 'booti 0x40400000 0x46000000:%s 0x47f00000\n' "$INITRD_SIZE_HEX"
) | \
AARCHVM_UART_RX_SCRIPT="$RX_SCRIPT" \
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
  -fb-sdl off \
  > "$LOG" 2>&1

tr -d '\r\000' < "$LOG" > "${LOG%.log}.clean.log"
CLEAN_LOG="${LOG%.log}.clean.log"
grep -a '^PERF-RESULT' "$CLEAN_LOG" > "$RESULTS"
RESULT_COUNT=$(wc -l < "$RESULTS")
if [[ "$RESULT_COUNT" -lt 1 ]]; then
  echo 'no PERF-RESULT lines captured' >&2
  tail -n 120 "$CLEAN_LOG" >&2
  exit 1
fi
printf 'linux algorithm perf suite passed\nlog: %s\nresults: %s\nprompt steps: %s\nresult_count: %s\n' "$LOG" "$RESULTS" "$PROMPT_STEPS" "$RESULT_COUNT"
