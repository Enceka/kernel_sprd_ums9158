#!/bin/bash
# repack_vendor_dlkm.sh - rebuild the E5 vendor_dlkm image from our own modules
#
# What vendor_dlkm is on this device (all measured 2026-09-12, see
# artifacts_e5/stock_img_2026-09-12.md):
#
#	* a *logical* partition inside super - there is no /dev/block/by-name/
#	  vendor_dlkm; it was dumped from /dev/block/mapper/vendor_dlkm_a
#	  (111,296,512 bytes, i.e. the whole partition);
#	* EROFS, not ext4, and the fstab demands exactly that:
#		vendor_dlkm /vendor_dlkm erofs ro wait,logical,first_stage_mount,slotselect
#	  so the image has to be produced by mkfs.erofs - an ext4 image would not
#	  even mount.  (EROFS is also why it cannot be edited in place: it is
#	  read-only by design, and the stock filesystem is 100% full.)
#	* layout: /etc/{build.prop,NOTICE.xml.gz,fs_config_files,fs_config_dirs} +
#	  /lib/modules/*.ko (flat, no <kernelrelease> subdir) + modules.dep,
#	  modules.alias, modules.load, modules.load.{cali,charger}, modules.softdep,
#	  init.insmod.cfg;
#	* every file is 0644 root:root carrying the SELinux label
#	  u:object_r:vendor_file:s0.  That label is not cosmetic - init/modprobe
#	  read these files, and an unlabelled module is denied.  The label is set on
#	  the staging tree with sudo setfattr (an unprivileged setxattr of
#	  security.selinux fails with EPERM) and mkfs.erofs carries it into the
#	  image; the build verifies it afterwards and fails if it is missing.
#
# Policy - plan A, the default: stock modules we cannot rebuild are dropped.
# They can never load anyway (they are 5.15.119 builds, so each of them dies on
# "disagrees about version of symbol module_layout"), and a good part of them is
# built into our kernel (=y).  Set DLKM_KEEP_STOCK_ONLY=1 to keep them instead
# (bigger, and every boot logs ~45 failed loads).
#
# Output: stock-img/vendor_dlkm_e5.img, same size or smaller than the stock
# image so it can be flashed straight into the partition.
#
# Env:
#	O=out_e5		kernel build dir (modules.order is the source of truth)
#	STOCK=...		stock vendor_dlkm image
#	RESULT=...		output image
#	DLKM_KEEP_STOCK_ONLY=1	keep stock modules we do not rebuild
#	DLKM_KEEP_STAGING=1	keep the staging tree for inspection
#	DLKM_EROFSCOMP=-zlz4hc,9	compression (stock image is lz4)
#	DLKM_LABEL=u:object_r:vendor_file:s0
set -e
cd "$(dirname "$0")"

O="${O:-out_e5}"
STOCK="${STOCK:-stock-img/vendor_dlkm_a.img}"
RESULT="${RESULT:-stock-img/vendor_dlkm_e5.img}"
STAMP="$(date +%Y%m%d-%H%M%S)"
STAGE="${DLKM_STAGE:-vendor_dlkm_e5_tree}"
KEEP_STAGING="${DLKM_KEEP_STAGING:-0}"
KEEP_STOCK_ONLY="${DLKM_KEEP_STOCK_ONLY:-0}"
EROFSCOMP="${DLKM_EROFSCOMP:--zlz4hc,9}"
LABEL="${DLKM_LABEL:-u:object_r:vendor_file:s0}"
MNT="/tmp/vendor_dlkm_stock.$$"
CHK="/tmp/vendor_dlkm_check.$$"
SANDBOX="/tmp/vendor_dlkm_sandbox.$$"

say() { echo; echo "== $* =="; }
die() { echo "repack_vendor_dlkm: $*" >&2; exit 1; }
norm() { printf '%s' "$1" | tr '-' '_'; }

