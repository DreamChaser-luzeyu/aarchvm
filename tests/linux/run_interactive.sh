#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT_DIR"

SNAPSHOT="${AARCHVM_INTERACTIVE_SNAPSHOT:-out/linux-usertests-shell-v1.snap}"
FASTPATH="${AARCHVM_BUS_FASTPATH:-1}"
TIMER_SCALE="${AARCHVM_TIMER_SCALE:-10000}"
STEPS="${AARCHVM_INTERACTIVE_STEPS:-3000000000}"

if [[ ! -f "$SNAPSHOT" ]]; then
  echo "missing snapshot: $SNAPSHOT" >&2
  echo "build it first with tests/linux/build_linux_shell_snapshot.sh" >&2
  exit 1
fi

echo "snapshot interactive"
echo "  snapshot: $SNAPSHOT"
echo "  fastpath: $FASTPATH"
echo "  timer:    $TIMER_SCALE"
echo "  steps:    $STEPS"
echo

env \
  AARCHVM_BUS_FASTPATH="$FASTPATH" \
  AARCHVM_TIMER_SCALE="$TIMER_SCALE" \
  ./build/aarchvm \
  -snapshot-load "$SNAPSHOT" \
  -steps "$STEPS"
