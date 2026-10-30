# Running the firmware without the FM-1

Three ways to hear and see the firmware on the computer. To check changes as we go, use the
**host simulator** (section 2): it builds with the Mac's own clang, runs far faster than real time,
and writes audio, screenshots and a MIDI log from a script.

| | What it runs | Interactive | Needs |
|---|---|---|---|
| 1. Online emulator | the published AMB-1 only | yes | a browser |
| 2. Host simulator | this checkout | no (scripted) | clang, Python + Pillow |
| 3. Local browser emulator | this checkout | yes | Emscripten (emsdk) |

## 1. Online emulator

<https://charlesvestal.github.io/fm1-amb/emu/> runs the released AMB-1 in the browser. Good for a
reference listen; it does not show local changes.

## 2. Host simulator

The whole app (UI, project, engine) runs against a simulated FM-1, driven by a script.

### One-time setup

The build generates the screen font (`build/gen/felucca_font.h`) with Pillow. `tools/gen_font.py`
asks for the OpenType `tnum` feature (tabular figures), which Pillow can only use with libraqm,
and libraqm needs the FriBiDi library. On this Mac, Homebrew is the Intel install in `/usr/local`
on Apple silicon (Tier 3), and `brew install fribidi` fails. So:

```
# a Python with Pillow, kept in the repo (.venv/ is git-ignored)
/usr/bin/python3 -m venv .venv
.venv/bin/pip install pillow

# the font, with OpenType features dropped
mkdir -p build/gen
.venv/bin/python3 tools/gen_font_noraqm.py tools/gen_font.py build/gen/felucca_font.h
```

`tools/gen_font_noraqm.py` runs `gen_font.py` unchanged, with the `features=` argument stripped
from Pillow's calls. The only difference is that digits on the simulated screen lose their tabular
widths, so numbers may shift by a pixel as they change. Release builds (`./build.sh`) are not
affected; they need a Pillow with libraqm. (A native Apple-silicon Homebrew in `/opt/homebrew`
with `brew install fribidi` removes the need for the wrapper: delete `build/gen/felucca_font.h`
and let the normal build regenerate it.)

The build skips the font step when `build/gen/felucca_font.h` exists, so the wrapper is needed
only once (and again after `rm -rf build`).

### Build (after every change to the code)

```
PATH=$PWD/.venv/bin:$PATH sh host/build_host.sh     # -> build/host/amb_host
```

The `PATH` part puts the venv's Python first for the build's other Python steps.

### Run a script

```
mkdir -p build/out
build/host/amb_host tests/scenarios/first.amb build/out
open build/out/first.wav          # listen
open build/out/garden30.png       # the screen
```

It prints each `expect` check and ends with the simulated time and the render cost, e.g.
`37960 ms simulated ... render 0.3% of real time (host CPU)`. Minutes of music render in seconds.
The exit code is 1 if an `expect` fails.

Files written to the output folder: the WAVs and PNGs the script names, and `midi_out.txt` (the
MIDI the engine sent).

### Script commands

One command per line, `#` starts a comment (the full list is at the top of `host/amb_host.c`):

| Command | Does |
|---|---|
| `wait MS` | run the device for MS milliseconds |
| `press BTN` · `release BTN` · `tap BTN` | buttons: `FX SEL ENV LFO EDIT GLO HOME SAVE ARP SEQ PLAY REC OCT- OCT+` |
| `key K down\|up` · `tapkey K` | keys: `0..26` = F3..G5, or `w0..w15` white keys, `b0..b10` black keys |
| `turn ENC N [MS]` | encoder `SELECT ALGO PRESET K1 K2 K3 K4`, N detents (negative turns back), MS apart (default 80) |
| `spin ENC N` | N detents at once, exactly (no acceleration) |
| `master N` | the MASTER pot, 0..4096 |
| `midi B0 B1 B2` | an incoming USB MIDI message, hex bytes |
| `wav FILE` · `wavstop` | start / stop recording the output |
| `peakreset` | restart the output peak measurement |
| `shot FILE.png` | save the screen |
| `leds` | print the lit buttons and keys |
| `expect WHAT VALUE` | check state; VALUE may be `<N` or `>N`. WHAT: `playing frozen view piece screen pool dirty store_writes clocks_out sixteenths ext midi_out pad_level bass_level chordroot loopnotes peak_db_max peak_db_min` |
| `reboot` | boot again from the simulated flash |

Screen-reader commands (`listen`, `where`, `said`, `said_none`, `speakdump`) are also there; see the file.

`film FILE` / `filmstop` record the device at 30 frames a second (the screen, the buttons and keys held
and lit, the knobs turned), and `splashrun` plays the launch screen as at power-on. `bar N` and `at S` run until a bar's downbeat or a time in
the film, and print the film's time there (a cue sheet).
`tools/film.py FILM WAV OUT.mp4` draws that as the FM-1's panel and encodes it with the audio (ffmpeg);
`tools/make_film.sh` makes the video's clips from `tests/film/*.amb` into `build/film/`.

