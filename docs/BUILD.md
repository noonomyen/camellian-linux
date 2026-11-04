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

## KernelSU

`camellian_gl_ksu_defconfig` is `camellian_gl_base_defconfig` plus KernelSU
support. Build and pack it like any other config, but use a clean boot image
because KernelSU does not touch the ramdisk:

```bash
./scripts/build_kernel.sh camellian_gl_ksu_defconfig
./scripts/pack_boot.sh --clean
```

The driver is built from the sources vendored in
`camellian-t-oss/drivers/kernelsu/` (official v0.9.5, the last release that
supports non-GKI kernels). The `KernelSU` submodule at the repository root
pins that tag and `KSU_VERSION` is baked into the vendored Makefile, so
builds do not depend on the submodule being checked out.

Options that matter:

| Option | Value | Reason |
| --- | --- | --- |
| `CONFIG_KSU` | `y` | builds the driver and the manual hooks (built-in only) |
| `CONFIG_KPROBES` | `n` | selects the manual hook path; kprobes would register a second set of hooks |
| `CONFIG_KALLSYMS_ALL` | `y` | KernelSU resolves local `*.cfi_jt` symbols during boot |
| `CONFIG_OVERLAY_FS` | `y` | required by the KernelSU Kconfig and used to mount modules |
| `CONFIG_SECURITY_SELINUX` | `y` | KernelSU injects `su`/`ksud` policy rules |
| `CONFIG_KSU_DEBUG` | `n` | grants uid 2000 root before the manager allowlist is applied |

`camellian_gl_custom_defconfig` also enables KernelSU, together with the KVM
and virtio options.

Install the matching manager (`KernelSU_v0.9.5_11872-release.apk`). Safe mode
is available: press Volume Down three times while booting.

Known limitation: "umount modules" does not work on this device yet.
`try_umount()` runs in the shared mount namespace, so detaching `/system`,
`/vendor` and `/product` fails with `EBUSY` and apps keep seeing the module
overlays.

