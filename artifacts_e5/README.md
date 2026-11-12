# E5 stock-image tools

Legacy tools for inspecting E5 USB state and repacking stock-based kernel images.
For LineageOS, use the device tree's normal image build instead.

- `usb_probe.sh` and `usb_dt_probe.sh`: collect USB runtime and device-tree state.
- `vendor_boot/build_vendor_boot_e5.py`: replace first-stage kernel modules in a
  stock vendor_boot image; see its [usage](vendor_boot/README.md).
- `MANIFEST.sha256`: historical artifact hashes.

Image, modules and device trees must come from the same build. Stock images
and generated artifacts are local inputs and are not distributed here.
