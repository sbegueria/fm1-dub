# DUB-1: plan and progress

A fork of AMB-1 (generative ambient for the M-VAVE FM-1) turned toward dub techno: chord stabs into a
tape echo, a sub bass, a four-on-the-floor kick, vinyl crackle, and an arrangement that drops and
brings back its layers in eight-bar phrases. Working name **DUB-1** (easy to change). Work happens on
the `dub` branch; `EMULATOR.md` says how to build and listen on the computer.

## The design

| AMB-1 | DUB-1 |
|---|---|
| Pad: four breathing voices | **Chord**: a four-voice stab (two detuned saws a voice, or FM keys) on a Euclidean pattern off the beat, through a resonant low-pass with its own envelope. One voicing (open m7, or a 9 without its fifth) shifted whole to each root, as a chord memory |
| Bells: four tape loops (13/17/23/29 quarters) | **Pings**: short FM blips on the same loop lengths, in eighths, mostly into the echo |
| Bass: sine and sub-octave, gliding | **Bass**: a sub (E1..Eb2) on five grooves: Drone, Offbeat, Dotted, Roller, Sync |
| Pulse: three Euclidean patterns (16/12/7) | **Drums**: four-on-the-floor kick with a click, a shaker on the sixteenths (Hits), an open hat off the beat; the kick ducks everything else (Duck) |
| Arc: a minutes-long tide | **Arrangement**: eight-bar phrases (Full, Break, Groove, Thin), a fill before the kick returns, the filter sweeping with it |
| Tape echo + Echoplex + plate | The same, with a band-pass that moves in the echo's feedback, a soft clip so the feedback can pass 100%, dub throws (a stab sent hard into the echo) |
| Freeze: the chord holds, the reverb rings | **Dub hold**: the echo stops listening and repeats its loop for ever; the chord holds |
| (none) | **Crackle**: vinyl hiss and clicks |

Tempo 90–140 BPM (default 122). Echo times 1/8, 1/8. (default), 1/4, 1/4., 1/2, 1 bar. Drift in bars
(2..64) or Hold.

### The pages (planned)

| Button | Page | KNOB 1–4 |
|---|---|---|
| HOME | Dub | Tempo, Evolve, Motion, Drift |
| ENV | Chord | Level, Stabs, Decay, Cutoff |
| ARP | Texture | Pings, Density, Tone, Crackle |
| LFO | Bass | Level, Octave, Groove, Fifth |
| SEQ | Drums | Level, Hits, Swing, Duck |
| FX | Space | Echo, Echo time, Feedback, Reverb |
| GLO | Setup | Root, Tune, MIDI out, Lights |

Set by the piece only: Mode (ALGORITHM), Chord tone (Saws / Keys), Hats colour, Plate size, ping octave
(OCT- / OCT+).

## Steps

1. Tempo range and the four-on-the-floor kick
2. The chord stab and its filter
3. The echo's feedback filter, throws, dub hold
4. The phrase arrangement
5. The pieces (eight recipes)
6. The UI (pages, labels, help cards, screen reader words, colours), storage, tests, manual

Steps 1–4 are done as one engine rewrite (`firmware/src/dsp/amb.c`), then tuned by ear (yours) and by
measurement (renders analysed for level, timing and spectrum).

## Done

- [x] Local simulator set up (`EMULATOR.md`, `.venv`, `tools/gen_font_noraqm.py`)
- [x] `dub` branch
- [x] Engine rewrite (steps 1–4): chord stabs, pings, bass grooves, drums with ducking, arrangement,
      dub echo (band-pass + soft-clipped feedback, throws, hold), crackle (`amb.c`, `amb.h`)
- [x] `host/en_render.c`: the engine alone to a WAV (parameter overrides, timed changes `NAME=V@T`,
      `freeze=T0-T1`, a bar-by-bar log); `tools/analyze_wav.py`: levels, bands, kick timing, spectrogram
- [x] First tuning by measurement: layer balance (the chord +12 dB, the pings +9 dB), longer echo tails,
      no half-diminished chords, fewer throws, keys as loud as the saws, crackle only while playing,
      feedback eased under 90% while stopped (tails always die); feedback at 100% stays bounded
- [x] CPU: on the host the new engine costs the same as AMB-1's or less (0.20–0.30% of real time vs
      0.27–0.31%)
- [x] Eight pieces, first draft: Basic, Chamber, Steppers, Fog, Concrete, Night Drive, Pressure, Haze
- [x] UI: pages, labels, help cards, messages, a dark palette, the section and phrase on the main
      screen, "hold"; screen reader texts
