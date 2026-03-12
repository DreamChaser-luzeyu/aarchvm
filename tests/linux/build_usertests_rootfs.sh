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
  out/initramfs-usertests-root/bin/bench_runner \
  out/initramfs-usertests-root/bin/fpsimd_selftest \
  out/initramfs-usertests-root/bin/fpint_selftest \
  out/initramfs-usertests-root/bin/run_usertests \
  out/initramfs-usertests-root/bin/run_functional_suite

( cd out/initramfs-usertests-root && find . -print0 | cpio --null -o -H newc > ../initramfs-usertests.cpio )
gzip -n -f -c out/initramfs-usertests.cpio > out/initramfs-usertests.cpio.gz

printf 'usertests rootfs ready: %s\n' out/initramfs-usertests.cpio.gz
