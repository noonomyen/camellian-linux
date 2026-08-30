#!/usr/bin/env bash
set -e

# Container Environment Check
if [ "${IS_CONTAINER}" != "1" ]; then
    echo "[error] This script is designed to run exclusively inside the Docker container."
    echo "[error] Please execute './scripts/build_kernel.sh' on the host system instead."
    exit 1
fi

WORKSPACE_DIR="/workspace"
KERNEL_SRC="${KERNEL_SRC:-camellian-t-oss}"
KERNEL_DIR="$WORKSPACE_DIR/$KERNEL_SRC"
OUT_DIR="$WORKSPACE_DIR/out"
TOOLCHAIN_DIR="$WORKSPACE_DIR/toolchain"

if [ -d "/opt/clang-r383902/bin" ]; then
    CLANG_BIN="/opt/clang-r383902/bin"
    GCC64_BIN="/opt/aarch64-linux-android-4.9/bin"
    GCC32_BIN="/opt/arm-linux-androideabi-4.9/bin"
elif [ -d "/opt/toolchain/clang-r383902/bin" ]; then
    CLANG_BIN="/opt/toolchain/clang-r383902/bin"
    GCC64_BIN="/opt/toolchain/aarch64-linux-android-4.9/bin"
    GCC32_BIN="/opt/toolchain/arm-linux-androideabi-4.9/bin"
else
    CLANG_BIN="$TOOLCHAIN_DIR/clang-r383902/bin"
    GCC64_BIN="$TOOLCHAIN_DIR/aarch64-linux-android-4.9/bin"
    GCC32_BIN="$TOOLCHAIN_DIR/arm-linux-androideabi-4.9/bin"
fi

export PATH="$CLANG_BIN:$GCC64_BIN:$GCC32_BIN:$PATH"
export ARCH=arm64
export SUBARCH=arm64
export KBUILD_BUILD_USER="${KBUILD_BUILD_USER:-builder}"
export KBUILD_BUILD_HOST="${KBUILD_BUILD_HOST:-camellian-builder}"

DEFCONFIG="${1:-camellian_gl_base_defconfig}"

echo "[kernel-build] Source:    $KERNEL_DIR"
echo "[kernel-build] Defconfig: $DEFCONFIG"
echo "[kernel-build] Out:       $OUT_DIR"
echo "[kernel-build] Toolchain: $CLANG_BIN"

USE_CCACHE="${USE_CCACHE:-1}"
USE_LLVM_CACHE="${USE_LLVM_CACHE:-1}"

if [ "$USE_CCACHE" != "0" ] && [ "$USE_CCACHE" != "false" ] && command -v ccache >/dev/null 2>&1; then
    export CCACHE_DIR="${CCACHE_DIR:-/workspace/.ccache}"
    mkdir -p "$CCACHE_DIR"
    COMPILER="ccache clang"
    echo "[kernel-build] Compiler:   $COMPILER (CCACHE: $CCACHE_DIR)"
else
    COMPILER="clang"
    echo "[kernel-build] Compiler:   $COMPILER (CCACHE: Disabled)"
fi

MAKE_ARGS=(
    -C "$KERNEL_DIR"
    O="$OUT_DIR"
    ARCH=arm64
    SUBARCH=arm64
    CC="$COMPILER"
    CLANG_TRIPLE=aarch64-linux-gnu-
    CROSS_COMPILE=aarch64-linux-androidkernel-
    CROSS_COMPILE_ARM32=arm-linux-androideabi-
    LD=ld.lld
    AR=llvm-ar
    NM=llvm-nm
    OBJCOPY=llvm-objcopy
    OBJDUMP=llvm-objdump
    READELF=llvm-readelf
    STRIP=llvm-strip
    HOSTCFLAGS_extract-cert.o="-DOPENSSL_ENGINE_STUBS"
    HOSTCFLAGS_sign-file.o="-DOPENSSL_ENGINE_STUBS"
)

if [ "$USE_LLVM_CACHE" != "0" ] && [ "$USE_LLVM_CACHE" != "false" ]; then
    export LLVM_CACHE_DIR="${LLVM_CACHE_DIR:-/workspace/.llvm_cache}"
    mkdir -p "$LLVM_CACHE_DIR"
    echo "[kernel-build] LLVM Link:  ThinLTO Cache Enabled ($LLVM_CACHE_DIR)"
fi

mkdir -p "$OUT_DIR"

echo "[kernel-build] Applying configuration..."
make "${MAKE_ARGS[@]}" "$DEFCONFIG" >/dev/null

echo "[kernel-build] Compiling Image.gz..."
make "${MAKE_ARGS[@]}" -j$(nproc) Image.gz

if [ -f "$OUT_DIR/arch/arm64/boot/Image.gz" ]; then
    SIZE=$(stat -c %s "$OUT_DIR/arch/arm64/boot/Image.gz" 2>/dev/null || stat -f %z "$OUT_DIR/arch/arm64/boot/Image.gz")
    echo "[kernel-build] Output: $OUT_DIR/arch/arm64/boot/Image.gz ($SIZE bytes)"
    echo "[kernel-build] Status: Success"
else
    echo "[kernel-build] Status: Failed - Image.gz not generated"
    exit 1
fi
