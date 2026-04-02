#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT_DIR"

if [[ ! -d out/initramfs-full-root ]]; then
  echo "missing out/initramfs-full-root; build the full busybox rootfs first" >&2
  exit 1
fi

ROOTFS_ROOT="${AARCHVM_DEBIAN_BOOT_ROOT:-out/initramfs-debian-systemd-root}"
INITRD="${AARCHVM_DEBIAN_BOOT_INITRD:-out/initramfs-debian-systemd.cpio.gz}"
INITRD_RAW="${INITRD%.gz}"
INITRD_RAW_ABS="$(cd "$(dirname "$INITRD_RAW")" && pwd)/$(basename "$INITRD_RAW")"
INITRD_ABS="$(cd "$(dirname "$INITRD")" && pwd)/$(basename "$INITRD")"

mkdir -p "$(dirname "$INITRD")"

rm -rf "$ROOTFS_ROOT"
cp -a out/initramfs-full-root "$ROOTFS_ROOT"

cat > "$ROOTFS_ROOT/init" <<'EOS'
#!/bin/sh
set -eu

export PATH=/bin:/sbin:/usr/bin:/usr/sbin

log() {
  echo "$*"
}

mkdir -p /dev /proc /sys /run /tmp /newroot /dev/pts
/bin/busybox mount -t devtmpfs devtmpfs /dev
[ -c /dev/console ] || /bin/busybox mknod /dev/console c 5 1
[ -c /dev/null ] || /bin/busybox mknod /dev/null c 1 3

serial_stdio=/dev/console
if [ -c /dev/ttyAMA0 ]; then
  serial_stdio=/dev/ttyAMA0
fi
exec >"$serial_stdio" 2>&1 <"$serial_stdio"

log "AARCHVM-DEBIAN-INITRAMFS START"
/bin/busybox mount -t proc proc /proc
/bin/busybox mount -t sysfs sysfs /sys
/bin/busybox mount -t tmpfs tmpfs /run
/bin/busybox mount -t devpts devpts /dev/pts 2>/dev/null || true
[ -e /dev/ptmx ] || /bin/busybox ln -snf pts/ptmx /dev/ptmx
/bin/busybox uname -a

log "AARCHVM-DEBIAN-INITRAMFS WAIT-BLOCK"
i=0
while [ "$i" -lt 50 ]; do
  if [ -b /dev/vda ]; then
    break
  fi
  i=$((i + 1))
  sleep 1
done

if [ ! -b /dev/vda ]; then
  log "AARCHVM-DEBIAN-INITRAMFS missing /dev/vda"
  exec /bin/busybox sh
fi

/bin/busybox mount -t ext4 -o rw /dev/vda /newroot
log "AARCHVM-DEBIAN-ROOT MOUNTED"

/bin/busybox mkdir -p \
  /newroot/proc \
  /newroot/sys \
  /newroot/dev \
  /newroot/run \
  /newroot/tmp \
  /newroot/dev/pts \
  /newroot/etc/systemd/system/getty.target.wants \
  /newroot/etc/systemd/system/multi-user.target.wants \
  /newroot/etc/systemd/system/serial-getty@ttyAMA0.service.d

/bin/busybox mount -o bind /proc /newroot/proc
/bin/busybox mount -o bind /sys /newroot/sys
/bin/busybox mount -o bind /dev /newroot/dev
/bin/busybox mount -o bind /run /newroot/run

if [ -x /newroot/debootstrap/debootstrap ] && [ ! -f /newroot/etc/.aarchvm-debootstrap-second-stage-done ]; then
  log "AARCHVM-DEBIAN-SECOND-STAGE START"
  if ! /bin/busybox chroot /newroot /bin/sh -c 'export DEBIAN_FRONTEND=noninteractive; /debootstrap/debootstrap --second-stage'; then
    log "AARCHVM-DEBIAN-SECOND-STAGE FAILED"
    exec /bin/busybox sh
  fi
  log "AARCHVM-DEBIAN-SECOND-STAGE DONE"
  : > /newroot/etc/.aarchvm-debootstrap-second-stage-done
fi

if [ ! -f /newroot/etc/.aarchvm-systemd-configured ]; then
  log "AARCHVM-DEBIAN-CONFIG START"
  cat > /newroot/etc/hostname <<'EOF_HOSTNAME'