`stems DIR [NAME,NAME,..]` (before `film`) records, with the film, each of the screen's pictures alone as its
own video, `DIR/NAME.mp4`: to lay full-screen under the film in an edit. Every stem has the film's frames
(30 a second of the film's time, from its first frame to `filmstop`), so it lines up with no offset. A stem is
the picture as its page draws it, all the time whatever page the film shows: no header, no knobs, no words, on
the screen's ground; 240 x 135 of the screen (16:9) blown up 8x, nearest, to 1920 x 1080, H.264. The names
(all of them without a list, or `all`):

| Stem | The picture |
|---|---|
| `ripples` | HOME's ripples: the stabs' rings and their echoes, the bass's rings, the noise's specks, the ping loops (whole rings: the screen cuts them at the lanes) |
| `ripples-chord` `ripples-pings` `ripples-bass` `ripples-drums` | a layer's page's ripples: its notes alone, in its colour |
| `specks` | the Noise pages': the noise's specks alone |
| `scope` `spectrum` `tape` | HOME's other views (the scope and the spectrum the whole output, not a page's layer; the tape without its counter) |
| `lfo` `lfo-noise` | the chord's and the noise's LFO pages' traces |
| `walk` | the Walk page's circle of fifths, the root's journey round it |
| `meters` | the Arrangement page's meters |
| `lanes` | the overview's seven rows (the bar's notes, the meters at the left) |
| `lane-chord` `lane-pings` `lane-bass` `lane-drums` `lane-noise` | a layer's page's lane, alone, large |
| `song` | the Song page's song: its parts, the rows coming in and leaving, where it is |
| `pattern` | the Pattern page's steps |

The stems draw from a second copy of the UI (`host/stems.c`, compiled on its own), so the film and the WAV are
the same, byte for byte, with stems or without. `AMB_STEMS="DIR [NAMES]"` turns them on for a script without
editing it, `AMB_STEMS_FF` sets ffmpeg's options (default `-vf scale=1920:1080:flags=neighbor -c:v libx264
-preset veryfast -crf 18 -pix_fmt yuv420p`). For the hour videos, `STEMS=1 tools/make_set.sh` (or
`STEMS=ripples,scope tools/make_night.sh`) adds a third pass that makes them into `$OUT/stems/`;
`STEMS_ONLY=1` skips the picture's pass. All 21 for an hour: about 3 GB, and about two hours on a 16-core Mac
(x264 at 1080p is the work: the drawing is a few seconds).

### Listening to changes

Keep our own listening scripts apart from the test scenarios, e.g. in `tests/listen/` (scripts)
with the output in `build/listen/` (git-ignored, as all of `build/`). A typical one: pick a piece,
record a few minutes, take a screenshot at the end.

```
# tests/listen/long.amb: three minutes of the current piece
wav long.wav
wait 180000
shot end.png
wavstop
expect peak_db_max -0.5          # no clipping
```

```
mkdir -p build/listen && build/host/amb_host tests/listen/long.amb build/listen && open build/listen/long.wav
```

To compare before and after a change, render the same script with both builds into different
folders (for example `git stash` the change, build and render to `build/listen/before`, then
`git stash pop`, build and render to `build/listen/after`). The engine is seeded from the simulated
clock, not the real one, so the same script on the same build gives the same audio, sample for
sample (`cmp` two WAVs to check).

### The full test suite

```
PATH=$PWD/.venv/bin:$PATH tests/run_tests.sh
```

It runs the engine's unit tests, the scenarios, and (with Node.js and Emscripten installed) the web
builds. Changing the music will break some of the scenario expectations (`tests/scenarios/*.amb`)
and engine tests (`tests/host/amb_test.c`); update them as the instrument changes.

## 3. Local browser emulator

The same code compiled to WebAssembly, with the FM-1's panel in a web page: click the buttons,
turn the knobs, hear it live. It needs Emscripten. Homebrew's would likely hit the same Tier 3
problem, so install emsdk:

emsdk needs Python 3.10 or newer; this Mac's `python3` is 3.6. A private Python 3.12 comes from
`uv` (a single binary in `~/.local/bin`, from github.com/astral-sh/uv releases), and `EMSDK_PYTHON`
tells emsdk to use it:

```
~/.local/bin/uv python install 3.12
export EMSDK_PYTHON=$(~/.local/bin/uv python find 3.12)
git clone https://github.com/emscripten-core/emsdk ~/emsdk
cd ~/emsdk && ./emsdk install latest && ./emsdk activate latest
```

(Installed this way on 2026-10-16: emcc 6.0.12, its own node 24.) Then, in each new terminal:

```
export EMSDK_PYTHON=$(~/.local/bin/uv python find 3.12)
source ~/emsdk/emsdk_env.sh
cd ~/Documents/GitHub/fm1-amb
PATH=$PWD/.venv/bin:$PATH web/emu/build.sh        # -> build/emu/
node tests/host/emu_test.mjs build/emu/amb.wasm   # its test
.venv/bin/python3 -m http.server -d build/emu 8000 --bind 127.0.0.1   # open http://localhost:8000
```

`web/emu/build.sh` needs `build/gen/felucca_font.h` from the one-time setup above. It looks for a
sibling checkout of fm1-reader (`../fm1-reader`) for the screen reader's voice; without it, it
prints a note and the emulator still works.

**MIDI in.** Under the FM-1, *Connect MIDI* lets the page read this computer's MIDI inputs (Web MIDI: Chrome
and Edge; the browser asks first). A controller then plays the emulated FM-1 as it would over USB: channel messages
(notes, CCs) and the clock, start, continue and stop. *From* picks one input, or every one. The page passes each
message to the firmware's MIDI queue (`web_midi` in `web/emu/amb_web.c`, as the host simulator's `midi` command).

`web/app/build.sh` builds the engine alone into the phone/desktop web app, the same way.
