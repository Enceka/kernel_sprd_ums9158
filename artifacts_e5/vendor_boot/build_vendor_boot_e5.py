#!/usr/bin/env python3
"""
build_vendor_boot_e5.py

Rebuild the E5 vendor_boot image: keep header / DTB / ramdisk-table /
bootconfig / AVB structure byte-identical, and replace ONLY the module
payload inside the vendor ramdisk (lib/modules/*.ko + modules.* metadata)
with the modules built from our own kernel tree.

Why: the stock first-stage modules are built for 5.15.119-android13-8, so
they can never load on the rebuilt 5.15.211 kernel (MODVERSIONS CRC
mismatch). init loads first-stage modules from /lib/modules in the vendor
ramdisk, so putting our own .ko there is what actually brings eMMC up.

Modules are taken from out_e5/modules.order (i.e. only what the *current*
build produced) - out_e5 keeps stale .ko files from earlier builds around
and those must never be injected.
"""
import os, struct, subprocess, sys, shutil, glob

HERE = os.path.dirname(os.path.abspath(__file__))


def _find_stock():
    # The stock image is not kept in the kernel tree (it is a device dump);
    # look next to the script, then in the usual dump directory, else require
    # E5_STOCK_VENDOR_BOOT to be set.
    env = os.environ.get('E5_STOCK_VENDOR_BOOT')
    cands = [env] if env else []
    cands += [os.path.join(HERE, 'vendor_boot_a.img'),
              os.path.join(HERE, '..', '..', '..', 'stock-img', 'vendor_boot_a.img'),
              '/home/hema/Workspace/e5/stock-img/vendor_boot_a.img']
    for c in cands:
        if c and os.path.exists(c):
            return os.path.abspath(c)
    sys.exit('stock vendor_boot_a.img not found - set E5_STOCK_VENDOR_BOOT')


STOCK = _find_stock()
OUT = os.environ.get('E5_OUT_VENDOR_BOOT') or os.path.join(HERE, 'vendor_boot_e5.img')
# default: <kernel tree>/out_e5, i.e. ../../out_e5 from artifacts_e5/vendor_boot
BUILD = os.environ.get('E5_BUILD_DIR') or \
    os.path.normpath(os.path.join(HERE, '..', '..', 'out_e5'))
PAGE = 0x1000
align = lambda x: (x + PAGE - 1) & ~(PAGE - 1)
align4 = lambda x: (x + 3) & ~3

# ---------------------------------------------------------------- modules ----
def current_modules():
    order = [l.strip() for l in open(os.path.join(BUILD, 'modules.order')) if l.strip()]
    mods = {}
    for p in order:
        fp = os.path.join(BUILD, p)
        if os.path.exists(fp):
            mods[os.path.basename(p)] = fp
    return mods

MODS = current_modules()
if not MODS:
    raise SystemExit('no modules in %s - build the kernel first' % BUILD)

# Out-of-tree modules we build ourselves, absent from out_e5/modules.order.
# mali_kbase is the Mali GPU DDK (gpu-mali/): /vendor/lib/modules only carries
# the 5.15.119 build, which cannot load here, and without it surfaceflinger
# aborts in GLESRenderEngine and the screen stays black.
EXTRA_MODS = {}
for _p in (os.path.normpath(os.path.join(HERE, '..', '..',
                                         'gpu-mali', 'mali', 'mali_kbase.ko')),):
    if os.path.exists(_p):
        EXTRA_MODS[os.path.basename(_p)] = _p
MODS.update(EXTRA_MODS)
print('modules from current build: %d tree + %d out-of-tree'
      % (len(MODS) - len(EXTRA_MODS), len(EXTRA_MODS)))
stale = [os.path.basename(p) for p in glob.glob(BUILD + '/**/*.ko', recursive=True)]
stale = [x for x in stale if x not in MODS]
if stale:
    print('ignoring %d stale .ko left over in out_e5: %s' % (len(stale), stale[:8]))

b = open(STOCK, 'rb').read()
u32 = lambda o: struct.unpack_from('<I', b, o)[0]

vr_size  = u32(0x18)     # vendor ramdisk (compressed) size
dtb_size = u32(0x834)
tbl_size = u32(0x840)
bc_size  = u32(0x84c)

