# camellian-linux

Kernel source and tools for **camellian**, which is the codename for **POCO M3 Pro 5G / Redmi Note 10 5G / Redmi Note 10T** as specified in `MiCode/Xiaomi_Kernel_OpenSource`.

The target device used for testing is **POCO M3 Pro 5G (`M2103K19PG`)** with an unlocked bootloader.

---

## Kernel Source Integration

The kernel source was assembled by taking the `camellian-t-oss` branch from `MiCode/Xiaomi_Kernel_OpenSource` and merging the missing drivers from `xiaomi-mt6833-dev/kernel_xiaomi_mt6833`.

The codebase and `camellian_gl_base_defconfig` were modified and patched until successfully booting on stock ROM **`camellian_global_images_V14.0.6.0.TKSMIXM_13.0`**. Note that the defconfig may not be entirely complete or perfect yet.

---

## Sources & Attributions

| Source | Description |
| :--- | :--- |
| [MiCode/Xiaomi_Kernel_OpenSource](https://github.com/MiCode/Xiaomi_Kernel_OpenSource/tree/camellian-t-oss) | Initial base kernel source |
| [xiaomi-mt6833-dev/kernel_xiaomi_mt6833](https://github.com/xiaomi-mt6833-dev/kernel_xiaomi_mt6833/tree/lineage-23.2) | Missing driver source |
| [topjohnwu/Magisk](https://github.com/topjohnwu/Magisk/tree/v30.7) | Root access and boot image utilities |
| [bkerler/mtkclient](https://github.com/bkerler/mtkclient) | MediaTek flashing tool utilizing Kamakiri exploit |
| [kasnria001/pwnage24mtk](https://github.com/kasnria001/pwnage24mtk) | MediaTek certificate exploit tool |

---

## Building

For build prerequisites and step-by-step compilation instructions, please see [BUILD.md](BUILD.md).
