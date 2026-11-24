#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# (1.5.0, step 7) The compiler from film scripts (tools/cues_film.py), end to end on a short one (tests/host/cues_film.amb):
# the set made from it (its setup, `setfile`), its moves recorded and compiled into the set's cues, which the simulator
# loads; then the set played from its file, its cues firing: every move of the film happens in the set's run, on the
# same semicorchea (16th) (one at most apart: the film's keys fall between the steps), none more, none less; the move
# made after the music stopped not mapped (said); the rides (the DJ filter, a knob) the film's value at almost every
# semicorchea (16th), two apart at most. (The Winter Set's own, from tests/film/21-set.amb: tools/make_sets.sh.)
#   sh tests/host/cues_film_test.sh
set -eu
cd "$(dirname "$0")/../.."
PY="${PYTHON:-python3}"
OUT=build/host/cues_film
HOST=build/host/amb_host_cues   # (the cues played: DUB1_CUES=1, host/build_host.sh)
rm -rf "$OUT"
mkdir -p "$OUT"
[ -x "$HOST" ] || sh host/build_host.sh
{
    echo "include tests/host/cues_film.amb splash"
    echo "setfile $OUT/film.dub1 6,7 Film Test"
} > "$OUT/make.amb"
"$HOST" "$OUT/make.amb" "$OUT" > "$OUT/make.log" || { tail "$OUT/make.log"; exit 1; }
grep -q '^  ok setfile' "$OUT/make.log" || { tail "$OUT/make.log"; exit 1; }
"$PY" tools/cues_film.py compile tests/host/cues_film.amb "$OUT/film.dub1" 6,7 --work "$OUT/rec" --host "$HOST" \
    > "$OUT/compile.log"
cat "$OUT/compile.log"
grep -q 'not mapped' "$OUT/compile.log" || { echo "FAIL: the move after the music stopped not reported"; exit 1; }
[ "$(sed -n '/not mapped/,$p' "$OUT/compile.log" | grep -c 'after its song ended')" = 1 ] ||
    { echo "FAIL: not one move left out (the one after the stop)"; exit 1; }
{
    echo "wait 1500"
    echo "tap PLAY"
    echo "wait 300"
    echo "expect playing 0"
    echo "setin $OUT/film.dub1"
    echo "expect slotcues0 10"
    echo "expect slotcues1 3"
    echo "cueson 1"
    echo "cuerec $OUT/set-events.txt"
    echo "tap PLAY"
    echo "wait 240000"
    echo "expect playing 0"
    echo "cuerec off"
} > "$OUT/play.amb"
"$HOST" "$OUT/play.amb" "$OUT" > "$OUT/play.log" || { grep 'FAIL\|^line' "$OUT/play.log"; exit 1; }
"$PY" tools/cues_film.py compare "$OUT/rec/film-events.txt" "$OUT/set-events.txt" --slots 6,7 --strict 1
echo "cues-film: the film's moves compiled, played from the set on the same semicorcheas (16ths)"
