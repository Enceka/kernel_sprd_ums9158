# UNISOC UMS9621 / Qogirn6L kernel

Android 13 Linux 5.15 kernel with UNISOC platform drivers and RongYue E5 support.
The LineageOS device tree builds the kernel and matching modules automatically.

## Standalone build

With Clang and the kernel build tools on `PATH`, run from this directory:

```sh
./build_sprd.sh
```

The default output directory is `out_sprd/`; set `O` to override it.
Configuration: `arch/arm64/configs/sprd_qogirn6l_defconfig`.
Device trees: `ums9621-base.dtb` and `qogirn6l-1800-overlay.dtbo`.

Build the Image, modules and device trees together from the same revision.
Module vermagic must match the running kernel. For Android images, use the
device tree's normal build and packaging flow.

Legacy stock-image repacking tools are described in [artifacts_e5](artifacts_e5/README.md).
General kernel documentation is in [Documentation](Documentation/).
