#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT_DIR"

LOG="${AARCHVM_FUNCTIONAL_LOG:-out/linux-functional-suite.log}"
STEPS="${AARCHVM_FUNCTIONAL_STEPS:-1200000000}"
TIMEOUT_SEC="${AARCHVM_FUNCTIONAL_TIMEOUT:-180s}"
TIMER_SCALE="${AARCHVM_TIMER_SCALE:-100}"
FASTPATH="${AARCHVM_BUS_FASTPATH:-1}"
UBOOT_DELAY_SEC="${AARCHVM_UBOOT_COMMAND_DELAY_SEC:-3}"
PROMPT_DELAY_SEC="${AARCHVM_UBOOT_PROMPT_DELAY_SEC:-1}"
COMMAND_STEP_DELTA="${AARCHVM_FUNCTIONAL_COMMAND_STEP_DELTA:-50000}"
COMMAND_STEP_GAP="${AARCHVM_FUNCTIONAL_COMMAND_STEP_GAP:-2000}"
BUILD_LOG="${AARCHVM_USERTEST_SNAPSHOT_LOG:-out/linux-usertests-shell-v1-build.log}"
LINUX_DTB="${AARCHVM_LINUX_DTB:-dts/aarchvm-linux-min.dtb}"
DTB_ADDR=0x47f00000
AARCHVM_ARGS_RAW="${AARCHVM_ARGS:-}"
AARCHVM_EXTRA_ARGS=()
if [[ -n "$AARCHVM_ARGS_RAW" ]]; then
  read -r -a AARCHVM_EXTRA_ARGS <<< "$AARCHVM_ARGS_RAW"
fi
INITRD="out/initramfs-usertests.cpio.gz"
INITRD_SIZE_HEX=$(printf '0x%x' "$(stat -c '%s' "$INITRD")")
DEFAULT_CMD=$(cat <<'EOC'
echo FUNCTIONAL-BEGIN
/bin/busybox uname -a
pwd
cat /proc/version
/bin/busybox ls /bin
ps
dmesg -s 128 >/dev/null
echo DMESG-OK
tmp=/tmp/dmesg-stress.log
: > "$tmp"
echo DMESG-STRESS-BEGIN
i=1
while [ "$i" -le 4 ]; do
  echo DMESG-STRESS-ROUND:$i
  dmesg | tee -a "$tmp"
  i=$((i + 1))
done
bad=$(LC_ALL=C tr -d '\11\12\15\40-\176' < "$tmp" | wc -c)
echo DMESG-STRESS-BADCOUNT:$bad
[ "$bad" -eq 0 ]
echo DMESG-STRESS PASS
echo DMESG-STRESS-END
ping -c 1 127.0.0.1 || true
mount
df
/bin/fpsimd_selftest
/bin/fpint_selftest
echo USERTESTS PASS
echo FUNCTIONAL-SUITE PASS
EOC
)
CMD="${AARCHVM_FUNCTIONAL_COMMAND:-$DEFAULT_CMD}"
STOP_PATTERN="FUNCTIONAL-SUITE PASS"

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

print_cmd() {
  local label="$1"
  shift
  printf '%s' "$label"
  printf ' %q' "$@"
  printf '\n'
}

if [[ ! -f "$BUILD_LOG" || ! -f "$INITRD" || tests/linux/build_usertests_rootfs.sh -nt "$BUILD_LOG" || tests/linux/build_linux_shell_snapshot.sh -nt "$BUILD_LOG" || "$INITRD" -nt "$BUILD_LOG" ]]; then
  tests/linux/build_linux_shell_snapshot.sh >/dev/null
fi

PROMPT_STEPS=$(tr -d '\r' < "$BUILD_LOG" | sed -n 's/.*SUMMARY: steps=\([0-9][0-9]*\).*/\1/p' | tail -n 1)
[[ -n "$PROMPT_STEPS" ]] || { echo 'prompt step summary missing from snapshot build log' >&2; tail -n 120 "$BUILD_LOG" >&2; exit 1; }
RX_SCRIPT=$(build_uart_rx_script "$CMD" "$((PROMPT_STEPS + COMMAND_STEP_DELTA))" "$COMMAND_STEP_GAP")

mkdir -p "$(dirname "$LOG")"
UBOOT_BOOT_CMDS=$(cat <<EOC
setenv bootargs console=ttyAMA0,115200 earlycon=pl011,0x09000000 rdinit=/init initramfs_async=0
booti 0x40400000 0x46000000:${INITRD_SIZE_HEX} ${DTB_ADDR}
EOC
)
AARCHVM_CMD=(
  ./build/aarchvm
  "${AARCHVM_EXTRA_ARGS[@]}"
  -bin u-boot-2026.01/build-qemu_arm64/u-boot.bin
  -load 0x0
  -entry 0x0
  -sp 0x47fff000
  -dtb "$LINUX_DTB"
  -dtb-addr "$DTB_ADDR"
  -segment linux-6.12.76/build-aarchvm/arch/arm64/boot/Image@0x40400000
  -segment "$INITRD"@0x46000000
  -steps "$STEPS"
  -stop-on-uart "$STOP_PATTERN"
  -fb-sdl off
)
print_cmd 'AARCHVM functional command:' "${AARCHVM_CMD[@]}"
printf 'U-Boot scripted input:\n%s\n' "$UBOOT_BOOT_CMDS"

set -o pipefail
(
  sleep "$UBOOT_DELAY_SEC"
  printf '\n'
  sleep "$PROMPT_DELAY_SEC"
  printf '%s\n' "$UBOOT_BOOT_CMDS"
) | \
AARCHVM_UART_RX_SCRIPT="$RX_SCRIPT" \
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

check 'FUNCTIONAL-BEGIN'
check 'Linux (none) 6.12.76'
check 'Linux version 6.12.76'
check 'busybox'
check 'PID   USER     TIME  COMMAND'
check 'DMESG-OK'
check 'DMESG-STRESS PASS'
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
printf 'linux functional suite passed\nlog: %s\nprompt steps: %s\n' "$LOG" "$PROMPT_STEPS"
