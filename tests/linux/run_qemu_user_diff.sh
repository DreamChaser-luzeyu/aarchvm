#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT_DIR"

TIMEOUT_SEC="60s"

if ! command -v qemu-aarch64 >/dev/null 2>&1; then
  echo "missing qemu-aarch64" >&2
  exit 1
fi

./tests/linux/build_usertests_rootfs.sh >/dev/null

run_expect_lines() {
  local bin="$1"
  shift
  local output
  output="$(timeout "$TIMEOUT_SEC" qemu-aarch64 "./out/${bin}")"
  local filtered
  filtered="$(printf '%s\n' "$output" | tr -d '\r')"
  for expected in "$@"; do
    if ! grep -Fxq "$expected" <<<"$filtered"; then
      echo "unexpected qemu-aarch64 output for ${bin}: missing '${expected}'" >&2
      printf '%s\n' "$filtered" >&2
      exit 1
    fi
  done
}

run_expect_lines fpsimd_selftest "FPSIMD_SELFTEST PASS"
run_expect_lines fpint_selftest "FPINT_SELFTEST PASS"
run_expect_lines mprotect_exec_stress "EXECVE-OK" "MPROTECT-EXEC PASS iters=6"
run_expect_lines pthread_sync_stress "PTHREAD-SYNC PASS counter=200000 rounds=20000"

printf 'qemu user diff passed\n'
