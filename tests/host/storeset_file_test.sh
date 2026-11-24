#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# (the user's, 2026-11-23) A set stored from the FM-1 itself (the Set page's KNOB 3, Store: tests/scenarios/storeknob.amb)
# is a set like any other: read by the Python twin (tools/dub1_container.py, the Sets page's dub1container.js's twin),
# its shape and CRC, kind SET, today's format, its name and pieces as the rules say: with none loaded, all eight and
# "SET 3"; into the Winter Set loaded, its five, its name, its pieces' names and their cues kept; and re-coded the same.
# Needs build/host/amb_host (host/build_host.sh).
#   sh tests/host/storeset_file_test.sh
set -e
cd "$(dirname "$0")/../.."
PY="${PYTHON:-python3}"
OUT=build/scenarios/storeset-file
mkdir -p "$OUT"
sed "s|build/scenarios/storeknob/|$OUT/|g" tests/scenarios/storeknob.amb > "$OUT/storeknob.amb"
build/host/amb_host "$OUT/storeknob.amb" "$OUT" > "$OUT/storeknob.log" 2>&1 || { tail -20 "$OUT/storeknob.log"; exit 1; }
"$PY" - "$OUT" <<'PYEOF'
import re, sys
from pathlib import Path
sys.path.insert(0, "tools")
import dub1_container as D
out = Path(sys.argv[1])
fmt = int(re.search(r"#define PROJ_FORMAT (\d+)u", Path("firmware/src/app/app.h").read_text()).group(1))
want = {"set3.dub1": ("SET 3", ["", "", "", "", "", "", "", ""], 0),
        "winter-stored.dub1": ("Winter Set", ["Hoarfrost", "Black Ice", "Snowfield", "Aurora", "Thaw"], 4)}
bad = 0
for f, (name, pieces, cues0) in want.items():
    b = (out / f).read_bytes()
    c = D.parse(b)
    h = c["header"]
    meta = [ch for ch in c["chunks"] if ch["id"] == b"META"]
    pc = [ch for ch in c["chunks"] if ch["id"] == b"PIEC"]
    cues = {ch["index"]: len(ch["cues"]) for ch in c["chunks"] if ch["id"] == b"CUES"}
    got = (h["kind"], h["format"], h["npieces"], D.name(meta[0]["name"]) if meta else None, [D.name(p["name"]) for p in pc],
           cues.get(0, 0), any(ch["id"] == b"GLOB" for ch in c["chunks"]))
    exp = (2, fmt, len(pieces), name, pieces, cues0, True)
    if got != exp:
        print("FAIL %s: %r, not %r" % (f, got, exp))
        bad = 1
        continue
    if D.build(c) != b:
        print("FAIL %s: not re-coded the same" % f)
        bad = 1
        continue
    print("  ok %s: a SET, format %d, \"%s\", %d pieces (%s), %d cues on the first, %d bytes" % (
        f, h["format"], name, len(pieces), ", ".join(p or "-" for p in pieces), cues0, h["total"]))
sys.exit(bad)
PYEOF
