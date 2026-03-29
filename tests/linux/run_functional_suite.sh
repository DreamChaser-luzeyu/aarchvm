#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT_DIR"

LOG="${AARCHVM_FUNCTIONAL_LOG:-out/linux-functional-suite.log}"
SNAPSHOT="${AARCHVM_USERTEST_SNAPSHOT_OUT:-out/linux-usertests-shell-v1.snap}"
STEPS="${AARCHVM_FUNCTIONAL_STEPS:-1200000000}"
TIMEOUT_SEC="${AARCHVM_FUNCTIONAL_TIMEOUT:-180s}"
TIMER_SCALE="${AARCHVM_TIMER_SCALE:-1}"
FASTPATH="${AARCHVM_BUS_FASTPATH:-1}"
BUILD_LOG="${AARCHVM_USERTEST_SNAPSHOT_LOG:-out/linux-usertests-shell-v1-build.log}"
STDIN_RX_GAP="${AARCHVM_STDIN_RX_GAP:-20000}"
AARCHVM_ARGS_RAW="${AARCHVM_ARGS:-}"
AARCHVM_EXTRA_ARGS=()
if [[ -n "$AARCHVM_ARGS_RAW" ]]; then
  read -r -a AARCHVM_EXTRA_ARGS <<< "$AARCHVM_ARGS_RAW"
fi
DEFAULT_CMD="/bin/run_functional_suite"
CMD="${AARCHVM_FUNCTIONAL_COMMAND:-$DEFAULT_CMD}"
STOP_PATTERN="FUNCTIONAL-SUITE PASS"

print_cmd() {
  local label="$1"
  shift
  printf '%s' "$label"
  printf ' %q' "$@"
  printf '\n'
}

if [[ ! -f "$SNAPSHOT" || ! -f "$BUILD_LOG" || ./build/aarchvm -nt "$BUILD_LOG" || tests/linux/build_usertests_rootfs.sh -nt "$BUILD_LOG" || tests/linux/build_linux_shell_snapshot.sh -nt "$BUILD_LOG" ]]; then
  tests/linux/build_linux_shell_snapshot.sh >/dev/null
fi

test -f "$SNAPSHOT" || { echo "missing snapshot: $SNAPSHOT" >&2; exit 1; }

mkdir -p "$(dirname "$LOG")"
AARCHVM_CMD=(
  ./build/aarchvm
  "${AARCHVM_EXTRA_ARGS[@]}"
  -snapshot-load "$SNAPSHOT"
  -steps "$STEPS"
  -stop-on-uart "$STOP_PATTERN"
  -fb-sdl off
)
print_cmd 'AARCHVM functional command:' "${AARCHVM_CMD[@]}"

set -o pipefail
printf '%s\n' "$CMD" | \
AARCHVM_BUS_FASTPATH="$FASTPATH" \
AARCHVM_TIMER_SCALE="$TIMER_SCALE" \
AARCHVM_STDIN_RX_GAP="$STDIN_RX_GAP" \
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

check 'FUNCTIONAL-BEGIN'
check 'Linux (none) 6.12.76'
check 'Linux version 6.12.76'
check 'busybox'
check 'PID   USER     TIME  COMMAND'
check 'DMESG-OK'
check 'GREP-HANG-OK'
check 'DMESG-STRESS PASS'
check 'EXECVE-OK'
check 'MPROTECT-EXEC PASS'
check 'PING 127.0.0.1 (127.0.0.1): 56 data bytes'
check 'Filesystem           1K-blocks'
check 'FPSIMD_SELFTEST PASS'
check 'FPINT_SELFTEST PASS'
check 'USERTESTS PASS'
check 'FUNCTIONAL-SUITE PASS'
if grep -aFq 'Illegal instruction' "$CLEAN_LOG"; then
  echo 'unexpected illegal instruction in functional suite' >&2
  tail -n 120 "$CLEAN_LOG" >&2
  exit 1
fi
python3 - "$CLEAN_LOG" <<'PY'
import pathlib, sys
path = pathlib.Path(sys.argv[1])
data = path.read_bytes()
start = b'DMESG-STRESS-BEGIN\n'
end = b'DMESG-STRESS-END\n'
pos = 0
count = 0
while True:
    s = data.find(start, pos)
    if s < 0:
        break
    e = data.find(end, s)
    if e < 0:
        raise SystemExit('missing DMESG-STRESS-END marker')
    block = data[s + len(start):e]
    bad = [b for b in block if b not in (9, 10) and not (32 <= b <= 126)]
    if bad:
        raise SystemExit(f'unexpected non-printable byte in dmesg stress output: 0x{bad[0]:02x}')
    count += 1
    pos = e + len(end)
if count == 0:
    raise SystemExit('missing dmesg stress block in functional suite log')
PY
printf 'linux functional suite passed\nlog: %s\nsnapshot: %s\n' "$LOG" "$SNAPSHOT"