rd_off  = 0x1000
dtb_off = align(rd_off + vr_size)
tbl_off = align(dtb_off + dtb_size)
bc_off  = align(tbl_off + tbl_size)
print('stock layout: ramdisk @0x%x (0x%x)  dtb @0x%x (0x%x)  table @0x%x (0x%x)  '
      'bootconfig @0x%x (0x%x)' %
      (rd_off, vr_size, dtb_off, dtb_size, tbl_off, tbl_size, bc_off, bc_size))

ramdisk  = b[rd_off:rd_off + vr_size]
dtb      = b[dtb_off:dtb_off + dtb_size]
table    = bytearray(b[tbl_off:tbl_off + tbl_size])
bootconf = b[bc_off:bc_off + bc_size]

# ---------------------------------------------------------------- cpio ----
def parse_cpio(data):
    out, off = [], 0
    while True:
        if data[off:off + 6] != b'070701':
            raise SystemExit('bad cpio magic at 0x%x' % off)
        g = lambda a, z: int(data[off + a:off + z], 16)
        e = dict(ino=g(6, 14), mode=g(14, 22), uid=g(22, 30), gid=g(30, 38),
                 nlink=g(38, 46), mtime=g(46, 54), filesize=g(54, 62),
                 devmajor=g(62, 70), devminor=g(70, 78),
                 rdevmajor=g(78, 86), rdevminor=g(86, 94),
                 namesize=g(94, 102), check=g(102, 110))
        ns = off + 110
        e['name'] = data[ns:ns + e['namesize'] - 1].decode('utf-8', 'replace')
        ds = align4(ns + e['namesize'])
        e['data'] = data[ds:ds + e['filesize']]
        off = align4(ds + e['filesize'])
        out.append(e)
        if e['name'] == 'TRAILER!!!':
            break
    return out

def emit(e):
    name = e['name'].encode() + b'\0'
    hdr = b'070701' + b''.join(b'%08X' % (e[k] & 0xffffffff) for k in
          ('ino', 'mode', 'uid', 'gid', 'nlink', 'mtime', 'filesize',
           'devmajor', 'devminor', 'rdevmajor', 'rdevminor'))
    hdr += b'%08X' % len(name) + b'%08X' % e.get('check', 0)
    buf = bytearray(hdr + name)
    buf += b'\0' * (-len(buf) % 4)
    buf += e['data']
    buf += b'\0' * (-len(buf) % 4)
    return bytes(buf)

# --------------------------------------------------------- new metadata ----
def kernel_version():
    v = subprocess.check_output(['modinfo', '-F', 'vermagic',
                                 sorted(MODS.values())[0]], text=True)
    return v.split()[0]

KVER = kernel_version()
print('kernel version for depmod:', KVER)
dep_root = '/tmp/depmod_e5'
shutil.rmtree(dep_root, ignore_errors=True)
kdir = os.path.join(dep_root, 'lib', 'modules', KVER)
os.makedirs(kdir)
for fp in MODS.values():
    shutil.copy2(fp, kdir)
# modules.order / modules.builtin come from the kernel build tree; they are
# only needed to silence depmod and to keep builtin symbols accurate.
for f in ('modules.order', 'modules.builtin', 'modules.builtin.modinfo'):
    src = os.path.join(BUILD, f)
    if os.path.exists(src):
        shutil.copy2(src, os.path.join(kdir, f))
subprocess.run(['depmod', '-b', dep_root, KVER], check=True)

meta = {}
for f in ('modules.dep', 'modules.alias', 'modules.softdep', 'modules.symbols'):
    p = os.path.join(kdir, f)
    if os.path.exists(p):
        meta[f] = open(p, 'rb').read()

# Load order: reuse the STOCK first-stage list verbatim.
#
# Why not the alphabetical order used before: the stock list is hand tuned
# (ADI/PMIC/clock layers -> regulators -> storage).  Putting
# ump9620-regulator.ko first makes dev_get_regmap() return NULL and the driver
# dereferences it right after, killing first-stage init long before ramoops
# (and therefore pstore) is up - which is exactly why the previous images
# rebooted with no log at all.
# Modules the stock image never loads in first stage (panfrost, wcn_bsp,
# coresight, touch, ...) are dropped too: their power/clock is not ready yet.
stock_load = os.path.join(HERE, 'modules.load.stock')
stock_list = [l.strip() for l in open(stock_load) if l.strip().endswith('.ko')]
order, seen = [], set()
for n in stock_list:
    if n in MODS and n not in seen:
        seen.add(n)
        order.append(n)
