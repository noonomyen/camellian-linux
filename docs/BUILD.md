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

`camellian_gl_custom_defconfig` also enables KernelSU, together with the KVM,
virtio and SUSFS options.

Install the matching manager (`KernelSU_v0.9.5_11872-release.apk`). Safe mode
is available: press Volume Down three times while booting.

Known limitation: "umount modules" does not work on this device yet.
`try_umount()` runs in the shared mount namespace, so detaching `/system`,
`/vendor` and `/product` fails with `EBUSY` and apps keep seeing the module
overlays.

## SUSFS

SUSFS is a KernelSU add-on that hides root traces. The kernel side is applied
directly to the tree and is guarded by `CONFIG_KSU_SUSFS*`, so building a
KernelSU config without those options produces an image without any SUSFS
code. The userspace tool and module come from the `susfs4ksu` submodule
(`1.4.2-kernel-4.14`).

Defconfigs that enable it:

- `camellian_gl_ksu_susfs_defconfig` - KernelSU + SUSFS for the stock ROM
- `camellian_gl_custom_defconfig` - KernelSU + SUSFS + KVM/virtio

```bash
./scripts/build_kernel.sh camellian_gl_ksu_susfs_defconfig
./scripts/pack_boot.sh --clean
```

Options:

| Option | Value | Reason |
| --- | --- | --- |
| `CONFIG_KSU_SUSFS` | `y` | builds `fs/susfs.c` and the filesystem hooks |
| `CONFIG_KSU_SUSFS_SUS_PATH` | `y` | hides paths from processes with uid > 2000 |
| `CONFIG_KSU_SUSFS_SUS_MOUNT` | `y` | hides mount entries from `/proc/*/mounts` |
| `CONFIG_KSU_SUSFS_SUS_KSTAT` | `y` | spoofs stat data per inode |
| `CONFIG_KSU_SUSFS_SUS_OVERLAYFS` | `y` | spoofs statfs for overlayed files |
| `CONFIG_KSU_SUSFS_SPOOF_UNAME` | `y` | reports a configurable `uname` |
| `CONFIG_KSU_SUSFS_ENABLE_LOG` | `y` | kernel logging for the tool |
| `CONFIG_KSU_SUSFS_TRY_UMOUNT` | `n` | depends on the broken KernelSU umount path |
| `CONFIG_KSU_SUSFS_SUS_SU` | `n` | requires kprobes, which this tree disables |

The settings live in kernel memory and are lost on reboot, so they are
re-applied by the module every boot. The build script writes the
configuration block into both `post-fs-data.sh` and `service.sh`, and spoofs
the boot properties through `resetprop` when the binary is available:

```bash
./scripts/build_susfs_module.sh
adb push out/ksu_module_susfs.zip /data/local/tmp/
adb shell su -c '/data/adb/ksud module install /data/local/tmp/ksu_module_susfs.zip'
```

Defaults can be overridden with environment variables:
`SUSFS_UNAME_RELEASE`, `SUSFS_UNAME_VERSION`, `SUSFS_SPOOF_PROPS`,
`SUSFS_SUS_MOUNTS`, `SUSFS_SUS_PATHS`, `SUSFS_ENABLE_LOG` and
`SUSFS_EXTRA_SCRIPT`.

The userspace tool can also be used directly:
`/data/adb/ksu/bin/ksu_susfs {add_sus_path,add_sus_mount,set_uname,enable_log}`.
The 1.4.2 tool has no command to remove entries; they are cleared by a reboot.

### Limitations on this kernel

KernelSU and SUSFS cannot be fully used on this device. The vendor 4.14
tree has been unmaintained for years and lacks the infrastructure that
newer releases assume:

- `SUS_SU` needs kprobes and is not supported on non-GKI kernels.
- `TRY_UMOUNT` depends on KernelSU's umount path, which cannot detach
  `/system`, `/vendor` or `/product` here because the mount namespace is
  shared (it fails with `EBUSY`).
- Newer hiding features (`OPEN_REDIRECT`, `SUS_MAP`, cmdline/bootconfig
  spoofing, kallsyms symbol hiding and `AUTO_ADD_*`) only exist on the
  unreleased `kernel-4.14` branch and may not apply to this tree.
- The KernelSU 0.9.5 manager package name is still visible to apps.
- Boot properties are overridden in `post-fs-data`/`service`. Values that the
  framework writes later, such as `sys.oem_unlock_allowed`, revert and need a
  boot-completed stage to be applied again.

Treat SUSFS here as partial anti-detection, not a complete hiding
solution.


