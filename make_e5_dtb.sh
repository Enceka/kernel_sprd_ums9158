#!/bin/bash
# make_e5_dtb.sh - build Rongyue E5 merged device tree (base + overlay)
#
# Requires: out_e5 dtbs built by build_e5.sh (produces ums9621-base.dtb and
# e5-rongyue-overlay.dtbo in $OUT/arch/arm64/boot/dts/sprd/).
#
# The kernel's ums9621-base.dtb has no __symbols__ (labels are lost when a
# dtb is compiled without -@), so we re-compile the base from the cpp-
# preprocessed source kbuild left behind (.ums9621-base.dtb.dts.tmp) with
# dtc -@ and then apply the overlay with fdtoverlay.
set -e
cd "$(dirname "$0")"
OUT="${O:-out_e5}"
SPRD="$OUT/arch/arm64/boot/dts/sprd"
RESULT="${RESULT:-e5-rongyue.dtb}"

[ -f "$SPRD/ums9621-base.dtb" ] || { echo "base dtb missing - run build_e5.sh first"; exit 1; }
[ -f "$SPRD/e5-rongyue-overlay.dtbo" ] || { echo "overlay dtbo missing - run build_e5.sh first"; exit 1; }
[ -f "$SPRD/.ums9621-base.dtb.dts.tmp" ] || { echo "preprocessed base source missing"; exit 1; }

echo "== building base with __symbols__ =="
dtc -@ -I dts -O dtb "$SPRD/.ums9621-base.dtb.dts.tmp" -o /tmp/ums9621-base-sym.dtb 2>/dev/null

echo "== applying e5-rongyue-overlay.dtbo =="
fdtoverlay -i /tmp/ums9621-base-sym.dtb -o "$RESULT" "$SPRD/e5-rongyue-overlay.dtbo"
ls -la "$RESULT"
echo
echo "compare with the device live FDT:"
echo "  dtc -I dtb -O dts kernel_probe/fdt.dtb | grep -c node"