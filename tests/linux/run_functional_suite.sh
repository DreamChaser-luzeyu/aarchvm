#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT_DIR"

SNAPSHOT="${AARCHVM_USERTEST_SNAPSHOT:-out/linux-usertests-shell-v1.snap}"
LOG="${AARCHVM_FUNCTIONAL_LOG:-out/linux-functional-suite.log}"
STEPS="${AARCHVM_FUNCTIONAL_STEPS:-400000000}"
TIMEOUT_SEC="${AARCHVM_FUNCTIONAL_TIMEOUT:-120s}"
TIMER_SCALE="${AARCHVM_TIMER_SCALE:-1}"
FASTPATH="${AARCHVM_BUS_FASTPATH:-1}"
CMD="${AARCHVM_FUNCTIONAL_COMMAND:-run_functional_suite}"

if [[ ! -f "$SNAPSHOT" || out/initramfs-usertests.cpio -nt "$SNAPSHOT" || out/fpsimd_selftest -nt "$SNAPSHOT" || out/fpint_selftest -nt "$SNAPSHOT" ]]; then
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
check() {
  local pattern="$1"
  if ! grep -Fq "$pattern" "$CLEAN_LOG"; then
    echo "missing expected output: $pattern" >&2
    echo "log: $LOG" >&2
    tail -n 120 "$CLEAN_LOG" >&2
    exit 1
  fi
}

check 'FUNCTIONAL-BEGIN'
check 'Linux (none) 6.12.76'
check 'run_functional_suite'
check 'Linux version 6.12.76'
check 'busybox'
check 'PID   USER     TIME  COMMAND'
check 'PING 127.0.0.1 (127.0.0.1): 56 data bytes'
check 'Filesystem           1K-blocks'
check 'FPSIMD_SELFTEST PASS'
check 'FPINT_SELFTEST PASS'
check 'USERTESTS PASS'
check 'FUNCTIONAL-SUITE PASS'
if grep -Fq 'Illegal instruction' "$CLEAN_LOG"; then
  echo 'unexpected illegal instruction in functional suite' >&2
  tail -n 120 "$CLEAN_LOG" >&2
  exit 1
fi
printf 'linux functional suite passed\nlog: %s\n' "$LOG"
