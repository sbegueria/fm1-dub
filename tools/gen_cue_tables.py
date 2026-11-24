#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""(1.5.0, step 7b) The Sets page's cue editor's tables, from the firmware itself: web/dub1cuetables.js.

The host simulator (build/host/amb_host, host/build_host.sh) writes them ("cuetables FILE": the project format, each
parameter's name, range, the screen's text for its values, its pages and knob labels, whether a cue may ride it; the
sirens' knobs, the master actions, the grid's effects, PERFORM's cards, the recipes' song lengths), so the page checks a
cue exactly as the FM-1 does (project.c's cue_ok) and names it as the FM-1's screen and the manual do.

  python3 tools/gen_cue_tables.py           write web/dub1cuetables.js
  python3 tools/gen_cue_tables.py --check   the file as the firmware would write it now (the tests: else regenerate)
"""
import json
import os
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "web", "dub1cuetables.js")
HOST = os.path.join(ROOT, "build", "host", "amb_host")

HEAD = """// SPDX-License-Identifier: GPL-3.0-only
// (1.5.0, step 7b) What the cue editor (dub1cues.js) knows of the firmware: written by tools/gen_cue_tables.py from the
// host simulator ("cuetables"), the firmware's own tables: do not edit (the tests check it is the firmware's).
//   format, npar, maxcue (a piece's cues at most), ops (app.h's CUE_* in order), params [{ i, name, lo, hi, def, cue (a
//   cue may ride it), at [[page, knob label, help]], text (the screen's text for lo..hi, or null: the number) }],
//   sirens [{ name, card, tap, held, knobs [{ name, lo, hi, def, text }] }], macros [{ name, lo, hi, text }], fx (the
//   grid's names), perffx (PERFORM's four), moves [[card, what]] (EN_PF_* bits), parts, recipes [{ name, song [intro,
//   main, outro] (phrases) }], smodes, song [P_SMODE, P_SINTRO, P_SMAIN, P_SOUTRO, P_TEMPO]
"""


def tables():
    if not os.path.exists(HOST):
        sys.exit("no build/host/amb_host: run host/build_host.sh")
    with tempfile.TemporaryDirectory() as d:
        js, sc = os.path.join(d, "t.json"), os.path.join(d, "t.amb")
        with open(sc, "w") as f:
            f.write("cuetables %s\n" % js)
        subprocess.run([HOST, sc, d], check=True, stdout=subprocess.DEVNULL)
        with open(js) as f:
            return json.load(f)


def render(t):
    lines = []
    for k, v in t.items():
        if isinstance(v, list) and v and isinstance(v[0], dict):
            body = ",\n".join("    " + json.dumps(x, ensure_ascii=False, separators=(",", ":")) for x in v)
            lines.append("  %s: [\n%s,\n  ]" % (k, body))
        else:
            lines.append("  %s: %s" % (k, json.dumps(v, ensure_ascii=False, separators=(",", ":"))))
    return HEAD + "export const T = {\n" + ",\n".join(lines) + ",\n};\n"


def main():
    text = render(tables())
    if "--check" in sys.argv[1:]:
        with open(OUT, encoding="utf-8") as f:
            if f.read() != text:
                sys.exit("web/dub1cuetables.js is not the firmware's: run python3 tools/gen_cue_tables.py")
        print("web/dub1cuetables.js: the firmware's")
        return
    with open(OUT, "w", encoding="utf-8") as f:
        f.write(text)
    print("wrote", OUT, len(text), "bytes")


if __name__ == "__main__":
    main()
