# Rongyue E5 (kernel_ts305_ums9158) build artifacts

This directory documents the rebuilt kernel artifacts for the Rongyue E5
(Unisoc UMS9621 / qogirn6lite / product ums9158_1h10_cmcc, Android 13).

**2026-09-09**: device-side forensics found zero evidence the rebuilt kernel
completed a boot that day (all archived ylog sessions were stock 5.15.119),
plus a new UART-free log-capture path and two capacity risks (vendor_boot AVB
footer headroom, boot.img kernel-region headroom). See
`diagnostics_2026-09-09.md` before trusting the "fixed" framing in the
vendor_boot section below - it documents attempts, not a confirmed fix.

**2026-09-12**: systematic driver-gap pass against the newly cloned vendor
trees - 130 stock modules cross-checked against what this tree can build and
against what actually binds hardware on the device; 24 modules imported (audio
stack + VPU).  See `driver_gap_2026-09-12.md`, which also records what is
confirmed to have no source anywhere (aw322xx_charger, the camera group) and
why sprd-jpg cannot be imported.

## Build inputs

- Base kernel: Google android13-5.15 (5.15.211 GKI, commit 21bbfb609)
- Unisoc platform: ported from motorola-sprd-andriod-14-release-uoa34 (5.15.149),
  cross-checked against ulas-android-14 (5.15.178) and
  android_kernel_zte_ums9620_mifi_u30air (5.4.254 UMS9620 SDK)
- Device evidence: kernel_probe/ (config.gz, kallsyms.txt, btf-vmlinux, fdt.dtb,
  modules.txt, dmesg/pstore).  Re-dumped 2026-09-12 together with stock-img/;
  see stock_img_2026-09-12.md for the measurements and for the checksum files.

## Build & repack flow

```sh
./build_e5.sh              # Image + modules + e5-rongyue-overlay.dtbo + ums9621-base.dtb
./make_e5_dtb.sh           # merge base+overlay -> e5-rongyue.dtb
./stage_vendor_modules.sh  # collect .ko -> vendor_modules_e5/  (.gitignore'd)
BASE=stock-img/boot_a.img ./repack_boot_e5.sh
                           # swap kernel into boot image -> boot-e5.img
                           # (the script's default BASE path does not exist here)
python3 artifacts_e5/vendor_boot/build_vendor_boot_e5.py
                           # repack vendor_boot with our own first-stage modules
./repack_vendor_dlkm.sh    # rebuild the EROFS vendor_dlkm
                           # -> stock-img/vendor_dlkm_e5.img
```

## Rebuild & repack, 2026-09-12 20:38

First rebuild after the USB and touchscreen commits; all three images were
regenerated from the same tree.

Built from `34a5b11f6` (usb: musb host-mode NULL deref + usb31pllv always on)
and `a293f173b` (touchscreen: tlsc6x moved to drivers/input/touchscreen, vendor
framework dropped), with the `e05c37add` modules.load check in the repack path.
Kernel release `5.15.211-ge05c37adde49` (clean tree, no -dirty).

`make Image modules dtbs` finished with no errors: 155 .ko, `modules-only.symvers`
produced normally. (A single-target `make O=out_e5 <path>.o` deletes that file and
the next modpost run then reports bogus unresolved symbols for wcn_bsp - that is
build-state damage, not a real symbol problem.)

| artifact | size | sha256 |
|---|---|---|
| `out_e5/arch/arm64/boot/Image` | 34,740,736 | `16ec5bda5a99b4d89229ab09414419d353648afa92d266d141c1e14c2bb379a7` |
| `boot-e5.img` | 67,108,864 | `5c7e458cea71d26437807af1e85a39dcc55c02c29ae4fe21d0659930d9fccfd6` |
| `stock-img/vendor_boot_e5.img` | 104,857,600 | `8668bf8f59e5b0a624b73b2fd807b47f1b364efa5d9490180ad651c105b58d0b` |
| `stock-img/vendor_dlkm_e5.img` | 65,458,176 | `c8bc87ff73a2adc7d42d1a1965e8ac2662fed47231193189722c42ef6088dbe4` |
| `e5-rongyue.dtb` | 202,549 | `6ed5c7745bccb3ececb45d714dfdaaafde0a7eed8742425abd4e35e6d9d3d2fb` |

- **boot**: repacked on `stock-img/boot_a.img` (the stock-kernel + Magisk-ramdisk
  container). The new Image is 0x2121a00 bytes inside a 0x2c97000-byte region,
  i.e. 0xb75600 still free - the headroom check that has bitten us before is fine.
