#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# The ready-made .dub1 files for the 1.5.0 beta's device test (build/design/BETA-1.5.0-HOWTO.txt), in build/beta-sets/:
# three sets the host simulator makes from the recipes (tools/make_beta_sets.amb: five-echospace, eight-full,
# three-noglob), a whole backup holding only sets 7 and 8 (backup-sets-7-8: a restore that leaves the pieces alone) and
# a set whose name is too wide for the FM-1's header (too-wide-name: "Woodward Dub", 92 px: the page and the FM-1
# refuse it). The simulator is built as the beta names itself (its files say "1.5.0-BETA1").
#   tools/make_beta_sets.sh
set -e
cd "$(dirname "$0")/.."
PY="${PYTHON:-python3}"
OUT=build/beta-sets
mkdir -p build/host build/gen "$OUT"
[ -f build/gen/felucca_font.h ] || "$PY" tools/gen_font.py build/gen/felucca_font.h >/dev/null
"$PY" tools/gen_words.py firmware/words.tsv web/words build/gen/ann_words.h >/dev/null
"$PY" tools/gen_sets.py sets build/gen/first_sets.h >/dev/null   # (1.5.0: the sets the FM-1 comes with)
${CC:-cc} -O2 -ffp-contract=off -std=c99 -w -D_DEFAULT_SOURCE -DEN_HOST '-DEN_VERSION="1.5.0-BETA1"' -Ifirmware/src \
    -Ifirmware/src/dsp -Ibuild/gen -o build/host/amb_host_beta host/amb_host.c host/stems.c firmware/src/dsp/amb.c -lm
mkdir -p build/scenarios/make_beta_sets
build/host/amb_host_beta tools/make_beta_sets.amb build/scenarios/make_beta_sets >/dev/null
"$PY" - "$OUT" <<'EOF'
import sys
sys.path.insert(0, "tools")
import dub1_container as D
out = sys.argv[1]
c = D.parse(open(out + "/five-echospace.dub1", "rb").read())
for ch in c["chunks"]:
    if ch["id"] == b"META":
        ch["name"] = b"Woodward Dub".ljust(16, b"\0")   # (92 px: past the header's 87, written raw, not through name_field)
open(out + "/too-wide-name.dub1", "wb").write(D.build(c))
sets = [(2, 6, open(out + "/five-echospace.dub1", "rb").read()), (2, 7, open(out + "/eight-full.dub1", "rb").read())]
open(out + "/backup-sets-7-8.dub1", "wb").write(D.bundle(sets, "1.5.0-BETA1"))   # (sets 7 and 8, no pieces)
EOF
for f in "$OUT"/*.dub1; do
    echo "$f: $(wc -c < "$f" | tr -d ' ') bytes"
done
