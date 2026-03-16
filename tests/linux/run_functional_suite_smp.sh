#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT_DIR"

SNAPSHOT="${AARCHVM_USERTEST_SNAPSHOT_OUT:-out/linux-smp-shell-v1.snap}"
BUILD_LOG="${AARCHVM_USERTEST_SNAPSHOT_LOG:-out/linux-smp-shell-v1-build.log}"
LOG="${AARCHVM_SMP_FUNCTIONAL_LOG:-out/linux-functional-suite-smp.log}"
STEP_LIMIT="${AARCHVM_SMP_FUNCTIONAL_STEP_LIMIT:-2500000000}"
TIMEOUT_SEC="${AARCHVM_SMP_FUNCTIONAL_TIMEOUT:-180s}"
FASTPATH="${AARCHVM_BUS_FASTPATH:-1}"
TIMER_SCALE="${AARCHVM_TIMER_SCALE:-10}"
LESS_BEGIN_SNAP="${AARCHVM_SMP_LESS_BEGIN_SNAPSHOT:-out/linux-smp-less-begin.snap}"
LESS_BEGIN_LOG="${AARCHVM_SMP_LESS_BEGIN_LOG:-out/linux-smp-less-begin.log}"
LESS_RESUME_LOG="${AARCHVM_SMP_LESS_RESUME_LOG:-out/linux-smp-less-resume.log}"
LESS_BEGIN_TIMEOUT="${AARCHVM_SMP_LESS_BEGIN_TIMEOUT:-60s}"
LESS_RESUME_TIMEOUT="${AARCHVM_SMP_LESS_RESUME_TIMEOUT:-40s}"

print_cmd() {
  local label="$1"
  shift
  printf '%s' "$label"
  printf ' %q' "$@"
  printf '\n'
}

if [[ ! -f "$SNAPSHOT" || ! -f "$BUILD_LOG" || tests/linux/build_linux_smp_shell_snapshot.sh -nt "$BUILD_LOG" ]]; then
  ./tests/linux/build_linux_smp_shell_snapshot.sh >/dev/null
fi

mkdir -p "$(dirname "$LOG")"
AARCHVM_CMD=(
  ./build/aarchvm
  -smp 2
  -snapshot-load "$SNAPSHOT"
  -steps "$STEP_LIMIT"
  -stop-on-uart 'SMP-FUNCTIONAL-SUITE PASS'
  -fb-sdl off
)
print_cmd 'AARCHVM SMP functional command:' "${AARCHVM_CMD[@]}"

set -o pipefail
printf 'for i in 1 2 3; do\n  echo SMP-ROUND:$i\n  uname -a\n  /bin/busybox uname -a\n  ps\n  dmesg -s 128 >/dev/null\n  echo SMP-DMESG-OK:$i\n  mount\n  df\n  cat /proc/cpuinfo\n  /bin/busybox ls /bin >/dev/null\n  ping -c 1 127.0.0.1 || true\ndone\necho SMP-FUNCTIONAL-SUITE PASS\n' | \
  AARCHVM_BUS_FASTPATH="$FASTPATH" \
  AARCHVM_TIMER_SCALE="$TIMER_SCALE" \
  timeout "$TIMEOUT_SEC" "${AARCHVM_CMD[@]}" \
    > "$LOG" 2>&1

tr -d '\r\000' < "$LOG" > "${LOG%.log}.clean.log"
CLEAN_LOG="${LOG%.log}.clean.log"

check() {
  local pattern="$1"
  if ! grep -aFq "$pattern" "$CLEAN_LOG"; then
    echo "missing expected output: $pattern" >&2
    echo "log: $LOG" >&2
    tail -n 120 "$CLEAN_LOG" >&2
    exit 1
  fi
}

check_absent() {
  local pattern="$1"
  local file="$2"
  if rg -n "$pattern" "$file" >/dev/null; then
    echo "unexpected output matching: $pattern" >&2
    tail -n 120 "$file" >&2
    exit 1
  fi
}

check 'SMP-ROUND:1'
check 'SMP-ROUND:2'
check 'SMP-ROUND:3'
check 'Linux (none) 6.12.76'
check 'GNU/Linux'
check 'PID   USER     TIME  COMMAND'
check '/bin/busybox sh'
check 'proc on /proc type proc'
check 'sysfs on /sys type sysfs'
check 'Filesystem           1K-blocks'
check '/dev'
check $'processor\t: 0'
check $'processor\t: 1'
check 'PING 127.0.0.1 (127.0.0.1): 56 data bytes'
check 'SMP-DMESG-OK:3'
check 'ping: sendto: Network is unreachable'
check 'SMP-FUNCTIONAL-SUITE PASS'

