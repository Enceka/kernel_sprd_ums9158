#!/bin/bash
# Rongyue E5 (Unisoc UMS9621) kernel build script
# Base: Google android13-5.15 (5.15.211) + Unisoc UMS9621 platform + e5 defconfig
set -e
cd "$(dirname "$0")"

export ARCH=arm64
export LLVM=1
export LLVM_IAS=1
O="${O:-out_e5}"

# Unisoc DT overlay build (same wiring as moto/ulas SDK: qogirn6l family).
# With these exported, arch/arm64/boot/dts/sprd/Makefile selects the
# qogirn6l dtbo list, and scripts/Makefile.dtbo (included from
# scripts/Makefile.build) builds each .dtbo against its -base .dtb.
export BSP_BUILD_DT_OVERLAY=y
export BSP_BUILD_ANDROID_OS=y
export BSP_BUILD_FAMILY=qogirn6l
export CONFIG_DTB_ORIGINAL=y

# clang22 is newer than the kernel's clang14 reference build; silence warnings
# that are promoted to errors by -Werror in 5.15.
KCFLAGS="-Wno-frame-larger-than -Wno-deprecated-declarations -Wno-constant-conversion -Wno-uninitialized-const-pointer -Wno-unused-function"

# refresh .config from the committed defconfig (picks up defconfig edits)
# $O must exist first: "cp x $O/.config" fails silently when it does not, and
# olddefconfig then quietly falls back to Kconfig defaults (e.g.
# MALI_PLATFORM_NAME="devicetree" instead of the e5 defconfig's "qogirn6l"),
# so the build no longer tests the defconfig at all.
mkdir -p "$O"
cp arch/arm64/configs/e5_rongyue_defconfig "$O/.config"

echo "== olddefconfig =="
make O="$O" olddefconfig

echo "== building Image + modules + dtbs =="
make O="$O" -j"$(nproc)" KCFLAGS="$KCFLAGS" Image modules dtbs

echo "== build summary =="
ls -la "$O/arch/arm64/boot/Image"
echo "sprd dtbs:"
ls "$O/arch/arm64/boot/dts/sprd/" | grep -E "e5|ums9621" || true
echo "modules: $(find "$O" -name '*.ko' | wc -l) .ko files"
