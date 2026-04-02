#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT_DIR"

PROFILE="${AARCHVM_DEBIAN_PROFILE:-smoke}"
if [[ "$PROFILE" == "systemd" ]]; then
  ROOTFS_DIR_DEFAULT="out/debian-arm64-bookworm-systemd-rootfs"
  IMAGE_PATH_DEFAULT="out/debian-arm64-bookworm-systemd.ext4"
  VARIANT_DEFAULT="minbase"
  EXTRA_MB_DEFAULT="512"
  INCLUDE_DEFAULT="systemd-sysv,dbus,udev"
else
  ROOTFS_DIR_DEFAULT="out/debian-arm64-bookworm-rootfs"
  IMAGE_PATH_DEFAULT="out/debian-arm64-bookworm.ext4"
  VARIANT_DEFAULT="minbase"
  EXTRA_MB_DEFAULT="256"
  INCLUDE_DEFAULT=""
fi

ROOTFS_DIR="${AARCHVM_DEBIAN_ROOTFS_DIR:-$ROOTFS_DIR_DEFAULT}"
IMAGE_PATH="${AARCHVM_DEBIAN_ROOTFS_IMAGE:-$IMAGE_PATH_DEFAULT}"
DEBOOTSTRAP_MODE="${AARCHVM_DEBIAN_DEBOOTSTRAP_MODE:-auto}"
SUITE="${AARCHVM_DEBIAN_SUITE:-bookworm}"
ARCH="${AARCHVM_DEBIAN_ARCH:-arm64}"
DEBOOTSTRAP_CACHE_DIR="${AARCHVM_DEBIAN_CACHE_DIR:-out/debootstrap-cache-$PROFILE-$ARCH}"
MIRROR="${AARCHVM_DEBIAN_MIRROR:-http://deb.debian.org/debian}"
VARIANT="${AARCHVM_DEBIAN_VARIANT:-$VARIANT_DEFAULT}"
INCLUDE_PACKAGES="${AARCHVM_DEBIAN_INCLUDE:-$INCLUDE_DEFAULT}"
EXTRA_MB="${AARCHVM_DEBIAN_IMAGE_EXTRA_MB:-$EXTRA_MB_DEFAULT}"

mkdir -p out
mkdir -p "$(dirname "$ROOTFS_DIR")" "$(dirname "$IMAGE_PATH")" "$(dirname "$DEBOOTSTRAP_CACHE_DIR")"
ROOTFS_DIR_ABS="$(cd "$(dirname "$ROOTFS_DIR")" && pwd)/$(basename "$ROOTFS_DIR")"
IMAGE_PATH_ABS="$(cd "$(dirname "$IMAGE_PATH")" && pwd)/$(basename "$IMAGE_PATH")"
DEBOOTSTRAP_CACHE_DIR_ABS="$(cd "$(dirname "$DEBOOTSTRAP_CACHE_DIR")" && pwd)/$(basename "$DEBOOTSTRAP_CACHE_DIR")"
mkdir -p "$DEBOOTSTRAP_CACHE_DIR_ABS"
rm -rf "$ROOTFS_DIR_ABS"
mkdir -p "$ROOTFS_DIR_ABS"

find_debootstrap() {
  if command -v debootstrap >/dev/null 2>&1; then
    command -v debootstrap
    return 0
  fi
  PATH="$PATH:/sbin" command -v debootstrap >/dev/null 2>&1 || return 1
  PATH="$PATH:/sbin" command -v debootstrap
}

select_debootstrap_mode() {
  case "$DEBOOTSTRAP_MODE" in
    auto)
      case "$(uname -m):$ARCH" in
        aarch64:arm64|arm64:arm64)
          printf 'native\n'
          ;;
        *)
          if [[ -e /proc/sys/fs/binfmt_misc/qemu-aarch64 ]]; then
            printf 'native\n'
          else
            echo "native arm64 debootstrap requires either an arm64 host or qemu-aarch64 binfmt support; foreign/second-stage mode is no longer used" >&2
            exit 1
          fi
          ;;
      esac
      ;;
    native)
      printf 'native\n'
      ;;
    *)
      echo "unsupported AARCHVM_DEBIAN_DEBOOTSTRAP_MODE: $DEBOOTSTRAP_MODE (only 'native' or 'auto' are supported)" >&2
      exit 1
      ;;
  esac
}

