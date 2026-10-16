# DUB-1 on the M-Vave SMK-37

DUB-1 also builds for the M-Vave SMK-37, a 37-key controller with the same chip as the FM-1. The FM-1 build is
unchanged; everything for the SMK-37 is behind `BOARD_SMK37`. On the SMK-37 it is the same DUB-1: the same
launch screen, pieces, pages, effects grid, Chords and Pattern pages, screen and manual.

## Supported models

- **SMK-37 Pro**: the default build (package identity `SMK-37 Pro_800`).
- **SMK-37 Elite**: `MODEL=elite` (identity `SMK-37 Elite_800`).
- **MKE-P37**: `MODEL=mke` (identity `MKE-P37_800`).
- **Starrykey 37 Play**: uses the default build.

All of them are the same hardware. Each build carries its model's name, so M-Vave's updater still takes that
unit's stock package later: install the package for your model. The installers take a package for any of these
models, and refuse an FM-1 package (and an SMK package on an FM-1).

## Controls

The FM-1's controls keep their DUB-1 meaning, so the manual applies; the screen keeps the FM-1's names (HOME,
SEL, ENV, ...), and KNOB 1-4 are the knobs K1-K4. The SMK-37 adds the rest.

| SMK-37 | DUB-1 |
| --- | --- |
| Keys F3..G5 (the middle 27) | the FM-1's 27 keys: the scale (each adds its note or takes it out), the Chords page's root, and with REC held, in PERFORM and on the Pattern page the white keys as on the FM-1 |
| Keys C3..E3 and G#5..C6 (the outer 10) | more of the scale; on the Chords page, the slot's root too |
| 16 pads | the 16 white keys F3..G5. As the scale and the Chords page's roots, low notes on the bottom row (F3..F4), G4..G5 on the top row. Where the white keys are a row of sixteen (the effects grid while REC is held, PERFORM's 1-10, the Pattern page's steps) the top row is 1-8 and the bottom row 9-16, as the screen reads |
| Pad lights | what the FM-1's key LEDs show, in three colours: off dark orange, on light blue, active bright white-blue. The scale: out of it orange, in it blue (the root a stronger blue), the chord's tones as a stab plays bright (red in dub hold). The Pattern page: steps blue (accents stronger), the step playing bright. PERFORM: layers on blue, off orange, the moves red while held. The Chords page's root and the picker's piece and mode bright. The effects grid keeps its group colours (held: white). A note lights all its pads: the 16 pads are the white keys F3..G5, so a pad and the one diagonally up and to the left (and F3 and F4 at the bottom row's ends) are the same note an octave apart |
| K1..K4 | KNOB 1..4 (the page's knobs) |
| K5 | MASTER (volume) |
| K6 / K7 / K8 | SELECT / ALGORITHM / PRESETS |
| PATCH | HOME: the Dub page (again: ripples, scope, spectrum) |
| CHORD / ARP / NOTE REPEAT / SEQ | ENV / ARP / LFO / SEQ: the Chord, Texture, Bass and Drums pages (again: the sound page, then FM or Pattern) |
| FX / GLOBE | FX / GLO: the Space and Setup pages |
| PARA | EDIT: tapped, the Chords page; held, the next chord |
| SCALE | SEL: PERFORM |
| SEQ PLAY / SEQ REC | PLAY / REC: play or rest / dub hold (tapped), the effects grid (held) |
| BT | SAVE |
| OCTAVE- / OCTAVE+ | OCT- / OCT+: the pings' octave |
| < / > | the previous / next page, as SELECT |
| KNOB BANK | the picker: the screen and the pads show the eight pieces (top row) and the eight modes (bottom row); a pad picks. Lit while it is open; press again to close it |
| FADER BANK | what the faders set: A the levels (Chord, Pings, Bass, Drums, as the Mix page), B the space (Echo, Echo time, Feedback, Reverb), C the shape (Cutoff, Decay, Density, Hits). Lit off bank A. The fader card shows the bank and its values |
| Faders 1..4 | the current bank's four values; a fader takes over only once it moves (also after a bank change) |
| Pitch wheel | the tape: up runs it faster (the echo's time down to half), down slower (up to twice); the repeats bend on the way. At rest, the echo is as set |
| Mod wheel | the filter: opens the chord's Cutoff on top of its knob |
| OCTAVE- + OCTAVE+ held 5 s | UPDATE MODE |
| OCTAVE- + OCTAVE+ held at power-on | HARDWARE CALIBRATION: teach the buttons and knobs again (it asks by the FM-1's names, KNOB 1-4 as K1-K4) |

Only one card is up at a time: KNOB BANK closes the fader card. The wheels never change what is saved. Lights Off
(Setup page) darkens the scale on the pads; the SMK-37 has no key lights and no dim button glow, so On and Keys
look the same there. The keys' and pads' velocity is read but DUB-1 does not use it: the keys steer, they do not
play notes.

The SMK-37's TRS jack is a MIDI output and this build does not use it; there is no TRS MIDI IN, so MIDI and its
clock come in over USB only.

## Build

See [BUILDING.md](BUILDING.md) for the toolchain.

```sh
BOARD=smk37 ./build.sh                 # build/dub-smk37.fwsc (SMK-37 Pro, Starrykey 37 Play)
BOARD=smk37 MODEL=elite ./build.sh     # build/dub-smk37-elite.fwsc
BOARD=smk37 MODEL=mke ./build.sh       # build/dub-smk37-mke.fwsc
./build.sh                             # the FM-1 build again before tests/run_tests.sh (the tests use its files)
```

## Install

Connect the keyboard by USB and run, with the package for your model:

```sh
python3 tools/fm1_install.py build/dub-smk37.fwsc
```

`fm1_install.py` needs `mido` and `python-rtmidi` (`pip3 install mido python-rtmidi`). It checks that the package
is for the connected keyboard.

## Going back to the stock firmware

Install M-Vave's firmware package for your model with M-Vave's updater, M-UPGRADE. It works directly, because
the installed package keeps the keyboard's model name.

## Safety

- Back up first: have M-Vave's stock package for your model and M-UPGRADE ready before you install.
- Keep the USB cable connected until the install has finished and the keyboard has restarted.
- The first start clears the stock firmware's settings, and any other firmware's data, in the shared data area.
- The firmware never writes the keyboard's calibration, Bluetooth pairing or key areas; the flash map refuses
  them at compile time and again in the flash driver.
- Installing other firmware is at your own risk.
