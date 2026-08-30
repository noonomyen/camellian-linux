# Build Instructions

## Build Docker Images

```bash
docker build -t android-11-builder -f Dockerfile.builder .
docker build -t magisk-builder -f Dockerfile.magisk .
```

## Compile Kernel

```bash
./scripts/build_kernel.sh camellian_gl_base_defconfig
```

## Repack Boot Image

```bash
# Repack clean boot image
./scripts/pack_boot.sh --clean

# Repack Magisk-patched boot image
./scripts/pack_boot.sh --magisk

# Repack both
./scripts/pack_boot.sh --both
```
