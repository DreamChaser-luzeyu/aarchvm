#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT_DIR"

SNAPSHOT="${AARCHVM_PERF_SNAPSHOT:-out/linux-usertests-shell-v1.snap}"
LOG="${AARCHVM_PERF_LOG:-out/perf-suite.log}"
RESULTS="${AARCHVM_PERF_RESULTS:-out/perf-suite-results.txt}"
STEPS="${AARCHVM_PERF_STEPS:-1000000000}"
TIMEOUT_SEC="${AARCHVM_PERF_TIMEOUT:-240s}"
TIMER_SCALE="${AARCHVM_TIMER_SCALE:-1}"
FASTPATH="${AARCHVM_BUS_FASTPATH:-1}"
CMD="${AARCHVM_PERF_COMMAND:-bench_runner}"

if [[ ! -f "$SNAPSHOT" || out/initramfs-usertests.cpio -nt "$SNAPSHOT" || out/bench_runner -nt "$SNAPSHOT" ]]; then
  tests/linux/build_linux_shell_snapshot.sh >/dev/null
fi

mkdir -p "$(dirname "$LOG")"
set -o pipefail
(
  sleep 1
  printf '%s\n' "$CMD"
) | \
AARCHVM_BUS_FASTPATH="$FASTPATH" \
AARCHVM_TIMER_SCALE="$TIMER_SCALE" \
timeout "$TIMEOUT_SEC" ./build/aarchvm \
  -snapshot-load "$SNAPSHOT" \
  -steps "$STEPS" \
  > "$LOG" 2>&1

tr -d '\r' < "$LOG" > "${LOG%.log}.clean.log"
CLEAN_LOG="${LOG%.log}.clean.log"
grep '^PERF-RESULT' "$CLEAN_LOG" > "$RESULTS"
RESULT_COUNT=$(wc -l < "$RESULTS")
if [[ "$RESULT_COUNT" -lt 1 ]]; then
  echo 'no PERF-RESULT lines captured' >&2
  tail -n 120 "$CLEAN_LOG" >&2
  exit 1
fi
printf 'linux algorithm perf suite passed\nlog: %s\nresults: %s\nresult_count: %s\n' "$LOG" "$RESULTS" "$RESULT_COUNT"
