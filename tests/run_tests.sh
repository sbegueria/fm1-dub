#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# AMB-1 host tests: the maths, the platform pieces kept from Felucca / X0X, the instrument, and the
# whole firmware app in the simulator (tests/scenarios/*.amb: UI, chords, rhythm, persistence), with
# screenshots and audio in build/scenarios/.
#   tests/run_tests.sh
set -u
cd "$(dirname "$0")/.."
CC="${CC:-cc}"
FAIL=0
OUT=build/host
mkdir -p "$OUT" build/scenarios
run() {
    name="$1"; shift
    if "$@" > "$OUT/$name.log" 2>&1; then
        echo "  ok   $name"
    else
        echo "  FAIL $name (see $OUT/$name.log)"
        tail -15 "$OUT/$name.log" | sed 's/^/       /'
        FAIL=1
    fi
}
run fastmath sh -c "$CC -O2 -ffp-contract=off -Wall -Wextra -Werror -o $OUT/fastmath_test tests/host/fastmath_test.c -lm && $OUT/fastmath_test"
run encoder sh -c "$CC -O2 -w -Ifirmware/hal -o $OUT/encoder_test tests/host/encoder_test.c && $OUT/encoder_test"
run uac sh -c "$CC -O2 -w -Ifirmware/src -Ifirmware/hal -o $OUT/uac_test tests/host/uac_test.c -lm && $OUT/uac_test"
run trs sh -c "$CC -O2 -w -Ifirmware/src -Ifirmware/hal -o $OUT/trs_test tests/host/trs_test.c && $OUT/trs_test"
run amb sh -c "$CC -O2 -ffp-contract=off -std=c99 -Wall -Wextra -Wdouble-promotion -Werror -DEN_HOST -Ifirmware/src/dsp \
    -o $OUT/amb_test tests/host/amb_test.c firmware/src/dsp/amb.c -lm && $OUT/amb_test"
run storage sh -c "$CC -O2 -o $OUT/storage_test tests/storage_test.c && $OUT/storage_test"
# (1.5.0) the container's codec alone, with ASan and UBSan where the compiler has them (its fuzz), and its twins
run container sh -c "{ $CC -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all -o $OUT/container_test \
    tests/host/container_test.c 2>/dev/null || $CC -O2 -o $OUT/container_test tests/host/container_test.c; } && $OUT/container_test"
run container-twins sh tests/host/container_twins.sh
# (1.5.0, step 4) the set protocol (setwire.c) alone, on the real frame store and codec, under ASan and UBSan where there
run set-wire sh -c "{ $CC -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all -o $OUT/set_wire_test \
    tests/host/set_wire_test.c 2>/dev/null || $CC -O2 -o $OUT/set_wire_test tests/host/set_wire_test.c; } && $OUT/set_wire_test"
# the update path, against the firmware package (Felucca's tests; needs ./build.sh)
if [ -f build/amb.fwsc ]; then
    run ota-entry sh -c "$CC -o $OUT/ota_test tests/ota_test.c && $OUT/ota_test build/amb.fwsc"
    if [ -z "${AC79_SDK:-}" ]; then
        echo "  skip update-loader (needs AC79_SDK, as the build)"
    else
    run update-loader sh -c "head -c 100000 build/amb.bin > $OUT/old_app.bin && \
        python3 tools/fm1pkg_make.py $OUT/old_app.bin build/loader/ota.bin $OUT/old.fwsc >/dev/null && \
        $CC -o $OUT/ldr_test tests/ldr_test.c && $OUT/ldr_test $OUT/old.fwsc build/amb.fwsc"
    fi
    run installer python3 tests/install_test.py
else
    echo "  skip update-path tests (no build/amb.fwsc: run ./build.sh)"
