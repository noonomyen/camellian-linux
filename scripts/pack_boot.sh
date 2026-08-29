#!/usr/bin/env bash
set -e

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
IMAGE_NAME="${MAGISK_IMAGE:-magisk-utils}"

if ! docker image inspect "$IMAGE_NAME" >/dev/null 2>&1; then
    echo "[docker-build] Building image: $IMAGE_NAME"
    docker build -t "$IMAGE_NAME" -f "$REPO_DIR/Dockerfile.magisk" "$REPO_DIR" >/dev/null
fi

docker run --rm \
    -u "$(id -u):$(id -g)" \
    -v "$REPO_DIR:/workspace" \
    "$IMAGE_NAME" \
    /workspace/scripts/container/pack_boot.sh "$@"
