# Build Instructions

## 1. Build Docker Images

```bash
docker build -t android-11-builder -f Dockerfile.builder .
docker build -t magisk-builder -f Dockerfile.magisk .
```

## 2. Compile Kernel

```bash
./scripts/build_kernel.sh camellian_gl_base_defconfig
```

## 3. Repack Boot Image

```bash
# Repack clean boot image
./scripts/pack_boot.sh --clean

# Repack Magisk-patched boot image
./scripts/pack_boot.sh --magisk

# Repack both
./scripts/pack_boot.sh --both
```