skipped_builtin = [n for n in stock_list if n not in MODS]
extra = [n for n in MODS if n not in seen]
print('first-stage load list: %d modules (stock asks for %d)' % (len(order), len(stock_list)))
print('  stock-only, built into our kernel (=y), skipped: %d' % len(skipped_builtin))
print('  ours but never first-stage on stock, dropped: %d %s' % (len(extra), extra[:8]))

# Modules stock loads from /vendor/lib/modules during second stage, which our
# build has to supply from this ramdisk instead (the only place we control
# without touching vendor).
#
# sprd_vpu_pw_domain.ko registers the genpd provider for <&vpu_pd_top>
# ("sprd,vpu-pd", i.e. soc:mm:power-domain@0/1/3).  dpu/dsi/gsp all hang off
# that domain, and without a registered provider both fw_devlink and
# genpd_dev_pm_attach() return -EPROBE_DEFER forever, so the whole display
# stack never probes.  It has no dependency on the PMIC/clock layers, so it is
# inserted ahead of the display modules.
for n in ('sprd_vpu_pw_domain.ko', 'sprd-gsp.ko', 'sprd-drm.ko', 'ocp2131.ko'):
    if n in MODS and n not in seen:
        seen.add(n)
        order.append(n)
        print('  appended out-of-tree: %s' % n)

load = (''.join(n + '\n' for n in order)).encode()
meta['modules.load'] = load
meta['modules.load.recovery'] = load

# -------------------------------------------------------------- rebuild ----
open('/tmp/vb_stock_ramdisk.lz4', 'wb').write(ramdisk)
subprocess.run(['lz4', '-d', '-f', '/tmp/vb_stock_ramdisk.lz4',
                '/tmp/vb_stock_ramdisk.cpio'], check=True)
cpio_data = open('/tmp/vb_stock_ramdisk.cpio', 'rb').read()
print('ramdisk decompressed: 0x%x bytes' % len(cpio_data))

entries = parse_cpio(cpio_data)
print('cpio entries: %d' % len(entries))

out_entries, dropped, replaced = [], [], []
seen_dir = False
for e in entries:
    n = e['name']
    if n == 'TRAILER!!!':
        continue
    if n.startswith('lib/modules/'):
        base = n[len('lib/modules/'):]
        if base.endswith('.ko'):
            dropped.append(base)
            continue
        if base in ('modules.dep', 'modules.alias', 'modules.softdep',
                    'modules.symbols', 'modules.load', 'modules.load.recovery',
                    'modules.order', 'modules.builtin', 'modules.devname'):
            if base in meta:
                e = dict(e, data=meta.pop(base))
                e['filesize'] = len(e['data'])
                replaced.append(base)
                out_entries.append(e)
            continue
    out_entries.append(e)
    if n == 'lib/modules' and not seen_dir:
        seen_dir = True
        for name in sorted(n[:-3] for n in MODS):
            fn = name + '.ko'
            data = open(MODS[fn], 'rb').read()
            out_entries.append(dict(ino=0, mode=0o100644, uid=0, gid=0, nlink=1,
                                    mtime=0, filesize=len(data), devmajor=0,
                                    devminor=0, rdevmajor=0, rdevminor=0,
                                    check=0, name='lib/modules/' + fn, data=data))
        for f, data in sorted(meta.items()):
            out_entries.append(dict(ino=0, mode=0o100644, uid=0, gid=0, nlink=1,
                                    mtime=0, filesize=len(data), devmajor=0,
                                    devminor=0, rdevmajor=0, rdevminor=0,
                                    check=0, name='lib/modules/' + f, data=data))

print('dropped %d stock .ko, replaced %d metadata files, inserted %d of our .ko'
      % (len(dropped), len(replaced), len(MODS)))

out_cpio = bytearray()
for e in out_entries:
    out_cpio += emit(e)
out_cpio += emit(dict(ino=0, mode=0, uid=0, gid=0, nlink=1, mtime=0,
                      filesize=0, devmajor=0, devminor=0, rdevmajor=0,
                      rdevminor=0, check=0, name='TRAILER!!!', data=b''))
