#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# (1.5.0, step 6) The ready-made sets in sets/ (public: for anyone to download, on the site's Sets page and in the
# emulator's "Hear a set"), each made by the host simulator from the scenario of its video, so the video's set and the
# downloadable one are the same pieces (SETFILES.md piece 6): the scenario run up to its film (`include FILE splash`:
# its setup, off camera, the film's first command not run), each piece named as the video's chapters name it, then
# `setfile`: those pieces, in the video's order, as one SET (slots 1..n, as the FM-1 will hold it) with the scenario's
# Song mode, Transition, the sirens' knobs and Tune. The scenarios are read only (tests/film).
# (step 7) A set marked so gets its video's performer moves as cues: tools/cues_film.py runs the script (its film left
# out) recording every move, compiles them into each piece's cues and writes them into the set (CUES chunks).
# Each set is then checked, in WORK (default build/sets-work; the heavy part: the renders):
#   round trip  a fresh simulator loads the file as the FM-1 loads a set (`setin`) and writes its pieces out: each the
#               same as the scenario's own (every value, the ping loops, the notes, the recipe, the name, the view),
#               and Song mode, Transition, the sirens' knobs and Tune the scenario's; (step 7) its cues WORK's, the same
#   sound       the first SECS (30) seconds of each piece, rendered twice by fresh simulators running the same commands
#               at the same times: once from the scenario's pieces (its WORK, written at the film's start: as a fresh
#               FM-1 would boot with them), once from the set (its cues off: the pieces alone): bit-identical, or the
#               script fails
# The simulator is built with EN_VERSION 1.5.0 (the files' META says which DUB-1 made them). sets/sets.json lists the
# sets for the pages (tools/make_pages.py); sets/README.md describes them.
#   tools/make_sets.sh                 WORK=dir  SECS=n  NOCHECK=1 (the files only)
set -e
cd "$(dirname "$0")/.."
PY="${PYTHON:-.venv/bin/python3}"
WORK="${WORK:-build/sets-work}"
SECS="${SECS:-30}"
HOST=build/host/amb_host_sets
mkdir -p build/host build/gen sets "$WORK"
[ -f build/gen/felucca_font.h ] || "$PY" tools/gen_font.py build/gen/felucca_font.h >/dev/null
"$PY" tools/gen_words.py firmware/words.tsv web/words build/gen/ann_words.h >/dev/null
"$PY" tools/gen_sets.py sets build/gen/first_sets.h >/dev/null   # (1.5.0: the sets the FM-1 comes with)
${CC:-cc} -O2 -ffp-contract=off -std=c99 -w -D_DEFAULT_SOURCE -DEN_HOST -DDUB1_CUES=1 '-DEN_VERSION="1.5.0"' -Ifirmware/src \
    -Ifirmware/src/dsp -Ibuild/gen -o "$HOST" host/amb_host.c host/stems.c firmware/src/dsp/amb.c -lm