MOUNTS=""
part_cleanup() {
	local m
	for m in $MOUNTS; do umount "$m" 2>/dev/null || true; done
	rmdir "$MNT" "$CHK" 2>/dev/null || true
	rm -rf "$SANDBOX"
}
trap part_cleanup EXIT

# --------------------------------------------------------------- sanity ----
say "checking inputs"
[ -f "$STOCK" ] || die "stock image not found: $STOCK"
[ -d "$O" ] || die "kernel build dir not found: $O (run ./build_e5.sh first)"
[ -f "$O/modules.order" ] || die "$O/modules.order missing - run ./build_e5.sh first"
for t in mkfs.erofs dump.erofs depmod modinfo setfattr getfattr findmnt; do
	command -v "$t" >/dev/null || die "missing tool: $t"
done
sudo -n true 2>/dev/null || die "this script needs passwordless sudo (mount, mkfs.erofs, setfattr)"
[ ! -e "$RESULT" ] || echo "   (overwriting $RESULT)"

# ------------------------------------------------------- our module set ----
say "collecting our modules from $O/modules.order"
rm -rf "$SANDBOX"
mkdir -p "$SANDBOX/flat"
declare -A OUR_KO=()
declare -A OUR_PATH=()
ORDER=()
while read -r p; do
	[ -n "$p" ] || continue
	f="$O/$p"
	[ -f "$f" ] || continue
	b="$(basename "$f")"
	case "$b" in *.ko) ;; *) continue ;; esac
	n="$(norm "$b")"
	OUR_PATH[$b]="$f"
	OUR_KO[$n]="$b"
	ORDER+=("$b")
	cp -f "$f" "$SANDBOX/flat/$b"