open('/tmp/vb_new.cpio', 'wb').write(bytes(out_cpio))
print('new cpio: 0x%x bytes (%d entries)' % (len(out_cpio), len(out_entries)))

subprocess.run('lz4 -l -9 -c /tmp/vb_new.cpio > /tmp/vb_new.lz4', shell=True, check=True)
new_lz4 = open('/tmp/vb_new.lz4', 'rb').read()
assert new_lz4[:4] == bytes.fromhex('02214c18'), 'not lz4 legacy: %s' % new_lz4[:4].hex()
print('new ramdisk (lz4 legacy): 0x%x bytes (was 0x%x)' % (len(new_lz4), vr_size))

# ------------------------------------------------------- vendor_boot out ---
new_dtboff = align(rd_off + len(new_lz4))
new_tbloff = align(new_dtboff + dtb_size)
new_bcoff  = align(new_tbloff + tbl_size)
new_end    = align(new_bcoff + bc_size)

table[0:4] = struct.pack('<I', len(new_lz4))          # ramdisk size
table[4:8] = struct.pack('<I', 0)                     # ramdisk offset

img = bytearray(b)
struct.pack_into('<I', img, 0x18, len(new_lz4))

def place(off, blob):
    img[off:off + len(blob)] = blob

for off in range(rd_off, align(bc_off + bc_size)):
    img[off] = 0
place(rd_off, new_lz4)
place(new_dtboff, dtb)
place(new_tbloff, bytes(table))
place(new_bcoff, bootconf)

# AVB: the footer carries magic "AVBf" (the vbmeta image itself is "AVB0")
# and lives in the last 64 bytes of the partition. It points at the vbmeta
# blob, which sits at original_image_size - i.e. right behind the payload.
# We move the vbmeta blob to the new end of the payload and repoint the
# footer at it, otherwise the bootloader reads zeros and fails with
# "invalid vbmeta header" / ERROR_INVALID_METADATA.
foots, s = [], 0
while True:
    i = b.find(b'AVBf', s)
    if i < 0:
        break
    foots.append(i)
    s = i + 1
print('AVBf footer offsets:', [hex(x) for x in foots])
if foots:
    foff = foots[-1]
    _maj, _min, orig_sz, vbm_off, vbm_size = struct.unpack_from('>IIQQQ', b, foff + 4)
    print('footer @0x%x: original_image_size=0x%x vbmeta @0x%x size 0x%x' %
          (foff, orig_sz, vbm_off, vbm_size))
    assert vbm_off + vbm_size <= len(b), 'bogus vbmeta location'
    vbm = b[vbm_off:vbm_off + vbm_size]
    for off in range(vbm_off, vbm_off + vbm_size):
        img[off] = 0
    # new_end must land before the footer (last 64 bytes of the partition) -
    # if our module set is large enough to push the payload past that point,
    # placing vbmeta here would silently overwrite the footer (or run past
    # the end of the partition), producing an image that still "writes"
    # successfully but fails AVB with ERROR_INVALID_METADATA / slot_data[0]=0x0
    # on every other boot depending on what garbage ends up in the footer.
    assert new_end + vbm_size <= foff, (
        'new payload (0x%x) + vbmeta (0x%x bytes) overruns the AVB footer at '
        '0x%x - vendor_boot payload has grown %d bytes too large for this '
        'partition; trim the module set or grow the partition' %
        (new_end, vbm_size, foff, new_end + vbm_size - foff))
    place(new_end, vbm)
    nf = bytearray(b[foff:foff + 64])
    struct.pack_into('>QQQ', nf, 12, new_end, new_end, vbm_size)
    place(foff, bytes(nf))
    print('  -> vbmeta moved to 0x%x, footer repointed' % new_end)

open(OUT, 'wb').write(bytes(img))
print()
print('wrote %s (0x%x bytes)' % (OUT, len(img)))
print('  ramdisk @0x%x..0x%x' % (rd_off, rd_off + len(new_lz4)))
print('  dtb     @0x%x' % new_dtboff)
print('  table   @0x%x' % new_tbloff)
print('  bootcfg @0x%x' % new_bcoff)
