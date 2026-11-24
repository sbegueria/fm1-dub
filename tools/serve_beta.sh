#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# The Sets page and the emulator on this computer, for a beta's device test (build/design/BETA-1.5.0-HOWTO.txt):
# tools/serve_beta.py on 127.0.0.1 only (Chrome allows Web MIDI with SysEx there, as on https; it still asks), laid out
# as the site: the page, its ready-made sets beside it (sets/), the emulator at /emu/ (build/emu); no-store, every file.
#   tools/serve_beta.sh [PORT]        (PORT the Sets page's, 8000; the emulator alone on PORT + 1)
# then, in Chrome:  http://127.0.0.1:8000/dub1_sets.html        (?lang=ja: in Japanese)
#                   http://127.0.0.1:8001/                      (the emulator; built first if build/emu has none:
#                                                                web/emu/build.sh, which needs emsdk's emcc)
# Ctrl-C stops both. Nothing is published.
set -e
cd "$(dirname "$0")/.."
PORT="${1:-8000}"
PY="${PYTHON:-python3}"
[ -x .venv/bin/python ] && [ -z "${PYTHON:-}" ] && PY="$PWD/.venv/bin/python"   # (any python3: serve_beta.py needs no 3.7)
if [ ! -f build/emu/amb.wasm ]; then
    if command -v emcc >/dev/null 2>&1; then
        sh web/emu/build.sh
    else
        echo "serve_beta: no build/emu (and no emcc to build it): the Sets page only"
    fi
fi
exec "$PY" tools/serve_beta.py "$PORT"
