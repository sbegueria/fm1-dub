#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# (1.5.0) the container's three readers and writers the same: firmware/src/app/container.c (tests/host/container_test.c),
# tools/dub1_container.py, web/dub1container.js (in Node, when there is one). Each makes sample containers; every
# sample, any maker's, dumps the same in all three (line for line) and is written again by each byte for byte; the
# WORK fixtures the simulator wrote (tests/host/*.dub1) too.
#   sh tests/host/container_twins.sh
set -eu
cd "$(dirname "$0")/../.."
CC="${CC:-cc}"
PY="${PYTHON:-python3}"
OUT=build/host/twins
rm -rf "$OUT"
mkdir -p "$OUT/c" "$OUT/py" "$OUT/js" "$OUT/x"
$CC -O2 -o build/host/container_twin_c tests/host/container_test.c
build/host/container_twin_c emit "$OUT/c" 60
$PY tools/dub1_container.py emit "$OUT/py" 60
NODE=""
if command -v node >/dev/null 2>&1; then
    NODE=node
    node tests/host/container_twin.mjs emit "$OUT/js" 60
else
    echo "  (no node: the JS twin not run)"
fi
for f in tests/host/*.dub1; do
    if [ -f "$f" ]; then cp "$f" "$OUT/x/"; fi
done
n=0
for f in "$OUT"/*/*.dub1; do
    build/host/container_twin_c dump "$f" > "$OUT/dump.c"
    $PY tools/dub1_container.py dump "$f" > "$OUT/dump.py"
    cmp -s "$OUT/dump.c" "$OUT/dump.py" || { echo "dump differs (C, Python): $f"; diff "$OUT/dump.c" "$OUT/dump.py" | head -5; exit 1; }
    build/host/container_twin_c recode "$f" "$OUT/re.c"
    $PY tools/dub1_container.py recode "$f" "$OUT/re.py"
    cmp -s "$f" "$OUT/re.c" || { echo "C recode differs: $f"; exit 1; }
    cmp -s "$f" "$OUT/re.py" || { echo "Python recode differs: $f"; exit 1; }
    if [ -n "$NODE" ]; then
        node tests/host/container_twin.mjs dump "$f" > "$OUT/dump.js"
        cmp -s "$OUT/dump.c" "$OUT/dump.js" || { echo "dump differs (C, JS): $f"; diff "$OUT/dump.c" "$OUT/dump.js" | head -5; exit 1; }
        node tests/host/container_twin.mjs recode "$f" "$OUT/re.js"
        cmp -s "$f" "$OUT/re.js" || { echo "JS recode differs: $f"; exit 1; }
    fi
    n=$((n + 1))
done
echo "twins: $n containers, the same dump and the same bytes in C, Python${NODE:+ and JS}"
# (1.5.0, step 4) a name as the device takes it (names.h): the same answer, and the same width, from the C, the Python
# and the JS (each with its copy of the header font's widths: tools/gen_name_w.py --check, the copies the font's)
$CC -O2 -o build/host/name_test tests/host/name_test.c
build/host/name_test
if [ -f build/gen/felucca_font.h ]; then
    $PY tools/gen_name_w.py --check || { echo "the name widths' copies differ from the font"; exit 1; }
fi
$PY tools/dub1_container.py namelist > "$OUT/names.txt"
build/host/name_test names < "$OUT/names.txt" > "$OUT/names.c"
$PY tools/dub1_container.py names < "$OUT/names.txt" > "$OUT/names.py"
cmp -s "$OUT/names.c" "$OUT/names.py" || { echo "names differ (C, Python)"; diff "$OUT/names.c" "$OUT/names.py" | head -5; exit 1; }
if [ -n "$NODE" ]; then
    node tests/host/container_twin.mjs names < "$OUT/names.txt" > "$OUT/names.js"
    cmp -s "$OUT/names.c" "$OUT/names.js" || { echo "names differ (C, JS)"; diff "$OUT/names.c" "$OUT/names.js" | head -5; exit 1; }
fi
echo "names: $(wc -l < "$OUT/names.txt" | tr -d ' ') names, $(grep -vc no "$OUT/names.c") taken, the same in C, Python${NODE:+ and JS}"
# (1.5.0, step 5) a BACKUP (the web page's whole backup: WORK and the sets, each whole): made the same, byte for byte,
# by the Python and the JS, and read back the same by both
$PY tools/dub1_container.py bundle "$OUT/bk.py" 1:0:tests/host/work-1.5.0-step4.dub1 2:3:"$OUT/py/p001.dub1" 2:7:tests/host/work-1.5.0.dub1
$PY tools/dub1_container.py unbundle "$OUT/bk.py" > "$OUT/unb.py"
[ "$(wc -l < "$OUT/unb.py" | tr -d ' ')" = 3 ] || { echo "a backup: not its three objects"; cat "$OUT/unb.py"; exit 1; }
if [ -n "$NODE" ]; then
    node tests/host/container_twin.mjs bundle "$OUT/bk.js" 1:0:tests/host/work-1.5.0-step4.dub1 2:3:"$OUT/py/p001.dub1" 2:7:tests/host/work-1.5.0.dub1
    cmp -s "$OUT/bk.py" "$OUT/bk.js" || { echo "a backup differs (Python, JS)"; exit 1; }
    node tests/host/container_twin.mjs unbundle "$OUT/bk.py" > "$OUT/unb.js"
    cmp -s "$OUT/unb.py" "$OUT/unb.js" || { echo "a backup read differently (Python, JS)"; exit 1; }
fi
build/host/container_twin_c dump "$OUT/bk.py" > /dev/null || { echo "a backup: not a container to the C"; exit 1; }
echo "backup: WORK and two sets in one file, the same in Python${NODE:+ and JS}"
