#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# (1.5.0, step 6) The ready-made sets in sets/ (published for download: tools/make_sets.sh made them) still load, so a
# later change to the format or the decoder can't silently break them: each read by the Python twin (a SET, its CRC,
# a format this DUB-1 reads, every name as wide as the header shows), listed in sets/sets.json as it is (name,
# pieces, length) and named in sets/README.md; then each loaded by the simulator as the FM-1 loads a set (`setin`:
# the firmware's own decoder, every value checked), its pieces' names and count as the file says, (step 7) each piece's
# cues all taken, WORK written and read back the same. Needs build/host/amb_host (host/build_host.sh).
#   sh tests/host/published_sets_test.sh
set -e
cd "$(dirname "$0")/../.."
PY="${PYTHON:-python3}"
OUT=build/scenarios/published-sets
mkdir -p "$OUT"
"$PY" - "$OUT/load.amb" <<'PYEOF'
import json, re, sys
from pathlib import Path
sys.path.insert(0, "tools")
import dub1_container as D
fmt = int(re.search(r"#define PROJ_FORMAT (\d+)u", Path("firmware/src/app/app.h").read_text()).group(1))
listed = {o["file"]: o for o in json.loads(Path("sets/sets.json").read_text())}
readme = Path("sets/README.md").read_text()
files = sorted(Path("sets").glob("*.dub1"))
bad = []
lines = ["wait 600", "tap PLAY", "wait 300", "expect playing 0"]
if not files:
    bad.append("no sets in sets/")
for f in files:
    try:
        c = D.parse(f.read_bytes())
    except D.Refused as e:
        bad.append("%s: doesn't read (%s)" % (f.name, e))
        continue
    h = c["header"]
    meta = [ch for ch in c["chunks"] if ch["id"] == b"META"]
    pieces = [ch for ch in c["chunks"] if ch["id"] == b"PIEC"]
    name = D.name(meta[0]["name"]) if meta else ""
    if h["kind"] != 2 or not 1 <= h["npieces"] <= 8 or h["npieces"] != len(pieces):
        bad.append("%s: not a set of 1 to 8 pieces" % f.name)
    if h["format"] > fmt:
        bad.append("%s: format %d, newer than this DUB-1's %d" % (f.name, h["format"], fmt))
    for ch in meta + pieces:
        if D.name_fits(ch["name"]) is None:
            bad.append("%s: the name %r too wide or not allowed" % (f.name, D.name(ch["name"])))
    o = listed.get(f.name)
    if not o or o["name"] != name or o["pieces"] != [D.name(p["name"]) for p in pieces] or o["bytes"] != h["total"]:
        bad.append("%s: not in sets/sets.json as it is" % f.name)
    if f.name not in readme:
        bad.append("%s: not in sets/README.md" % f.name)
    print("  ok %s: \"%s\", %d pieces (%s), format %d, %d bytes" % (f.name, name, len(pieces), ", ".join(
        D.name(p["name"]) for p in pieces), h["format"], h["total"]))
    lines += ["setin %s" % f, "expect setlen %d" % (len(pieces) % 8), "expect piece 0", "workcheck"]
    lines += ["expectname %d %s" % (i, D.name(p["name"])) for i, p in enumerate(pieces) if D.name(p["name"])]
    cues = {ch["index"]: len(ch["cues"]) for ch in c["chunks"] if ch["id"] == b"CUES"}   # (step 7) its cues: the FM-1's
    lines += ["expect slotcues%d %d" % (i, cues.get(i, 0)) for i in range(len(pieces))]  # (each checked: cue_ok)
    if cues:
        print("     its cues: %s" % ", ".join("piece %d %d" % (i + 1, cues[i]) for i in sorted(cues)))
for x in sorted(set(listed) - {f.name for f in files}):
    bad.append("sets/sets.json lists %s: not there" % x)
for b in bad:
    print("FAIL " + b)
open(sys.argv[1], "w").write("\n".join(lines) + "\n")
sys.exit(1 if bad else 0)
PYEOF
build/host/amb_host "$OUT/load.amb" "$OUT" > "$OUT/load.log" || { grep -E 'FAIL|^line' "$OUT/load.log"; exit 1; }
grep -E 'ok setin' "$OUT/load.log"
