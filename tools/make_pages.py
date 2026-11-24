#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Make the DUB-1 site for GitHub Pages (https://sbegueria.github.io/fm1-dub/):

  index.html                  what DUB-1 is, and the ways in: try, install, manual, download, source
  img/                        screenshots (docs/img)
  emu/                        DUB-1 in the browser (build/emu, from web/emu/build.sh)
  words/                      the screen reader's words files (web/words, from firmware/words.tsv)
  install/index.html          the web installer (web/amb_installer.html, with fm1pkg.js, fm1ota.js and
                              the package's metadata inlined; Chrome or Edge, Web MIDI)
  sets/index.html             (1.5.0) the Sets page: back up, list, send, load, delete, download sets (web/dub1_sets.html,
                              with web/dub1sets.js, web/dub1container.js, (step 7b) dub1cues.js, dub1cuetables.js and dub1makeset.js beside it; Chrome or Edge, Web MIDI)
  sets/*.dub1, sets.json      (1.5.0, step 6) the ready-made sets (sets/, tools/make_sets.sh) to download, send or hear:
                              their list for the Sets page and the emulator's "Hear a set"; README.txt (sets/README.md)
  manual/index.html, img/     the manual (tools/manual_html.py, from docs/MANUAL.md)
  cheatsheets/                the printable cheat sheets (docs/cheatsheets: each page and its PDF), and their list
  firmware/dub-VERSION.fwsc   the package the installer writes; also the download

  tools/make_pages.py build/dub-1.1.0.fwsc 1.1.0 OUT_DIR

The package must be a release build (./build.sh --release X.Y): its identity, FM-1_8XXYYZZ, is what
the installer checks the download against and what the FM-1 reports after the install."""
import html
import json
import re
import shutil
import subprocess
import sys
from pathlib import Path

SRC = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(SRC / "web"))
from make_site import product_of, strip_module  # noqa: E402  (Felucca's: the package format)

REPO = "https://github.com/sbegueria/fm1-dub"

LANDING = """<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>DUB-1 · Dub techno for the FM-1</title>
<meta name="description" content="A generative dub techno machine for the M-VAVE FM-1: chord stabs thrown into a tape echo, a sub bass, a steady kick, vinyl crackle, playing itself, and played live.">
<meta name="theme-color" content="#10141a">
<style>
/* DUB-1's colours, as its screen: a dark room, a cold teal; the layers' four colours for the list */
:root { --ground: #10141a; --card: #1e252f; --ink: #dee4e8; --muted: #9aa8b6; --rule: #2e3846; --teal: #5cc4cc;
        --teal-2: #2b6e72; --chord: #54c8ba; --pings: #80aae0; --bass: #ccb470; --drums: #ec9654; color-scheme: dark; }
body { background: var(--ground); color: var(--ink); margin: 0; padding-inline: 16px; padding-block: 48px 64px;
       font: 400 17px/1.6 "Helvetica Neue", Helvetica, Arial, sans-serif; }
main { max-width: 42rem; margin: 0 auto; }
h1 { font-size: 3.2rem; font-weight: 300; line-height: 1; letter-spacing: .08em; margin: 0 0 .3rem; }
h1 .e1 { color: var(--chord); } h1 .e2 { color: var(--pings); } h1 .e3 { color: var(--bass); } h1 .e4 { color: var(--drums); }
h1 span { font-size: .55em; opacity: .75; margin-left: .25em; }
h1 span + span { font-size: .45em; opacity: .55; } h1 span + span + span { font-size: .38em; opacity: .4; }
h1 span + span + span + span { font-size: .32em; opacity: .28; }
.sub { text-transform: uppercase; letter-spacing: .08em; font-size: .9rem; color: var(--teal); margin: 0 0 1.2rem;
       padding-bottom: .8rem; border-bottom: 1px solid var(--rule); }
h2 { font-size: 1.1rem; font-weight: 400; text-transform: uppercase; letter-spacing: .06em; margin: 2.4rem 0 .6rem;
     padding-top: .6rem; border-top: 1px solid var(--rule); color: var(--teal); }
.lede { font-size: 1.15rem; margin: 0 0 1.6rem; max-width: 36rem; }
.shots { display: grid; grid-template-columns: repeat(auto-fit, minmax(150px, 1fr)); gap: 12px; margin: 0 0 1.8rem; }
.shots img { width: 100%; height: auto; image-rendering: pixelated; border: 1px solid var(--rule); border-radius: 6px; }
.ways { display: grid; gap: 10px; margin: 0 0 1rem; }
.ways a { display: block; text-decoration: none; color: var(--ink); background: var(--card); border: 1px solid var(--rule);
          border-radius: 6px; padding: 12px 16px; }
.ways a:first-child { background: var(--teal-2); border-color: var(--teal); }
.ways a:hover, .ways a:focus-visible { border-color: var(--teal); }
a:focus-visible { outline: 2px solid var(--teal); outline-offset: 2px; }
.ways strong { font-weight: 400; font-size: 1.15rem; text-transform: uppercase; letter-spacing: .05em; display: block; }
.ways span { color: var(--muted); font-size: .95rem; }
.ways em.v { font-style: normal; text-transform: none; letter-spacing: 0; }
ul.layers { padding-left: 1.2rem; margin: 0 0 1rem; }
ul.layers li { margin: 0 0 .45rem; }
ul.layers li.c::marker { color: var(--chord); } ul.layers li.p::marker { color: var(--pings); }
ul.layers li.b::marker { color: var(--bass); } ul.layers li.d::marker { color: var(--drums); }
ul.layers li.s::marker { color: var(--teal); }
p.small { color: var(--muted); font-size: .9rem; }
a { color: var(--ink); text-decoration-color: var(--teal); text-decoration-thickness: 2px; text-underline-offset: 3px; }
code { font-size: .9em; }
</style>
</head>
<body>
<main>
<h1 aria-label="DUB-1">DUB-1<span class="e1" aria-hidden="true">DUB-1</span><span class="e2" aria-hidden="true">DUB-1</span><span class="e3" aria-hidden="true">DUB-1</span><span class="e4" aria-hidden="true">DUB-1</span></h1>
<p class="sub">Dub techno for the M-VAVE FM-1</p>
<p class="lede">A generative dub techno machine. It plays itself from the moment it's switched on: chord stabs thrown
into a tape echo, a sub bass, a steady kick, vinyl crackle, eight-bar phrases that drop the kick for a break and bring
it back with a fill. Leave it running for hours, or play it: the mix and dub moves on the keys, sixteen effects on REC,
your own chords and drum patterns.</p>
<div class="shots">
  <img src="img/screen-launch.png" width="240" height="240" alt="DUB-1's launch screen: the name and its echoes in the layers' colours">
  <img src="img/screen-main.png" width="240" height="240" alt="DUB-1's main screen: the stabs' ripples round the chord Fm7, the layers' meters, the lanes of the bar">
  <img src="img/screen-perform.png" width="240" height="240" alt="The Arrangement page: a desk's meters, a row a part and the master, the duck under it">
</div>
<nav class="ways" aria-label="Get DUB-1">
  <a href="emu/"><strong>Try it in the browser</strong><span>The same code the FM-1 runs, with sound. Mouse, touch or keyboard; no FM-1 needed.</span></a>
  <a href="install/"><strong>Install <em class="v">v__VERSION__</em></strong><span>From Chrome or Edge, with the FM-1 connected by USB. Nothing to install on the computer.</span></a>
  <a href="sets/"><strong>Sets</strong><span>Back up your pieces and sets, send a set to the FM-1, load it, make your own. Chrome or Edge, DUB-1 1.5 or later.</span></a>
  <a href="manual/"><strong>Manual</strong><span>Getting started in a few minutes, then everything else.</span></a>
  <a href="cheatsheets/"><strong>Cheat sheets</strong><span>One A4 page each: the panel, every layer, the arrangement, songs and sets. To print and keep by the FM-1.</span></a>
  <a href="firmware/__PKG__"><strong>Download <em class="v">__PKG__</em></strong><span>For the command-line installer: <code>python3 tools/fm1_install.py __PKG__</code></span></a>
  <a href="__REPO__"><strong>Source</strong><span>GitHub, GPL-3.0. A fork of AMB-1 by Charles Vestal, built on Felucca by Hügelton Instruments.</span></a>
</nav>

<h2>What it plays</h2>
<ul class="layers">
  <li class="c"><b>Chord.</b> Off-beat stabs through a resonant filter, mostly into the echo: saws, keys, two organs, a pad, a bell, noise. Its own progression if you write one, or chords moving by themselves.</li>
  <li class="p"><b>Pings.</b> Short FM blips on four loops of unequal length, sliding against each other.</li>
  <li class="b"><b>Bass.</b> A sub on five grooves, from a drone to a roller; FM basses with bite.</li>
  <li class="d"><b>Drums.</b> A kick tuned to the key ducking everything else, a shaker, an off-beat hat, a rim and a rare clap thrown into the echo and the plate; your own patterns if you like.</li>
  <li class="s"><b>Space.</b> A tape echo that can feed back past 100%, a granular cloud, a plate reverb, vinyl crackle. REC tapped holds the echo; held, sixteen effects on the keys.</li>
</ul>
<p>Eight pieces, each remembering how you left it; a voice for each layer on ALGORITHM, two more pages of
sound under each layer's button, an FM page with the algorithm drawn. The screen shows the echo: each stab
a ring, its repeats following.</p>
<p>Or play it live: a DJ filter on SELECT, on every page; the Arrangement page (SEL), a dub desk, mutes and solos
the layers on the keys, throws them into the echo, opens, shuts and drops the floor, and plays four effects and the
dub sirens. A piece can play as a song, an intro, the main and an outro, and the pieces as a set, one into the next.
Keep your own sets on the FM-1: it comes with two.</p>
<p class="small">To go back to the official firmware, use the installer's "Back to the stock firmware", or
M-VAVE's updater, M-UPGRADE, from <a href="https://www.m-vave.com/download">m-vave.com/download</a>.
DUB-1 is a fork of <a href="https://github.com/charlesvestal/fm1-amb">AMB-1</a> by Charles Vestal; its granular
cloud follows JIANT's; the platform is Felucca by Leo Kuroshita (Hügelton Instruments). GPL-3.0.
M-VAVE and FM-1 are trademarks of their owners. DUB-1 is not affiliated with M-VAVE or Hügelton Instruments.</p>
</main>
</body>
</html>
"""


# the cheat sheets, in the order they're read (docs/cheatsheets: NAME.html and NAME.pdf), each with its line
SHEETS = [
    ("panel", "The panel", "Every button, knob and key, and what holding one does."),
    ("arrangement", "Arrangement", "The dub desk: the meters, the parts' strips, the throws, the moves, the sirens."),
    ("chords", "Chords", "The eight slots, the walk, Endless and Loop, the Walk page."),
    ("chord-voice", "Chord voice", "The stab's sound: its envelope, colour, FM and LFO."),
    ("pings", "Pings", "The pings' loops, octave, wet and cloud."),
    ("bass", "Bass", "The bass's density, octave, groove and fifth, and its voice."),
    ("drums", "Drums", "The kick's shapes, the hits, the hats, the rumble; the pattern page."),
    ("noise", "Noise", "The noise machine: its density, air, tone and decay, its width and sends."),
    ("space", "The space", "The echo, the plate and the width."),
    ("effects", "Effects grid", "The sixteen effects on REC."),
    ("variety", "Variety", "How the music changes: Vary, Motion, Drift, Energy, the LFO pages."),
    ("songs", "Songs and sets", "Endless, Song and Set; the intro and outro; Transition."),
    ("pieces", "Pieces", "The eight pieces, each after a record."),
    ("screen", "The screen", "Everything the screen shows, page by page."),
]

CHEATS = """<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>DUB-1 · Cheat sheets</title>
<meta name="description" content="DUB-1's cheat sheets: one printable A4 page for each part of the machine.">
<meta name="theme-color" content="#10141a">
__STYLE__
</head>
<body>
<main>
<p class="sub"><a href="../">DUB-1</a> · Dub techno for the M-VAVE FM-1</p>
<h1 class="sheets">Cheat sheets</h1>
<p class="lede">One A4 page each, for DUB-1 __VERSION__. Open a sheet to read it on screen, or take its PDF to print.</p>
<nav class="ways" aria-label="The cheat sheets">
__LIST__
</nav>
<p class="small">The manual has the details: <a href="../manual/">the manual</a>.</p>
</main>
</body>
</html>
"""


def cheatsheets(version, out):
    """the sheets (each page with its sheet.css, and its PDF) and their list"""
    src = SRC / "docs" / "cheatsheets"
    shutil.copytree(src, out, ignore=shutil.ignore_patterns(".*"))
    rows = []
    for name, title, line in SHEETS:
        for ext in ("html", "pdf"):
            if not (src / f"{name}.{ext}").exists():
                raise SystemExit(f"no docs/cheatsheets/{name}.{ext}: run tools/make_cheatsheets.sh")
        rows.append(f'  <a href="{name}.html"><strong>{html.escape(title)}</strong><span>{html.escape(line)}'
                    f'</span></a><a class="pdf" href="{name}.pdf" aria-label="{html.escape(title)}, PDF">PDF</a>')
    style = re.search(r"<style>.*?</style>", LANDING, re.S).group(0).replace(
        "</style>", ".ways { grid-template-columns: minmax(0, 1fr) auto; }\n.ways a.pdf { display: flex; align-items: center; "
        "background: var(--card); border-color: var(--rule); }\n.ways a:first-child { background: var(--card); "
        "border-color: var(--rule); }\nh1.sheets { font-size: 2.2rem; letter-spacing: .04em; margin: 0 0 1rem; }\n"
        ".sub a { color: var(--teal); }\n</style>")
    (out / "index.html").write_text(CHEATS.replace("__STYLE__", style).replace("__VERSION__", html.escape(version))
                                    .replace("__LIST__", "\n".join(rows)), encoding="utf-8")


def ready_sets(out):
    """(1.5.0, step 6) the ready-made sets beside the Sets page: each file sets/sets.json lists, read first (the Python
    twin: a SET, its CRC, its name and pieces as listed), with the list and the README"""
    sys.path.insert(0, str(SRC / "tools"))
    import dub1_container as D
    listed = json.loads((SRC / "sets" / "sets.json").read_text(encoding="utf-8"))
    for o in listed:
        c = D.parse((SRC / "sets" / o["file"]).read_bytes())
        meta = next(ch for ch in c["chunks"] if ch["id"] == b"META")
        if c["header"]["kind"] != 2 or D.name(meta["name"]) != o["name"]:
            raise SystemExit(f"sets/{o['file']}: not the set sets/sets.json lists (tools/make_sets.sh)")
        shutil.copy(SRC / "sets" / o["file"], out / o["file"])
    shutil.copy(SRC / "sets" / "sets.json", out / "sets.json")
    shutil.copy(SRC / "sets" / "README.md", out / "README.txt")


def main(pkg, version, out):
    pkg, out = Path(pkg), Path(out)
    raw = pkg.read_bytes()
    product = product_of(raw)
    if not re.fullmatch(r"FM-1_8\d{6}", product):
        raise SystemExit(f"{pkg}: identity {product!r}: make a release build (./build.sh --release X.Y)")
    if b"FELUCCA-LOADER-1" not in raw:
        raise SystemExit(f"{pkg}: no update loader in it")
    name = f"dub-{re.sub(r'[^A-Za-z0-9.-]', '-', version)}.fwsc"
    if out.exists():
        shutil.rmtree(out)
    for d in ("install", "sets", "manual", "firmware"):
        (out / d).mkdir(parents=True)
    shutil.copy(pkg, out / "firmware" / name)
    # the installer
    page = (SRC / "web" / "amb_installer.html").read_text(encoding="utf-8")
    lib = strip_module((SRC / "web" / "fm1pkg.js").read_text(encoding="utf-8")) + "\n" + \
        strip_module((SRC / "web" / "fm1ota.js").read_text(encoding="utf-8"))
    meta = json.dumps({"version": version, "product": product, "pkg": "../firmware/" + name,
                       "sets": "../sets/dub1sets.js"})   # (Decisions 5: the check asks a 1.5+ FM-1 its full version)
    for mark in ("/*LIB*/", "/*META*/"):
        if page.count(mark) != 1:
            raise SystemExit(f"amb_installer.html must contain {mark} once")
    (out / "install" / "index.html").write_text(page.replace("/*LIB*/", lib).replace("/*META*/", meta), encoding="utf-8")
    # (1.5.0) the Sets page, its modules beside it (the emulator has its own copies: web/emu/build.sh); (step 7b) the cue
    # editor's two
    shutil.copy(SRC / "web" / "dub1_sets.html", out / "sets" / "index.html")
    for f in ("dub1sets.js", "dub1container.js", "dub1cues.js", "dub1cuetables.js", "dub1makeset.js"):
        shutil.copy(SRC / "web" / f, out / "sets" / f)
    ready_sets(out / "sets")
    # the manual (a complete page; its images beside it)
    subprocess.run([sys.executable, str(SRC / "tools" / "manual_html.py"), str(SRC / "docs" / "MANUAL.md"),
                    str(out / "manual" / "index.html")], check=True, capture_output=True)
    shutil.copytree(SRC / "docs" / "img", out / "manual" / "img")
    # the landing page and its screenshots
    shutil.copytree(SRC / "docs" / "img", out / "img")
    (out / "index.html").write_text(LANDING.replace("__VERSION__", html.escape(version)).replace("__PKG__", name)
                                    .replace("__REPO__", REPO), encoding="utf-8")
    emu = SRC / "build" / "emu"
    if not (emu / "amb.wasm").exists():
        raise SystemExit("no build/emu/amb.wasm: run web/emu/build.sh (needs Emscripten)")
    shutil.copytree(emu, out / "emu")
    # the screen reader's words files (fm1-reader): every one ever made, for the devices that point at them
    shutil.copytree(SRC / "web" / "words", out / "words")
    cheatsheets(version, out / "cheatsheets")
    (out / ".nojekyll").write_text("")
    print(f"site: {out}: index.html, install/ ({product}), sets/, manual/, cheatsheets/, emu/, words/, firmware/{name} ({len(raw)} B)")


if __name__ == "__main__":
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    main(*sys.argv[1:4])
