#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT_DIR"

mkdir -p out

aarch64-linux-gnu-gcc -O2 -static -Wall -Wextra -march=armv8-a -mno-outline-atomics -fno-tree-vectorize -fno-tree-slp-vectorize \
  -o out/bench_runner tests/linux/bench_runner.c

aarch64-linux-gnu-gcc -O2 -static -Wall -Wextra -march=armv8-a -mno-outline-atomics -fno-tree-vectorize -fno-tree-slp-vectorize \
  -o out/fpsimd_selftest tests/linux/fpsimd_selftest.c tests/linux/fpsimd_selftest.S

aarch64-linux-gnu-gcc -O2 -static -Wall -Wextra -march=armv8-a -mno-outline-atomics -fno-tree-vectorize -fno-tree-slp-vectorize \
  -o out/fpint_selftest tests/linux/fpint_selftest.c

aarch64-linux-gnu-gcc -O2 -static -Wall -Wextra -march=armv8-a -mno-outline-atomics -fno-tree-vectorize -fno-tree-slp-vectorize \
  -o out/functional_init tests/linux/functional_init.c

aarch64-linux-gnu-gcc -O2 -static -Wall -Wextra -march=armv8-a -mno-outline-atomics -fno-tree-vectorize -fno-tree-slp-vectorize \
  -o out/fb_mark tests/linux/fb_mark.c

aarch64-linux-gnu-gcc -O2 -static -Wall -Wextra -march=armv8-a -mno-outline-atomics -fno-tree-vectorize -fno-tree-slp-vectorize \
  -o out/console_mux tests/linux/console_mux.c -lutil

aarch64-linux-gnu-gcc -O2 -static -Wall -Wextra -march=armv8-a -mno-outline-atomics -fno-tree-vectorize -fno-tree-slp-vectorize \
  -o out/poll_tty_smoke tests/linux/poll_tty_smoke.c

aarch64-linux-gnu-gcc -O2 -static -Wall -Wextra -march=armv8-a -mno-outline-atomics -fno-tree-vectorize -fno-tree-slp-vectorize \
  -o out/poll_tty_affine tests/linux/poll_tty_affine.c

aarch64-linux-gnu-gcc -O2 -static -Wall -Wextra -march=armv8-a -mno-outline-atomics -fno-tree-vectorize -fno-tree-slp-vectorize \
  -o out/poll_pipe_tty_affine tests/linux/poll_pipe_tty_affine.c

aarch64-linux-gnu-gcc -O2 -static -Wall -Wextra -march=armv8-a -mno-outline-atomics -fno-tree-vectorize -fno-tree-slp-vectorize \
  -o out/read_tty_spin_affine tests/linux/read_tty_spin_affine.c

if [[ ! -d out/initramfs-full-root ]]; then
  echo "missing out/initramfs-full-root; build the full busybox rootfs first" >&2
  exit 1
fi

rm -rf out/initramfs-usertests-root
cp -a out/initramfs-full-root out/initramfs-usertests-root
mkdir -p out/initramfs-usertests-root/bin
cp out/bench_runner out/initramfs-usertests-root/bin/bench_runner
cp out/fpsimd_selftest out/initramfs-usertests-root/bin/fpsimd_selftest
cp out/fpint_selftest out/initramfs-usertests-root/bin/fpint_selftest
cp out/functional_init out/initramfs-usertests-root/bin/functional_init
cp out/fb_mark out/initramfs-usertests-root/bin/fb_mark
cp out/console_mux out/initramfs-usertests-root/bin/console_mux
cp out/poll_tty_smoke out/initramfs-usertests-root/bin/poll_tty_smoke
cp out/poll_tty_affine out/initramfs-usertests-root/bin/poll_tty_affine
cp out/poll_pipe_tty_affine out/initramfs-usertests-root/bin/poll_pipe_tty_affine
cp out/read_tty_spin_affine out/initramfs-usertests-root/bin/read_tty_spin_affine

cat > out/initramfs-usertests-root/init <<'EOS'
#!/bin/sh

export PATH=/bin:/sbin:/usr/bin:/usr/sbin

echo "INIT-START"

/bin/busybox mkdir -p /dev /proc /sys /tmp /run /mnt /root /etc /dev/pts
/bin/busybox mount -t devtmpfs devtmpfs /dev
[ -c /dev/console ] || /bin/busybox mknod /dev/console c 5 1
[ -c /dev/null ] || /bin/busybox mknod /dev/null c 1 3