done < "$O/modules.order"
[ "${#ORDER[@]}" -gt 0 ] || die "no modules found via $O/modules.order"
first="${ORDER[0]}"
KVER="$(modinfo -F vermagic "$SANDBOX/flat/$first" | awk '{print $1}')"
[ -n "$KVER" ] || die "could not read a vermagic from $first"
echo "   ${#ORDER[@]} modules, kernel release $KVER"
# depmod insists on <base>/lib/modules/<release>/ - the device's own layout is
# flat, but depmod only ever runs in this sandbox
mkdir -p "$SANDBOX/lib/modules/$KVER"
mv "$SANDBOX/flat"/*.ko "$SANDBOX/lib/modules/$KVER/"
rmdir "$SANDBOX/flat"
for f in modules.order modules.builtin modules.builtin.modinfo; do
	[ -f "$O/$f" ] && cp -f "$O/$f" "$SANDBOX/lib/modules/$KVER/$f"
done

# --------------------------------------------- extract the stock image ----
say "extracting $STOCK (EROFS, read-only)"
sudo rm -rf "$STAGE"
mkdir -p "$MNT" "$STAGE"
sudo mount -o loop,ro "$STOCK" "$MNT"
MOUNTS="$MNT"
sudo cp -a "$MNT/." "$STAGE/"
sudo umount "$MNT"; MOUNTS=""
[ -d "$STAGE/lib/modules" ] || die "unexpected layout: $STAGE/lib/modules missing"
STOCK_KO=$(ls "$STAGE"/lib/modules/*.ko 2>/dev/null | wc -l)
echo "   stock: $STOCK_KO modules"

# read the stock load lists *before* deleting anything
cp -f "$STAGE/lib/modules/modules.load" /tmp/vd_load.stock.$$
for extra in modules.load.cali modules.load.charger; do
	[ -f "$STAGE/lib/modules/$extra" ] && cp -f "$STAGE/lib/modules/$extra" "/tmp/vd_${extra}.$$"
done

# ------------------------------------------------------ replace modules ----
say "replacing modules (plan A: drop stock modules we cannot rebuild)"
dropped=0 replaced=0 kept=0
for f in "$STAGE"/lib/modules/*.ko; do
	b="$(basename "$f")"
	n="$(norm "$b")"
	if [ -n "${OUR_PATH[$b]:-}" ] || [ -n "${OUR_KO[$n]:-}" ]; then
		# we build this one - drop the stock copy (it may differ in -/_),
		# our file is added below under its own name
		sudo rm -f "$f"; dropped=$((dropped + 1))
	elif [ "$KEEP_STOCK_ONLY" = 1 ]; then
		kept=$((kept + 1))
	else
		sudo rm -f "$f"; dropped=$((dropped + 1))
	fi
done
for b in "${ORDER[@]}"; do
	sudo cp -f "${OUR_PATH[$b]}" "$STAGE/lib/modules/$b"
	replaced=$((replaced + 1))
done
echo "   removed $dropped stock modules, installed $replaced of ours, kept $kept"

# ------------------------------------------------------------- metadata ----
say "rebuilding modules.dep / modules.alias / modules.softdep / modules.symbols"
depmod -b "$SANDBOX" "$KVER"
mkdir -p "$CHK"
for m in modules.dep modules.alias modules.softdep modules.symbols; do
	src="$SANDBOX/lib/modules/$KVER/$m"
	[ -f "$src" ] || continue
	# depmod writes paths relative to its base dir; the device's files use
	# /vendor/lib/modules/<name>.ko, so rewrite both possible shapes
	sed -E -e "s#(^|[[:space:]])/lib/modules/$KVER/#\1/vendor/lib/modules/#g" \
	       -e "s#(^|[[:space:]])([A-Za-z0-9_.+-]+\.ko)#\1/vendor/lib/modules/\2#g" \
	       -e "s#(/vendor/lib/modules/)+#/vendor/lib/modules/#g" "$src" > "$CHK/$m"
	if ! grep -q '^/vendor/lib/modules/' "$CHK/$m"; then
		echo "   warning: could not normalise $m to /vendor/lib/modules/... - keeping depmod output"
		cp -f "$src" "$CHK/$m"
	fi
	sudo cp -f "$CHK/$m" "$STAGE/lib/modules/$m"
	echo "   $m: $(wc -l < "$CHK/$m") lines, e.g. $(head -1 "$CHK/$m" | cut -c1-90)"
done

say "rebuilding modules.load (stock order first, then the rest)"
# The stock list is hand tuned (ADI/PMIC/clock layers before the regulators),
# so keep its relative order and only swap in our own file names; modules that
# stock never listed (ours, plus anything new) are appended.
build_load() {
	local src="$1" dst="$2"; shift 2
	local line b n hit out=() seen=""
	if [ -f "$src" ]; then
		while read -r line; do
			b="$(printf '%s' "$line" | tr -d '\r' | sed 's/[[:space:]]*$//')"
			[ -z "$b" ] && continue
			n="$(norm "$b")"
			hit="${OUR_KO[$n]:-}"
			if [ -n "$hit" ]; then
				out+=("$hit"); seen="$seen $n"
			fi
		done < "$src"
	fi
	# everything we built that the stock list never mentioned has to be appended,
	# otherwise those modules simply never get loaded (this bit bit us once: the
	# fuel gauge was silently absent and the battery never appeared)
	for b in "$@"; do
		n="$(norm "$b")"
		case " $seen " in *" $n "*) ;; *) out+=("$b") ;; esac
	done
	printf '%s\n' "${out[@]}" > "$dst"
	echo "   $(basename "$dst"): ${#out[@]} entries"
}
build_load "/tmp/vd_load.stock.$$" "$CHK/modules.load" "${ORDER[@]}"
for extra in modules.load.cali modules.load.charger; do
	if [ -f "/tmp/vd_${extra}.$$" ]; then
		build_load "/tmp/vd_${extra}.$$" "$CHK/$extra" "${ORDER[@]}"
		sudo cp -f "$CHK/$extra" "$STAGE/lib/modules/$extra"
	fi
done
sudo cp -f "$CHK/modules.load" "$STAGE/lib/modules/modules.load"
rm -f /tmp/vd_load.stock.$$ /tmp/vd_modules.load.cali.$$ /tmp/vd_modules.load.charger.$$

# ------------------------------------------------- permissions and labels ----
say "fixing permissions and SELinux labels ($LABEL)"
sudo find "$STAGE" -type d -exec chmod 755 {} +
sudo find "$STAGE" -type f -exec chmod 644 {} +
sudo chown -R 0:0 "$STAGE"
if ! sudo find "$STAGE" -exec setfattr -n security.selinux -v "$LABEL" {} + 2>/dev/null; then
	die "could not set security.selinux on the staging tree"
fi

# ---------------------------------------------------------------- build ----
say "building the EROFS image"
STOCK_SIZE=$(stat -c %s "$STOCK")
STOCK_UUID=$(sudo dump.erofs -s "$STOCK" | awk -F': *' '/Filesystem UUID/{print $2}')
EROFSTIME="${DLKM_EROFS_TIME:-$(stat -c %Y "$STOCK")}"
set -x
sudo mkfs.erofs $EROFSCOMP -b 4096 --all-root -T "$EROFSTIME" ${STOCK_UUID:+-U "$STOCK_UUID"} "$RESULT" "$STAGE"
set +x
NEW_SIZE=$(stat -c %s "$RESULT")
say "image check"
echo "   stock : $STOCK_SIZE bytes (uuid ${STOCK_UUID:-n/a})"
echo "   new   : $NEW_SIZE bytes  $(stat -c '(%s sha256 below)' "$RESULT")"
[ "$NEW_SIZE" -le "$STOCK_SIZE" ] || die "new image is larger than the stock one ($NEW_SIZE > $STOCK_SIZE):
     it would not fit the logical partition.  Drop modules (DLKM_KEEP_STOCK_ONLY=0)
     or let the partition grow first."
sudo dump.erofs -s "$RESULT" | grep -E 'features|kernel version|blocksize|inode count' | sed 's/^/   /'
sha256sum "$RESULT"

# ------------------------------------------------------------- verify ----
say "verifying (mount the new image and inspect)"
mkdir -p "$CHK"
sudo mount -o loop,ro "$RESULT" "$CHK"
MOUNTS="$CHK"
ko=$(ls "$CHK"/lib/modules/*.ko | wc -l)
echo "   modules in image      : $ko (expected ${#ORDER[@]})"
bad=""
for f in $(ls "$CHK"/lib/modules/*.ko | head -3) "$CHK/lib/modules/modules.dep" "$CHK/etc/build.prop"; do
	l=$(sudo getfattr -n security.selinux "$f" 2>/dev/null | sed -n 's/.*security.selinux="\(.*\)"/\1/p')
	[ "$l" = "$LABEL" ] || bad="$bad $(basename "$f")"
done
mn=$(ls "$CHK"/lib/modules/*.ko | head -3 | xargs -n1 basename | tr '\n' ' ')
if [ -n "$bad" ]; then
	echo "   !! missing/wrong SELinux label on:$bad" >&2
	die "the image would ship unlabelled modules - init/modprobe reads would be denied.
     Check that mkfs.erofs copied the xattrs (default -x 2 keeps them) and that the
     staging tree really carries $LABEL."
fi
echo "   labels                : $LABEL on $mn ... OK"
err=$(sudo find "$CHK" -type f -exec md5sum {} + 2>&1 >/dev/null | wc -l)
echo "   unreadable files      : $err"
[ "$err" = 0 ] || die "some files in the new image cannot be read"
sudo umount "$CHK"; MOUNTS=""

if [ "$KEEP_STAGING" != 1 ]; then sudo rm -rf "$STAGE"; else echo; echo "   staging tree kept: $STAGE"; fi

say "done"
echo "flash it into the logical partition (fastbootd, not the bootloader):"
echo "  fastboot reboot fastboot        # or: adb reboot fastboot"
echo "  fastboot flash vendor_dlkm $RESULT"
echo "  fastboot reboot"
echo
echo "after boot, check:"
echo "  adb shell su -c 'ls /vendor/lib/modules | wc -l'      # ${#ORDER[@]}"
echo "  adb shell su -c 'dmesg | grep -c \"module_layout\"'    # 0"