if rg -n 'Illegal instruction|Kernel panic|malloc\(|free\(\)|stack smashing|Attempted to kill init|Failed to load snapshot' "$CLEAN_LOG" >/dev/null; then
  echo 'unexpected failure during SMP functional suite' >&2
  tail -n 160 "$CLEAN_LOG" >&2
  exit 1
fi

LESS_CMD_RX='1000:0x73,5000:0x74,9000:0x74,13000:0x79,17000:0x20,21000:0x2d,25000:0x65,29000:0x63,33000:0x68,37000:0x6f,41000:0x0a,45000:0x64,49000:0x6d,53000:0x65,57000:0x73,61000:0x67,65000:0x20,69000:0x7c,73000:0x20,77000:0x6c,81000:0x65,85000:0x73,89000:0x73,93000:0x0a'
LESS_BEGIN_CMD=(
  ./build/aarchvm
  -smp 2
  -snapshot-load "$SNAPSHOT"
  -steps 150000000
  -stop-on-uart 'standard input'
  -snapshot-save "$LESS_BEGIN_SNAP"
  -fb-sdl off
)
print_cmd 'AARCHVM SMP less-begin command:' "${LESS_BEGIN_CMD[@]}"
AARCHVM_BUS_FASTPATH="$FASTPATH" \
AARCHVM_TIMER_SCALE="$TIMER_SCALE" \
AARCHVM_UART_RX_SCRIPT="$LESS_CMD_RX" \
timeout "$LESS_BEGIN_TIMEOUT" "${LESS_BEGIN_CMD[@]}" > "$LESS_BEGIN_LOG" 2>&1
tr -d '\r\000' < "$LESS_BEGIN_LOG" > "${LESS_BEGIN_LOG%.log}.clean.log"
LESS_BEGIN_CLEAN="${LESS_BEGIN_LOG%.log}.clean.log"
if [[ ! -f "$LESS_BEGIN_SNAP" ]]; then
  echo "missing less begin snapshot: $LESS_BEGIN_SNAP" >&2
  exit 1
fi
if ! grep -aFq 'standard input' "$LESS_BEGIN_CLEAN"; then
  echo 'failed to reach less prompt in SMP less smoke' >&2
  tail -n 120 "$LESS_BEGIN_CLEAN" >&2
  exit 1
fi
check_absent 'Illegal instruction|Kernel panic|Attempted to kill init|Failed to load snapshot' "$LESS_BEGIN_CLEAN"

LESS_RESUME_CMD=(
  ./build/aarchvm
  -smp 2
  -snapshot-load "$LESS_BEGIN_SNAP"
  -steps 30000000
  -fb-sdl off
)
print_cmd 'AARCHVM SMP less-resume command:' "${LESS_RESUME_CMD[@]}"
AARCHVM_BUS_FASTPATH="$FASTPATH" \
AARCHVM_TIMER_SCALE="$TIMER_SCALE" \
AARCHVM_UART_RX_SCRIPT='1000000:0x71' \
timeout "$LESS_RESUME_TIMEOUT" "${LESS_RESUME_CMD[@]}" > "$LESS_RESUME_LOG" 2>&1
tr -d '\r\000' < "$LESS_RESUME_LOG" > "${LESS_RESUME_LOG%.log}.clean.log"
LESS_RESUME_CLEAN="${LESS_RESUME_LOG%.log}.clean.log"
if ! grep -aFq '~ # ' "$LESS_RESUME_CLEAN"; then
  echo 'failed to exit less and return to shell prompt in SMP less smoke' >&2
  tail -n 120 "$LESS_RESUME_CLEAN" >&2
  exit 1
fi
check_absent 'Illegal instruction|Kernel panic|Attempted to kill init|Failed to load snapshot' "$LESS_RESUME_CLEAN"

printf 'linux SMP functional suite passed\nlog: %s\nless-begin log: %s\nless-resume log: %s\n' "$LOG" "$LESS_BEGIN_LOG" "$LESS_RESUME_LOG"