# one set: KEY FILE SCRIPT SLOTS "SET NAME" "PIECE|PIECE|.." [cues]
#   SLOTS: the scenario's pieces (0..7), in the set's order; cues: the script's moves compiled into the set's cues
make() {
    key=$1 file=$2 script=$3 slots=$4 sname=$5 pnames=$6 withcues=${7:-}
    w="$WORK/$key"
    mkdir -p "$w"
    {
        echo "# (made by tools/make_sets.sh) $sname from $script"
        echo "include $script splash"
        i=0
        echo "$slots" | tr ',' '\n' | while read -r s; do
            n=$(echo "$pnames" | cut -d'|' -f$((i + 1)))
            echo "name $s $n"
            i=$((i + 1))
        done
        echo "setfile sets/$file $slots $sname"
        echo "dub1out $w/scenario-work.dub1"
    } > "$w/make.amb"
    "$HOST" "$w/make.amb" "$w" > "$w/make.log" || true
    if grep -q '^FAIL\|^line ' "$w/make.log" || ! grep -q '^  ok setfile' "$w/make.log"; then
        grep -E '^FAIL|^line |setfile' "$w/make.log" | head -20
        echo "$key: the scenario's expectations or the set failed (see $w/make.log)"
        [ -n "$LENIENT" ] || exit 1
    fi
    grep 'ok setfile' "$w/make.log"
    if [ -n "$withcues" ]; then
        "$PY" tools/cues_film.py compile "$script" "sets/$file" "$slots" --work "$w/cues" --host "$HOST" > "$w/cues.log" ||
            { cat "$w/cues.log"; exit 1; }
        head -1 "$w/cues.log" | sed 's/^/  ok cues: /'
        if grep -q 'not mapped' "$w/cues.log"; then
            sed -n '/not mapped/,$p' "$w/cues.log"
        fi
    fi
    [ -n "$NOCHECK" ] && return 0
    # the same commands at the same times, from the scenario's pieces (its WORK) and from the set
    for from in scenario set; do
        {
            echo "wait 600"
            echo "tap PLAY"
            echo "wait 300"
            echo "expect playing 0"
            if [ $from = scenario ]; then
                echo "dub1in $w/scenario-work.dub1"
                list=$(echo "$slots" | tr ',' ' ')
            else
                echo "setin sets/$file"
                echo "cueson 0"
                echo "dub1out $w/set-work.dub1"
                list=$(echo "$slots" | tr ',' '\n' | awk '{ printf "%d ", NR - 1 }')
            fi
            k=0
            for p in $list; do
                echo "piece $p"
                echo "wait 800"
                echo "wav $from-$k.wav"
                echo "tap PLAY"
                echo "wait $((SECS * 1000))"
                echo "wavstop"
                echo "tap PLAY"
                echo "wait 3000"
                k=$((k + 1))
            done
        } > "$w/hear-$from.amb"
        "$HOST" "$w/hear-$from.amb" "$w" > "$w/hear-$from.log" || true
        if grep -q '^FAIL\|^line ' "$w/hear-$from.log"; then
            grep -E '^FAIL|^line ' "$w/hear-$from.log" | head
            exit 1
        fi
    done
    "$PY" - "$w" "sets/$file" "$slots" <<'EOF'
import sys
sys.path.insert(0, "tools")
import dub1_container as D
w, f, slots = sys.argv[1], sys.argv[2], [int(x) for x in sys.argv[3].split(",")]
P_TUNE, P_SMODE = 16, 158                        # (amb.h's enum: a set's GLOB gives these two)
def chunks(p, cid):
    return [c for c in D.parse(open(p, "rb").read())["chunks"] if c["id"] == cid]
scen, got = chunks(w + "/scenario-work.dub1", b"PIEC"), chunks(w + "/set-work.dub1", b"PIEC")
sg, gg = chunks(w + "/scenario-work.dub1", b"GLOB")[0], chunks(w + "/set-work.dub1", b"GLOB")[0]
GLOBAL = (P_TUNE, 17, P_SMODE)                   # (Tune, MIDI out, Song mode: the project's, a slot's copy unread)
bad, fresh = [], 0
for i, s in enumerate(slots):
    a, b = scen[s], got[i]
    # (a piece left as its recipe the FM-1 takes as today's recipe, its loops grown afresh: project.c untouched_now)
    afresh = a["pings_ok"] and not b["pings_ok"]
    fresh += bool(afresh)
    for k in ("kind", "recipe", "screen", "flags", "nparams", "pool", "seed", "name") + \
            (() if afresh else ("pings_ok", "pings")):
        if a[k] != b[k]:
            bad.append("piece %d (the scenario's %d): %s differs" % (i + 1, s + 1, k))
    if [v for j, v in enumerate(a["par"]) if j not in GLOBAL] != [v for j, v in enumerate(b["par"]) if j not in GLOBAL]:
        bad.append("piece %d (the scenario's %d): its values differ" % (i + 1, s + 1))
for k in ("trans", "sir"):
    if sg[k] != gg[k]:
        bad.append("the set's %s differs" % k)
for k, n in ((P_TUNE, "Tune"), (P_SMODE, "Song mode")):
    if sg["par"][k] != gg["par"][k]:
        bad.append("%s differs" % n)
# (step 7) the set's cues: WORK's after the set is loaded, the same lists for the same pieces
sc = {c["index"]: c["cues"] for c in chunks(f, b"CUES")}
wc = {c["index"]: c["cues"] for c in chunks(w + "/set-work.dub1", b"CUES")}
if sc != wc:
    bad.append("the cues differ (set %s, WORK %s)" % (sorted(sc), sorted(wc)))
if bad:
    print("FAIL round trip: " + "; ".join(bad))
    sys.exit(1)
print("  ok round trip: %d pieces as the scenario's (values, loops, notes, recipe, name, view), its Song mode %d, "
      "Transition %d, the sirens, Tune %d%s" % (len(slots), sg["par"][P_SMODE], sg["trans"], sg["par"][P_TUNE],
      " (%d as its recipe: its loops grown afresh at load, as on the FM-1)" % fresh if fresh else ""))
if sc:
    print("  ok cues round trip: %d cues (%s), WORK's after the set loaded the same" % (
        sum(len(v) for v in sc.values()), ", ".join("piece %d %d" % (i + 1, len(sc[i])) for i in sorted(sc))))
EOF
    k=0
    for s in $(echo "$slots" | tr ',' ' '); do
        if cmp -s "$w/scenario-$k.wav" "$w/set-$k.wav"; then
            echo "  ok sound: piece $((k + 1)) (the scenario's $((s + 1))), ${SECS} s: bit-identical ($(wc -c < "$w/set-$k.wav" | tr -d ' ') bytes)"
        else
            echo "FAIL sound: piece $((k + 1)) (the scenario's $((s + 1))) differs"
            cmp "$w/scenario-$k.wav" "$w/set-$k.wav" | head -1
            exit 1
        fi
        k=$((k + 1))
    done
}

