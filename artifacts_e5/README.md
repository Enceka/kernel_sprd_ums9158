# Rongyue E5 (kernel_ts305_ums9158) build artifacts

This directory documents the rebuilt kernel artifacts for the Rongyue E5
(Unisoc UMS9621 / qogirn6lite / product ums9158_1h10_cmcc, Android 13).

## Build inputs

- Base kernel: Google android13-5.15 (5.15.211 GKI, commit 21bbfb609)
- Unisoc platform: ported from motorola-sprd-andriod-14-release-uoa34 (5.15.149),
  cross-checked against ulas-android-14 (5.15.178) and
  android_kernel_zte_ums9620_mifi_u30air (5.4.254 UMS9620 SDK)
- Device evidence: kernel_probe/ (config.gz, kallsyms.txt, btf-vmlinux, fdt.dtb,
  modules.txt, dmesg/pstore)

## Build & repack flow

```sh
./build_e5.sh              # Image + modules + e5-rongyue-overlay.dtbo + ums9621-base.dtb
./make_e5_dtb.sh           # merge base+overlay -> e5-rongyue.dtb
./stage_vendor_modules.sh  # collect .ko -> vendor_modules_e5/
./repack_boot_e5.sh        # swap kernel into boot image -> boot-e5.img
python3 artifacts_e5/vendor_boot/build_vendor_boot_e5.py
                           # repack vendor_boot with our own first-stage modules
```

## Key facts learned from the device images

- boot.img is Android boot header v4: kernel only (arm64 Image with EFI stub,
  MZ/PE magic), ramdisk_size=0 in the stock image; Magisk patched image adds a
  small lz4 ramdisk stub that the bootloader loads.
- boot.img carries NO device tree (no FDT magic anywhere in the 64 MB image);
  the bootloader loads the DT from the device's own dtbo partition. Therefore
  repack_boot_e5.sh swaps ONLY the kernel and keeps the device's dtbo.
- AVB footer on the device image uses SHA256_RSA4096; the Magisk image boots
  with a non-matching digest, so the bootloader does not enforce AVB on this
  device (unlocked). repack re-adds the footer with --algorithm NONE.
- Device live FDT: model="Unisoc UMS9621-base Board", compatible
  "sprd,ums512-base","sprd,ums9621", sc-id "ums9621 1000 1000";
  bootargs select lcd_name=lcd_st7365p_mipi_hdp (480x320).

## vendor_boot repack (first-stage kernel modules)

The rebuilt kernel (5.15.211) cannot load the stock first-stage modules — they
are built for 5.15.119 and every one of them fails with
`disagrees about version of symbol module_layout`. Without them no block device
ever appears and init reboots via `Failed to mount required partitions early`,
so the vendor ramdisk has to be repacked with our own modules.

Two non-obvious traps, both of which produce a silent reboot with no log:

- the AVB footer magic is **`AVBf`** (not `AVB0`) and it lives in the last 64
  bytes of the partition; after changing the ramdisk size the vbmeta blob must
  be moved *and* the footer repointed, otherwise the bootloader reports
  `invalid vbmeta header` / `ERROR_INVALID_METADATA`;
- the stock `modules.load` is a **hand-tuned 83-entry subset** of the 158 .ko
  files (ADI/PMIC/clock layers first, regulators at position 18). An
  alphabetical list puts `ump9620-regulator.ko` first, where
  `dev_get_regmap()` returns NULL and the driver dereferences it — killing
  first-stage init long before pstore is up.

Full layout tables, the stock load order and how to read the bootloader log:
`artifacts_e5/vendor_boot/README.md`.

## Known deltas vs device FDT (source-fidelity, non-blocking for boot)

The merged e5-rongyue.dtb (base + our overlay) matches the live FDT for
~1000 nodes. Remaining differences:
- live lcds (lcd_st7365p_mipi_hdp, lcd_nt36672e_truly_mipi_fhd,
  lcd_st7796s_mipi_hdp) are not in our overlay (overlay includes the moto
  1h10 panels instead);
- E5 touch (sitronix_touch@29 / tlsc6x@2e on i2c@2270000), aw87xxx_pa@58,
  vl53l0@52, sprd keypad and E5 pinctrl groups are in the live FDT but not
  yet in the overlay;
- moto-only nodes (battery2/3, pdbg, faceid/oemcrypto/widevine, ucp1301,
  extra audio dai-links, pa5g/skin thermal zones, virtual_typec, wcn-dump)
  remain in the merged tree (overlay cannot /delete-node/ base labels).
- Full node-level diff tooling: .port_work/cmp_nodes.py

Since the device boots with its own dtbo partition, these deltas do not
affect the kernel-swap boot verification. To use e5-rongyue.dtb on device,
the overlay should first be completed to 100% node parity and flashed to
the dtbo partition with Unisoc's own tooling.