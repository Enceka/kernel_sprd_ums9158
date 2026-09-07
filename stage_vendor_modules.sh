#!/bin/bash
# stage_vendor_modules.sh - collect rebuilt .ko modules for the E5 /vendor partition
set -e
cd "$(dirname "$0")"
OUT="${O:-out_e5}"
DST="${DST:-vendor_modules_e5}"

rm -rf "$DST"
mkdir -p "$DST"

# flat copy of every built module
find "$OUT" -name "*.ko" -exec cp -t "$DST" {} +

# manifest: module name, size, sha256
python3 - "$DST" "$OUT" <<PYEOF
import hashlib, os, sys
dst, out = sys.argv[1], sys.argv[2]
rows = []
for f in sorted(os.listdir(dst)):
    if not f.endswith(".ko"): continue
    p = os.path.join(dst, f)
    h = hashlib.sha256(open(p,"rb").read()).hexdigest()[:16]
    rows.append((f, os.path.getsize(p), h))
with open(os.path.join(dst, "modules.sha256.txt"), "w") as w:
    for f, sz, h in rows:
        w.write("%s  %10d  %s\n" % (f, sz, h))
print("staged %d modules in %s" % (len(rows), dst))
PYEOF

echo "vendor modules staged in $DST/  (flash/copy to /vendor/lib/modules/ on device)"