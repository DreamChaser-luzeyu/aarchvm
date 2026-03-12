#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT_DIR"

SNAPSHOT="${AARCHVM_BUSYBOX_SNAPSHOT:-out/linux-shell-v11.snap}"
LOG="${AARCHVM_BUSYBOX_LOG:-out/busybox-smoke.log}"
CLEAN_LOG="${LOG%.log}.clean.log"
STEPS="${AARCHVM_BUSYBOX_STEPS:-80000000}"
TIMEOUT_SEC="${AARCHVM_BUSYBOX_TIMEOUT:-40s}"
TIMER_SCALE="${AARCHVM_TIMER_SCALE:-1}"
FASTPATH="${AARCHVM_BUS_FASTPATH:-1}"

if [[ ! -f "$SNAPSHOT" ]]; then
  echo "snapshot not found: $SNAPSHOT" >&2
  exit 1
fi

mkdir -p "$(dirname "$LOG")"

set -o pipefail
(
  sleep 1
  printf 'echo READY\n'
  sleep 1
  printf 'uname -a\n'
  sleep 4
  printf 'pwd\n'
  sleep 2
  printf 'cat /proc/version\n'
  sleep 4
  printf 'ls /bin\n'
  sleep 4
  printf 'ps\n'
  sleep 4
  printf 'mount\n'
  sleep 4
  printf 'df\n'
  sleep 3
  printf 'echo DONE\n'
  sleep 1
) | \
AARCHVM_BUS_FASTPATH="$FASTPATH" \
AARCHVM_TIMER_SCALE="$TIMER_SCALE" \
timeout "$TIMEOUT_SEC" ./build/aarchvm \
  -snapshot-load "$SNAPSHOT" \
  -steps "$STEPS" \
  > "$LOG" 2>&1

tr -d '\r' < "$LOG" > "$CLEAN_LOG"

check() {
  local pattern="$1"
  if ! grep -Fq "$pattern" "$CLEAN_LOG"; then
    echo "missing expected output: $pattern" >&2
    echo "log: $LOG" >&2
    tail -n 80 "$CLEAN_LOG" >&2
    exit 1
  fi
}

check 'READY'
check 'Linux (none) 6.12.76'
check '~ # pwd'
check '~ # cat /proc/version'
check 'Linux version 6.12.76'
check 'busybox'
check 'PID   USER     TIME  COMMAND'
check 'rootfs on / type rootfs'
check 'Filesystem           1K-blocks'
check 'DONE'

printf 'busybox smoke passed\nlog: %s\n' "$LOG"
