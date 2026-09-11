#!/bin/bash
#
# Compile-check driver code on macOS, without a Linux box or a container.
#
# This is NOT a replacement for build_e5.sh - it cannot produce Image or .ko
# files on this host (see "Limits" below).  What it does give you is a real
# aarch64 clang compile of selected directories against the e5 defconfig, which
# is enough to catch the 5.4 -> 5.15 API breakage that keeps showing up in
# freshly imported vendor drivers.
#
# Usage:
#   artifacts_e5/macos_compile_check.sh                       # default targets
#   artifacts_e5/macos_compile_check.sh drivers/foo/bar.o ...  # explicit targets
#
# Requirements (Homebrew):
#   brew install llvm lld
#
# Limits - why this is a check, not a build:
#
#   * The repo is usually checked out on case-insensitive APFS, where 13 pairs
#     of netfilter files that differ only in case (xt_DSCP.c / xt_dscp.c,
#     xt_MARK.h / xt_mark.h, ...) collapse onto one inode.  Both halves of each
#     pair are enabled in this defconfig, so net/netfilter cannot build here at
#     all.  git reports those files as permanently "modified"; that is the same
#     artifact, not a real diff.  A full build needs a case-sensitive volume
#     (or a container with its own filesystem - clone from .git, do not copy
#     the working tree, or the collapsed files come along).
#
#   * modpost runs but vmlinux/.ko linking is not attempted.  MODVERSIONS CRCs
#     produced here have not been validated against the device, and given the
#     vermagic history on this port, nothing built here should ever be flashed.
#
#   * CONFIG_HEADERS_INSTALL, CONFIG_DEBUG_INFO_BTF and CONFIG_STACK_VALIDATION
#     are turned off in the scratch .config only: they pull in headers_install
#     (needs GNU sed), resolve_btfids/libbpf and objtool (need libelf), none of
#     which exist on macOS.  The committed defconfig is not touched.
#
set -e
cd "$(dirname "$0")/.."

SHIM="${SHIM:-/tmp/kbuild-hostinc}"
O="${O:-/tmp/out_e5_check}"
export PATH="/opt/homebrew/opt/llvm/bin:/opt/homebrew/bin:$PATH"

for tool in clang ld.lld llvm-ar; do
	command -v "$tool" >/dev/null || { echo "missing $tool (brew install llvm lld)"; exit 1; }
done

# ---------------------------------------------------------------------------
# Host-tool shims.  Kernel host tools (modpost, sorttable, selinux genheaders,
# vdsomunge) assume a glibc userspace: <elf.h> and <asm/*.h> that macOS has no
# equivalent of.  Everything below lives outside the tree.
# ---------------------------------------------------------------------------
mkdir -p "$SHIM/asm"

# asm/<x>.h -> the tree's own uapi asm-generic/<x>.h, the way a generic arch does.
for f in include/uapi/asm-generic/*.h; do
	b=$(basename "$f")
	printf '#include <asm-generic/%s>\n' "$b" > "$SHIM/asm/$b"
done
# asm-generic/bitsperlong.h defaults to 32; this host is 64-bit.
printf '#define __BITS_PER_LONG 64\n#include <asm-generic/bitsperlong.h>\n' > "$SHIM/asm/bitsperlong.h"

# file2alias.c defines its own uuid_t, so that one TU is built with -D_UUID_T to
# suppress the SDK typedef - which then breaks <gethostuuid.h> via <unistd.h>.
# file2alias does not call gethostuuid(), so drop the declaration there only.
cat > "$SHIM/gethostuuid.h" <<'SHIM_EOF'
#ifndef _UUID_T
#include_next <gethostuuid.h>
#endif
SHIM_EOF

if [ ! -f "$SHIM/elf.h" ]; then
	cp artifacts_e5/macos_hostshim_elf.h "$SHIM/elf.h"
fi

HOSTFLAGS="-I$SHIM"

# ---------------------------------------------------------------------------
# Scratch .config
# ---------------------------------------------------------------------------
if [ ! -f "$O/.config" ]; then
	make ARCH=arm64 LLVM=1 LLVM_IAS=1 O="$O" e5_rongyue_defconfig
	./scripts/config --file "$O/.config" \
		-d HEADERS_INSTALL -d DEBUG_INFO_BTF -d STACK_VALIDATION
	make ARCH=arm64 LLVM=1 LLVM_IAS=1 O="$O" olddefconfig
fi

# clang 22+ is far newer than the kernel's clang14 reference build; same waivers
# build_e5.sh uses, plus -ferror-limit=0 so a bad file reports everything at once.
KC="-ferror-limit=0 -Wno-frame-larger-than -Wno-deprecated-declarations"
KC="$KC -Wno-constant-conversion -Wno-uninitialized-const-pointer -Wno-unused-function"

TARGETS=("$@")
if [ ${#TARGETS[@]} -eq 0 ]; then
	TARGETS=(
		drivers/vendor/common/touchscreen_v2/
		drivers/unisoc_platform/sprd_fm/
		drivers/unisoc_platform/sprd_bt/
		drivers/unisoc_platform/sprd_wlan_combo/sprd_wlan_combo.o
	)
fi

# Note: a bare "dir/" target is unreliable here - kbuild's single-target descend
# silently does nothing for some directories.  Naming the module object
# (foo/foo.o) always works, and is what the wlan entry above does.
make ARCH=arm64 LLVM=1 LLVM_IAS=1 O="$O" \
	HOSTCFLAGS="$HOSTFLAGS" HOSTCFLAGS_file2alias.o="-D_UUID_T" \
	KCFLAGS="$KC" -j"$(sysctl -n hw.ncpu)" "${TARGETS[@]}"