echo "INIT-DEV-OK"
serial_stdio=/dev/console
if [ -c /dev/ttyAMA0 ]; then
  serial_stdio=/dev/ttyAMA0
fi
exec >"$serial_stdio" 2>&1 <"$serial_stdio"

echo "INIT-CONSOLE-OK"
/bin/busybox mount -t proc proc /proc
/bin/busybox mount -t sysfs sysfs /sys
/bin/busybox mount -t devpts devpts /dev/pts 2>/dev/null || true
[ -e /dev/ptmx ] || /bin/busybox ln -snf pts/ptmx /dev/ptmx
[ -c /dev/tty0 ] || /bin/busybox mknod /dev/tty0 c 4 0 2>/dev/null || true
[ -c /dev/tty1 ] || /bin/busybox mknod /dev/tty1 c 4 1 2>/dev/null || true

echo "*** AARCHVM BUSYBOX INITRAMFS OK ***"
/bin/busybox uname -a
/bin/busybox ls /

suite=""
shell_mode="serial"
if [ -r /proc/cmdline ]; then
  for arg in $(cat /proc/cmdline); do
    case "$arg" in
      aarchvm_suite=*)
        suite=${arg#*=}
        ;;
      aarchvm_shell=*)
        shell_mode=${arg#*=}
        ;;
    esac
  done
fi

case "$suite" in
  functional)
    echo "AARCHVM suite: functional"
    exec /bin/functional_init
    ;;
  perf)
    echo "AARCHVM suite: perf"
    exec /bin/bench_runner
    ;;
  usertests)
    echo "AARCHVM suite: usertests"
    exec /bin/sh /bin/run_usertests
    ;;
  *)
    if [ "$shell_mode" = "tty1" ] && [ -c /dev/tty1 ]; then
      echo "Entering BusyBox framebuffer shell on tty1"
      exec </dev/tty1 >/dev/tty1 2>&1
      exec /bin/busybox setsid /bin/busybox cttyhack /bin/busybox sh
    fi
    echo "Entering BusyBox serial shell"
    exec /bin/busybox setsid /bin/busybox cttyhack /bin/busybox sh
    ;;
esac
EOS

cat > out/initramfs-usertests-root/bin/run_dmesg_stress_check <<'EOS'
#!/bin/sh
set -e
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
EOS

cat > out/initramfs-usertests-root/bin/run_usertests <<'EOS'
#!/bin/sh
set -e
fpsimd_selftest
fpint_selftest
echo USERTESTS PASS
EOS

cat > out/initramfs-usertests-root/bin/run_functional_suite <<'EOS'
#!/bin/sh
set -e
echo FUNCTIONAL-BEGIN
/bin/busybox uname -a
pwd
cat /proc/version
/bin/busybox ls /bin
ps
dmesg -s 128 >/dev/null
echo DMESG-OK
/bin/run_dmesg_stress_check
ping -c 1 127.0.0.1 || true
mount
df
/bin/run_usertests
echo FUNCTIONAL-SUITE PASS
EOS

chmod 0755 \
  out/initramfs-usertests-root/init \
  out/initramfs-usertests-root/bin/bench_runner \
  out/initramfs-usertests-root/bin/fpsimd_selftest \
  out/initramfs-usertests-root/bin/fpint_selftest \
  out/initramfs-usertests-root/bin/functional_init \
  out/initramfs-usertests-root/bin/fb_mark \
  out/initramfs-usertests-root/bin/console_mux \
  out/initramfs-usertests-root/bin/poll_tty_smoke \
  out/initramfs-usertests-root/bin/poll_tty_affine \
  out/initramfs-usertests-root/bin/poll_pipe_tty_affine \
  out/initramfs-usertests-root/bin/read_tty_spin_affine \
  out/initramfs-usertests-root/bin/run_dmesg_stress_check \
  out/initramfs-usertests-root/bin/run_usertests \
  out/initramfs-usertests-root/bin/run_functional_suite

(
  cd out/initramfs-usertests-root
  find . -print0 | sort -z | cpio --null -o -H newc > ../initramfs-usertests.cpio
)
gzip -n -f -c out/initramfs-usertests.cpio > out/initramfs-usertests.cpio.gz

printf 'usertests rootfs ready: %s\n' out/initramfs-usertests.cpio.gz
