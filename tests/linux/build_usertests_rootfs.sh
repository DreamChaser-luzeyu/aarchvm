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

cat > out/initramfs-usertests-root/init <<'EOS'
#!/bin/sh

export PATH=/bin:/sbin:/usr/bin:/usr/sbin

echo "INIT-START"
/bin/busybox --install -s /bin >/dev/null 2>&1 || true

/bin/busybox mkdir -p /dev /proc /sys /tmp /run /mnt /root /etc /dev/pts
/bin/busybox mount -t devtmpfs devtmpfs /dev
[ -c /dev/console ] || /bin/busybox mknod /dev/console c 5 1
[ -c /dev/null ] || /bin/busybox mknod /dev/null c 1 3

echo "INIT-DEV-OK"
exec >/dev/console 2>&1 </dev/console

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
      aarchvm_suite=*) suite="${arg#aarchvm_suite=}" ;;
      aarchvm_shell=*) shell_mode="${arg#aarchvm_shell=}" ;;
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
  out/initramfs-usertests-root/bin/console_mux \
  out/initramfs-usertests-root/bin/run_usertests \
  out/initramfs-usertests-root/bin/run_functional_suite

(
  cd out/initramfs-usertests-root
  {
    printf '.\0'
    printf './init\0'
    printf './bin\0'
    printf './bin/busybox\0'
    printf './bin/sh\0'
    printf './bin/console_mux\0'
    printf './bin/fb_mark\0'
    printf './bin/bench_runner\0'
    printf './bin/fpsimd_selftest\0'
    printf './bin/fpint_selftest\0'
    printf './bin/functional_init\0'
    printf './bin/run_usertests\0'
    printf './bin/run_functional_suite\0'
    printf './dev\0'
    printf './proc\0'
    printf './sys\0'
    printf './tmp\0'
    printf './run\0'
    printf './mnt\0'
    printf './root\0'
    printf './etc\0'
    find . -print0 | \
      grep -zvxF '.' | \
      grep -zvxF './init' | \
      grep -zvxF './bin' | \
      grep -zvxF './bin/busybox' | \
      grep -zvxF './bin/sh' | \
      grep -zvxF './bin/console_mux' | \
      grep -zvxF './bin/fb_mark' | \
      grep -zvxF './bin/bench_runner' | \
      grep -zvxF './bin/fpsimd_selftest' | \
      grep -zvxF './bin/fpint_selftest' | \
      grep -zvxF './bin/functional_init' | \
      grep -zvxF './bin/run_usertests' | \
      grep -zvxF './bin/run_functional_suite' | \
      grep -zvxF './dev' | \
      grep -zvxF './proc' | \
      grep -zvxF './sys' | \
      grep -zvxF './tmp' | \
      grep -zvxF './run' | \
      grep -zvxF './mnt' | \
      grep -zvxF './root' | \
      grep -zvxF './etc' | \
      sort -z
  } | cpio --null -o -H newc > ../initramfs-usertests.cpio
)
gzip -n -f -c out/initramfs-usertests.cpio > out/initramfs-usertests.cpio.gz

printf 'usertests rootfs ready: %s\n' out/initramfs-usertests.cpio.gz