- **vendor_boot**: module payload replaced. 42 modules in the first-stage load
  list (stock asks for 83); 41 stock-only entries are now `=y` in our kernel.
  The ramdisk shrank, so vbmeta was moved and the AVBf footer repointed
  (vbmeta 0x4d18000 -> 0x4c42000).
- **vendor_dlkm**: 155 modules, `modules.load` lists 155 entries and covers every
  shipped .ko (the check added in `e05c37add`), SELinux labels
  `u:object_r:vendor_file:s0` verified on the mounted image, 65,458,176 bytes
  against the stock 111,296,512 - flashable as is.

Nothing above is tracked in git (`boot-e5.img`, `*.dtb` and `stock-img/` are
ignored/untracked); what is committed is the source, the scripts and this record.

Flash: `boot` <- `boot-e5.img`, `vendor_boot` <- `stock-img/vendor_boot_e5.img`,
`vendor_dlkm` <- `stock-img/vendor_dlkm_e5.img` (fastbootd - it is a logical
partition inside super, there is no `/dev/block/by-name/vendor_dlkm`).

### Re-cut 21:29, with the wifi and adb fixes

The device blackbox logs (see below) showed two places where our build did not
match stock; both are fixed in `0cedbf784` (wifi board config) and `fd321d33b`
(musb dr_mode), and all three images were rebuilt from `4a9bc1bdb`, clean tree,
kernel release `5.15.211-g4a9bc1bdb151`.

| artifact | size | sha256 |
|---|---|---|
| `out_e5/arch/arm64/boot/Image` | 34,740,736 | `a7ace88e4c7dd693023b512ce27a5eedb5e52509f7c96d0da9fe96ab4106ce88` |
| `boot-e5.img` | 67,108,864 | `233da1c8241c200dbfc588b9a25aa5f81e60fc2041988ece4c2788ab45b4b7f5` |
| `stock-img/vendor_boot_e5.img` | 104,857,600 | `dc0397328d17a9598079413b8ef01d80f61b3eb5f5c9910f002464d7ed7f317b` |
| `stock-img/vendor_dlkm_e5.img` | 65,454,080 | `188af55157bb3801b3e43ffa59cfd2482e380121b067e9ae39ed4c9b3f52350d` |
| `e5-rongyue.dtb` | 202,549 | unchanged |

**Never build the Image and the modules from different tree states.** UTS_RELEASE
carries both the `git describe` hash and a `-dirty` suffix when tracked files are
modified, and the kernel refuses any module whose vermagic differs. A
module-only rebuild on a dirty tree produces `...-dirty` modules against a clean
Image, and nothing loads; the same trap appears if HEAD moves between building
the Image and building the modules. Commit first, then build `Image modules`
together, and check:

```sh
cat out_e5/include/generated/utsrelease.h     # 5.15.211-g<hash>, no -dirty
modinfo -F vermagic out_e5/drivers/usb/musb/musb_sprd.ko   # must match it
```

**Identifying ylog sessions.** Every `log-img/blackbox/ylog/<n>/log_<n>.tar.gz`
holds the kernel release in the first lines of its `kernel.log`, which is the
quickest way to tell whether a session ran our kernel or fell back to stock:

```sh
cd log-img/blackbox/ylog
for f in */log_*.tar.gz; do s=${f%%/*}; printf '%-4s %s\n' "$s" \
  "$(tar xzOf "$f" kernel.log 2>/dev/null | grep -m1 -oE 'Linux version [^ ]+')"; done
```

As of 2026-09-12: 43 and 46 are ours (the latter `ge05c37adde49`), 44 and 47 are
stock - so 47, the newest, is a stock boot, not a failed attempt of ours.

## Key facts learned from the device images

- boot.img is Android boot header v4: kernel only (arm64 Image with EFI stub,
  MZ/PE magic).  Measured on the 2026-09-12 dump: boot_a/b carry kernel_size
  0x2C96A00 and a 360,840-byte LZ4 ramdisk holding Magisk markers
  (overlay.d/.backup) - i.e. what is flashed is a Magisk-patched boot whose
  kernel is still the stock 5.15.119, *not* a ramdisk_size=0 image (see
  stock_img_2026-09-12.md).
- boot.img carries NO device tree (no FDT magic in the 64 MB image); the
  bootloader loads the DT from the device's own dtbo partition - dtb_a/b are an
  all-zero 8 MB partition, so dtbo is the only candidate (vendor_boot does
  carry one FDT). Therefore repack_boot_e5.sh swaps ONLY the kernel and keeps
  the device's dtbo.
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