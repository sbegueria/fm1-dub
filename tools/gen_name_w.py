#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""(1.5.0, STORAGE-1.5.0.md Decisions 2) a name's width on the device, the same everywhere: the header's font (FONT_B)
advance by character, the digits at the widest digit's (tabular: a build with libraqm gives the digits one width, one
without it their own; the widest holds for both), as one table in the firmware (firmware/src/app/names.h) and in its
twins (tools/dub1_container.py, web/dub1container.js), so a name the web or a script accepts is never cut on the device.
NAME_PX, the widest a name may be, is the header's (ui.c's draw_header; the host's "namecheck" works it out again).

  tools/gen_name_w.py [build/gen/felucca_font.h]           the table, from the font, to stdout
  tools/gen_name_w.py --check [build/gen/felucca_font.h]   the three copies the same as the font gives (0: so)"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
NAME_PX = 87
NAME_CHARS = 12
COPIES = [ROOT / "firmware/src/app/names.h", ROOT / "tools/dub1_container.py", ROOT / "web/dub1container.js"]


def table(font_h):
    src = Path(font_h).read_text()
    adv = [int(x) for x in re.search(r"FONT_B_ADV\[95\] = \{([^}]*)\}", src).group(1).split(",")]
    d = max(adv[16:26])
    return [d if 16 <= i < 26 else a for i, a in enumerate(adv)]


def line(t):
    return "NAME_W = " + ",".join(str(x) for x in t)


def found(path):
    """the table and NAME_PX as a copy holds them (between its NAME_W markers)"""
    s = path.read_text()
    m = re.search(r"NAME_W(?:\[\d+\])?\s*=\s*[{\[]([0-9,\s]+)[}\]]", s)
    px = re.search(r"NAME_PX\s*=?\s*(\d+)", s)
    return ([int(x) for x in m.group(1).replace("\n", " ").split(",") if x.strip()] if m else None,
            int(px.group(1)) if px else None)


def main(argv):
    check = "--check" in argv
    args = [a for a in argv[1:] if a != "--check"]
    font = args[0] if args else ROOT / "build/gen/felucca_font.h"
    t = table(font)
    if not check:
        print(line(t))
        return 0
    bad = 0
    for p in COPIES:
        got, px = found(p)
        if got != t or px != NAME_PX:
            print("%s: NAME_W %s, NAME_PX %s (the font's: %s, %d)" % (p.relative_to(ROOT), got, px, t, NAME_PX))
            bad = 1
    return bad


if __name__ == "__main__":
    sys.exit(main(sys.argv))
