#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT_DIR"

SNAPSHOT="${AARCHVM_INTERACTIVE_SNAPSHOT:-out/linux-usertests-shell-v1.snap}"
FASTPATH="${AARCHVM_BUS_FASTPATH:-1}"
TIMER_SCALE="${AARCHVM_TIMER_SCALE:-1}"
ARCH_TIMER_MODE="${AARCHVM_ARCH_TIMER_MODE:-host}"
STEPS="${AARCHVM_INTERACTIVE_STEPS:-3000000000}"

print_cmd() {
  local label="$1"
  shift
  printf '%s' "$label"
  printf ' %q' "$@"
  printf '\n'
}

if [[ ! -f "$SNAPSHOT" ]]; then
  echo "missing snapshot: $SNAPSHOT" >&2
  echo "build it first with tests/linux/build_linux_shell_snapshot.sh" >&2
  exit 1
fi

AARCHVM_CMD=(
  ./build/aarchvm
  -snapshot-load "$SNAPSHOT"
  -steps "$STEPS"
  -arch-timer-mode "$ARCH_TIMER_MODE"
)

echo "snapshot interactive"
echo "  snapshot: $SNAPSHOT"
echo "  fastpath: $FASTPATH"
echo "  timer:    $TIMER_SCALE"
echo "  arch-timer-mode: $ARCH_TIMER_MODE"
echo "  steps:    $STEPS"
print_cmd 'AARCHVM interactive command:' "${AARCHVM_CMD[@]}"
echo

env \
  AARCHVM_BUS_FASTPATH="$FASTPATH" \
  AARCHVM_TIMER_SCALE="$TIMER_SCALE" \
  "${AARCHVM_CMD[@]}"