restore_output_owner() {
  if [[ -n "${SUDO_UID:-}" && -n "${SUDO_GID:-}" ]]; then
    chown -R "$SUDO_UID:$SUDO_GID" "$ROOTFS_DIR_ABS" "$DEBOOTSTRAP_CACHE_DIR_ABS" 2>/dev/null || true
    if [[ -e "$IMAGE_PATH_ABS" ]]; then
      chown "$SUDO_UID:$SUDO_GID" "$IMAGE_PATH_ABS" 2>/dev/null || true
    fi
  fi
}

if ! find_debootstrap >/dev/null 2>&1; then
  echo "missing host debootstrap; install it on the host and rerun this script" >&2
  exit 1
fi

DEBOOTSTRAP_BIN="$(find_debootstrap)"
DEBOOTSTRAP_MODE_SELECTED="$(select_debootstrap_mode)"
echo "building Debian rootfs with host-native debootstrap..."
DEBOOTSTRAP_CMD=(
  env "PATH=$(dirname "$DEBOOTSTRAP_BIN"):$PATH:/sbin"
  "$DEBOOTSTRAP_BIN"
  --arch="$ARCH"
  --variant="$VARIANT"
  --cache-dir="$DEBOOTSTRAP_CACHE_DIR_ABS"
)
if [[ -n "$INCLUDE_PACKAGES" ]]; then
  DEBOOTSTRAP_CMD+=("--include=$INCLUDE_PACKAGES")
fi
DEBOOTSTRAP_CMD+=(
  "$SUITE"
  "$ROOTFS_DIR_ABS"
  "$MIRROR"
)
if [[ "$(id -u)" -ne 0 ]]; then
  echo "native debootstrap requires running directly on the host with root privileges; rerun this script outside the sandbox via sudo or as root" >&2
  exit 1
fi
"${DEBOOTSTRAP_CMD[@]}"
mkdir -p "$ROOTFS_DIR_ABS/etc"
: > "$ROOTFS_DIR_ABS/etc/.aarchvm-debootstrap-second-stage-done"

USED_KB="$(du -sk "$ROOTFS_DIR_ABS" | awk '{print $1}')"
SIZE_KB="$((USED_KB + EXTRA_MB * 1024))"
SIZE_MB="$(((SIZE_KB + 1023) / 1024))"

rm -f "$IMAGE_PATH_ABS"

echo "building ext4 image..."
if ! PATH="$PATH:/sbin" command -v mke2fs >/dev/null 2>&1; then
  echo "missing host mke2fs; install e2fsprogs on the host and rerun this script" >&2
  exit 1
fi
truncate -s "${SIZE_MB}M" "$IMAGE_PATH_ABS"
PATH="$PATH:/sbin" mke2fs -q -t ext4 -F -L aarchvm-debian -d "$ROOTFS_DIR_ABS" "$IMAGE_PATH_ABS" "${SIZE_KB}K"
restore_output_owner

printf 'Debian arm64 rootfs ready\nrootfs: %s\nimage: %s\nsize: %s MiB\nsuite: %s\narch: %s\nmirror: %s\n' \
  "$ROOTFS_DIR_ABS" "$IMAGE_PATH_ABS" "$SIZE_MB" "$SUITE" "$ARCH" "$MIRROR"
printf 'profile: %s\nvariant: %s\n' "$PROFILE" "$VARIANT"
printf 'debootstrap mode: %s\n' "$DEBOOTSTRAP_MODE_SELECTED"
if [[ -n "$INCLUDE_PACKAGES" ]]; then
  printf 'include: %s\n' "$INCLUDE_PACKAGES"
fi
printf 'cache dir: %s\n' "$DEBOOTSTRAP_CACHE_DIR_ABS"