aarchvm-debian
EOF_HOSTNAME
  cat > /newroot/etc/hosts <<'EOF_HOSTS'
127.0.0.1 localhost
127.0.1.1 aarchvm-debian
EOF_HOSTS
  cat > /newroot/etc/fstab <<'EOF_FSTAB'
/dev/vda  /         ext4    defaults,rw  0 1
proc      /proc     proc    defaults     0 0
sysfs     /sys      sysfs   defaults     0 0
devtmpfs  /dev      devtmpfs mode=0755   0 0
devpts    /dev/pts  devpts  gid=5,mode=620,ptmxmode=666 0 0
tmpfs     /run      tmpfs   mode=0755,nosuid,nodev      0 0
tmpfs     /tmp      tmpfs   mode=1777,nosuid,nodev      0 0
EOF_FSTAB
  : > /newroot/etc/machine-id

  if [ -x /newroot/usr/lib/systemd/systemd ]; then
    /bin/busybox ln -snf /usr/lib/systemd/systemd /newroot/sbin/init
  elif [ -x /newroot/lib/systemd/systemd ]; then
    /bin/busybox ln -snf /lib/systemd/systemd /newroot/sbin/init
  fi

  if [ -e /newroot/usr/lib/systemd/system/serial-getty@.service ]; then
    /bin/busybox ln -snf /usr/lib/systemd/system/serial-getty@.service \
      /newroot/etc/systemd/system/getty.target.wants/serial-getty@ttyAMA0.service
  elif [ -e /newroot/lib/systemd/system/serial-getty@.service ]; then
    /bin/busybox ln -snf /lib/systemd/system/serial-getty@.service \
      /newroot/etc/systemd/system/getty.target.wants/serial-getty@ttyAMA0.service
  fi

  cat > /newroot/etc/systemd/system/serial-getty@ttyAMA0.service.d/autologin.conf <<'EOF_AUTOLOGIN'
[Service]
ExecStart=
ExecStart=-/sbin/agetty --noreset --noclear --autologin root 115200,38400,9600 %I vt102
EOF_AUTOLOGIN

  cat > /newroot/etc/systemd/system/aarchvm-ready.service <<'EOF_READY'
[Unit]
Description=AARCHVM Debian systemd ready marker
After=systemd-user-sessions.service getty.target serial-getty@ttyAMA0.service
Wants=serial-getty@ttyAMA0.service

[Service]
Type=oneshot
ExecStart=/bin/sh -c 'echo AARCHVM-DEBIAN-SYSTEMD-READY'
RemainAfterExit=yes

[Install]
WantedBy=multi-user.target
EOF_READY
  /bin/busybox ln -snf ../aarchvm-ready.service \
    /newroot/etc/systemd/system/multi-user.target.wants/aarchvm-ready.service
  : > /newroot/etc/.aarchvm-systemd-configured
  log "AARCHVM-DEBIAN-CONFIG DONE"
fi

if [ ! -f /newroot/etc/.aarchvm-root-password-v1 ]; then
  log "AARCHVM-DEBIAN-ROOT-PASSWORD START"
  if ! /bin/busybox chroot /newroot /bin/sh -c 'printf "root:000000\n" | /usr/sbin/chpasswd'; then
    log "AARCHVM-DEBIAN-ROOT-PASSWORD FAILED"
    exec /bin/busybox sh
  fi
  : > /newroot/etc/.aarchvm-root-password-v1
  log "AARCHVM-DEBIAN-ROOT-PASSWORD DONE"
fi

log "AARCHVM-DEBIAN-SWITCH-ROOT"
if ! /bin/busybox chroot /newroot /bin/sh -c 'test -x /sbin/init'; then
  log "AARCHVM-DEBIAN-CONFIG missing /sbin/init"
  exec /bin/busybox sh
fi
exec /sbin/switch_root -c /dev/console /newroot /sbin/init
EOS

chmod 0755 "$ROOTFS_ROOT/init"

(
  cd "$ROOTFS_ROOT"
  find . -print0 | sort -z | cpio --null -o -H newc > "$INITRD_RAW_ABS"
)
gzip -n -f -c "$INITRD_RAW_ABS" > "$INITRD_ABS"

printf 'Debian systemd initramfs ready\nroot: %s\ninitrd: %s\n' "$ROOTFS_ROOT" "$INITRD"
