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

cat > out/initramfs-usertests-root/init <<'EOS'
#!/bin/sh

echo "INIT-START"

mount -t devtmpfs devtmpfs /dev
[ -c /dev/console ] || mknod /dev/console c 5 1
[ -c /dev/null ] || mknod /dev/null c 1 3

echo "INIT-DEV-OK"
exec >/dev/console 2>&1 </dev/console

echo "INIT-CONSOLE-OK"
mount -t proc proc /proc
mount -t sysfs sysfs /sys
mkdir -p /dev/pts
mount -t devpts devpts /dev/pts 2>/dev/null
[ -c /dev/tty0 ] || mknod /dev/tty0 c 4 0 2>/dev/null || true

if [ -x /bin/fb_mark ]; then
  /bin/fb_mark || true
fi

if [ -c /dev/tty0 ]; then
  printf '\033[2J\033[H' >/dev/tty0
  echo '*** AARCHVM LINUX FRAMEBUFFER OK ***' >/dev/tty0
  uname -a >/dev/tty0
  echo >/dev/tty0
fi

echo "*** AARCHVM BUSYBOX INITRAMFS OK ***"
uname -a
ls /

suite=""
if [ -r /proc/cmdline ]; then
  for arg in $(cat /proc/cmdline); do
    case "$arg" in
      aarchvm_suite=*) suite="${arg#aarchvm_suite=}" ;;
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
    echo "Entering BusyBox shell"
    exec setsid cttyhack sh
    ;;
esac
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
uname -a
pwd
cat /proc/version
ls /bin
ps
ping -c 1 127.0.0.1 || true
mount
df
run_usertests
echo FUNCTIONAL-SUITE PASS
EOS

chmod 0755 \
  out/initramfs-usertests-root/init \
  out/initramfs-usertests-root/bin/bench_runner \
  out/initramfs-usertests-root/bin/fpsimd_selftest \
  out/initramfs-usertests-root/bin/fpint_selftest \
  out/initramfs-usertests-root/bin/functional_init \
  out/initramfs-usertests-root/bin/fb_mark \
  out/initramfs-usertests-root/bin/run_usertests \
  out/initramfs-usertests-root/bin/run_functional_suite

( cd out/initramfs-usertests-root && find . -print0 | cpio --null -o -H newc > ../initramfs-usertests.cpio )
gzip -n -f -c out/initramfs-usertests.cpio > out/initramfs-usertests.cpio.gz

printf 'usertests rootfs ready: %s\n' out/initramfs-usertests.cpio.gz
