# mtkclient

Before using `mtkclient`, always verify your device's UFS regions.

On the target device (**`M2103K19PG`**), the default `mtkclient` LUN mapping
incorrectly redirects `boot1` and `boot2` operations to `LUN0` (User Area),
which will immediately overwrite and corrupt the GPT header upon writing.

## Verifying UFS LUN Sizes

Check the detected storage layout using `printgpt`:

```text
DAXFlash - UFS LU0 Size: 0x1dcb000000
DAXFlash - UFS LU1 Size: 0x400000
DAXFlash - UFS LU2 Size: 0x400000
```

From this layout:

- **`LU0`** (~128 GB) is the primary User Area containing the GPT table and all
  system partitions.
- **`LU1`** (4 MB) and **`LU2`** (4 MB) store **Preloader A/B**.

## Applying the Fix Patch

Apply the patch located at
[`patches/0001-fix-ufs-lun-mapping.patch`](patches/0001-fix-ufs-lun-mapping.patch):

```bash
cd mtkclient
git apply ../patches/0001-fix-ufs-lun-mapping.patch
```

## Backup Recommendations

Before writing to any partition, it is strongly recommended to back up:

- **Preloader A/B** (`LU1` and `LU2`)
- **Primary Header and Backup GPT**
- **All partitions except `super` and `userdata`** (these two partitions are
  very large; `super` can be obtained directly from Fastboot/OTA packages, and
  `userdata` is automatically formatted and regenerated on first boot).
