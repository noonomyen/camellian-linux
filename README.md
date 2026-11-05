# camellian-linux

Kernel source and tools for **camellian**, which is the codename for **POCO M3
Pro 5G / Redmi Note 10 5G / Redmi Note 10T** as specified in
`MiCode/Xiaomi_Kernel_OpenSource`.

The target device used for testing is **POCO M3 Pro 5G (`M2103K19PG`)** with an
unlocked bootloader.

## Kernel Source Integration

The kernel source was assembled by taking the `camellian-t-oss` branch from
`MiCode/Xiaomi_Kernel_OpenSource` and merging the missing drivers from
`xiaomi-mt6833-dev/kernel_xiaomi_mt6833`.

The codebase and `camellian_gl_base_defconfig` were modified and patched until
successfully booting on stock ROM
**`camellian_global_images_V14.0.6.0.TKSMIXM_13.0`**. Note that the defconfig
may not be entirely complete or perfect yet.

## Feature Support

| Feature | Status | Notes |
| --- | --- | --- |
| [Magisk][3] | Supported | Repack the boot image with `./scripts/pack_boot.sh --magisk` |
| [KernelSU][6] | Supported (v0.9.5) | `camellian_gl_ksu_defconfig`; built-in with manual hooks |
| [SUSFS][7] | Supported (experimental) | `camellian_gl_ksu_susfs_defconfig` or `camellian_gl_custom_defconfig`; partial hiding only, because the vendor 4.14 tree is unmaintained |
| KVM | Supported | `camellian_gl_custom_defconfig`; needs the patched `tee.img` described in [KERNEL_EL2.md](docs/KERNEL_EL2.md) |

See [BUILD.md](docs/BUILD.md#kernelsu) for the KernelSU and SUSFS options and
the matching manager version.

## Sources & Attributions

- [MiCode/Xiaomi_Kernel_OpenSource][1]: Initial base kernel source
- [xiaomi-mt6833-dev/kernel_xiaomi_mt6833][2]: Missing driver source
- [topjohnwu/Magisk][3]: Root access and boot image utilities
- [bkerler/mtkclient][4]: MediaTek flashing tool (Kamakiri exploit)
- [kasnria001/pwnage24mtk][5]: MediaTek certificate exploit tool
- [tiann/KernelSU][6]: KernelSU v0.9.5, vendored into `drivers/kernelsu`
- [simonpunk/susfs4ksu][7]: SUSFS kernel patches and userspace module

[1]: https://github.com/MiCode/Xiaomi_Kernel_OpenSource/tree/camellian-t-oss
[2]: https://github.com/xiaomi-mt6833-dev/kernel_xiaomi_mt6833/tree/lineage-23.2
[3]: https://github.com/topjohnwu/Magisk/tree/v30.7
[4]: https://github.com/bkerler/mtkclient
[5]: https://github.com/kasnria001/pwnage24mtk
[6]: https://github.com/tiann/KernelSU/tree/v0.9.5
[7]: https://gitlab.com/simonpunk/susfs4ksu/-/tree/1.4.2-kernel-4.14

## Flashing & Tools

For instructions on `mtkclient` UFS LUN mapping, preloader flashing, and
partition backup guidelines, please see [MTKCLIENT.md](docs/MTKCLIENT.md).

## Building

For build prerequisites and step-by-step compilation instructions, please see
[BUILD.md](docs/BUILD.md).

## KVM & EL2 Booting (Hypervisor Support)

For detailed documentation on how we reverse-engineered the MTK bootchain,
patched the ATF (`tee.img`) to bypass GenieZone (GZ), and resolved Clang CFI
kernel panics to successfully boot Linux in EL2 mode with KVM enabled, please
see [KERNEL_EL2.md](docs/KERNEL_EL2.md).
