#!/usr/bin/env bash
set -e
set -f

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SRC_DIR="$REPO_DIR/susfs4ksu/ksu_module_susfs"
OUT_DIR="$REPO_DIR/out"
OUT_ZIP="$OUT_DIR/ksu_module_susfs.zip"

SUSFS_ENABLE_LOG="${SUSFS_ENABLE_LOG:-0}"
SUSFS_SPOOF_PROPS="${SUSFS_SPOOF_PROPS:-1}"

# Stock strings of the installed ROM. Set SUSFS_UNAME_RELEASE="" to skip the
# uname spoof. SUSFS_UNAME_VERSION keeps uname -v consistent with stock.
SUSFS_UNAME_RELEASE="${SUSFS_UNAME_RELEASE-4.14.186-perf-gf8d872e9279c}"
SUSFS_UNAME_VERSION="${SUSFS_UNAME_VERSION-#1 SMP PREEMPT Tue Sep 19 12:46:50 UTC 2023}"

SUSFS_SUS_MOUNTS="${SUSFS_SUS_MOUNTS:-/data/adb/modules /debug_ramdisk /system}"
SUSFS_SUS_PATHS="${SUSFS_SUS_PATHS:-/data/adb/modules /data/adb/ksu /data/adb/ksud /data/adb/post-fs-data.d /data/adb/service.d /debug_ramdisk /system/xbin}"
SUSFS_EXTRA_SCRIPT="${SUSFS_EXTRA_SCRIPT:-}"

SUSFS_PROPS=(
    "ro.boot.verifiedbootstate=green"
    "ro.boot.flash.locked=1"
    "ro.boot.vbmeta.device_state=locked"
    "ro.secureboot.lockstate=locked"
    "sys.oem_unlock_allowed=0"
    "ro.boot.warranty_bit=0"
)

if [ ! -d "$SRC_DIR" ]; then
    echo "[susfs-module] missing $SRC_DIR" >&2
    echo "[susfs-module] run: git submodule update --init susfs4ksu" >&2
    exit 1
fi

WORK_DIR="$(mktemp -d)"
trap 'rm -rf "$WORK_DIR"' EXIT

cp -a "$SRC_DIR/." "$WORK_DIR/"

BLOCK="$WORK_DIR/.susfs-config.sh"
{
    echo "# Camellian SUSFS configuration"
    echo 'if [ -x "$SUSFS_BIN" ]; then'

    if [ "$SUSFS_SPOOF_PROPS" = "1" ]; then
        echo '    RESETPROP=/data/adb/ksu/bin/resetprop'
        echo '    if [ -x "$RESETPROP" ]; then'
        for kv in "${SUSFS_PROPS[@]}"; do
            echo "        \"\$RESETPROP\" ${kv%%=*} ${kv#*=}"
        done
        echo '    fi'
    fi

    echo "    \$SUSFS_BIN enable_log $SUSFS_ENABLE_LOG"

    if [ -n "$SUSFS_UNAME_RELEASE" ]; then
        echo "    \$SUSFS_BIN set_uname \"$SUSFS_UNAME_RELEASE\" \"$SUSFS_UNAME_VERSION\""
    fi

    for p in $SUSFS_SUS_MOUNTS; do
        echo "    \$SUSFS_BIN add_sus_mount \"$p\""
    done
    for p in $SUSFS_SUS_PATHS; do
        echo "    \$SUSFS_BIN add_sus_path \"$p\""
    done

    if [ -n "$SUSFS_EXTRA_SCRIPT" ]; then
        sed 's/^/    /' "$SUSFS_EXTRA_SCRIPT"
    fi

    echo 'fi'
} > "$BLOCK"

inject() {
    local file="$1"
    sed -i "/^source \${MODDIR}\/utils.sh$/r $BLOCK" "$file"
    if ! grep -q '^# Camellian SUSFS configuration$' "$file"; then
        echo "[susfs-module] failed to inject into $(basename "$file")" >&2
        exit 1
    fi
}

inject "$WORK_DIR/post-fs-data.sh"
inject "$WORK_DIR/service.sh"
rm -f "$BLOCK"

mkdir -p "$OUT_DIR"
rm -f "$OUT_ZIP"
(cd "$WORK_DIR" && zip -qr9 "$OUT_ZIP" .)

echo "[susfs-module] output: ${OUT_ZIP#$REPO_DIR/}"