fi
run recipe-prints sh -c "\${PYTHON:-python3} tools/gen_recipe_prints.py | cmp - firmware/src/app/recipe_prints.h"   # (the pieces' past recipes: regenerate after changing one)
run host-build sh host/build_host.sh
run cue-tables sh -c "\${PYTHON:-python3} tools/gen_cue_tables.py --check"   # (step 7b) the cue editor's tables the firmware's
run dub1-set sh -c "\${PYTHON:-python3} tests/host/dub1_set_test.py"   # (1.5.0, step 4) tools/dub1_set.py against the simulator
run published-sets sh tests/host/published_sets_test.sh   # (1.5.0, step 6) the ready-made sets in sets/ still load
run storeset-file sh tests/host/storeset_file_test.sh   # (2026-11-23) a set stored by the Set page's Store reads as a set
run cues-film sh tests/host/cues_film_test.sh   # (step 7) a film script's moves compiled into a set's cues, played on its bars
if command -v node >/dev/null 2>&1; then         # (1.5.0, step 5) the Sets page's protocol (web/dub1sets.js) against it
    run dub1-sets node tests/host/dub1_sets_test.mjs
    run cue-editor node tests/host/cue_editor_test.mjs   # (step 7b) the Sets page's cue editor, checked as the firmware checks
    run make-set node tests/host/make_set_test.mjs   # (1.5.0) Make a set (web/dub1makeset.js, dub1_set.py make): which, order, names, cues
    run installer-ota node tests/host/installer_ota_test.mjs   # the web installer's protocol (web/fm1ota.js): stalls, resumes, its log
fi
# (1.5.0) the simulator under ASan and UBSan (the engine's shifts of negatives aside: fastmath's, by design), through
# tests/scenarios/container.amb: every format through project_load and the container, and the project's fuzz
run container-asan sh -c "if $CC -O1 -g -fsanitize=address,undefined -fno-sanitize=shift -fno-sanitize-recover=all \
    -ffp-contract=off -std=c99 -Wno-unused-function -Wno-unused-parameter -DEN_HOST -DDUB1_CUES=1 -Ifirmware/src -Ifirmware/src/dsp \
    -Ibuild/gen -o $OUT/amb_host_asan host/amb_host.c host/stems.c firmware/src/dsp/amb.c -lm 2>/dev/null; then \
    mkdir -p build/scenarios/container-asan build/scenarios/cues-asan && \
    $OUT/amb_host_asan tests/scenarios/container.amb build/scenarios/container-asan && \
    $OUT/amb_host_asan tests/scenarios/cues.amb build/scenarios/cues-asan; \
    else echo 'no sanitizers here: skipped'; fi"
if command -v ffmpeg >/dev/null 2>&1; then   # (the film's stems: the film the same with them, host/stems.c)
    run stems sh tests/host/stems_test.sh
fi
if command -v emcc >/dev/null 2>&1; then
    run emu sh -c "sh web/emu/build.sh >/dev/null && node tests/host/emu_test.mjs build/emu/amb.wasm && \
        node tests/host/emu_cues_test.mjs build/emu/amb.wasm off && \
        DUB1_CUES=1 EMU_OUT=build/emu-cues sh web/emu/build.sh >/dev/null && \
        node tests/host/emu_cues_test.mjs build/emu-cues/amb.wasm"   # (step 7b: an edited set's cues heard on their bars;
                                                 # (1.5.0) the emulator's with them played, the page's has none)
    run app sh -c "sh web/app/build.sh >/dev/null && node tests/host/app_test.mjs build/app/amb.wasm"
    CHROME="${CHROME:-$(command -v chromium || command -v google-chrome || true)}"
    MACCHROME="/Applications/Google Chrome.app/Contents/MacOS/Google Chrome"
    [ -z "$CHROME" ] && [ -x "$MACCHROME" ] && CHROME="$MACCHROME"   # (macOS: Chrome where it installs itself)
    if [ -n "$CHROME" ]; then   # (the page itself, in a headless browser, a fake MIDI controller in it)
        run emu-browser node tests/host/emu_browser_test.mjs build/emu "$CHROME"
        run sets-browser node tests/host/sets_browser_test.mjs "$CHROME"   # (1.5.0, step 5) the Sets page, on the simulator
        run installer-browser node tests/host/installer_browser_test.mjs "$CHROME"   # (Decisions 5) the installer's check
    else
        echo "  skip emu-browser (no chromium: CHROME=)"
    fi
fi
for s in tests/scenarios/*.amb; do
    n=$(basename "$s" .amb)
    mkdir -p "build/scenarios/$n"
    h=build/host/amb_host   # (the release's: DUB1_CUES 0; the cue player's own scenarios on amb_host_cues)
    grep -q '^# (build: DUB1_CUES=1)' "$s" && h=build/host/amb_host_cues
    run "scenario-$n" $h "$s" "build/scenarios/$n"
done
[ $FAIL -eq 0 ] && echo "all tests passed" || echo "TESTS FAILED"
exit $FAIL