# the second hour (Winter Set, tests/film/21-set.amb, 1.4.0's video): its five songs, slots 4-8 in the film
make winter winter-set.dub1 tests/film/21-set.amb 3,4,5,6,7 "Winter Set" "Hoarfrost|Black Ice|Snowfield|Aurora|Thaw" cues
# the first hour (4 a.m. to 5 a.m., tests/film/20-night.amb, 1.1.0's video): its four pieces in the night's order,
# today's recipes (the film's own were 1.1.0's)
make night four-to-five.dub1 tests/film/20-night.amb 5,6,3,7 "4 to 5 a.m." "Night Drive|Pressure|Fog|Haze"

"$PY" - <<'EOF'
import json, sys
sys.path.insert(0, "tools")
import dub1_container as D
SETS = [   # (file, what it is): the pages' list, in this order
    ("winter-set.dub1", "Five songs after DeepChord presents Echospace, as in the hour-long Winter Set video: Set mode, "
                        "Transition Mix (each song's outro mixed into the next)."),   # (its cues kept in the file, not
                                                   # played by 1.5.0: not said)
    ("four-to-five.dub1", "The four pieces of the hour-long night video (4 a.m. to 5 a.m.), in its order, as DUB-1 1.5 "
                          "plays them (the video was 1.1.0's); Endless: change pieces by hand, as the video does."),
]
out = []
for f, blurb in SETS:
    c = D.parse(open("sets/" + f, "rb").read())
    meta = next(ch for ch in c["chunks"] if ch["id"] == b"META")
    pieces = [D.name(ch["name"]) for ch in c["chunks"] if ch["id"] == b"PIEC"]
    out.append(dict(file=f, name=D.name(meta["name"]), pieces=pieces, bytes=c["header"]["total"],
                    format=c["header"]["format"], made=D.name(meta["fw"]), about=blurb))
open("sets/sets.json", "w").write(json.dumps(out, indent=1, ensure_ascii=False) + "\n")
print("sets/sets.json: " + ", ".join("%s (%s)" % (o["file"], o["name"]) for o in out))
EOF
