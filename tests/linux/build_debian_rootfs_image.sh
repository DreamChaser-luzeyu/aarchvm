#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT_DIR"

ROOTFS_DIR="${AARCHVM_DEBIAN_ROOTFS_DIR:-out/debian-arm64-bookworm-rootfs}"
IMAGE_PATH="${AARCHVM_DEBIAN_ROOTFS_IMAGE:-out/debian-arm64-bookworm.ext4}"
ROOTFS_SOURCE="${AARCHVM_DEBIAN_ROOTFS_SOURCE:-auto}"
SUITE="${AARCHVM_DEBIAN_SUITE:-bookworm}"
ARCH="${AARCHVM_DEBIAN_ARCH:-arm64}"
MIRROR="${AARCHVM_DEBIAN_MIRROR:-http://deb.debian.org/debian}"
VARIANT="${AARCHVM_DEBIAN_VARIANT:-minbase}"
DOCKER_IMAGE="${AARCHVM_DEBIAN_DOCKER_IMAGE:-debian:bookworm-slim}"
HELPER_IMAGE="${AARCHVM_DEBIAN_HELPER_IMAGE:-debian:bookworm-slim}"
PLATFORM="${AARCHVM_DEBIAN_PLATFORM:-linux/arm64}"
EXTRA_MB="${AARCHVM_DEBIAN_IMAGE_EXTRA_MB:-256}"

HOST_HELPER_PLATFORM="${AARCHVM_DEBIAN_HELPER_PLATFORM:-}"
if [[ -z "$HOST_HELPER_PLATFORM" ]]; then
  case "$(uname -m)" in
    x86_64) HOST_HELPER_PLATFORM="linux/amd64" ;;
    aarch64) HOST_HELPER_PLATFORM="linux/arm64" ;;
    *) HOST_HELPER_PLATFORM="" ;;
  esac
fi

HELPER_ARGS=()
if [[ -n "$HOST_HELPER_PLATFORM" ]]; then
  HELPER_ARGS+=(--platform "$HOST_HELPER_PLATFORM")
fi

mkdir -p out
rm -rf "$ROOTFS_DIR"
mkdir -p "$ROOTFS_DIR"

find_debootstrap() {
  if command -v debootstrap >/dev/null 2>&1; then
    command -v debootstrap
    return 0
  fi
  PATH="$PATH:/sbin" command -v debootstrap >/dev/null 2>&1 || return 1
  PATH="$PATH:/sbin" command -v debootstrap
}

select_rootfs_source() {
  case "$ROOTFS_SOURCE" in
    auto)
      if find_debootstrap >/dev/null 2>&1; then
        printf 'debootstrap\n'
      else
        printf 'docker\n'
      fi
      ;;
    debootstrap|docker)
      printf '%s\n' "$ROOTFS_SOURCE"
      ;;
    *)
      echo "unsupported AARCHVM_DEBIAN_ROOTFS_SOURCE: $ROOTFS_SOURCE" >&2
      exit 1
      ;;
  esac
}

cleanup_container() {
  if [[ -n "${CONTAINER_ID:-}" ]]; then
    docker rm -f "$CONTAINER_ID" >/dev/null 2>&1 || true
  fi
}
trap cleanup_container EXIT

ROOTFS_SOURCE_SELECTED="$(select_rootfs_source)"

if [[ "$ROOTFS_SOURCE_SELECTED" == "debootstrap" ]]; then
  DEBOOTSTRAP_BIN="$(find_debootstrap)"
  echo "building Debian rootfs with debootstrap..."
  DEBOOTSTRAP_CMD=(
    env "PATH=$(dirname "$DEBOOTSTRAP_BIN"):$PATH:/sbin"
    "$DEBOOTSTRAP_BIN"
    --arch="$ARCH"
    --foreign
    --variant="$VARIANT"
    "$SUITE"
    "$ROOTFS_DIR"
    "$MIRROR"
  )
  if [[ "$(id -u)" -ne 0 ]]; then
    if command -v fakeroot >/dev/null 2>&1; then
      DEBOOTSTRAP_CMD=(fakeroot "${DEBOOTSTRAP_CMD[@]}")
    else
      echo "debootstrap requires root or fakeroot" >&2
      exit 1
    fi
  fi
  "${DEBOOTSTRAP_CMD[@]}"
else
  echo "exporting Debian rootfs from docker image..."
  CONTAINER_ID="$(docker create --platform "$PLATFORM" "$DOCKER_IMAGE")"
  docker export "$CONTAINER_ID" | tar \
    --extract \
    --file - \
    --directory "$ROOTFS_DIR" \
    --no-same-owner \
    --exclude='./dev/*' \
    --exclude='./proc/*' \
    --exclude='./sys/*' \
    --exclude='./tmp/*'
  docker rm -f "$CONTAINER_ID" >/dev/null
  CONTAINER_ID=""
fi

USED_KB="$(du -sk "$ROOTFS_DIR" | awk '{print $1}')"
SIZE_KB="$((USED_KB + EXTRA_MB * 1024))"
SIZE_MB="$(((SIZE_KB + 1023) / 1024))"

rm -f "$IMAGE_PATH"

echo "building ext4 image..."
if PATH="$PATH:/sbin" command -v mke2fs >/dev/null 2>&1; then
  truncate -s "${SIZE_MB}M" "$IMAGE_PATH"
  PATH="$PATH:/sbin" mke2fs -q -t ext4 -F -L aarchvm-debian -d "$ROOTFS_DIR" "$IMAGE_PATH" "${SIZE_KB}K"
else
  docker run --rm \
    "${HELPER_ARGS[@]}" \
    -v "$ROOT_DIR:/work" \
    -w /work \
    "$HELPER_IMAGE" \
    bash -lc '
      set -euo pipefail
      export DEBIAN_FRONTEND=noninteractive
      if ! command -v mke2fs >/dev/null 2>&1; then
        apt-get update >/dev/null
        apt-get install -y e2fsprogs >/dev/null
      fi
      truncate -s "'"${SIZE_MB}"'M" "'"${IMAGE_PATH}"'"
      mke2fs -q -t ext4 -F -L aarchvm-debian -d "'"${ROOTFS_DIR}"'" "'"${IMAGE_PATH}"'" "'"${SIZE_KB}"'K"
    '
fi

printf 'Debian arm64 rootfs ready\nrootfs: %s\nimage: %s\nsize: %s MiB\nsource: %s\nsuite: %s\narch: %s\nmirror: %s\n' \
  "$ROOTFS_DIR" "$IMAGE_PATH" "$SIZE_MB" "$ROOTFS_SOURCE_SELECTED" "$SUITE" "$ARCH" "$MIRROR"
if [[ "$ROOTFS_SOURCE_SELECTED" == "docker" ]]; then
  printf 'docker image: %s\nplatform: %s\n' "$DOCKER_IMAGE" "$PLATFORM"
fi
