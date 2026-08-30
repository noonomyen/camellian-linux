# Booting the Kernel in EL2 Mode for KVM Support

KVM on ARM architecture requires the Linux kernel to run in Exception Level 2
(EL2). However, on Android devices powered by MediaTek SoCs, a proprietary
hypervisor called **GenieZone (GZ)** typically takes over EL2. It forces the
remaining boot chain-starting from LittleKernel (LK)-to execute at EL1.
Consequently, the kernel cannot enable KVM, even if virtualization support is
explicitly enabled in the kernel configuration.

To solve this, we first need to understand the device's boot process.

## Boot Process Architecture

```text
[ BROM (BL1) ]
      │
      ▼
[ Preloader (BL2 / LU1, LU2) ]
      │
      ▼
[ tee.img (GPT Partition: tee) ]
  ├── BL31 (ATF - ARM Trusted Firmware)
  ├── BL32 (TEE - Secure World)
  └── BL33 (Non-Secure Handoff)
      │
      ├─────────────────────────────┐
      │                             │
 (Stock Flow)                 (Patched Flow)
      ▼                             ▼
[ GenieZone (GZ) - EL2 ]      [ LittleKernel (LK) - EL2 ]
      │                             │
      ▼                             ▼
[ LittleKernel (LK) - EL1 ]   [ Linux Kernel - EL2 (KVM Enabled!) ]
      │
      ▼
[ Linux Kernel - EL1 ]
```

## Diagnostic Symptoms

When modifying the bootchain or kernel, you can diagnose failures based on
device behavior:

- **Boot hangs for 20 seconds, then reboots:** The system triggered a Watchdog
  Timer reset.
- **Stuck on boot logo (No system boot):** Processes up to LK succeeded (since
  `logo.img` is rendered by LK). The error (Kernel Panic) occurred shortly after
  LK handed off execution to the kernel. _(Tip: Extract the panic log via BROM
  using `mtk r expdb`)._
- **No display (Black screen):** Boot process error occurred _before_ reaching
  LK.

## Bypassing GenieZone (GZ)

In the stock boot process, GZ acts as a restrictive hypervisor layer. Through
reverse engineering, we discovered that **BL31 (ATF)** contains a switch to
conditionally load GZ. Furthermore, LK natively supports EL2 execution and can
successfully hand off EL2 privileges to the kernel.

Therefore, our strategy is:

1. **Patch the GZ check (`check_gz_enabled`)** to unconditionally return 0
   (False).
2. **Patch the BL33 Entrypoint** to jump directly to LK instead of GZ.

### Patching and ASM Diff

Since `tee.img` on MediaTek is simply a raw binary (containing the ATF code
starting at offset `0x0`) with certificates appended at the end, we can modify
it directly in-place using a hex editor or Python script without any complex
unpacking/repacking steps.

We modified two critical opcodes:

**1. Bypass GZ Check (Offset: `0x45D8`)**

```diff
  0x45D0: a8c37bfd   ldp   x29, x30, [sp], #48
  0x45D4: d65f03c0   ret
- 0x45D8: 90000180   adrp  x0, 0x34000
- 0x45DC: b9409000   ldr   w0, [x0, #144]
+ 0x45D8: 52800000   mov   w0, #0x0         // Force return 0
+ 0x45DC: d65f03c0   ret
  0x45E0: 12000000   and   w0, w0, #0x1
  0x45E4: 52000000   eor   w0, w0, #0x1
```

**2. Redirect BL33 Handoff to LK (Offset: `0xBF2C`)**

```diff
  0xBF24: 911b6013   add   x19, x0, #0x6d8
  0xBF28: 391b6016   strb  w22, [x0, #1752]
- 0xBF2C: f9400aa0   ldr   x0, [x21, #16]
+ 0xBF2C: d2a90400   mov   x0, #0x48200000  // LK load address (this device)
  0xBF30: 39000676   strb  w22, [x19, #1]
  0xBF34: 79000677   strh  w23, [x19, #2]
```

### Signing and Verifying

Because we modified the signed `tee.img`, the Preloader (BL2) will reject it. To
bypass this, we use the `pwnage24mtk` certificate exploit tool to recalculate
the hashes, re-sign the image, and verify the CERT1/CERT2 chain before flashing.

## Kernel Source Issue (Clang CFI)

After successfully bypassing GZ and allowing the kernel to boot in EL2, a
**Clang CFI** issue causes KVM initialization to fail immediately (HYP Panic).

This is caused by `CONFIG_CFI_CLANG`. CFI is a compile-time security feature
that relocates function addresses into a Jump Table (`.cfi_jt`) residing in the
`.text` section.

However, the KVM EL2 MMU is strictly configured to map only the `.hyp.text`
section; it does not map `.text`. When EL2 code attempts to call a hypervisor
function, the indirect call resolves to `.cfi_jt`, which is not mapped in the
EL2 page tables. This triggers an **Instruction Abort (Translation Fault
Level 3)**.

**Solution:** We backported the official AOSP approach (commit `2f4d6c9`). We
introduced the `__va_function` macro, which utilizes inline assembly and
stringification to resolve the true function address. This completely bypasses
Clang's CFI instrumentation on that symbol, allowing KVM to locate the function
in `.hyp.text` and initialize successfully.

## Required Kernel Configs

- `CONFIG_KVM=y` (and related Virtualization configs)
- Disable GZ in the kernel (`# CONFIG_MTK_ENABLE_GENIEZONE is not set` or remove
  it entirely)

## Conclusion

Once successfully booted with these patches and configs, KVM will be fully
operational and `/dev/kvm` will appear in your system.
