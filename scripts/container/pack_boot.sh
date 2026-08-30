#!/bin/sh
set -e

# Container Environment Check
if [ "${IS_CONTAINER}" != "1" ]; then
    echo "[error] This script is designed to run exclusively inside the Docker container."
    echo "[error] Please execute './scripts/pack_boot.sh' on the host system instead."
    exit 1
fi

MODE="clean"
INPUT_BOOT="/workspace/images/boot.stock.img"
KERNEL_IMAGE="/workspace/out/arch/arm64/boot/Image.gz"
CONFIG_FILE="/workspace/images/magisk-flags.txt"
[ ! -f "$CONFIG_FILE" ] && CONFIG_FILE="/workspace/images/stock-magisk/flags.txt"

while [ $# -gt 0 ]; do
    case "$1" in
        --both)
            MODE="both"
            shift
            ;;
        --magisk)
            MODE="magisk"
            shift
            ;;
        --clean)
            MODE="clean"
            shift
            ;;
        -i|--input)
            INPUT_BOOT="$2"
            shift 2
            ;;
        -k|--kernel)
            KERNEL_IMAGE="$2"
            shift 2
            ;;
        *)
            if [ -z "$POS1" ]; then
                INPUT_BOOT="$1"
                POS1=1
            elif [ -z "$POS2" ]; then
                KERNEL_IMAGE="$1"
                POS2=1
            fi
            shift
            ;;
    esac
done

if [ ! -f "$INPUT_BOOT" ]; then
    echo "[pack-boot] Error: Input boot image not found: $INPUT_BOOT"
    exit 1
fi

if [ ! -f "$KERNEL_IMAGE" ]; then
    echo "[pack-boot] Error: Kernel image not found: $KERNEL_IMAGE"
    exit 1
fi

echo "[pack-boot] Mode:   $MODE"
echo "[pack-boot] Input:  $INPUT_BOOT"
echo "[pack-boot] Kernel: $KERNEL_IMAGE"

if [ "$MODE" = "clean" ] || [ "$MODE" = "both" ]; then
    CLEAN_OUT="/workspace/out/boot.img"
    WORK_DIR=$(mktemp -d)
    cd "$WORK_DIR"
    /bin/magiskboot unpack "$INPUT_BOOT" >/dev/null
    cp "$KERNEL_IMAGE" ./kernel
    mkdir -p "$(dirname "$CLEAN_OUT")"
    /bin/magiskboot repack "$INPUT_BOOT" "$CLEAN_OUT" >/dev/null
    rm -rf "$WORK_DIR"
    echo "[pack-boot] Output (Clean):  $CLEAN_OUT"
fi

if [ "$MODE" = "magisk" ] || [ "$MODE" = "both" ]; then
    MAGISK_OUT="/workspace/out/boot_magisk.img"
    WORK_DIR=$(mktemp -d)
    cd "$WORK_DIR"

    if [ -f "$CONFIG_FILE" ]; then
        while IFS='=' read -r key val || [ -n "$key" ]; do
            case "$key" in
                \#*|"") continue ;;
                KEEPVERITY) KEEPVERITY="$val" ;;
                KEEPFORCEENCRYPT) KEEPFORCEENCRYPT="$val" ;;
                RECOVERYMODE) RECOVERYMODE="$val" ;;
                VENDORBOOT) VENDORBOOT="$val" ;;
                PATCHVBMETAFLAG) PATCHVBMETAFLAG="$val" ;;
                PREINITDEVICE) PREINITDEVICE="$val" ;;
            esac
        done < "$CONFIG_FILE"
    fi

    KEEPVERITY="${KEEPVERITY:-true}"
    KEEPFORCEENCRYPT="${KEEPFORCEENCRYPT:-true}"
    RECOVERYMODE="${RECOVERYMODE:-false}"
    VENDORBOOT="${VENDORBOOT:-false}"
    PATCHVBMETAFLAG="${PATCHVBMETAFLAG:-false}"
    PREINITDEVICE="${PREINITDEVICE:-metadata}"

    export KEEPVERITY KEEPFORCEENCRYPT RECOVERYMODE VENDORBOOT PATCHVBMETAFLAG PREINITDEVICE

    cp "$INPUT_BOOT" ./boot.img
    /bin/magiskboot unpack boot.img >/dev/null
    cp "$KERNEL_IMAGE" ./kernel

    /bin/magiskboot compress=xz /payload/magisk magisk.xz
    /bin/magiskboot compress=xz /payload/stub.apk stub.xz
    /bin/magiskboot compress=xz /payload/init-ld init-ld.xz

    printf "KEEPVERITY=%s\n" "$KEEPVERITY" > config
    printf "KEEPFORCEENCRYPT=%s\n" "$KEEPFORCEENCRYPT" >> config
    printf "RECOVERYMODE=%s\n" "$RECOVERYMODE" >> config
    printf "VENDORBOOT=%s\n" "$VENDORBOOT" >> config
    [ -n "$PREINITDEVICE" ] && printf "PREINITDEVICE=%s\n" "$PREINITDEVICE" >> config
    SHA1=$(/bin/magiskboot sha1 boot.img 2>/dev/null || true)
    [ -n "$SHA1" ] && printf "SHA1=%s\n" "$SHA1" >> config

    cp ramdisk.cpio ramdisk.cpio.orig
    /bin/magiskboot cpio ramdisk.cpio \
        "add 0750 init /payload/magiskinit" \
        "mkdir 0750 overlay.d" \
        "mkdir 0750 overlay.d/sbin" \
        "add 0644 overlay.d/sbin/magisk.xz magisk.xz" \
        "add 0644 overlay.d/sbin/stub.xz stub.xz" \
        "add 0644 overlay.d/sbin/init-ld.xz init-ld.xz" \
        "patch" \
        "backup ramdisk.cpio.orig" \
        "mkdir 000 .backup" \
        "add 000 .backup/.magisk config" >/dev/null

    rm -f ramdisk.cpio.orig config *.xz
    mkdir -p "$(dirname "$MAGISK_OUT")"
    /bin/magiskboot repack boot.img "$MAGISK_OUT" >/dev/null
    rm -rf "$WORK_DIR"
    echo "[pack-boot] Output (Magisk): $MAGISK_OUT"
fi
