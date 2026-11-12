# E5 vendor_boot repacking

Replace a stock vendor ramdisk's modules with those from a matching kernel build.
For LineageOS, use the device tree's normal packaging flow.

Run from the kernel source root:

```sh
E5_STOCK_VENDOR_BOOT=/path/to/vendor_boot.img \
E5_BUILD_DIR=/path/to/kernel-output \
E5_OUT_VENDOR_BOOT=/path/to/vendor_boot-repacked.img \
    python3 artifacts_e5/vendor_boot/build_vendor_boot_e5.py
```

The script selects modules from `modules.order` and preserves the dependency
order in `modules.load.stock`, skipping modules built into the kernel. It also
includes the separately built Mali module when present. Use the same kernel
revision and configuration for all modules and the Image.

The script preserves stock DTB, bootconfig and AVB metadata, relocating vbmeta
and its footer to accommodate the new ramdisk. The payload change invalidates
the original signature; use only with an unlocked bootloader.