- [x] Storage: project magic "DUB1", format 1 (AMB-1's saved projects give the recipes)
- [x] Engine unit tests rewritten (`tests/host/amb_test.c`)
- [x] A listening pack: `build/listen/pack/` (each piece, dub hold, feedback swell, the arrangement)
- [x] Simulator scenarios and the screen reader's words for DUB-1; the whole suite passes
- [x] README, manual, licensing note and screenshots for DUB-1
- [x] The device build works here (`EN_FONT_NORAQM=1 ./build.sh` -> `build/amb.fwsc`): RAM 52.6 of
      96 KB, pool 332 of 344 KB, no soft doubles
- [x] DUB-1 by name on the device: splash, crash and safe-mode screens, USB strings; MIDI out off
      silences every channel

## TODO

- [ ] **Your ears**: listen to the pack (`build/listen/pack/`, rendered again with the cloud); what to
      change in the sound and the pieces
- [ ] The web app's own page (`web/app/index.html`, the phone/desktop app): still AMB-1's UI (the
      emulator page is done)
- [ ] **On the device**: install (`python3 tools/fm1_install.py build/amb.fwsc`), check the Setup
      page's CPU and the sound on the FM-1's speaker and on headphones
- [x] The name: DUB-1 (decided 2026-10-16)
- [ ] Publishing (the words file, a hosted emulator and installer): later, after trying it longer
      (decided 2026-10-16). Where (the `# url:` in
      `firmware/words.tsv` still points at AMB-1's site)
- [ ] With everything at maximum the mix drives the output limiter (peak 0.98): fine as a limit, but
      the levels could leave more room

- [ ] `tools/gen_manual_figures.py` and `tools/make_pages.py` still make AMB-1's panel figure and site
- [x] Decisions on behaviour (2026-10-16):
  - Bass Octave −1: a true octave down (E0..Eb1)
  - Fifth with Drone: how often a whole bar is the fifth
  - Evolve 0 (with Motion 0): a strict loop, no stab gaps, no throws, no fills
  - The kick follows the key: the root in F1..B1, below F its fifth (44..62 Hz)
  - Concrete keeps its chords in fourths (7sus4 sounds)
- [ ] **Sound design** with your ears: the stab's character, the kick, the bass, the hats, the
      echo's colour, the cloud, the crackle (a round of its own)

## Release 1.1.0 (in progress, 2026-10-17)

- [x] PERFORM's Open and Shut you can hear (a filter on the music, the kick left out)
- [x] PERFORM reworked: white 1-5 mutes, black 1-5 solos (above the mutes, a mixer's strip), white 6
      dark, white 7-11 the moves; every key that does something lit (a solo blinks while on, a move
      goes dark while held); the key map on SEL's card; SOLO on the screen
- [x] Ping lock: at Evolve 0 the loops stay as they are; SAVE keeps them with the piece (format 9; 1.0.0's
      projects are read and kept)
- [x] The Feedback knob shows the real loop gain (45-112%)
- [x] White 10 is Drop: the kick and the bass out while held (was Kick drop, the same as the kick's mute)
- [x] The video take again with the new keys (`tests/film/00-take.amb`, `build/video-script.md`)
- [x] The chord's name bigger; the Setup page's battery in the header, the raw reading gone
- [ ] Manual, README, the emulator page, release notes; `./build.sh --release 1.1.0`; the site; push
      and the tag (when the user says)

## Release 1.2.0: live performance (released 2026-10-19)

Decided with the user (2026-10-19):

- [x] **HOME, Dub (the song as it plays):** Filter, Evolve, Motion, Drift. Tempo leaves knob 1 for
      **Filter**. Filter is bipolar: down is Shut's low-pass (the music under water), up is Open's resonant
      high-pass, the middle is off (the knob stops at the middle on the way through), and the kick is always
      left out. It is the same filter as PERFORM's Open and Shut keys, which push on top of it.
- [x] **HOME again: the Key page:** Tempo, Root, Mode, Swing (the piece's settings, rarely changed in a
      dub song). HOME + SELECT stays as the tempo shortcut from any page. HOME blinks on the Key page.
- [x] **ALGORITHM on HOME: the screen view** (ripples, scope, spectrum; was HOME again). ALGORITHM still
      sets the voice on a layer's page and the mode elsewhere.
- [x] **GLO only for what is global:** Tune, MIDI out, Lights (and CPU and battery in the header). Root
      moves to the Key page.
- [x] **PERFORM's knobs: the master actions** Filter, Dub, Energy, Space (Filter on knob 1, as on HOME, the
      same value). **White 6 held: the mix** (chord, pings, bass, drums), as the knobs were; the mutes still
      act on the press. White 6 is lit in PERFORM and goes dark while held.
  - Dub: the echo send (+40 at full) and feedback (+35). Energy: Density, Hits and Stabs (+45 each).
    Space: Reverb and Cloud (+45), the plate's Size (+30). All bipolar: 0 is the piece as written.
  - **For playing only:** never saved. They keep their value across a piece change (filter down, change
    piece, open it up: the night video's transition by hand), and are back in the middle on boot.
- [x] The scenarios updated (screen, scale, voices, pings, help, reader, perform: the master actions and white 6),
      the help cards, the emulator page's help
- [x] Listened to on the FM-1 (the user): the master actions widened, white 6 blinking, Swing fixed, each piece its own view
- [x] Manual, README, the release notes; `./build.sh --release 1.2.0`, the site, the tag
- [ ] The film takes (`tests/film/*.amb`) use 1.1.0's controls (HOME again for
      the screen): update them before rendering with 1.2.0
- [x] Fixed: Tape stop past the tape's end in dub hold (genkigenki's report; reproduced under AddressSanitizer);
      ALGORITHM and PRESETS skipping detents on a quick turn; the clap's bursts clicking (they started at full level)
- [x] Haze's "faint clicks" (the shorts agent): nothing broken. They're the kick's designed click, the hats' noise and the
      Blip pings' attacks, made more noticeable by Haze's long echo.
- [ ] SMK-37: genkigenki's PR #1, kept on an `smk37` branch until he tests it on his unit (the user, 2026-10-19:
      accept it in principle; host the SMK packages and an SMK emulator; the user will get an SMK-37 Pro later).
      See `build/smk37/REPORT.md` and `MERGE.md`

## Next: after 1.0.0 (the user's ideas, 2026-10-17)

**The main direction (the user, 2026-10-17):** after the small fixes and ideas, expand how playable
the instrument is. DUB-1 should be something you play live, not only leave running. PERFORM (mutes,
solos, moves), the effects grid and dub hold are the start. More hands-on control comes next, and
each idea below is weighed by how much it adds to playing it.


- [ ] **A video for YouTube and other platforms** (the most urgent). Real action and sound from the
      simulator, not stills: the screen inside a drawn FM-1 panel, with the buttons and keys lit as they
      are pressed (`film` in the host simulator, `tools/film.py`). The script follows the Reddit post:
      it plays as soon as it starts; playing it (PERFORM, dub hold, the effects grid); making your own
      piece (sounds, chord progression, drums). Clips to cut in DaVinci Resolve.
  - [x] The clips: `tools/make_film.sh` runs `tests/film/*.amb` and makes three MP4s of each in
        `build/film/` (the panel with the screen large, the panel alone, the screen alone) and the WAV
  - [x] One take (`tests/film/00-take.amb`, 2:48): every move on a downbeat (`bar N` in the simulator,
        printing the cue sheet), so the music runs unbroken; the edit cuts between the framings only.
        The script with its cues: `build/video-script.md`
  - [ ] Cut, titles, voice-over or captions (the user, in DaVinci)
- [ ] **Next release, to think through carefully: where Tempo, Root and the master actions go**
      (the user, 2026-10-17). Nothing decided yet.
  - **GLO only for what is really global:** settings that hold for every piece and the whole
    machine (Tune, MIDI out, Lights, the battery). The piece's own settings stay out of it.
  - **Tempo and Root are the piece's**, and they rarely change within a song. They could sit
    together on HOME (Root moving there from GLO), or elsewhere. HOME + SELECT stays as the tempo
    shortcut from any page.
  - **Master actions at hand.** Knobs that act on the whole music, for playing it live. Where they
    belong is open; the HOME page is a good candidate. The ideas:
    - **Filter**, bipolar: left is Shut's low-pass (the music under water), right is Open's
      high-pass (thin and bright), the middle is off, and the kick always stays out of it. It's the
      DJ mixer's filter, a continuous version of PERFORM's Open and Shut keys.
    - **Dub** macro: the echo send, feedback and band-pass on one knob. Turned up, more goes into
      the echo, it feeds back longer and turns darker, towards a runaway; turned down, it's dry and
      tight.
    - **Energy** macro: Density, Hits and Stabs together. Down, the pings, hats and stabs thin out
      towards a sparse groove; up, everything fills in.
    - (More can come in the same spirit: one knob, several parameters, a musical direction.)
  - **The user's idea (2026-10-17): the master actions in PERFORM.** The keys stay the mutes, solos
    and moves; the four knobs become the master actions (Filter, Dub, Energy, and a fourth: for
    example Space, the reverb and the cloud together). The mix is still there: **hold a key and turn
    a knob** for the levels, as the knobs are now. Two ways to read "a key":
    - *the layer's own mute key* held: any knob sets that layer's level (a mixer's channel, picked
      by hand). The mute would then act on the key's release, and only if no knob was turned while
      it was held (as the buttons do). That costs a press-to-release delay on the mutes.
    - *one shift key* held (SEL itself, or white 6, the dark key between the sets): the knobs are
      the Mix page as now (chord, pings, bass, drums). The mutes keep acting on the press.
    - The screen shows which: the master actions' page, or MIX while the key is held.
    If the master actions live in PERFORM, HOME can keep Tempo, Root and the piece's settings, and
    GLO stays for what is really global.
  - **The questions:** HOME has four knobs (Tempo, Evolve, Motion, Drift today). Do Evolve, Motion
    and Drift stay there, or move to make room for the master actions? Should the master actions
    live on their own page, or in PERFORM (the Mix knobs)? And what happens to the knobs' values when
    you change pieces: saved per piece, or performance-only, reset to the middle?
- [ ] **Projects outside the FM-1, and new sets of pieces for people to load** (the user's idea).
  - **What travels:** a whole project is the eight pieces with everything in them: tempo, scale and
    root, the voices and their sound and FM pages, the mix, the echo and space, the chord slots, the
    drum patterns. It's the same data the FM-1 keeps in flash (`project_t`, `PROJ_FORMAT` 8, magic
    "DUB1"), about 2 KB.
  - **Out and in:** from a web page (the installer's site), over USB-MIDI SysEx, the way the screen
    reader already talks to the FM-1. "Save to file" reads the project into a `.dub1` file; "Load"
    writes one back. The firmware checks the magic, the format and a CRC before taking it, and keeps
    a single piece or all eight.
  - **New sets of pieces:** write other banks of eight for people to explore, for example a deep and
    slow set, a Chain Reaction-style set, a minimal set, or a set built on two chords. Publish them
    as a library on the site, each with a short listen, loadable in one click. Load them in the
    emulator too: try before you load.
  - **Later:** people share their own `.dub1` files, and a set can carry its own names.
  - **To decide:** whether a loaded set replaces the eight pieces or goes into slots, and whether
    the firmware ships with more than one bank.
- [ ] **Nail the sounds:** some voices sound too alike. Make each voice on ALGORITHM clearly its own.
- [ ] **The live side, further:** more to play by hand (see the Reddit feedback below).
- [ ] **From the feedback (Reddit, 2026-10-17):**
  - "More for composing my own melodies": a way to write or lock the ping line. At Evolve 0 the
    loops still add and drop notes (`loop_target` rounds at random). Keep them fixed at Evolve 0, and
    let SAVE keep the current pings with the piece.
  - The Feedback knob reads 0-100 while the loop runs 45-112%: show the real figure.
- [ ] **Ideas from "Understanding Dub Techno: A Musicologist's Approach"** by Bahadırhan Koçer
      (https://www.youtube.com/watch?v=2XMWPQmSBOw; studied from its subtitles, 2026-10-17). It rests on
      three ideas: spontaneous repetition ("a river": the loop the same, its echo, reverb and filter
      always moving, some of it barely heard), atmosphere (the mix as a room, a "backcloth" filling the
      gaps between the stabs), and noise as the canvas. DUB-1 already has slow random movement on the
      cutoff and the echo, feedback past 100% held by a clip, crackle, the cloud and the plate, and a rim
      on two and four into the echo. To try:
  - the stab's decay moving at random (tight, then loose): `mod[1..4]` are named the stab's but only
    `mod[0]` (the cutoff) is read;
  - a faster random layer under Motion: small sixteenth-step jitter on the cutoff and the echo feedback;
  - the crackle tuned to the key: through a resonator on the root or the fifth and a moving band-pass,
    part of it sent into the echo;
  - part of the crackle on the sixteenths, as a quiet percussion lane;
  - a backcloth: a hum near the root and filtered tape noise, always under everything (a sibling of
    Crackle);
  - noise in the kick (a breath) and a little in the sub;
  - a "gritty shimmer": an octave-up return through a saturator into the plate (the cloud could be it);
  - a two-pad backcloth in Thin and Break (low and held, high and swept), so the gaps are never empty;
  - two-chord pieces: a recipe that keeps the walk on two degrees, the harmony still, the effects moving;
  - a flanger or phaser in the echo's feedback, at an odd rate (or as a grid effect);
  - a spring colour on the rim or the clap.
- [ ] **For later: other genres.** Fork AMB-1 again and bring DUB-1's new parts (the FM core, the
      effects grid, PERFORM, the pattern page, the screen views) to other styles: techno, industrial.

## Done: voices (a first approach)

- ALGORITHM on a layer's page steps through its voices; on Dub, Space, Setup and Mix it is still the
  mode. The voice shows at the left of the knob strip, is said ("VOICE Organ"), and is kept by the
  piece.
- Chord: Saws, Keys, Organ, Pad, Bell, Tonewheel (replaced Brass, too like the others), Noise. Pings: Blip, Marimba, Glass, Wood, Drop. Bass:
  Sub, Round, Growl, Pulse. Kits: Tight, Deep, Punch, Soft, Dusty.
- Each voice is a row of a table in `amb.c` (`CV`, `PV`, `BV_GAIN`, `KIT`). Levels are matched by
  measurement (within ~2 dB of the layer's first voice).
- The pieces got their own voices. The project format is now 2.
- CPU (host): Organ is the heaviest, 0.33% of real time (AMB-1's worst: 0.31%).

### Voices: feedback (2026-10-16)

- The Mix page's labels (CHORD, PINGS, BASS, DRUMS) are fine.
- [ ] The basses are too alike: something punchier, from the classic FM bass patches (later).
- [x] Brass was too like the other chord voices: now Tonewheel, a second organ.

## Next: a sound page for each layer (the user's plan)

Each layer gets a second page, its **sound page**, to shape its voice beyond the preset.

### How it works

- **Reaching it:** press the layer's button again (ENV on the Chord page: the Chord's sound page;
  ENV again: back). The page's name shows it ("CHORD · SOUND"), and its button blinks while there.
  SELECT's round of pages stays as it is (the sound pages are off it). ALGORITHM still picks the
  voice, on either page.
- **Four knobs a layer**, with the same meaning for every voice of that layer: each voice maps
  them onto its own insides (for a saw voice, "Colour" is the detune; for an FM voice, the
  modulation index; for the organ, the drawbars' balance).
- **Relative to the preset:** each knob's centre (shown as 0) is the preset as it is; turning
  moves away from it. Changing the voice with ALGORITHM keeps your turns, so a "brighter, slower"
  chord stays brighter and slower on the next voice. A press of SAVE held (the piece's reset)
  puts them back to 0, with the rest.
- **Saved with the piece**, like everything else: 16 more values a piece (4 layers x 4); the
  project grows from ~700 to ~980 bytes, under the 3840-byte limit. Project format 3.
- **The screen**, on a sound page: the main area shows the voice's shape instead of the ripples,
  its envelope drawn (attack, decay, the filter's own), redrawn as you turn. Later, for the FM
  voices, the algorithm as a diagram.
- Two values only the pieces set today get a knob here: the pings' tone and the hats' colour.

### The knobs

| Layer | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
|---|---|---|---|---|
| Chord | **Attack** (a stab .. a swell) | **Colour** (detune / FM index / drawbars / noise colour) | **Resonance** (the filter's) | **Env** (how far each stab opens the filter) |
| Pings | **Tone** (round .. metallic: the index, today's Ping tone) | **Ratio** (the modulator: harmonic .. bell-like .. clangy) | **Length** | **Bend** (the pitch falling at the strike) |
| Bass | **Bite** (FM index / drive at each note) | **Shape** (sine .. square-ish) | **Length** (how long each note holds, against the groove's) | **Slide** (glide between notes) |
| Drums | **Kick tune** (semitones from the key's note) | **Kick length** | **Punch** (the kick's start pitch and click) | **Hats** (dark .. bright, today's piece value) |

### Then: a real FM core (phase 2)

- One 4-operator FM engine (sine operators, ratios, levels, feedback on one, an index
  envelope), with the classic algorithms (the eight 4-op layouts, as the FM-1's own firmware and
  the DX-style synths have them), for the chord and the pings. The FM voices (Keys, Bell, Marimba,
  Glass, Wood) become patches of it, and new ones come from the classic FM patch libraries (the
  punchier basses wanted, electric pianos, clavs, bells).
- The sound page gets a second set of four on an FM voice (the layer's button a third time):
  **Algorithm**, **Ratio** (of the modulator in focus), **Feedback**, **Index env** (its decay).
- **CPU first:** four chord voices x 4 operators is the heaviest thing DUB-1 would do. Measure on
  the device (the Setup page's CPU) before, and keep a lighter algorithm set if it's tight.

### Steps

- [x] The sound pages: the UI (the second press, the page, the help cards, the screen reader), 16
      parameters, project format 3, tests (`tests/scenarios/sound.amb`)
- [x] Each voice's mapping of the four knobs (a first pass; to tune by ear)
- [x] The envelope drawing on a sound page
- [x] The FM core (4 operators, the eight 4-op algorithms and a 2-op one, op 4's feedback, an envelope an
      operator), the FM voices moved onto it (Keys, Bell, all the pings); new basses: Punch, Rubber, Pluck
      (Round and Pulse gone), Growl
- [x] The FM page (Algo, Ratio of ops 3 and 4, Feedback, Mod dec), a third press on an FM voice; the
      algorithm drawn; project format 4; `tests/scenarios/fm.amb`
- [x] **CPU on the device** (2026-10-16, DUB-1 installed on the user's FM-1, over the stock V15 put back
      first): Night Drive shows 20..50% (smoothed). The host's 0.42% of real time is ~80x faster than the
      device. Setup now shows the peak too (now/peak over 2 s) and any dropouts (!n)
- [x] Readings (2026-10-16, now/peak, no dropouts anywhere; "sounds good"): Basic 20/50, Night Drive
      30/67, Night Drive stressed (Cloud 100, Density 100, the pings on algorithm 1) 50/72. About a
      quarter of the time is left at the worst moment: what's added must be light
- [x] Found on the way: the command queue (64) dropped commands once a piece had 62 parameters (PLAY
      among them): now 256, with a compile-time check and a test

## Done: DUB-1's own interface

- **Main screen, echo ripples:** each stab drops a ring that spreads and fades; the echo's repeats
  follow it at the echo's own time and feedback. Throws are double rings; dub hold is red and
  doesn't fade. Also the chord's name, the section and phrase, the layers' levels, a duck meter,
  and four lanes (stabs, kick, hats, bass) showing the bar as it plays. The scale ring, ping loops
  and drum rings are gone (the keys still light the scale).
- **PERFORM (SEL):** the knobs become the Mix page (chord, pings, bass, drums levels); white keys 1–5 mute the chord, pings, bass, kick and hats; 6–10, held, throw
  every stab, push the filter open or shut, drop the kick, push the echo. The keys light up for
  the layers playing and the moves held. Mutes keep the music's random choices, so a layer comes
  back where it would have been. The engine API is `en_mute`, `en_perf`.
- Scenario `tests/scenarios/perform.amb`; the manual, the README, the emulator page and the
  screenshots are updated.
- The web emulator (`web/emu`) is ported and builds with emsdk (`EMULATOR.md`); its test and the
  web app's engine test pass.

## Done: a granular reverb (CLOUD), after JIANT

After JIANT's CLOUD (github.com/juanjiant-bit/JIANT, GPL-3.0; credited in the README, the manual
and `amb.c`):
- A 0.74 s line at a quarter rate (11 kHz, 16 KB, in RAM: the pool is full), fed by the reverb send.
- Up to 6 grains of 80–250 ms (triangle windows, random place and pan, a few cents off, sometimes
  an octave down, rarely up), denser as Cloud turns up and as the filter opens.
- The grains go into the plate, and the plate's tail back into the line, so the wash grows with
  a bigger plate. More of it in a Break; dub hold freezes it.
- It has its own random numbers, so turning Cloud doesn't change the music's choices.
- **Cloud** is the Texture page's third knob; the pings' tone is now set by the piece.
- Measured:
  - in Haze the cloud is ~10 dB under the mix;
  - at everything max it stays bounded and dies once the layers stop;
  - CPU within ~3% of AMB-1's (host);
  - RAM 69.2 of 96 KB.

## Next after the FM core: an effects grid on REC (the user's idea)

Holding REC shows a grid of effects; the white keys play them while held (a tap on REC stays dub
hold). Sixteen in four groups of four, the first of each group lit:

| Group | 1 | 2 | 3 | 4 |
|---|---|---|---|---|
| Echo | Throw | Time x2 / /2 | Tape stop (the echo slowing, pitch diving) | Swell (all in, feeding back; was Reverse: the tape can't run backwards without much more memory) |
| Filter | Low-pass sweep | High-pass ("radio") | Wah (resonant sweep) | Telephone (narrow band, lo-fi) |
| Space | Reverb throw | Spring crash | Cloud freeze | Wash (echo and reverb flooding) |
| Rhythm / FX | Stutter (1/8 or 1/16 repeat) | Gate (the chord in 16ths) | Siren | Noise riser |

- [x] The grid on screen while REC is held; the keys lit by group; said whole to a screen reader
- [x] Each effect, eased in and out (Stutter borrows the Echoplex's buffer; Gate lets the chord ring on to be
      chopped; Freeze brings the frozen cloud forward). CPU (host): one filter effect +22%, all sixteen +55%
      (in Night Drive on the device that could reach ~100%: the effects are momentary, but worth watching)
- [x] PERFORM keeps the mix and mutes; the grid is the effects (`tests/scenarios/effects.amb`)
- Found on the way: at Cloud 30 with Reverb 35, the cloud was all but silent (its input came after the
  Reverb knob): it now listens as if Reverb were at least 63

## From the user's testing (2026-10-16)

- [x] PERFORM's Mix knobs in the layers' colours (chord teal, pings blue, bass brass, drums amber)
- [x] The Drums knob follows the Kick and Hats keys (0 when both are out, 100 when one comes back)
- [x] A muted layer (or one at level 0) leaves its lane at once
- [x] The stabs' lane shows the echo's repeats as rings, flashing as the playhead reaches them (made
      brighter, with a white flash: "they don't blink" on the device)
- [x] **A launch screen** (asked: "fancier", then "the idea of the echoes", the voices' colours): ~1.4 s
      while the FM-1 starts, a delay throw: the name struck, then four repeats dropping out of it, smaller and
      fainter, in the layers' colours, each with its ring; the line under it, the version (`ui_splash`; the
      simulator's `splash N` draws a frame)
- [ ] Idea: the launch heard too: a stab thrown into the echo as the screen plays, its repeats in time with
      the drawn ones
- [x] The FM pages always open on the third press; on a voice that isn't FM they say so and name the FM
      voices (ALGORITHM there picks one)
- [x] **Battery:** an icon on every page (three bars), a rough % on Setup with the raw reading. The
      thresholds are dxsloop's (591 / 561 / 531 on the divider's raw reading, PB1, ADC ch 3); on USB the
      bars fill in turn (no charger line). The % (521..601) is an estimate: to check against the
      FM-1's real readings, full and low
- [x] **The Chords page** (the user's design, in place of option 2): EDIT tapped, eight slots; KNOB 1 the
      slot, the keys its root, KNOB 2 its kind (Empty, Major, Minor, Sus2, Sus4, Dim, Aug), KNOB 3 its type
      (Triad, 6, 7, Maj7, 9, 11, Add9), KNOB 4 Play (Random / Prog); an empty slot between two holds the
      one before; EDIT held: the next chord. Project format 6 (`tests/scenarios/chords.amb`)
- [x] (replaced) **Editing the chords** (option 2): EDIT held + keys, a progression of up to eight chords (the keys'
      notes in the scale), played in order every Drift bars (with Hold, on EDIT's tap); held 1.5 s
      alone, random again; "1/4" under the chord; kept by the piece (project format 5;
      `tests/scenarios/chords.amb`)
- [ ] **Idea: the chord's type** (option 3): per piece (or per chord of a progression), the chord
      built as an open minor 7 (now), a 9 without its fifth (now), a sus4, a minor 11, a major 7

## Done: the drums' pattern page and a rim (2026-10-16)

- [x] A fourth drum voice, the **rim** (two tones, ~520 Hz and ~1.7 kHz, a click of noise, mostly into the
      echo): Auto plays it on two and four once Hits is 70 or more; its own lane on the main screen (pink)
- [x] The **Pattern page** (SEQ a third time): KNOB 1 the lane (Kick, Shaker, Open, Rim), the white keys
      its sixteen steps, a black key held + a white one an accent, KNOB 2 Accent, KNOB 3 the lane's Chance,
      KNOB 4 Play (Auto / Pattern); the first edit takes Auto's pattern over. Project format 7
      (`tests/scenarios/pattern.amb`)
- [x] Found on the way: with 103 parameters, the UI's own knob codes (from 100) collided with real ones
      (Play was "no knob"; Lights would have set the rim's chance): moved to 200, with a compile-time check
- [x] A fifth drum lane, the **clap** (three bursts and a tail, into the plate): rare: Auto ~5% on two and four
      (3% + 5% x Evolve, in Full and Groove); on the Pattern page its Chance starts at 5% (was 12%:
      "too conspicuous"). Hits that only
      sometimes come are rings on the screen, filled when they play. Format 8
- [x] The effects grid takes the whole screen while REC is held; the knobs rest
- [ ] **Later: pattern styles** (the user's idea 2): a short list of classic dub techno grooves (steady,
      Steppers, Half-time, Broken, Shuffle with rim) to load into the Pattern page as a start

## Release 1.0.0 (2026-10-16)

- [x] Repository: github.com/sbegueria/fm1-dub (`origin`; AMB-1 kept as `upstream`)
- [x] Release identity FM-1_8XXYYZZ (AMB-1 is 7..., X0X 9...): 1.0.0 is FM-1_8010000; packages `dub-X.Y.Z.fwsc`
- [x] The site (`tools/make_pages.py`, `tools/publish_pages.sh`): landing, emulator, web installer, manual, the
      words files, the firmware; at https://sbegueria.github.io/fm1-dub/
- [x] The screen reader's words link: https://sbegueria.github.io/fm1-dub/words/
- [ ] GitHub Pages enabled on the gh-pages branch (repository Settings -> Pages), a GitHub Release v1.0.0 with
      dub-1.0.0.fwsc attached (the earlier v1.0 tag was never released)
- [ ] The phone/desktop web app (`web/app`) is still AMB-1's: not on the site; to port later
- [ ] Announce (later): Elektronauts, MatrixSynth, the FM-1 groups

## Done: a bolder screen, and the middle three ways (2026-10-16, in 1.0.0)

The user found the screen dim and its lines and features too small. They also felt the chord and
the ripples took too much of the middle.
- [x] Brighter greys and tints, ripples two pixels wide (a throw three), lane dots 6 px with
      rings to match, level and duck bars 8 px
- [x] The ping loops orbit the ripples: a dot each, a step at a time, lit as they play
- [x] HOME again on the Dub page: ripples, scope, spectrum (`proj.screen`, saved; said "SCREEN ...").
      The engine exports `en_scope` (256 samples, each the mean of 8) and `en_spec` (512 at 22 kHz).
      The scope triggers on a rising zero crossing, shows a phosphor of two traces and sets its own
      gain. The spectrum is Winamp-style: a 512-point FFT in the UI, 18 log bands (43 Hz-11 kHz),
      segment columns falling slowly, with peak caps. VU needle meters were tried first and
      dropped: they looked too realistic for a flat, schematic screen.
- [x] Test: `tests/scenarios/screen.amb`; screenshots from `tests/listen/shots.amb`

## Study: a sixth lane, one sample a piece (not started)

The user's idea: each piece has one sample of 1-4 seconds. It is thrown very sparsely by the
arrangement, or played only by hand (from PERFORM, a key). For now this is a study of where it
would fit.

- **RAM can't hold it.** RAM is 74.7 of 96 KB used and the pool 332 of 344 KB, which leaves about
  20 + 12 KB. One second of 16-bit audio at 44.1 kHz is 88 KB. The sample has to play straight
  from the SPI flash, which is memory-mapped (XIP at 0x02000000). Reading 2 bytes per output
  sample is little bandwidth; the cost is cache misses, to measure on the device.
- **Flash** is 1 MB. The app region (`app.ld` XIP) runs to about 0x8E000, and the image uses about
  170 KB of it. The calibration sits at 0xDC000, the update loader's staging area at
  0xE0000-0xFBFFF and the project at 0xFC000. That leaves roughly 0x2C000-0xDBFFF, about 700 KB.
  Some of it is the app's room to grow, so take about 300 KB for samples: from 0x90000 up to the
  calibration. All of this must be checked against the loader (what an update erases, and where
  it stages) before anything is written there.
- **Sizes, for 4 s mono:**
  - 16-bit at 22.05 kHz: 176 KB. Too big for eight.
  - 8-bit mu-law at 16 kHz: 64 KB.
  - 4-bit IMA ADPCM at 22.05 kHz: 44 KB. Eight pieces fit in about 350 KB; at 3 s each, 265 KB.
  - So: one shared bank of about 300 KB, with each piece pointing at a sample, or eight slots of
    about 36 KB (about 3 s of ADPCM each).
- **The engine side is small.** A voice with a position, a pitch (the key, or the root), a
  gain and a band-pass costs a few hundred bytes of RAM. ADPCM decoding is a few operations per
  sample, so about 1% CPU. The sample would go into the echo like a throw.
- **Getting a sample in:** built into the image (a fixed bank, the simplest), or uploaded from the
  web page over USB MIDI SysEx. The loader-style A/B scheme isn't needed: a torn sample only
  sounds wrong, but it needs a CRC check before it plays.
- **First steps:**
  - read the loader's flash use;
  - time XIP reads in the audio callback;
  - try one built-in ADPCM sample on a key in PERFORM.

## Ideas for later

- **Korg NTS-3 kaoss pad integration** (a future project) (the user's own setup; a Kaoss pad is great for dub techno).
  Send one voice to the NTS-3 and, ideally, bring it back and mix it with the rest.
  - **The NTS-3** (Korg Nu:Tekt kaoss pad kit):
    - stereo AUDIO IN and OUT (3.5 mm), SYNC IN and OUT (3.5 mm TRS), USB-C (power; MIDI over USB
      not confirmed);
    - up to four effects at once, 200 programs;
    - custom effects through Korg's logue SDK (`platform/nts-3_kaoss/`, SDK v2, Cortex-M7).
    - One retailer says an internal switch makes the sync jacks MIDI in and out (to check).
  - **The FM-1:** a stereo audio out, MIDI in over USB and the TRS jack, MIDI out over USB only, USB
    audio *to* a computer. No audio input is known.
  - **Sending one voice (doable): a "send" mode in DUB-1.** A setting (Off / Chord / Pings /
    Chord + Pings / Drums) puts that layer dry (after its filter, before DUB-1's echo) alone on the
    left channel, and the rest, with DUB-1's own echo and reverb, in mono on the right.
  - **The return, two ways:**
    1. *A mixer.* A TRS Y-splitter: left into the NTS-3; its output and the FM-1's right into a mixer.
       Works with the NTS-3's own effects; the mix happens outside.
    2. *The NTS-3 as the mixer (the elegant one).* The FM-1's whole output into the NTS-3, with a
       custom logue SDK effect that treats the left channel as the send (into the effect) and passes
       the right dry, summing both to stereo. No mixer, one cable each way, and the NTS-3's pad
       plays the send voice only.
       - To check: whether a custom unit can sit before the built-in effects in the chain (so
         Korg's own effects process only the send), or whether the custom unit has to be the dub
         effect itself, e.g. a tape echo with a band-pass in the feedback, written for the NTS-3.
    3. *Back into the FM-1:* only with an input it doesn't have: research whether the board's codec
       has an ADC and an input point (a hardware mod), or USB audio both ways through a computer
       (no longer standalone).
  - **Sync:** the NTS-3 has SYNC IN (Korg's analog sync pulses) and Tap BPM. The FM-1 has no sync
    out and sends MIDI clock over USB only. Ways to try:
    - MIDI clock over USB to the NTS-3, if its USB-C carries MIDI;
    - through a USB-MIDI host into the sync jack in its MIDI mode, if that switch exists;
    - a sync pulse on the audio (a click on the send channel's first sixteenth, which a custom
      unit could read as a clock);
    - else tap.
  - **Control from DUB-1:** if MIDI reaches the NTS-3, its X/Y and depth by CC, so the arrangement
    plays the pad: a sweep through a Break, the effect changing on a phrase.
  - First steps:
    - read `platform/nts-3_kaoss/` in the logue SDK (unit types, the chain, the I/O it gets);
    - check what the NTS-3's USB-C carries;
    - try the split mode with a Y-cable and the NTS-3's own effects.
- A spring reverb next to the plate
- MIDI CC out for the filter and the echo send, to drive other gear
- Keys as live mutes, or a held key as a manual throw

## Release 1.3.0, the musical release: the chord machine, visible, and a sound pass (2026-10-20; ready to publish)

At launch the firmware generates a full eight-chord progression (the random walk writes it into the eight slots), and
the Chords page (EDIT) shows it. Proposed: both of the user's behaviours, as KNOB 4 (Play) on the Chords page, which
today switches between Random and the slots.
- **Endless** (the default; option 1): the chord playing is always slot 1. When it moves on, all of them advance one
  slot and the walk writes a new chord in slot 8. You see the future and change it before it comes.
- **Loop** (option 2): the eight in turn, nothing new generated (the current "slots" mode, pre-filled). Removing the
  last slots shortens it. Switching from Endless to Loop freezes the eight on screen: catch a run you like.
- As now:
  - KNOB 1 picks a slot; the keys set its root, KNOB 2 its kind, KNOB 3 its type;
  - an empty slot holds the chord before it (a gap: the last one repeated);
  - Drift sets the bars a chord lasts (Hold: EDIT held moves on).
- Decided (the user, 2026-10-20): the walk's chords follow a change of key or mode (the user's stay as written);
  in Endless a gap stays a gap.
- Built: a slot is written (a kind), the walk's (no kind, the root a degree + 1, the type its voicing) or a gap
  (both 0), so no new parameters; PROJ_FORMAT 10, the same layout (9's and 8's empty slots read as gaps). The
  engine holds the slots and writes the walk's (`walk_from`, `walk_fill`, `chord_change`); the UI mirrors them
  (`en_prog_slot`, `en_prog_gen`, `prog_take`). A walk slot that becomes diminished after a change of scale is
  walked again. Tests: tests/host/amb_test.c (Loop, Endless), tests/scenarios/chords.amb.
- [x] The user tried it on the FM-1 (the builds up to build/dub-1.3.0-specks1.fwsc): "it feels more mature now"
- [ ] Publish (the user's go): push main, `tools/publish_pages.sh 1.3.0`, `gh release create v1.3.0`; then the SMK branch
- [x] **The sound pass, in 1.3.0** (decided by the user, 2026-10-20: the musical release, the chord machine and the
      sounds together): "Nail the sounds" and the sound design round below (the voices too alike, the kick, the bass,
      the hats, the echo's colour, the cloud, the crackle), by ear
- The engine: `chord_change()` already has the walk (from a degree other than home, back home 65% of the time;
  otherwise a weighted choice, IV favoured, then VII and V, never the current degree or a diminished one; now and
  then the other voicing). Slots are stored as absolute roots, kinds and types (P_CR*, P_CQ*, P_CX*); the walk would
  need to write those, or slots would store a degree.
- Added (the user, 2026-10-20): the walk's character on KNOB 4 (Home, Wander, Free, then Loop; Wander the default;
  the Dorian test: A-B-A bounces in 64 changes 22 / 2 / 5, at home 24 / 16 / 11); the modes stepped the most common
  first (Aeolian, Ionian, Dorian, Mixolydian, pentatonic minor and major, Phrygian, Lydian; the values kept as they
  were).
- [x] **The nudge and the shake** (the user's idea, 2026-10-20; tried on the FM-1: "works like a charm"): a layer's
      button held 0.5 s (ENV, ARP, LFO, SEQ) moves 3-4 of its sound values, steps up to an eighth of the range, a walk
      pulled back to the recipe (a quarter of the range away: always back); held on 1.5 s, a shake: another voice or
      kit (not the chord's Noise) and every value a big step. FX too (the space: echo, feedback, reverb, size, cloud; its
      shake a new echo time). EDIT held on 1.5 s: new chords (the walk's all walked again). The bass's groove (2026-10-20):
      its shake moves it to a neighbour; Evolve above 60 moves it now and then by itself, back towards the piece's
      (the user's earlier idea of slow groove changes within a song). Evolve above 40 nudges by itself on a phrase's first
      bar (1-2 values, half steps; never a shake). The help cards removed to make room for the hold.
- [x] **The pages and controls** (the user, 2026-10-20): the round in the panel's order; the levels in PERFORM's mix only,
      the freed knobs Throws, Ghost, Notes, Hats; OCT− / OCT+ a layer's level and mute (a level ring in the middle);
      format 13. The noise muddier (a worn cartridge). The screen: a layer's page shows that layer alone (ripples,
      scope, spectrum, its lanes in colour, the rest grey); the ripples' outlines the wave, the echo's repeats measured,
      the noise's clicks as specks; a knob's help line while it turns. Fixed: the grid flickering over the Chords and
      Pattern pages; line() overshooting a short slope (an endless loop).
- The videos for it: see "Videos to make" below.

## Videos to make (the channel's full plan: build/film/channel/CHANNEL.md)

- **After 1.3.0, with its firmware** (the user, 2026-10-20). Each one long, with Serena's voice-over, made in the
  simulator like the 1.2.0 video:
  1. **The chord machine** (4–5 min):
     1. a scale's seven chords;
     2. the walk's rules: home, the pull back, the favoured degrees, the voicings; Home, Wander and Free;
     3. Drift;
     4. Endless on the Chords page, the chords flowing in;
     5. steering it: edit a coming chord, make a gap, catch it in Loop, shorten it;
     6. what makes it generative: rules, chance, limits you set.
  2. **The modes** (music theory): the eight scales DUB-1 has, in its order (Aeolian, Ionian, Dorian, Mixolydian,
     the pentatonics, Phrygian, Lydian); how each sounds over the same groove; why dub techno lives in the minor
     ones; changing the mode live.
  3. **The chords** (music theory): a scale's chords as stacked thirds; kinds (major, minor, sus, dim, aug) and
     types (6, 7, maj7, 9, 11, add9); DUB-1's voicings (the open 7th, the 9th without its fifth); writing your own
     on the Chords page.
- Done: the 1.1.0 walkthrough, the 1-hour night video, shorts 1-9, the 1.2.0 launch video.

## To talk about (the user, 2026-10-21, after playing 1.3.0's sparse build on the FM-1)

- **Structured randomness for the drums and the bass.** The user liked the sparseness (and the muddy echoes), but
  thinning the drums and the bass at random "is not very dubby". Find other sources of more structured randomness:
  e.g. patterns that thin in a phrase-long shape, call and response between the kick and the bass, a sparse bar that
  comes back on the same beat, probabilities tied to the phrase's bar and the section, Euclidean thinning, motifs that
  return. Talk it through before building.
- **The noise machine, again.** "Too loud, and it sounds mostly like... simply noise" (zeroed in most pieces). Its
  reference: https://youtu.be/bbfI2fvCerc, the talk and its music example (build/research/dubtechno/REFERENCES.md #4;
  a study under way).
- **The filter, played.** The user enjoys playing the DJ filter most: a long, pleasant listen low down, then opening it
  up for a while. Worth building on (a slow filter journey? the filter's range, its feel at the low end).
- **A future set of pieces** in the vein of the listening list's ambient electronica (Max Cooper's "Parting Ways",
  REPORT3-partingways.md): an ostinato mode, the Tempo down to 75-80, real stereo width; and re-evaluating the eight.

## 1.4.0 (on the dub-1.4 branch; 1.3.0 released 2026-10-23, v1.3.0 at e6fe7b8)

The UI plan (build/design/UI-REVIEW.md; the user: "I agree with the plan"), and first:
- **The Filter on SELECT** (the user's idea, 2026-10-23: "I never use SELECT to move between pages ... second to
  volume, the most important and frequently used is the filter"): SELECT turns the DJ filter on every page (the
  same value as PERFORM's KNOB 1, which keeps it), a notch at Off, a big ring and "FILTER SHUT 40" while it turns
  (the reader says it); HOME held + SELECT still the tempo; the pages by their buttons only (a modifier for
  stepping, later, if a reader's user asks). The Dub page's KNOB 1 freed: HOME's first page the Arrangement
  (Evolve, Motion, the dub mixer, Drift).
- The pace: in steps (the user, 2026-10-23): the Filter on SELECT (built), then A (each layer: its part, then its
  voice; Change per layer), then B (HOME: Arrangement, Song, Key) with song structure and the dub mixer, then C (the
  space together). Each step tested on the FM-1.
- **The big ring for every knob** (the user, 2026-10-23, after the Filter on SELECT: "I like the large circle ... as
  when we use OCT ... take it further. The screen is a bit cluttered and it's difficult to see, when you rotate a
  knob, what / where the change is, because it is so small"). Turning any knob: its value as a big ring in the middle
  (as the level ring and the filter's), its name under it, the value inside, in the page's colour; fading a second
  after. Its kinds: a 0..100 knob (an arc from the start), a centred one (from the top, the notch at 0), a switch
  (big pips or the name: Groove "Offbeat", Echo time "1/8."), a knob with marks (Motion's 40 and 70 on the big ring,
  where they can be read). And the screen less cluttered around it: what to keep, what to drop (to look at with the
  UI review: the lanes, the level bars, the duck meter, the knob strip's rule and help line).
- **A layer's page shows its own lane only** (the user, 2026-10-23: "I don't need to see the whole pianoroll ... show
  only the line that matters, the current voice, even make it bigger ... a good way of visually showing the changes
  in the pattern when we move the groove knob"). In place of the six lanes: the page's layer large, in its colour, the
  playhead through the bar: the chord's stabs (each's strength, the throws), the pings' loops (the notes at their
  pitch), the bass's groove (each note as long as it holds; Groove and Density seen changing), the drums in three
  thin rows (kick, shaker, open hat), the noise's 32 steps. The B bars (4 and 8) shown as they change. The Dub, Space,
  Setup pages and PERFORM keep the overview. On a pattern knob (Groove, Density, Hits) the big ring might step
  aside: the lane itself the feedback. A first piece of the declutter.
- **A bass sequencer page** (the user, 2026-10-23, for the future): the bass's own line written by hand, as the
  drums' Pattern page: a page after the bass's FM page (LFO a fourth time), the white keys the 16 steps (on / off), a
  step's note (the root by default; the fifth, the octave, another degree of the scale) and its length (tied steps),
  Auto / Pattern as the drums' Play; following the chord as it moves (the steps as degrees, not fixed notes). In dub
  the bass seldom changes and mostly plays the root: so the root first, the rest a step further. Its lane (the Bass
  page's own) shows it.
- **A MIDI follow mode** (an arranger; a user, Erich, a guitarist with a MIDI guitar: "takes the played chords as
  they are and arranges the other instruments around it"). Today MIDI in, channel 1, toggles a note in or out of the
  scale (and the clock). Ideas: the chord played (the notes within a short window) becomes the chord playing,
  latched till the next, the change on the next beat or bar; or the notes played as the allowed notes (his acidbox
  way: the scale from what you play); a MIDI message for the root and the key. Asked him: his guitar's channels (one
  a string?), follow the chord or only the notes, when a change lands. A setting on the Song or Key page (B).

- **The LFO as structure** (the user, 2026-10-23, for the redesign, step A): "The LFO is an important part of the
  structure of each voice, and I would put its knob next to the groove one." The Bass's pages, the user's layout:
  first (structure) **Groove, LFO, Density, Fifth**; second (its sound) **Octave, Shape, Gate, Glide**; third (the
  FM voice) **Algo, Ratio, Feedback, Bite**. The other layers alike: the LFO among the part's knobs, the FM page
  without it. Built already (1.4.0): the bass's LFO moves Shape (±25 around the knob's), not the Bite (then gone,
  REPORT10: the bass has none); the chord's sweeps deeper (±2.5 octaves at 100, on a curve so the pieces' 20-30 stay as
  they were).
- **The LFOs, from REPORT10** (build/research/dubtechno/REPORT10-lfo.md, 86 tracks, 2026-10-24): the records move
  mostly by slow drift (60-87% of their slow movement; DUB-1 22-40%), rarely by a bar-locked LFO, never faster than
  a bar, the bass never. Proposed: a Drift mode (a smoothed random walk, 8-32 bars, ±0.3-0.5 oct) on the chord's
  filter and the noise first; a Rate (1-32 bars synced, free 4-60 s, Drift), default 4 bars; the noise slower and
  shallower; the bass's LFO off; the pings' reworked (barely heard now); per-stab scatter smaller. To decide with the
  user. Also found: the widener's 0.21 Hz Haas sweep combs the mono sum (about 5.7 dB on the hats): to check.
  **Built** (agreed with the user; branch dub-1.4-lfo): the chord's and the noise's LFO each a Rate (1, 2, 4, 8, 16,
  32 bars, the sine locked to the grid; or Drift: a new place within ±depth every 8-32 bars, anywhere in the bar,
  glided to with an ease, its own seeded randomness, the chord's and the noise's apart; at 100 ±0.6 octave on the
  chord, ±0.4 on the noise); defaults 4 bars; the noise shallower (0.27 of its tone at 100, was 0.67; a changed
  piece's saved depth converted); the bass's and the pings' LFO gone (P_BFDEC, P_PFDEC kept, unread). Interim pages,
  till the redesign: ENV a fourth time, Chord LFO; SAVE a third, Noise LFO (Depth, Rate; the FM pages' and Noise
  space's fourth knob grey); their middle the LFO's path as heard, the last 16 bars; the Noise pages' wave at the
  rate. Format 19 (P_CLRATE, P_NLRATE). The recipes: Chamber, Fog, Haze's chord on Drift (45), Fog and Haze's
  noise; the others 2-8 bars. Found on the way: the chord's LFO and the plate's wobble shared one variable (lfo_c),
  the LFO pulling the plate's modulation about since 1.3.0: separated (bfb8e9d). Not done: a free rate (4-60 s),
  other shapes (triangle, ramp), the per-stab scatter (still ±0.4 octave at full Evolve).
  **Then (the user, 2026-10-24):** the drift its own knob, always on, the wave over it ("in the analysis the
  brightness was following a random walk instead of a sine attached to the grid"): the LFO pages Depth, Rate (1-32
  bars), **Shape** (Sine, Triangle, Ramp up, Ramp down), **Drift**; every piece drifts (chord 25-45, noise 20-35),
  Chamber, Fog, Haze's chords on the drift alone. The noise's LFO a **low-pass on the whole noise** (hits and hiss;
  it moved the rings' tone, hardly heard): open at the top, closing up to 3.5 octaves. The trace bolder; the Noise
  lane's wave gone. Format 20 (P_CLSHAPE, P_NLSHAPE, P_CLWALK, P_NLWALK; a 19's Rate on Drift becomes the Drift knob).
  The drift's pace fixed for now (a new place every 8-32 bars; the user, 2026-10-24: "keep it fixed, I'll test it"); if
  it wants one, a pace switch in the redesign (Slow 16-64, Medium 8-32, Fast 4-16 bars).
- **The LFO seen on its knob** (the user, 2026-10-24): the Chord page's Cutoff ring shows the chord's LFO and drift,
  their reach two pale lines, just outside and inside the ring, a white dot where the filter is now, the knob's own
  dot left out (the teal's end its place) (built; the user's: "unclutter"). Next, with the
  redesign: an LFO **Destination** for the chord (Cutoff, Reso, Colour...), the moving knob following it on its page.
- **The noise's LFO seen** (the user's idea): a sine moving behind the Noise page's two rows of hits, its crest
  where the rings are brightest. (Built, fb2fb65; 1.4.0's rates: a bar's slice of the cycle, none on Drift, whose path
  the Noise LFO page draws.)

- **The pages' names** (the user, 2026-10-24): HOME's first page **Form** (Evolve, Motion, Drift: how the piece
  unfolds), PERFORM becomes **Arrangement**, the Space page **Dub** (the echo and the plate: dub's instrument). "Dub
  hold" keeps its name.
- **The Arrangement page** (the user, 2026-10-24: "the levels are of course the most important part"; the layout
  agreed):
  - The screen: horizontal LED meters, one row a part (Chord, Pings, Bass, Drums, Noise) and the master: the live
    signal left to right, a peak-hold segment, the level setting as a fader marker on the row, a muted row dark, a
    solo dimming the others; the duck as gain reduction eating segments from the right on each kick. Each row named
    (CHD, PNG, BAS, DRM, NOI, MST). Two builds to test on the FM-1, only the colours apart: classic green / yellow /
    red, and each row in its layer's colour with the top two segments amber and red.
  - The vertical strips and the duck meter gone from every other page (the ripples, scope and spectrum keep the
    colour and motion; the OCT ring shows a level as it changes); a small red dot at the top if the output clips.
  - KNOB 1 **Energy**, KNOB 2 **Mixer** (the virtual dub mixer: how much it rides the faders and the sends by
    itself; its moves seen on the meters' markers), KNOB 3 **Wet** (the master send), KNOB 4 **Duck** (the sidechain:
    the desk's compressor). The Filter on SELECT only.
  - A part's white key (1-5) held: the knobs that channel's strip, **Level, Echo send, Plate send**, and a fourth
    (**Low cut**: the user, 2026-10-24). The Chord and Pings pages' Wet move here (per-channel sends for every part). A
    tap still mutes, the black keys still throw and drop.
  - **Where we are, on every page** (the user, 2026-10-24: the strips' room freed, "an important info to have
    everywhere is an idea of where in the arrangement we are: intro, main, outro, phrase A or B"): a map in place of
    the strips and today's section word and bar dots. The phrase's eight bars as cells (the one playing lit; the B
    bars, 4 and 8, marked; a fill), the section (Full, Groove, Thin, Break; with song structure: Intro, Main, Outro and
    how far through the song), and what's coming (the next section at the phrase's last bar, as the next chord is
    shown). To design with song structure (B).
  - Not now: a three-band EQ (the voice pages have the tone), a bus compressor (the CPU).
  - **After step 1** (the user, 2026-10-24): the classic green / yellow / red meters chosen; the duck's row named
    DUCK and as tall as the others; the cards under a second everywhere (they distract; they go with the aids' off
    switch later); no big ring on the Mix (white 6 held: the notches show the change; Arrangement's own knobs keep it). The strips' freed room (the
    user: "both, let's see how it works in place"): five thin strips at the left (the meters', the noise too), and
    the phrase's bars as cells under the section (the one playing teal, the B bars ticked). Built; the song line to
    come with song structure.
  - **The lanes, seven rows, their meters at the left** (the user's design, 2026-10-24; branch dub-1.4-lanes): the
    overview's lanes (Form, Dub, Echo, Setup, Key) seven rows of 7 px, CHD the stabs, PNG the pings (new: the
    sixteenths a loop's note falls on this bar, those rung kept, those ahead from where each loop will be; the
    engine's en_lane_png), BAS, KCK, HAT (shaker and open hat), R/C (the rim pink, the clap yellow, side by side when
    both hit a step), NOI (new: the noise's hits this bar, en_nz_pat's half, a ghost dimmer). In place of the colour
    dots, a 22 px level meter a row, in its colour over a dim track: CHD, PNG, BAS, NOI the meters' (-48..0 dB, the
    Arrangement page's ballistics), KCK, HAT, R/C from their hits (en_drum_glow, 48 dB a second down; R/C yellow when
    the clap hit last); a mute dark, soloed out dimmed (KCK by the kick's mute, HAT and R/C the hats'). The voice
    pages: the same block in the same place, no notes, the page's row(s) bright (Drums: KCK, HAT, R/C), the others
    dimmed; no tags (FONT_XS's capitals taller than a row: the colours the cue); their lanes and A/B (1/2) moved right
    (the grid at x 50). The thin strips gone. The main screen's hash: the meters' lit lengths (as the strips' were).
  - **Step 1 built** (branch dub-1.4-arr, 2026-10-24): the names (Form, Dub, Arrangement; the identifiers V_DUB,
    V_SPACE, V_PERF kept), the knobs (Energy, Mixer grey, Wet, Duck), the meters (24 segments of 2 dB, -48..0; the
    peak held 1 s; the level's notch; mute dark, soloed out dimmed; GR under the master: the bass's duck while it
    plays, else the rest's), the engine's peaks (en_meter: each part's dry, every 4th sample, only on the page; the
    master into the soft clip, every sample; about 16 operations a sample on the page, 4 elsewhere), the strips and
    the duck meter gone, the clip mark a red "!" on its own red tag (Setup's grey "!n" the dropouts, apart). Two
    builds to try: build/dub-1.4.0-arr-classic.fwsc, build/dub-1.4.0-arr-layers.fwsc (EN_METER_CLASSIC=1 / 0).
    Open: the cards over the meters on a mute or solo (keep them there?); the big ring over the meters while a knob
    turns (the Mix page's notches already show it). Next: step 2, a part's white key held its channel strip; step 3,
    the dub mixer on KNOB 2.
  - **Step 2 built** (branch dub-1.4-strips, 2026-10-24; the user: "yes, go ahead"): white 1-5 held (from 300 ms, or
    at once with a knob turned) a part's channel strip, V_STRIP: Level, Echo send, Plate send, Low cut; the parts as
    the meters' rows (white 4, the kick's key, the drums; white 5, the hats', the noise); its row lit behind, its
    knobs in its colour, the big ring "CHORD ECHO", the reader "Chord strip page: ..."; let go, back; a tap still
    mutes, on its release now. The sends (P_CECHO .. P_DPLATE; the noise's P_NECHO, P_NPLATE): the part's own
    sends times v / 50, 50 bit for bit as before on all eight pieces; the bass's echo 0 (none before); the drums'
    the hats', the rim's, the clap's (the kick none); a throw whatever the send. The Low cuts (P_CLOW .. P_NLOW): a
    one-pole high-pass before the sends, off at 0 (not run), ~20 Hz at 1 to ~600 Hz at 100; the chord's after its
    own ~160 Hz; the noise's on its rings (their quarter rate) and its clicks; the rumble its own. The chord's and
    the pings' Wet (P_CGHOST, P_PGHOST) kept as each piece's dry balance, on no knob, Arrangement's Wet still
    moving them (a split into the sends would have been lossy; the sends over them are the strip's). The Noise
    space page gone: its Plate and Echo the noise's strip, its Width the Noise voice's KNOB 4. Format 23. Host CPU
    at the defaults ~+1% (the Low cuts' branches, spread over the parts; within the timings' noise), all at 50
    ~+1.5%. RAM 93,052 of 98,304. Open: white 5 taps the hats' mute but holds the noise's strip (the meters' rows;
    the noise has no mute key here): to see on the FM-1.
  - **Six parts on white 1-6** (the user, 2026-10-25, after dub-1.4.0zj: "there's no way to work on the hats ... we
    might need now to include the noise as a white key ... get rid of the sixth white key as a way to access the
    parts volume"; "I'd like to act separately on the kick and the hats, so for instance I can throw the hats to the
    echo"; "we now need an empty white key to distinguish the macros from the voices"). Built (branch dub-1.4-strips6):
    white 1-6 the chord, pings, bass, kick, hats (with the rim and the clap), noise: a tap a mute (the noise's
    EN_M_NOISE its own now), held its strip; the Mix (white 6 held, V_MIX) gone, its view number kept unused; white 7
    (E4) dark, the gap; the moves on white 8-12 (F4 G4 A4 B4 C5), as they were otherwise (the user: the macros to be
    rethought later); black 1-5 the solos as before (a solo quiets the noise too now: 0x3F); **black 6 (F#4, just
    above Throw): the hats thrown** (EN_PF_HATTHROW: the shaker's, the open hat's, the rim's and the clap's echo sends
    pushed while held, whatever their strip's Echo, the echo's feedback +0.08, in over ~15 ms, out over ~60 ms, the
    tails left to ring; the kick never: +7 dB of tail on the hats alone, 0.1 dB on the kick alone). The Drums strip
    split: the **Kick strip** (P_KLVL, its own fader x1 at 50, +-12 dB as Hats level; P_KECHO, P_KPLATE, its sends,
    0 none as before, 100 a full send; P_KLOW its Low cut, the rumble hearing the kick before it, so never cut) and the
    **Hats strip** (P_HATSLVL, the Hats page's Level, the shaker and the open hat; P_DECHO, P_DPLATE, P_DLOW, the
    drums' that were: the hats', the rim's, the clap's). P_DRUMS stays the Drums pages' OCT level over both (the two
    faders apart from each other); the rim and the clap in the Hats strip's sends and Low cut but not its Level (most
    recipes' Hats level isn't 50: scaling them would have changed every piece). The meters six rows and the master and
    the duck (CHD PNG BAS KCK HAT NOI MST DUCK, 14 px apart now), the engine's drums meter split (the kick its bus, the
    rest the mix's diff). Format 24 (8-23 read; a 23's drums Low cut the kick's too, as it sounded). Bit for bit on all
    eight pieces against dcf886e; host CPU ~0% (-0.3%, the timings' noise); RAM 94,248 of 98,304 (+1.2 KB: the kick
    before its cut, for the rumble's ear). Open: a noise solo (no free black key over white 6: C#4 and D#4 are the
    kick's and the hats' solos); the lanes' KCK and HAT meters still from the hits (the Arrangement page's from the
    sound).
  - **The moves rethought** (the user, 2026-10-25: "implement the moves rethink"; branch dub-1.4-moves): black 1-5
    each the part of the white key on its left (F#3 the chord, G#3 the pings, A#3 the bass, C#4 the hats, D#4 the
    noise; the kick, B3, none: no kick solo or throw), a tap its solo (the noise's new), held past 300 ms **its throw**
    into the echo while held (the chord every stab, EN_PF_THROW; the pings, the bass, the noise their echo sends pushed
    as the hats' were, EN_PF_PTHROW, _BTHROW, _NTHROW; the feedback +0.08; the tails left to ring: the hats +10.9 dB of
    tail, the pings +10.0, the noise +5.2, the bass +1.9, the echo taking nothing under ~350 Hz); white 8-11 the global
    moves (Open, Shut, Drop, Echo; THROW off the whites); white 12-16 five of the grid's effects held (Time x2, Freeze,
    Tape stop, Stutter, Siren; with the grid's); F#4 up nothing. The moves now in the command's 16-bit field (the
    noise's throw bit is past 8: the 8-bit field would have dropped it). The dub mixer's ride on a held strip's knobs
    bolder (the user: "brighter and thicker"): its band near white, over 3 px outside the ring and inside it, the dot
    bigger on a dark rim. No keys pressed: every piece bit for bit as before.
  - After the LFO work is merged (both touch ui.c).

- **Song structure, the user's answers** (2026-10-24; the records' intros and outros in REPORT9: 3-37% of the bars,
  layers coming in over a steady pulse, atmosphere first or pulse first, the hats last, outros the mirror):
  each piece chooses its own intro (atmosphere first or pulse first); a song ends in silence (or, as an option,
  goes on to the next piece); Endless starts full, as now (a user can save a piece in a state good for a gradual
  start). The plan: build/design/SONG.md (Endless, Song, Set; Intro, Main, Outro in bars, a phrase a click; the
  outro now on HOME + PLAY; Set stops after the eighth piece; on HOME's Song page: Mode, Intro, Main, Outro).
  - **Built** (branch dub-1.4-song, 2026-10-25): Song mode (P_SMODE, global as Tune) on GLO's second page (Setup,
    Song mode: the least disruptive, Setup's four kept); HOME's Song page between Form and Key (Intro, Main, Outro in
    phrases, shown as bars; Kind, Ambient or Pulse, each piece's recipe); format 26. The engine: the parts' faders
    (EN_NPART, a gain each, ramped a block at a time, at most a whole fade in two bars: the dub mixer's next), the
    song's places (en_song_ph, its bars), the intro (entries in the Kind's order, off the phrase lines, 2-4 bar fades;
    the kick One, Half, Four; the hats' hits growing; the chord's filter an octave under; the echo wetter; no
    sections), the outro (the hats out first .. the chord and the noise last, thrown into the echo), the end (PLAY
    off); outro now (HOME + PLAY) in any mode, again back to the main (four phrases at least). Set: the next piece
    from its intro 6 s after a song's end, through the last (Haze), then it rests (read "after the eighth" as "after
    the last piece"). The screen: INTRO / OUTRO, the song line under the bar cells, the Song pages' picture.
    Endless bit for bit on all eight pieces; the host CPU unchanged in Endless, lower in a song.
    **Pending:** the "where we are" map's last piece (the song line's room and the phrase map, with the bigger
    visualisations); the dub mixer on the same faders; a per-piece intro length (all 32 bars now); a song's own
    variation (each run its entries drawn anew: by the seed).
- **The header and the big ring** (the user, 2026-10-24): the header the piece's name alone (the scale gone; a
  knob's or a level's name not there any more: its ring names it); the ring's name over it, large (FONT_C), the
  value bigger in its middle, the ring a little lower (BIG_CY 64), the page's name stepping aside meanwhile.
  Built. The Chords page's name top right (it lacked it). Still in the header: the event messages (ADDED F#, KICK
  OFF...); maybe over the middle later.
- **The Chord's pages, agreed (built: below):** Chord (part) Density, Throws, **Change** (how long a chord lasts:
  Form's Drift, here for now; it belongs on the Chords page, no room), **Vary** (how often the stab pattern turns:
  Evolve's, the chord's own); Chord voice Cutoff, Reso, Attack, Decay; Chord FM (for every voice; only Colour on
  the non-FM ones) Algo, Ratio, Feedback, Colour; Chord LFO Depth, Rate, Shape, Drift, ALGORITHM the Destination.
  Form: Energy (KNOB 1, the Filter on SELECT only), Evolve, Motion, and a free fourth. Wet to the channel strip.
  "Vary" the word for each layer's how-often-it-changes knob. Song Mode (Endless, Song, Set) on GLO; the Song page
  on HOME: Intro, Main, Outro, the intro's kind.
  **Built** (branch dub-1.4-chord, 2026-10-24): the four pages as agreed (V_CSND named Chord voice); Change the same
  P_DRIFT; **Vary** (P_CVARY, 0..100, 50 every recipe's): the turns' odds Evolve's times 0 .. 1 (at 50) .. 3 (at 100),
  and the gaps (a stab left out, a bar with none) as Evolve's up to 50, none at 0, so Vary 0 holds the pattern still
  at any Evolve; at 50 the engine bit for bit as before (every piece's sound the same, the tests' bars changed the
  same); the **Destination** (P_CLDEST: Cutoff, Reso, Colour; ALGORITHM on the Chord LFO page, its big ring
  DESTINATION, the rule TO CUTOFF, the trace's line "4 BARS + DRIFT ON RESO"): on Reso and Colour the LFO goes as
  far along the knob as along the Cutoff's (an octave a 5.5th of the sweep: ±45 of 100 at Depth 100), within the
  knob's ends (the resonance never past the knob's own top); the saws' Colour, their detune, retuned a block at a
  time; the moving knob on the destination's knob, on its page. The Chord FM page's Colour turns on every voice
  ("IS NOT FM: ITS COLOUR ALONE"). Form: Energy, Evolve, Motion, the fourth free. Format 21. A nudge shows the page
  whose knob moved most (Colour: Chord FM). **Pending:** the chord's Wet (P_CGHOST) has no knob now: each piece
  keeps its own (the Arrangement page's Wet still moves it); it goes to the channel strip (step 2).
- **The layers' pages** (the user, 2026-10-24: as the Chord's; the empty places kept, "for coherence"; the pings'
  Octave on their voice page, as the bass's; the rumble back for a second try; the chord's Cutoff centred, "a filter
  cutoff, so it should sit at zero, negative and positive, like the main filter"). **Built** (branch dub-1.4-layers):
  Pings (Density, Vary, -, Cloud for now) / Pings voice (Octave, Tone, Length, Bend) / Pings FM (Algo, Ratio: the
  sound page's, the modulator's; Feedback; Upper: the FM page's Ratio, operators 3-4, grey on two-op); Bass (Groove,
  Vary, Density, Fifth) / Bass voice (Octave, Shape, Gate, Glide) / Bass FM (Algo, Ratio, Feedback, Bite: every
  voice's, on Sub the only one, "IS NOT FM: ITS BITE ALONE"); Drums (Kick, Hits, Vary, -) / Kick (Tune, Length,
  Punch, Rumble: EN_RUMBLE 1, its sound still to work on) / Hats (Level, Tone, Length, -: V_HATS, SEQ a third time) /
  Pattern (a fourth); Noise (Density, Vary, -, -) / Noise voice (Air, Tone, Decay, -: V_NVOX, SAVE again) / Noise
  space / Noise LFO. **Vary** per layer (P_PVARY, P_BVARY, P_DVARY, P_NVARY; 50 every recipe's), as the chord's:
  Evolve as that layer feels it (ev_of: Evolve times 0 .. 1 at 50 .. 3 at 100): the pings' loop moves (their rests
  capped at Evolve's, as the stabs' gaps), the bass's plan (its own hold, holdb; its octave note), the drums' (the
  kick's hold and its wander, kick_draw's lam; hats_evolve; the clap's odds), the noise's groove (its keep-or-draw
  odds times Vary, the recipe's Change and Evolve its 50). At 50 bit for bit as before (all eight pieces, Evolve 80,
  400 s, the rumble off for the comparison). The chord's **Cutoff** knob P_CCUT (-50..+50, a step 0.055 octave, as
  P_CUTOFF's), P_CUTOFF the recipe's base on no knob; the LFO's reach on it. The pings' Wet off the page (for the
  channel strip, as the chord's). Nudges open the page whose knob moved most among the layer's (voice, FM, Hats, the
  Noise page). Format 22 (a Cutoff you'd set kept as its offset from the recipe's). Host CPU: the pages' change
  nothing; the rumble on +1-2%. RAM 93,792 of 98,304 (the rumble's buffer back, 1.4 KB).
  **Pending:** ~~Cloud to the space pages (with Width to the master: proposal C); the noise's LFO Destination~~
  (both built, dub-1.4-space, below).
- **The space pages and the noise LFO's Destination** (the user, 2026-10-25: "go ahead with 1 and 2"; built,
  branch dub-1.4-space). The Echo page (FX again) **Blur, Tone, Size, Cloud** (Cloud from the Pings page, now
  Density, Vary and two empty). **Width**, the whole mix's stereo field (P_EWIDTH), the **Arrangement page's
  KNOB 1** (the user's, changing a first plan for the Form page's fourth): Arrangement = Width, Mixer (then grey),
  Wet, Duck; **Energy on the Form page alone** (one value: it was on both). The Noise LFO page's ALGORITHM its
  **Destination** (P_NLDEST, format 25): Filter (the low-pass folded into the noise's own filters, 69e6aff),
  Tone or Air, within their knobs' ranges (a quarter of the sweep a unit of the reach: Depth 100 and Drift 100
  together, half either way); the moving dot on the Noise voice's Tone or Air (the Filter no knob: the LFO page's
  trace shows it); the character on the other Noise pages. Bit for bit on all eight pieces with the defaults;
  the host's CPU the same.
- **The virtual dub mixer** (the user, 2026-10-25: "start the dub mixer"; built, branch dub-1.4-mixer). The
  Arrangement page's **Mixer** (P_MIXER, KNOB 2, the piece's; format 27): how much the desk rides itself. From
  REPORT9 and REPORT10: the records' movement slow and uneven (60-87% drift, nothing faster than a bar), the filter
  swept about twice a minute, the levels riding with the arrangement, the hats in slow 30-44 s sweeps, 24% of the
  changes on an 8-bar line (DUB-1's 46%). Twenty rides (each part's level, echo and plate send; the chord's filter;
  the echo's feedback), a new place every 10-60 s drawn at any sixteenth and glided over most of the wait, their
  reach Mixer's (at 100: levels -1.5 .. -7 dB / +1 .. +3, sends -8 .. +6, the filter +-0.55 octave; the kick's level
  the least, its sends none); rarer moves, one at a time, about one a minute at 100, half on a phrase's line: a part
  out and back (the pings, hats, noise out; the chord and bass -12 dB; the kick only over 75) or its echo swelled
  (+4 .. +8 dB, the feedback with it). Offsets times the song's faders, its own random draws; not in a Break or Thin,
  nor a song's intro or outro (half depth there); Lock and dub hold rest it. Mixer 0: bit for bit as before (all
  eight pieces, Endless and a song). The meters' notches and a held strip's knobs show the rides. Recipes: Basic
  25, Steppers 20, Pressure 30, Concrete 35, Chamber 45, Night Drive 55, Fog and Haze 60. Host CPU unchanged; RAM
  +552 B. Renders: build/mixer/ (Fog and Basic at 0, the recipe's, 100). To judge by ear: the rates, the depths,
  how often a part leaves.
- **The variety study, its first step** (the user, 2026-10-25: "the first step is cheap, let's do it";
  build/design/REPORT-variety.md, build/design/REPORT-variety1.md): no format step, no knob moved.
  - **Energy leans, it doesn't lock:** past -25 a Groove Motion picks comes as a Thin, past +25 a Break as a Groove;
    Motion's sections work at any Energy (it fixed the phrases Thin or Full before).
  - **The filter's sweep the slow movement's, not Motion's:** its depth (and the stabs' filter's, the echo heads',
    the band-pass's wander) from the chord LFO's Drift and Depth (0.015 a point of Drift, 0.005 of Depth, to 1; a
    Break 0.6 at least, a Thin 0.4, a Groove 0.3, eased over two bars); a Full's sweep a new place every 4-14 bars,
    off the phrase's line (it was redrawn on every phrase's first bar). Motion is the arrangement alone.
  - **The recipes against their models** (the suite's measures, two seeds, before/after in the report): the loop
    calmer (Fog, Pressure, Haze, Chamber, Basic, Night Drive), the sound moving more (Drift, Depth, Mixer up), Change
    64 bars on Concrete and Haze (32 on Night Drive).
  - **Pending, for discussion:** the bigger proposal (Vary, Drift, Motion on the Form page; the layer Varys as trims;
    the kick's short drop-outs). Decided: build/design/VARIETY-PLAN.md (decisions 1-11), built a step at a time.
- **The variety plan, step 1: Vary and its trims** (VARIETY-PLAN.md decision 7; branch dub-1.4-vary, format 29;
  build/design/REPORT-vary.md). **Built:**
  - Form's KNOB 2 **Vary** in Evolve's place (P_VARY, Evolve's index and value kept: no sound change at the recipes),
    continuous, 0 the loop held (the kick keeps its shape, the plan, the stabs, the loops, the noise's groove all
    still; a section still draws its kick).
  - The layers' Varys **trims** on it (P_CVARY, P_PVARY .. P_NVARY, -50..+50; the layer's amount Vary + trim, 0..100;
    0 follows, -50 **Hold**: that layer never turns). A centred ring (the knob's notch at 0), Hold's place marked at
    the ring's start and the word inside it. The recipes' turned into the trims that sound the same (Evolve times
    the old factor: Chamber -17/-12/-15/-15, Steppers pings -14 drums -21, Fog chord and pings -16, Night Drive bass
    and drums -16, Pressure four at -15, Haze four at -12).
  - **Evolve's own nudges removed** (by hand, a layer's button held, as ever). The bass's **borrowed groove**
    continuous: as the square of the bass's amount (100: 7 phrases in 400, as Evolve 100 had it; no threshold at 60).
  - **The looseness the piece's own** (P_LOOSE, on no knob; Evolve's 2.5 times, to 100, before): Basic, Fog,
    Concrete, Night Drive 100, Steppers 88, Pressure 75, Chamber 63, Haze 50. It sets each stab's cutoff, the hats'
    rests and lean, the bass's late notes and the throws (Vary 0 no longer stops the throws: Looseness does).
  - Format 29: 27 and 28 read (their layout), every older one as before; Evolve kept as Vary, each saved Vary the
    trim of the same amount (0 Hold), the looseness the recipe's; the host's store28 (and store21-27 writing the old
    Varys); the upgrade scenario a 28 project.
  - **To judge:** Vary's 50 and 100 against the classics and Coldest Season (still Evolve's scale: step 5 sets the
    recipes against their models); whether the drums' Hold should keep the kick through the sections too.
  - **The user's answers (on zzn, 2026-10-26):** Vary's scale fine; the drums' Hold not covering the sections fine;
    the clap's 3% at Hold fine; the trims keep the name Vary. Vary 0 must mean no variation at all: the throws stop
    there (built in step 2, below).
- **The variety plan, step 2: Drift and the LFO pages** (VARIETY-PLAN.md decisions 1, 2, 4, 9, 10; branch
  dub-1.4-drift on dub-1.4-vary, format 30; build/design/REPORT-drift.md). **Built:**
  - Form's KNOB 4 **Drift** (P_MOVE, 0..100, continuous): one amount for the sound moving. The chord's and the
    noise's LFOs (their wave and their random wander, half each), the filter's slow arc and the stabs' and the
    plate's slow wobble (sl_d: 2 x Drift to 1 at 50, 1.3 at 100), the echo's heads, the feedback's lean and the
    second head's moves (Motion's before: Motion is the arrangement alone now), the dub mixer's slow rides (the
    Mixer's before). The second head's moves draw from their own randomness: Drift turned leaves the music's
    choices as they were.
  - **The LFO pages, layout A: Trim, Rate, Shape, Target.** Trim on Drift, centred (-50..+50: x1 at 0, Still at
    -50, x2 at +50; the step 1 trims' look, the far left marked and named). The chord's reach a(2.3 - 0.6a)
    octaves (a = Drift x Trim, 0..2: about 1 octave at Drift 50, 1.7 at 100, 2.2 at most), the noise's a. Shape:
    Sine, Triangle, Ramp up, Ramp down (each with the wander) or Drift (the wander alone). Target (the old
    Destination, on ALGORITHM too) on KNOB 4 ("Target": "Destination" too wide for the cell). The trace at one
    scale (the most either reaches), so Drift and the Trim show. The old Depth and Drift (P_CFDEC, P_CLWALK, P_NLFO,
    P_NLWALK) unread, kept in the format.
  - **The Mixer knob gone** from the Arrangement page (KNOB 2 empty). Its moves (out and back, swells) stay at the
    piece's value (P_MIXER, on no knob) until Motion takes them in step 3.
  - **The throws on Vary** (the user's, from step 1): the chance times the chord's amount (Vary + its trim) over
    P_TVARY, the amount the piece's throws are set at (its recipe's, on no knob): none at Vary 0 or Hold, as before
    at the recipe, up to 3x.
  - The recipes from their old knobs by the same rule as old projects (en_move29): Drift from the slow arc's depth
    (or the Mixer's rides where the arc was full), each Trim the reach its LFO had: Basic 41 (-2, +5), Chamber 27
    (-25 wander, +50), Steppers 35 (+11, +21), Fog 72 (-12, -15 wander), Concrete 80 (+50, -6), Night Drive 50
    (+12, +15), Pressure 43 (+3, -3), Haze 41 (-25 wander, -13 wander).
  - Format 30: 29 read; every older one through it; the host's store29; the upgrade scenario a 29 project.
  - **To judge:** the fixed half-and-half share of wave and wander (Concrete's wave smaller, its wander bigger);
    the rides now on Drift (Fog and Concrete ride less than at Mixer 90, Chamber much less than at 55: the arc's
    depth chose their Drift); "Target" for Destination.
  - **Then (the user, 2026-10-26):** the Trim named **Drift** (as the layers' Vary on Form's Vary), the Shape Drift
    **Wander** (ceab43b); and **layout B** (dub-1.4-lfovis, still format 30; build/design/REPORT-lfovis.md):
    **Drift, Wave, Rate, Target**. Wave (P_CLWAVE, P_NLWAVE, in the Shape's slots): the wave's share against the
    wander, 0 the wander alone, 50 half each, 100 the wave alone, continuous; the wave always a sine (the other
    shapes unused in dub; the balance matters more). The recipes' Wave the share their old Depth and Drift gave the
    wave: Basic 51/56, Chamber 0/55, Steppers 59/60, Fog 48/0, Concrete 76/64 (its bigger wave back), Night Drive
    63/62, Pressure 56/50, Haze 0/0 (chord/noise); the same rule for old projects (en_move29); a format 30 save from
    zzo/zzp (no PROJ_WAVE_MARK in rsv[0]) its Shape a Wave, Wander 0, the others 50 (host: store30s).
  - **The LFO pages' picture across the screen** (VARIETY-PLAN.md, the user's 1 + 2 + 3; dub-1.4-lfovis): the
    past at the left (what's heard bold, the wave's part thin, the wander's dotted: en_lfo_wv, en_lfo_wd recorded
    a sixteenth at a time), now a dot in the middle, the wave ahead at the right (dim: en_lfo_wave_part); 4, 8 or
    16 bars each side by the Rate; the bars ticked under it, the cycles (or the phrases) taller; the reach in its
    Target's units at the left edge (the chord's cutoff in Hz around its knobs', en_chord_cut; Reso, Colour, Tone,
    Air as their knobs; the noise's Filter in Hz). Host: ~8.1 us a draw (7.1 before), once a sixteenth.
  - **zzr, the user's review of zzq:** the picture without its title (the knobs say the Rate, the Wave and the
    Target), taller (rows 8..80), the wander a solid pink line (dotted was lost on the device), the wave white, what's
    heard the layer's colour; the units inside the reach on a dark patch (outside it while small). **PERFORM's black 5
    (D#4) the kick** (a tap its solo, held its throw: EN_PF_KTHROW), the noise's solo and throw gone (white 6 still
    mutes it). **The throws made audible** (the user: "no audible difference"): measured on the host, Basic, Steppers,
    Fog, each part soloed, the echo's return held two bars against not: before, the bass and the hats 11-24 dB under the
    part's own peak, the kick none; their sends went through the piece's Echo (its square: Basic's 0.3) and the strip's,
    the feedback +0.08. Now each throw on its own bus into the echo (tsend), at the part's level whatever the Echo and
    the strip (chord 1, pings 1.6, bass 3 driven through sat, as an overdriven aux, the hats 4, the kick 3), the
    feedback pushed towards 0.93 while held (the repeats stack; the loop's soft clip), the strikes not ducking the
    returns; eased in 15 ms, out 60 ms. After: every throw's echo -7..+19 dB against the part's own peak. A few
    multiplies a sample; build/design/REPORT-lfovis.md.
- **The header, where we are** (the user, 2026-10-25: the top-left block "too small and reads badly, but at the same
  time it clutters"; option A; prototype on branch dub-1.4-header, to judge on the FM-1): the phrase in the header,
  beside the piece's tag (the section's word, or INTRO / OUTRO, and the eight bars as larger cells, the B bars
  taller, the bar playing flashing white on each beat, the 8th blinking before a change; resting dim, a play
  triangle); the tempo and the beat light out of it (the tempo on the Key page and the big ring; EXT while an
  external clock leads); the battery only when low (under 20%) or charging, always on Setup with its charge; HOLD,
  LOCK, SOLO, the dot unsaved and the red "!" at its right end; in a song, a hairline along its bottom (the whole
  song, the intro and outro tinted, filled as it goes). The page's name and its n/m moved to the rule above the
  knobs (at its left; the voice, kit, character or walk at its right; always: a knob's help line is the big ring's);
  the main area's corners free for the pictures; the line to the next chord on the Walk page only. Before/after
  shots and a video in build/header/.
- **Bigger visualisations** (the user, 2026-10-24; a plan, after the song structure's own display is decided): with
  the strips gone and the header freed, the middle's pictures can grow. The ripples further up and to the sides,
  the deepest layer (whatever is on top covers them: the section and bar cells, the page's name, the big ring, the
  lanes); the scope and the spectrum wider (the screen's width, or nearly). To fit with the song line and the
  "where we are" map, so the room is shared out once.
  **Built** (branch dub-1.4-vis, 2026-10-25; the user: "start directly ... we can even think on other animations"):
  the ripples out to the sides (radius 124, ~1.8 s across, deeper as they grow), kept above the lanes (cv_clip,
  rows 0-89), a dark ground under the section's word and bars and under the page's name; the specks and the ping
  orbits wider and flattened; the scope 224 px (0.75 samples a pixel), the spectrum's 18 bands 216 px. Three
  prototype views after them on ALGORITHM (UI_NSCREEN 6), to judge on the FM-1: the **Echo trail** (the echo as tape
  from the head, the measured repeats as bars), the **Fifths** (the circle of fifths: the key, the chord's shape, the
  walk's trail, the way to the next chord), the **Tape** (two reels at the tempo, winding through a song; record and
  play heads lit). Previews in build/vis/ (MP4, GIF, still). Cost: the ripples ~7.7k pixels a frame at their
  busiest (2.2k before), the rest under ~6.5k, against the 34.6k every frame clears and sends. **Kept, all three** (the user, 2026-10-25: "I like the three new visualisations, really"); more ideas
  for later (a dub desk of light
  columns, the Mixer's rides, the song as a horizon).
- **The screens and the Walk page** (the user, 2026-10-25; build/design/VARIETY-PLAN.md items 3 and 5): built. HOME's
  views on ALGORITHM Ripples, Scope, Spectrum, Tape (the echo trail removed; a save on it or on the fifths back on the
  ripples: ui_screen_ok, the stored numbers kept, no format step). The fifths moved to a second Chords page, **Walk**
  (EDIT again): the circle of fifths (the key, the chord's shape, the walk's trail, the way to the next chord), the
  eight slots as a numbered path inside it, the chord, the walk and the slots' list at the left; KNOB 1 Walk (Home,
  Wander, Free; ALGORITHM still too), KNOB 2 Change (the Chord page's, the same), two free (Root and Mode, later?).
  **Then, the user's** (2026-10-25: "too crowded", "every chord change the whole figure changes"): the trail, the
  slots' path and numbers gone, and the circle simplified to **the root's journey**: the notes named round it, the
  root a single dot (white with the kick), an arrow from the root before to the new one fading over four bars, the
  way to the next chord round the outside; no chord shape. The Chords page (1/2) lost its mode line on top (the walk's
  character on ALGORITHM's ring, the Walk page and the reader) and gained a **keyboard** under the slots (C3-G5): the
  chord of the slot the cursor is on, its four notes as the stabs voice them, the root dotted, yours white, the walk's
  teal, a gap the chord it holds, dimmer (slot_notes; the host's expect kbnoteN, kbheld).
- **Still missing (the user, 2026-10-24: "don't forget"):**
  - ~~**The virtual dub mixer**~~ (built, dub-1.4-mixer, below).
  - ~~**The LFO's Destination**~~: the chord's built (Cutoff, Reso, Colour; dub-1.4-chord); the noise's built
    (Filter, Tone, Air; dub-1.4-space).
  - ~~The Arrangement page's channel strip~~ (step 2: built, dub-1.4-strips).

- **An install stalled** (the user, 2026-10-24, dub-1.4.0zc.fwsc, tools/fm1_install.py): "the loader stopped
  answering after 562 requests" (about half the package), the screen blank (the loader's), then "FM-1 not found";
  unplugging and replugging the USB and installing again finished it. To look into: why the loader stopped
  mid-write (a USB drop? the loader's USB poll, its retries), and whether the installer could reconnect by itself.
- **The knobs without numbers** (the user, 2026-10-25; **prototyped**, branch dub-1.4-knobs: the rule below; with
  the big ring's panel, the user's: "more conspicuous ... a rectangle with semi-transparent background"): the
  continuous knobs (0-100, centred) lose their number (the ring shows it; the big ring gives it exactly while
  turning), their rings bigger (r 12 -> ~15, a thicker arc), the knob names brighter (white) and a size up; the knobs
  whose value is a word or a unit keep it (Wander, Endless, 64 bars, Four, Minor, LP 800 Hz, Rate 4 bars, To Reso);
  centred knobs a small mark at the ring's top (the 0); a **peek**: the numbers shown ~1.5 s when a page opens, then
  fading (the encoders can't be read without moving them); a Setup option to keep the numbers always (with the aids'
  switch); the reader unchanged. Built: a ring's number hidden unless it's a number alone (digits, a sign, %): the
  switches (pips) and words/units always shown; the rings r 14, 4.2 thick (were 12, 3.4); the names white, FONT_S where
  they fit 56 px; a centred knob's 0 a tick over its ring; the peek 1.5 s fading 0.3 s on every page change; Setup's
  second page KNOB 2 **Values** (Peek, Always), proj.values in the project's reserved byte (no format step). The big
  rings (a knob's, the level's) on a panel: a rounded rectangle under the ring, its name and its help, what's behind
  darkened to a quarter (cv_shade, only while it shows), a thin edge in its colour.
- **The knob strip redistributed** (the user, 2026-10-25: "no point in removing the numbers if the space they free is not used"; built, dub-1.4-strip2): the names 2 px lower (air under the rule's page name); the rings bigger (r 16, 5 px thick, were 14 and 4.2), centred lower in the numbers' old room; three kinds drawn as they are (knob_kind): a ring (its number inside it during the peek, or always with Values: Always), a ring with its unit inside ("LP 2.6k", the song's bars, the Feedback's seconds; the figure alone where too wide), a selector (under ten positions, or named values judged by the knob's ends) as its word large with a dot an option (a bar past twelve). The big ring's title 2 px lower in its panel.
- **A dub siren engine with more options** (the user, 2026-10-25: "to the list"). Today's Siren (the grid's effect 14,
  white key G5 on the Arrangement page) is one fixed voice: a square-ish tone swept ±1.3 octaves round 480 Hz by a
  0.8 Hz triangle, faded in over 20 ms and out over 150 ms, into the mix. A proper dub siren (the NJD / "dub siren"
  boxes): its own page or a hold-page, with the classic controls: **Pitch**, **LFO Rate**, **LFO Depth**, **LFO shape**
  (triangle, square for the two-tone "nee-naw", ramp up / down for the rising and falling "whoop"), the **wave**
  (square, saw, sine), **Decay / release** (the tail as the key lets go, the pitch falling with it, the classic sound),
  maybe a **second oscillator** or a sub; how it's triggered (held, one-shot, repeat in time); and its send into the
  echo (sirens live in the echo: a high send, the feedback pushed). To design with the user: where its knobs live
  (a Siren page under FX? a hold on its key?), presets, the CPU (one voice, cheap).
- **Requests from a user, KellyOffski** (GitHub issue, 2026-10-25, "Not an Issue, more of a request list"; the user's:
  "build the filter and scales, but I'm afraid of the CPU cost; floor at 40 BPM"). **Built** (dub-1.4-kellyoffski, format
  28; build/design/REPORT-kellyoffski.md):
  1. **A filter per part**: each strip's KNOB 4, the Low cut, a centred **Filter** (P_CLOW .. P_NLOW, P_KLOW,
     -50..+50): 0 off and not run; right a high-pass, 6 dB, the Low cut's own one-pole (bit for bit), 20 Hz at +1 ..
     2.5 kHz at +50 (the old 600 Hz near +36); left a low-pass, 12 dB (two one-poles), 18 kHz at -1 .. 200 Hz at -50;
     before the sends (the echo and plate filtered too), the rumble fed from before it; the noise's low-pass in its own
     filters (as its LFO's: free). Shown "Off", "LP 800", "HP 2.5k". Host CPU, the best of 3 on all 8 pieces: off
     +0% (bit for bit); one filter on, up to ~2% (noise +-1.5%); all six on: the high-pass +0.6..+4.7%, the
     low-pass +1.3..+6.0%. The policy for the worst: while the engine sheds (en_load past 75%), the low-pass drops to
     its 6 dB stage (as cheap as the high-pass), the 12 dB back without a step. 27's Low cut read as the high-pass's
     same cutoff (a step's 0.07 octave at most).
  2. **Japanese scales**: Hirajoshi (shown "Hirajosh"), In, Iwato, Yo, Insen, after the pentatonics (P_MODE 8..12,
     last in the Mode knob's order); their chords stacked in fourths as the pentatonics'; a five-note scale's half-step
     never two neighbouring voices in a stab (the upper an octave up: a minor ninth); the pentatonics unchanged.
  3. **Slower tempos**: 40-140 BPM; the echo already kept to its tape (5.6 s): under ~43 BPM a bar's echo stays at
     5.6 s, the Dub page showing "5.6 s"; the grid's rate checked at 40, 60 and 90. Not changed: Stutter catches 160
     ms (its buffer), less than a sixteenth under ~94 BPM, as before.
  A reply drafted for the user to post (build/design/REPLY-kellyoffski.md). Open: the 12 dB low-pass and the policy to
  try on the FM-1 (the device's own load with all six on); a resonance near the extremes (the brief's "DJ
  filter's character") not built: a one-pole has none, an SVF a part would cost ~5% each (the noise's, measured
  earlier).
- **Hardware and installer** (the user, 2026-10-25; from build/design/REPORT-reddit.md: u/ZenLegume's r/MVaveFM1 post
  "All right stop, collaborate and listen", its comments and the repos they link; the findings from the repos are
  the agent's, to verify before acting). In order:
  1. **The clock**: DUB-1 never sets it, so it runs at what the bootloader leaves: 360 MHz under M-VAVE's, reportedly
     320 MHz under the community's m-bootloader 1.2 (11% slower, against peaks of 80-82%). First check it on the
     user's FM-1, then set 360 MHz at boot (felucca-bsp's fm1_clock.h, MPL-2.0, fits a GPL project) and show the clock
     on the Setup page.
  2. **FM1 Open** (M-VAVE's own open firmware) blocks other installers: detect it in both installers (tools/ and the
     web one) and say "put the stock firmware back first"; a line in the manual (fm1-base already does this).
  3. **The loader's screen and the stall**: port upstream's "UPDATING / DO NOT UNPLUG" screen and progress bar (ours,
     from 2026-10-07, is blank: the user's stalled install); then the stall itself (upstream has the same error path):
     log the last request, retry, wait for the FM-1 to come back and resume.
  4. **The supply voltages** at M-VAVE's levels, as upstream now sets them.
  5. **The hot code in SRAM, again, judged by the peaks** (ZenLegume: code from flash shows as erratic, spiky load, not
     a higher average; our 2026-10-23 test gave the same averages and evener peaks, and was dropped on the averages):
     redo it with a load-spread figure on the CPU page.
  6. M-VAVE's compiler and link flags (fm1_open's Build.ps1) benchmarked on the engine, with the CPU audit.
  7. DUB-1 tested under m-bootloader; M-Flasher in the manual's recovery section.
  8. After 1.4.0: whether to move DUB-1 onto fm1-base (its migration keeps the GPL) or keep copying fixes over.
  9. The SMK-37 port reviewed against M-VAVE's new smk37_open SDK.
  10. **The second core** (the user, 2026-10-25; after 1.4.0, its own investigation): DUB-1 runs on one core (the
      audio in the I2S ISR, the UI in the main loop between blocks); the chip has two. Moving work to the second
      would about double the headroom (the busiest pieces peak ~80% of one core). Ways: (a) the effects bus (echo,
      plate, cloud) on core 2, a block behind (5.8 ms on the effects only), as ZenLegume suggests one core owning the
      audio and the other taking jobs off it; (b) some voices (the four FM chord voices); (c) the UI on core 2 (the
      least DSP freed, but the engine's code no longer evicted from the I-cache between blocks). Risks: upstream's
      second-core code is beta; a reported fault of core 1 misreading RAM on some units (fm1-x0x issue #10; to
      verify); the engine is one render touching shared state (a clean boundary, a hand-off buffer, timing). Steps:
      the CPU audit first (which part to move), then a prototype of (a) on the user's FM-1 with a test for the RAM
      fault, then decide; with item 8 (fm1-base already starts both cores) and after the clock and SRAM items, the
      cheaper headroom.
  Skipped: CMake, FreeRTOS (nothing gained now). In the thread, worth answering (only with the user's say-so and
  text): the unanswered "which codebase to start from", the stability-testing question (our CPU page, the dropout
  counter, safe mode), the SMK-37 port needing a tester, and the m-bootloader clock loss.

## Release 1.4.0: what's left (the user, 2026-10-27: "prepare what's left for 1.4.0 release")
- [x] The three fixes (dub-1.4-fixes3): a Mix into an Intro None piece; a key on a gap slot (the wrong root); the
      centred knobs' notch on a slow turn (the reset 3 s)
- [x] Step 5: the recipes against their models (REPORT-models.md), and 5b (REPORT-models2.md: Vary's scale, format 35,
      the slower sweep, Fog to Drift 100); the user's ears on build/models2/*-before/after.m4a still to come
- [x] The Winter Set re-based on them (REPORT-set2rebase.md); the PRESET race it found fixed (presetrace.amb)
- [x] Step 6: the manual, README and the cheat sheets (REPORT-docs-manual.md, REPORT-docs-sheets.md) (the errors in REPORT-cheatsheets*.md, -zones, -motion; step 5's
      values; web/emu/index.html's key layout; docs/img and fm1-panel.svg)
- [x] The release notes (build/release-notes-1.4.0.md; publish the part above "NOTES FOR CLAUDE")
- [x] The user on the FM-1 with the last test build (zzz8, 88d7922: "looks good")
- [x] `./build.sh --release 1.4.0` (bb1573e's code, FM-1_8010400); tests; the site built locally (source emsdk_env and the
      venv's python before publish_pages.sh: the system python3 is 3.6)
- [x] Basic's sweep the faster pace (the user: too static)
- [x] The site published (gh-pages, 2026-10-28) (`tools/publish_pages.sh 1.4.0`, the cheat sheets)
- [x] Published 2026-10-28: main = release-1.4 (dub-1.4 rewritten without build/, the user's: build/'s reports stay
      private; KellyOffski's name in the messages), tag v1.4.0 at f79a8de, the GitHub release with dub-1.4.0.fwsc
- [ ] The videos: the release video, the shorts, the cheat sheets' videos ("The videos" below); the Winter Set
      (build/film/channel/long_videos/set2: again on the release build, with stems and more pages?)
- [ ] After: the SMK branch; the wiki

## 1.4.1: a fix for 1.4.0 (2026-10-28)
- [x] PRESETS spun fast while playing (4 clicks or more within ~40 ms each) filled the engine's queue: the piece
      arrived at played partly as the pieces passed through, the screen showing its own (a 1.4.0 regression: the
      piece spread over blocks, the CPU work). Fixed, bbdd410 (presetflood.amb). Found making the LFO short.
- [x] Shorts 60 and 63 again on it (61 and 65 spin while stopped: unaffected); thumbnails for all seven shorts
- [ ] The Mix's hole (the user: "too much lag between the end of one piece and the start of the next"; short 61:
      ~9 s of draining echo, near silence before the downbeat; the Songs video, the outro called in the intro: 20 s of
      silence): a firmware fix being made (dub-1.4-mixgap, REPORT-mixgap); then a release (1.4.2)
- [x] The Mix fixed (REPORT-mixgap.md; the user: "subtle, but sounds better… now there's always something to hear");
      the manual and the Songs sheet updated
- [ ] Later: the intros and outros themselves are quiet, so a hand-over is quiet too (the user, 2026-10-29): their
      levels and curves, to tune
- [x] Release 1.4.2 (the Mix), the user's go 2026-10-29 (the hour and the release video stay as uploaded, old Mix)
- [ ] Then short 61 and the Songs sheet video again, on the Song page during the Mix (the user's: not the Arrangement
      page), the Songs take calling the outro once the chord is in
- [x] Released 1.4.1, 2026-10-28: v1.4.1 at 861a363 (the commits since 1.4.0 cherry-picked onto main, none touching
      build/), the GitHub release with dub-1.4.1.fwsc, the site (installer FM-1_8010401)

## To discuss: the pings (the user, 2026-10-28)
"One thing to reflect upon: the pings. Most of the tracks I've analysed do not have this voice. We shall discuss
about this later." The pings are DUB-1's own layer (four drifting loops of single notes), not a trait of the classics
(build/research/dubtechno/classics/PROFILES-INDEX.md). To talk over before 1.5.0: their place and level in the
recipes, off or rare by default, or another voice in their slot.

## Next hour set: after the Munich set (the user, 2026-10-28)
"We can aim at releasing a one-hour set inspired on this set, after we discuss a few things." The model: Abyss
Pulse's "Munich Edition" (REPORT11-munich.md: dub techno's sound and filter drift over a deep tech / deep house groove,
offbeat hats and a clap, sus chords changing about once a minute, breakdowns, a bright top, the chords gated on every
corchea (8th)). To discuss first: which of the traits DUB-1 lacks to build (the top end, the hats' level, the drift's
depth, a chord gate), the pings (above), and the stabs' fast filter closing (the user: "bright at the start, then it
rapidly closes", in the classics and in this set; not measured yet).

## After 1.5.0: the voices (the user, 2026-10-29; to talk over and plan first, nothing committed)
- **What voices the references use:** an analysis of the pings (or their absence) and of a lead voice in the classics
  and the less classic (the Munich set, REPORT11): which pieces have a lead, what kind, how often. The user: the pings
  aren't always heard in the references, sometimes there's a lead voice, sometimes not; DUB-1's pings sound repetitive
  over a set.
- **A "lead" voice:** other sources beside or instead of the pings: short samples, brief melodic phrases.
- **New voices for variety:** bring in some of the other Felucca instruments.
- An important overhaul of DUB-1: talk and plan before committing to anything (after 1.5.0 and 1.6.0's plans).

## From the wiki's sources (2026-10-29, REPORT-wiki-reddit.md sections 7-8)
- **The CPU clock:** hal/fm1_clock.h only reads the clock; under m-bootloader 1.2, or a V16 boot area if its 320 MHz
  holds [?], DUB-1 would run ~11% slower. felucca-bsp's clock reset (MPL-2.0) could set 360 MHz back. Check on a unit.
- **fm1-base** (MPL-2.0, usable from GPL DUB-1): the second core with cpu2_fallback and the idle-loop pause;
  velocity and aftertouch for live play; MIDI thru and USB host; USB audio. For the playability and second-core plans.

## For the wiki, later (the user, 2026-10-29)
- The Reddit thread "Claude skill" on r/MVaveFM1 and its linked references, "very useful for our wiki":
  https://www.reddit.com/r/MVaveFM1/comments/1xh6o7m/claude_skill/ (not read yet; the user is short of tokens).

## For research, later (the user, 2026-10-30)
- **Tessera**, another generative firmware project: https://rastapopolix.github.io/tessera/ (not read yet). To check
  how it generates, what it plays and how it's played, against DUB-1: ideas for the voices overhaul and playability, and
  for the wiki if it runs on the FM-1.

## 1.5.0: sets, their cues, the web (the user, 2026-10-27)
1. **Writing and loading sets**, of fewer than eight pieces too (Set mode ends at the set's last; the other slots as
   they were): build/design/SETFILES.md, pieces 1-4 and 6.
2. **The cues:** moves at set bars in a song (SETFILES.md, "Cues"), carried by the set; then recording them on the
   device.
3. **Managing them from the web:** a page (WebMIDI, beside the installer) to back up, edit, download and send sets
   and their cues; the web emulator to hear them first (SETFILES.md piece 5).

## 1.6.0: songs and sounds (the user, 2026-10-27)
- **Saving and loading songs and sounds:** a piece on its own; sound sets (other drum, bass, chord and ping
  instruments, maybe other noise sources: SETFILES.md, "Sound sets"). **A set is pieces + sounds** (the user's):
  one file, the pieces and the sounds they play; a sound set alone loads too.

## The videos (the user, 2026-10-27)
- **For 1.4.0** (settled, the user, 2026-10-27): **two series.**
  - **The release video**, and **shorts** (vertical, under a minute): one novelty each, a hook pointing to its
    full video (the sirens, a Mix between songs, the DJ filter on SELECT, throws on the desk, Motion's zones, the
    tape).
  - **The cheat sheets' videos** (horizontal, 3-6 min): one per sheet, a walk through it on the emulator, the
    sheet's link in the description (and the sheet's page linking back); the shorts can be cut from them.
  - **First:** the release video and the sheets Arrangement, Variety, Songs and sets; the rest over time. Filmed
    on the release build (after step 5).
- **The hour video again** after step 5 (the user, 2026-10-27), with a stem for every view (not only the scope and
  the spectrum: a choice of backgrounds) and the pages varied as below. The stems: dub-1.4-stems (being built).
- **The hour videos, next time:**
  - **Stems:** the host records every view all the time beside the main screen (each its own video, frame-exact),
    to use in the edit as a full-screen background under it.
  - **More variety:** stay on the Arrangement page while cueing moves; show the Chords page or the Walk page's wheel
    at a chord change; the Song page (the song line, the parts) at an intro or an outro; and the like.
  - **Later, in the edit:** the user's own footage (the city, nature, bicycle trips) blended in the background.

## Now: two lines in parallel (the user, 2026-10-23)

After 1.3.0's release (the docs done, e20d31b..e10ddf4; a release video being made in build/film/release-1.3.0/):
1. **The CPU: the hot code in RAM** (below, "CPU: the hot DSP code from RAM"): first the test build
   (build/dub-1.3.0l-perf.fwsc, EN_PERF=1: the stall counters on the Setup page), then the FAST region.
2. **The firmware, on:** the UI review (structure from sonority), song structure (an intro and an outro), the virtual
   dub mixer, the smaller engine features, the ring marks; all below.

## Next: the classics pass (the user, 2026-10-23; after REPORT6-9, build/research/dubtechno/)

The references (the seven classics, an AI session, Outskirts) agree: dub techno's core is in time, not sound (the
loop held, the kick never leaving, an echo you hear, movement by hand rather than by section). Decided:
- **The palette** (built: the recipes after their models, the user: "do the palette pass"): the eight pieces as eight different ideas, each after one reference: Basic (Basic Channel's
  BCD), Steppers (Maurizio's M-Series), Chamber (Fluxion's Vibrant Forms), Night Drive (Intrusion's The Seduction of
  Silence), Concrete (Monolake's Hongkong), Pressure (Porter Ricks' Biokinetics), Fog (Echospace's The Coldest
  Season), Haze (the sparse intro: Parting Ways, Liumin's softer side). Their recipes as the features arrive.
- **The echo, clearer** (muddy now, some pieces especially): a clean first repeat, Blur growing with each pass; the
  returns ducked under the dry; less plate on the returns, the band-pass a little higher. Measured against
  Radiance's echo (likeness >= 0.15). First.
- **Width** (built, 6e49d06: the echo's right a few ms behind, a crossed chorus on the chord's and the pings' dry,
  the mix mono under ~80 Hz; the Echo page's Width): the user liked Width 85 over 50 in Fog and Night Drive (the
  palette's wide pieces, after Echospace and Intrusion); the narrower ones (Basic Channel's, Maurizio's) less.
  **The echo, clearer:** built, 36f899b ("sounds right").
- **Motion, three regions:** 0-40 the loop holds (Full only: the kick on four, nothing drops; the change by the
  filter, the echo, the texture); 40-70 variations (Thin or Groove now and then, 32+ bars apart, the kick half-time
  at most, never gone); 70-100 Breaks too (shorter, 4-8 bars, rarer than now). The kick four outside Thin and Break;
  the kick's shape, the hats' pattern, the bass's plan redrawn every few phrases (Evolve). Live breaks stay
  (PERFORM's Energy, Drop). The recipes mostly at 30-40.
  - **On the knob's ring:** brighter marks at 40 and 70 (the regions' edges).
  - **HOME's shake of everything** keeps Motion inside its region.
- **The virtual dub mixer** (off the 8-bar grid): slow, continuous moves of the filter, the echo's send and feedback,
  the hats' level, the noise's level, each 10-60 s, starting at any bar; the structural changes (the kick, the
  chord) on phrase lines but rarer. (Today 46% of the changes fall on an 8-bar line, the classics 24%.)
- **The kick's rumble** (built, b93e56e; **out of 1.3.0**, b517faf: "not yet to standard, we need more work on it";
  EN_RUMBLE 0, its code and its parameter P_RUMBLE kept, saved; Pressure's 35 since the user's "too much" at 60): for
  1.4.0, more work on its sound. The kick into a dark reverb under ~150 Hz, ducked by the next kick; its knob in Hat len's
  place on the Drums sound page (then all the kick's there; Hat len back to the recipes). A new parameter (a format
  change, converted).
- **Smaller engine features:** hats on the four "ands" (an auto pattern); named bass figures (BCD's x.....x.x.....x.)
  as grooves; kick shapes 3+3+2 and a four leaning on 1 and 3; a dotted-half echo time; a lower chord voicing; a
  reggae offbeat chop (a stab pattern); intro and outro (for a set or a recording).
- [ ] **A CPU audit** (the user, 2026-10-24: after the next features): how the load splits across DUB-1's sub-systems (the
  chord's voices, the pings, the bass, the drums, the noise, the echo, the plate and cloud, the master, the meters,
  the effects grid), per piece, average and peak. On the host: each render stage timed per block (a profiling build,
  en_render's blocks); on the FM-1: the same stages timed with fm1_ticks in a test build, shown on the Setup page, so
  the host's split can be checked against the device's. Then a table, and what is worth slimming first.
- **CPU: the FM-1's peaks** (2026-10-26, the user on dub-1.4.0zzj at the stock 360 MHz, its registers read: a piece
  change spiking the peak to 100%; Steppers over 100% in steady play, Pressure and Haze ~95% with dropouts, Night
  Drive heavy; Basic, Chamber gentler; Fog, Concrete fine; the screen's view 74-79 on Fog, hardly a factor). Found
  (host, host/en_prof.c: the stages block by block, the least of 3 runs): the engine only 3-10% heavier than 99fddc2
  (the state the user called good) on every piece, mean and p99.9 alike, and its ranking not the FM-1's (Steppers
  middling on the host): so the FM-1's own; the steady half of every block the mix loop (echo 11 ns a sample, plate 7
  at half rate, cloud 3: run whatever the sends), the peaks where the stabs (5-11 us a block while they sound) and the
  pings (6-11) coincide; no denormals to speak of (the engine's statics scanned: only the kick's click and the glows,
  now flushed); a piece change every parameter and the regrow in one block (+4-7 us, ~25%: the 100%). The engine's
  code 93 KB (62 when the RAM test was tried), its hot path ~55 KB against a 32 KB I-cache with the UI's between the
  blocks: the likeliest cause of a ranking the host doesn't see. Built: **A** (f310743): a piece sent while playing
  spread over a few blocks (28 commands a block, a regrow 10; only a piece's burst, a regrow in it; resting all at
  once: renders bit for bit), its worst block back to the steady peak (host 33 -> 24 us, Fog to Steppers); the audio's
  time by stage on the FM-1 itself (the heaviest block of each ~2 s and the mean, on the Setup page: PEAK, MEAN, the
  total and the four biggest stages in ms); the clock from the registers ("360 MHz (stock)"); **B** (844e476, on its
  own commit): A and the hot code in RAM (FAST 64 KB, 55,292 B in it; tick and the rest of a step's events, a
  parameter, a piece, the walk, the start in the flash: EN_COLD). To read on the FM-1 with each: Steppers, Pressure,
  Haze, Night Drive's CPU now/peak and !n, a piece change's peak, the Setup page's PEAK and MEAN lines
  (build/design/REPORT-cpu.md).
- **CPU: 1.4.0's LFO rework** (2026-10-24, the user: dropouts and clicks on Chamber and Concrete, "the CPU higher in
  general"): on the host the engine had grown 5-8% over 1.3.0's, nearly all of it the noise LFO's low-pass (two SVFs a
  sample, on in every piece since each drifts its noise), enough to take 1.3.0's peaks (80-82 on the FM-1) over.
  Folded into the noise's own filters (the hiss's and the clicks' low-passes no higher than the LFO's, the rings a
  one-pole at their quarter rate): within ~2% of 1.3.0, the sweep as deep (build dub-1.4.0zh). To read on the FM-1:
  Setup's CPU now/peak and any !n on Chamber, Concrete, Haze.
- **CPU: the hot DSP code from RAM: tried, not kept** (2026-10-23). The core's stall counters don't run on this chip
  (F--, both orders); the engine in RAM (b757564: 62 KB in a FAST region) gave the same averages (Fog 30s, Haze
  50s-60s; flash a tad lower) and evened the peaks to 80-82 (Haze, Chamber down a little, Fog up from 63-70): the
  load is the engine's own work, not the cache. Reverted (cacd975), the counters' test too. What's left for the CPU:
  the engine's work itself (the noise machine, the voices, the plate), and the safety valve (from 75%).
- **(The study, kept for the record)** (the user's peaks to the low 90s on the FM-1, none on a Mac).
  The wl82 has a 32 KB I-cache (8 ways x 4 KB, 32 B lines) over SPI flash (XIP); the engine's hot code is ~33 KB
  (render_chunk 26.5 KB with reverb, echoes, stabs, crackle, cloud inlined; render_drums 2.3, fm_sample 1.9,
  render_bells 0.9, widen 0.9, render_hat 0.8), more than the cache, and the UI between the blocks (draw_main 9 KB,
  ~60 KB of font read from XIP) evicts it: the likely cause. All internal SRAM runs code (the SDK puts its RTOS and
  decoders there: .volatile_ram_code into ram0); POOL's "(rw)" is only the linker's label. Plan:
  1. **Measure first:** the core's counters (C0_IF_UACNT, C0_TL_CKCNT at 0x1EEE200.., to confirm: instruction-fetch
     misses?) read around render_chunk in a debug build, shown on the CPU page; if misses are the peaks, go on.
  2. **A FAST region:** POOL 0x54000 -> 0x44000, FAST (rwx) 0x01C64000, 64 KB, holding .ram_text and a new .fast_text
     (> FAST AT > XIP), copied by main_fm1.c's existing loop before the guard; the PC guard's window 1 over both.
  3. **Tag** the hot functions .fast_text; -fno-jump-tables for amb.c (its jump tables in XIP); its constant tables
     into RAM too (or accept their reads).
  4. **build.py:** calls inside FAST land inside it (the flash-driver part still call-free); FAST's size limit; the
     pool's 8 KB headroom with the new size.
  Risks: the ISR's entry stays in flash; the guard no longer protects that range; wait states unconfirmed.
- **Song structure: an intro and an outro** (the user, 2026-10-23: "a big change, plan it carefully"; not now, to
  plan before building). DUB-1 is endless today: it starts full and never ends. The references: the AI session's
  intros and outros take 24% of its bars (4-58% by song); the classics' US branch 21-27% (Echospace, Intrusion);
  BCD and M-Series start nearly full. Questions to settle first:
  - **When:** always (each piece a song: in, the body, out), or a mode (Endless / Song), or played (PLAY held: an outro
    from here; at start, an intro first)? A set (one piece into the next) and a recording are the main uses.
  - **How long:** the intro and outro in phrases (2-4: 16-32 bars), or set by a length for the whole song (5, 8, 12
    minutes)? Does the body's length stay endless till you ask for the outro?
  - **The order the layers come in** (and leave, the other way): e.g. the noise and the pad first, the hats, the
    kick, the bass last (the session's and Echospace's way); or the kick first (Basic Channel's). Per piece, from the
    palette?
  - **With what's built:** Motion's regions, the held loop, the virtual dub mixer (the filter and the echo opening
    over the intro), PERFORM's Energy and Drop, Lock; the phrase plan and the kick's shapes (an intro on the kick's
    ramp from none to four?).
  - **Between pieces:** an outro into the next piece's intro (a DJ's blend, same tempo or a tempo walk), PRESET
    turned during an outro.
  - **The UI:** where it's started and stopped, what the screen shows (the song's part, bars left), how it's said
    for the reader.
  - **Saved:** in the piece (its intro's length and order), the project format.
- **Ring marks elsewhere** (the user, 2026-10-23: Motion's marks "could be used in other places"; built for Motion,
  83b1ff9). A mark wherever a knob's behaviour changes at a point, not smoothly; to build with the UI review:
  - **Hits** (Drums): 15 (the open hat comes in, on the off-beats), 40 (two more), 70 (the rim on two and four).
  - **Kick density** (Drums sound / Drums): 90 (from here four in every Full bar) and the ladder's shapes at 25, 50,
    75 (one, half, ghosted: where the draw centres).
  - **Energy** (PERFORM): -25 (from here a Groove comes as a Thin) and +25 (a Break as a Groove; the variety study: it
    no longer fixes the phrases).
  - **Notes** (Bass): 30 (under it the B bars never fill).
  - **Evolve** (Dub): 40 (the playing as loose as it gets; past it only the change's rate rises).
  - Not for smooth knobs (levels, Feedback, Width): their notch (0) or nothing.
  - Possibly two marks' colours: white for "something new comes in", red for "something drops out" (Motion's 70:
    the Breaks).
- **Later: a review of the UI:** the main ways the user shapes a piece, which knobs and where; to improve the UX and
  the clarity, separating **structure** from **sonority**.

## Releases after 1.2.0 (the user's ideas, 2026-10-19)

1.2.0 is about live performance. Then, in roughly this order:

### Sound: noise, the bass, the stereo field

- **Noise as a substantial part of DUB-1**, maybe a layer of its own (to plan: a sixth lane? see "Study: a
  sixth lane"). Start from the ideas taken from the musicologist's video ("Understanding Dub Techno",
  Bahadırhan Koçer; the list above): noise as the canvas, the backcloth (hum and filtered tape noise), the
  crackle tuned to the key and sent into the echo, noise in the kick and the sub. Also read the notes from
  his talk on dub techno; the user would like to get his book.
- **The bass:** more grooves, and slow groove changes within a song (the pattern moving over phrases, as the
  chord does with Drift).
- **The stereo field:** panning that moves (layers or echo repeats shifting between left and right,
  modulated), so the soundscape breathes across the stereo image.

### Composition: your own melodic lines

- A way to write a melodic line, for the bass or another layer (the pings, a new lead), and keep it with
  the piece. Builds on 1.1.0's ping lock and the pattern page's step entry on the keys.

### Connectivity: the output split in two

- **A split mode:** some layers on the left output, the rest on the right (mono each), so each can be
  treated on its own in a mixer (the user's Behringer Xenyx 1200) or an effects unit (the Korg NTS-3).
  The stereo field is lost in that mode, but nothing like it exists in the FM-1 firmware ecosystem; other
  firmware authors would appreciate it. The details are in "Ideas for later": the Korg NTS-3 (the send
  voice, the return, sync).


### A sidekick firmware: a second FM-1 that plays along with DUB-1 (the user's idea, 2026-10-19)

Many people own several FM-1s, mostly to keep different firmwares ready. The user is getting a second one. The idea is a
**companion firmware** for the second FM-1 that plays with DUB-1: it throws melodic lines, phrases or samples
over DUB-1's groove, in time and in key. It goes a bit beyond dub, and that's fine.

- **The link:** DUB-1 already sends MIDI clock and start/stop, and its notes (the chord, the pings, the bass,
  the drums, each on its own channel). The FM-1 sends MIDI out over USB only. It takes MIDI in over USB and
  the TRS jack. Two FM-1s therefore need a USB-MIDI host between them, such as a small host box, a computer
  or a phone. The second FM-1 then follows the clock and reads the key and the chord from DUB-1's notes.
- **To study:**
  - what DUB-1 should send for a companion: the root, the mode and the pool as CC or SysEx, the section
    (Full, Break, Thin), the phrase bar, dub hold, so the sidekick can answer the arrangement;
  - which devices act as a host at the lowest cost;
  - whether the TRS jack could carry clock from a host;
  - what the sidekick plays: phrases from a scale-aware generator, a sample player (if the flash holds
    enough), call and response with the pings.
- Related: "Connectivity: the output split in two", and the MIDI notes in section 9 of the manual.

### The screen: richer visualisations (the user's idea, 2026-10-19)

- **More information in the ripples.** The rings could carry more of the music, for example:
  - each layer's hits in its own colour or shape;
  - the echo's feedback and its band-pass in the rings' width or blur;
  - the section (Full, Break, Thin) in the field's density;
  - the master actions (Filter, Dub, Space) bending or blurring the field as they're turned.
- **More views on ALGORITHM** besides ripples, scope and spectrum. First, survey other projects for ideas that fit a
  240×240 screen and the FM-1's CPU: Winamp/MilkDrop presets, Teenage Engineering's OP-1 and OP-Z screens, Elektron's
  scopes, demoscene effects (plasma, tunnels, starfields), the Lissajous stereo scope, a tape-loop view of the echo's
  buffer, a waterfall spectrogram.

### An X-Y pad for playing live (the user's idea, 2026-10-19)

Use a touch pad as an X-Y controller for live performance: one finger moving two things at once, such as the Filter
and its resonance, or the echo's feedback and time, Dub and Space. The community would appreciate it.

- **The way in that works first: MIDI CC.** DUB-1 takes MIDI in over USB and the TRS jack, but only notes (the scale)
  and clock today. Mapping CCs to the master actions and a few parameters (a fixed map, or a "learn") would let any
  X-Y source drive it:
  - the **Korg NTS-3**, if it sends its pad as CC (to check: its MIDI implementation, and whether USB-C carries
    MIDI);
  - a phone or tablet app (TouchOSC and the like) through a USB-MIDI host;
  - a laptop's trackpad through a small app that turns it into MIDI.
- **An Apple trackpad over Bluetooth, directly: a long shot.** It's a Bluetooth HID device, so the FM-1 would need a
  Bluetooth radio and antenna, and a Bluetooth HID host stack. The chip family has Bluetooth (the SMK-37 runs
  Bluetooth MIDI on the same chip), but it's unknown whether the FM-1's board has the radio and antenna fitted. To
  research:
  - the FM-1's board;
  - the SDK's Bluetooth stack: HID host support, its code size, and the CPU it leaves for the sound;
  - how the trackpad pairs.
- **The SMK-37 already has 16 pressure pads and a mod wheel.** The pads' pressure could also play X-Y-like moves.
- First steps:
  1. MIDI CC in for the master actions (small, useful on its own);
  2. check the NTS-3's CC output;
  3. then the Bluetooth question.
