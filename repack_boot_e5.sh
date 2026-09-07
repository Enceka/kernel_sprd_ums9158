#!/bin/bash
# repack_boot_e5.sh - in-place kernel swap for the Rongyue E5 boot image
#
# The Magisk-patched boot image is the only proven-bootable container we have:
#   * boot header v4 (ANDROID!), kernel = arm64 Image with EFI stub (MZ/PE)
#   * kernel payload region: [0x1000, ramdisk_off)
#   * magisk ramdisk (lz4) directly after it
#   * AVB footer: SHA256_RSA4096, carried over from the stock image
#
# We overwrite ONLY the kernel payload and zero the remainder of its region,
# leaving the header (including kernel_size), the ramdisk and the AVB footer
# byte-for-byte identical to the Magisk image:
#   * kernel_size keeps the stock value, so the bootloader still loads a
#     payload of the same length and still finds the ramdisk at the same
#     offset (the tail of the region is only padding after the kernel end);
#   * the AVB digest no longer matches - but the Magisk image boots today with
#     a non-matching digest, whereas a freshly generated footer
#     (--algorithm NONE) is a different descriptor type that the Unisoc
#     bootloader may reject outright.
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
base = bytearray(open(base_fn, "rb").read())
img = open(img_fn, "rb").read()

assert base[:8] == b"ANDROID!", "not an android boot image"
ks, rs = struct.unpack_from("<II", base, 8)
hv, = struct.unpack_from("<I", base, 40)
print("base: kernel_size=0x%x ramdisk_size=0x%x hv=%d" % (ks, rs, hv))
assert hv == 4, "expected boot header v4"

KSTART = 0x1000
LZ4 = (bytes.fromhex("02214c18"), bytes.fromhex("04224d18"))

# locate the magisk ramdisk: page-aligned right after the kernel region
off = KSTART + ((ks + 4095) // 4096) * 4096
while base[off:off + 4] not in LZ4 and off < KSTART + ks + 0x10000:
    off += 0x1000
assert base[off:off + 4] in LZ4, "could not locate ramdisk (lz4 magic)"
print("ramdisk: %d bytes at 0x%x" % (rs, off))

region = off - KSTART
print("kernel region: 0x%x bytes, new Image: 0x%x bytes (free 0x%x)"
      % (region, len(img), region - len(img)))
assert len(img) <= region, "new Image does not fit in the kernel region"

out = bytearray(base)
out[KSTART:KSTART + len(img)] = img
out[KSTART + len(img):off] = b"\x00" * (region - len(img))
open(out_fn, "wb").write(bytes(out))

# sanity: only the kernel payload may differ from the proven-bootable base
diff = sum(1 for a, b in zip(out, base) if a != b)
print("wrote %s: %d bytes, %d bytes changed vs base" % (out_fn, len(out), diff))
PYEOF

echo
echo "== repacked boot image: $RESULT =="
ls -la "$RESULT"
avbtool info_image --image "$RESULT" 2>/dev/null | grep -iE "Algorithm|Original|VBMeta offset"
echo
echo "flash with:  fastboot boot $RESULT   (or: fastboot flash boot_a $RESULT)"
