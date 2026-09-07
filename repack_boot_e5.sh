#!/bin/bash
# repack_boot_e5.sh - repack Rongyue E5 boot image with the rebuilt kernel
#
# Device boot chain (verified from boot_a.img / magisk_patched images):
#   * boot image header v4 (ANDROID!), kernel = arm64 Image with EFI stub (MZ/PE)
#   * boot.img carries NO dtb: the bootloader loads the DT from the device's
#     own dtbo partition (the stock E5 fdt we rebuilt from). We therefore
#     swap ONLY the kernel and keep the proven-bootable Magisk layout.
#   * AVB is re-added with --algorithm NONE (device is unlocked; Magisk image
#     already carries a non-matching digest and boots fine).
set -e
cd "$(dirname "$0")"

OUT="${O:-out_e5}"
BASE="${BASE:-/home/hema/Workspace/e5/magisk_patched-30700_hNDcW.img}"
IMAGE="$OUT/arch/arm64/boot/Image"
RESULT="${RESULT:-boot-e5.img}"

[ -f "$IMAGE" ] || { echo "Image not found: $IMAGE"; exit 1; }
[ -f "$BASE" ] || { echo "base image not found: $BASE"; exit 1; }

python3 - "$BASE" "$IMAGE" "$RESULT" <<PYEOF
import struct, sys
base_fn, img_fn, out_fn = sys.argv[1], sys.argv[2], sys.argv[3]
base = open(base_fn, "rb").read()
img = open(img_fn, "rb").read()

def pad(b, n=4096):
    return b + b"\x00" * ((-len(b)) % n)

# v4 header fields
ks  = struct.unpack_from("<I", base, 8)[0]
rs  = struct.unpack_from("<I", base, 12)[0]
hsz = struct.unpack_from("<I", base, 20)[0]
hv  = struct.unpack_from("<I", base, 40)[0]
print("base: kernel_size=0x%x ramdisk_size=0x%x hdr_size=0x%x ver=%d" % (ks, rs, hsz, hv))
assert hv == 4, "expected boot header v4"

# magisk ramdisk starts right after the padded kernel
kend = 0x1000 + ((ks + 4095)//4096)*4096
ramdisk = base[kend:kend+rs]
assert len(ramdisk) == rs, "ramdisk extraction failed"
print("magisk ramdisk: %d bytes at 0x%x" % (len(ramdisk), kend))

header = bytearray(base[:0x1000])
struct.pack_into("<I", header, 8, len(img))   # kernel_size = new Image size
# ramdisk_size, header fields stay identical

content = bytes(header) + pad(img) + pad(ramdisk)
open(out_fn, "wb").write(content)
print("wrote %s: %d bytes (header+kernel+ramdisk)" % (out_fn, len(content)))
PYEOF

# AVB: erase any old footer then add a fresh one (unsigned, unlocked device)
avbtool erase_footer --image "$RESULT" 2>/dev/null || true
avbtool add_hash_footer --image "$RESULT" --partition_name boot \
    --partition_size 67108864 --algorithm NONE

echo "== repacked boot image: $RESULT =="
ls -la "$RESULT"
avbtool info_image --image "$RESULT" | head -12
echo
echo "NOTE: kernel-only swap; the device keeps using its own dtbo partition.",
echo "      flash with:  fastboot flash boot_a $RESULT   (or flash boot)"