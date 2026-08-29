#!/usr/bin/env bash
set -e

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
IMAGE_NAME="${IMAGE_NAME:-android-11-builder}"

if ! docker image inspect "$IMAGE_NAME" >/dev/null 2>&1; then
    echo "[docker-build] Building image: $IMAGE_NAME"
    docker build -t "$IMAGE_NAME" -f "$REPO_DIR/Dockerfile.builder" "$REPO_DIR"
fi

docker run --rm \
    -u "$(id -u):$(id -g)" \
    -e KERNEL_SRC="${KERNEL_SRC:-camellian-t-oss}" \
    -e USE_CCACHE="${USE_CCACHE:-1}" \
    -e USE_LLVM_CACHE="${USE_LLVM_CACHE:-1}" \
    -v "$REPO_DIR:/workspace" \
    "$IMAGE_NAME" \
    /workspace/scripts/container/build_kernel.sh "$@"
