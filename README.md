# DUB-1 for the M-VAVE FM-1

DUB-1 turns the M-VAVE FM-1 into a generative dub techno machine. It plays itself
from the moment it's switched on: chord stabs through a resonant low-pass into a tape echo, short
FM pings on loops of unequal length, a sub bass on one of five grooves, a kick that ducks the rest
(four on the floor, or sparser, a phrase at a time), a shaker and an open hat, and a crackle that
rings on the chord. An eight-bar arrangement takes the kick out for a break, thins the layers and
brings them back with a fill, and now and then a stab is thrown into the echo, each repeat coming
back darker and more dissolved. The drums and the bass play in phrases, a main bar three times and
a variant on the fourth. You don't play it, you steer it: pick a piece and a scale, take notes out,
sweep the filter, push the echo, hold it, mute and solo the layers, lock the groove.

DUB-1 is a fork of [AMB-1](https://github.com/charlesvestal/fm1-amb), Charles Vestal's generative
ambient firmware for the FM-1, and keeps its platform, its panel and much of its sound engine.

![DUB-1's main screen, the Form page: Basic and FULL with the phrase's eight bar cells in the header, the stabs' ripples round the chord Fm7, the seven lanes below, FORM 1/3 on the rule and the knobs Energy, Vary, Motion and Drift](docs/img/screen-main.png) ![The Arrangement page: a desk's meters, a row a part and the master, the duck's gain reduction under it](docs/img/screen-perform.png) ![Dub hold: the rings, the chord's name and HOLD in red](docs/img/screen-hold.png)

- **Try it in your browser** (no FM-1 needed): <https://sbegueria.github.io/fm1-dub/emu/>
- **Install it** (Chrome or Edge, with the FM-1 plugged in): <https://sbegueria.github.io/fm1-dub/install/>
- **Read the manual**: <https://sbegueria.github.io/fm1-dub/manual/> (source: [docs/MANUAL.md](docs/MANUAL.md))
- **Print the cheat sheets** (fourteen, one page each, A4): <https://sbegueria.github.io/fm1-dub/cheatsheets/>
- **Download the firmware file**: [releases](https://github.com/sbegueria/fm1-dub/releases)
- To build it and listen to changes on a computer, see [EMULATOR.md](EMULATOR.md).

Installing it is at your own risk, though it's hard to get stuck: the web installer and M-VAVE's own
updater can put the stock firmware back, and an FM-1 that fails to start twice comes up in a safe mode that the
installer can reach.

## In short

| | |
|---|---|
| Keys | The scale: each key adds its note or takes it out |
| PLAY · REC · EDIT | Play / rest (in Song or Set mode a song: an intro, the main, an outro, then silence; HOME held + PLAY: the outro from the next phrase, in any mode, again back to the main) · a tap: dub hold, held: the effects grid (16 effects on the white keys) · a tap: the Chords page (eight slots: root, kind, type; Play Endless or Loop), again the Walk page (the circle of fifths: the walk's character, Change), held: the next chord, held on: new chords |
| SEL | A tap: the Arrangement page: every part's level on a desk's LED meters (the chord, the pings, the bass, the kick, the hats, the noise, the master; the duck as gain reduction); the knobs Width (the whole mix's stereo field, the piece's), Wet (the whole music dry or drowned in the echo) and the piece's Duck; white keys 1–6 mute the parts (their levels kept; held, the part's **channel strip**: its Level, Echo send, Plate send and Filter on the knobs (centred: left a low-pass, right a high-pass, the middle off), the chord, pings, bass, kick, hats and noise), black keys 1–5 solo the chord, pings, bass, hats and kick (the first four the white key on their left, D♯4 the kick's): a tap solos that part, held throws it into the echo (slammed in at its own level, the feedback climbing so the repeats stack); white 7–8 are the gap; white 9–12, held: open, shut, drop (the kick and the bass), push the echo; white 13–16, held: four of the grid's effects (Time x2, Freeze, Tape stop, Stutter); black 8–11, the dub sirens into the echo (Wail, Whoop, Hi-lo, Laser; Whoop out of tune on purpose, a box's, the others tuned to the key): a tap a short one, about a bar, held a slow one looping until let go, its own page while held: the knobs its **Pitch**, **Rate**, **Depth** and **Echo** (past half, the repeats stack), kept for its next tap or hold and saved. The keys that do something are lit; a page's button leaves it. Held: **Lock**, each phrase keeping the kick, the drums' and the bass's plan and the noise's groove |
| FX · ENV · LFO · GLO / HOME · SAVE · ARP · SEQ | The pages, as the buttons sit: Dub (the echo and the plate: Echo, Echo time, Feedback in seconds, Reverb), Chord (Density: 1 to 6 stabs a bar, Throws, Change: how long a chord lasts, Vary: how often the stabs' pattern turns), Bass (Groove, Vary, Density: Sparse, then Pickups from 30, Fifth), Setup (for every piece; GLO again: **Song mode**, Endless, Song or Set, a set the pieces in turn to the last, and the set's **Transition**: LP, a pause between songs; Segue, the next from the bar after the outro; Mix, the tempo glided to the next piece's, its pulse under the old echo) / Form (Energy, Vary: how much the loop changes, 50 the classic records' middle, Motion: how much the arrangement changes, in three zones, Breathes, Sections and Breaks, **Drift**: how much the sound moves, the LFOs, the filter's sweeps, the echo's wander and the **dub mixer**'s slow rides of the parts' levels and sends), Noise (Density, Vary), Pings (Density, Vary), Drums (Kick: its shapes, Hits: Shaker, Open hat, Offbeats, Rim, Vary): each layer's part, its Vary a trim on the Form page's Vary (0 follows it, the far left Hold); KNOB 1–4 set their four values; the line above them names the page (and its place, 2/4) at its left and the voice at its right; the big ring says what the one turning does. The header shows where you are: the section (or INTRO, OUTRO) and the phrase's eight bars, and in a song a hairline that is the whole song. FX again: the Echo page (Blur, Tone, Size, Cloud). HOME again: the **Song** page (the piece's Intro, Main and Outro in bars, a phrase a click, and its Kind: Ambient first or Pulse first; the song's shape on the screen, the song's hairline along the header while it plays); again: the Key page (tempo, root, mode, swing). A layer's button again: its voice page (Chord voice: Attack, Decay, then Chord filter: Cutoff around the piece's own, Reso, Env, Env decay; Pings voice: Octave, Tone, Length, Bend; Bass voice: Octave, Shape, Gate, Glide; the drums' Kick: Tune, Length, Punch, Rumble, then Hats: Level, Tone, Length). A button with one page stays on it pressed again; one with several steps through them |
| SELECT | The DJ filter, on every page (left shut, right open, Off in the middle; a ring shows it); with HOME held, the tempo |
| SEQ, twice, three, four times | The Kick page, the Hats page, then the **Pattern page**: five lanes (kick, shaker, open hat, rim, clap) of sixteen steps on the white keys, accents, chance |
| ALGORITHM | On a layer's page, its voice (7 chord, 5 ping, 5 bass voices, 5 kits); on the Chords page, the walk (Home, Wander, Free); on the Noise page, its character; on the Chord LFO page, where the chord's LFO goes (Cutoff, Reso, Colour); elsewhere, the screen (ripples, scope, spectrum, tape). The mode is on the Key page (HOME a third time). A layer's button a third time: its FM page (the algorithm, ratio, feedback; the chord's Colour and the bass's Bite, on every voice, FM or not; the pings' Upper, operators 3 and 4's ratio). ENV a fourth time: the **Chord LFO** page (a wave on the filter and a slow random wander with it, like a hand riding the filter, as much as Form's Drift says: Drift, more or less of it for this LFO, Wave, the wave's share against the wander (0 the wander alone, 100 the wave alone), Rate 1 to 32 bars, Target: Cutoff, Reso or Colour; the screen its past, now and the wave ahead, its reach in Hz); the bass and the pings have no LFO |
| PRESETS | The piece (eight of them) |
| OCT− / OCT+ | On a layer's page (Chord, Pings, Bass, Drums, Noise): its level, 5 a press (held: on), shown as a ring; both together, quickly: the layer muted or back, its level kept (the same mutes as the Arrangement page's); held 5 s: the update mode |
| SAVE | The Noise page (Density, Vary; ALGORITHM its character: Vinyl, Tape, Dust, Room, Machine); again, the Noise voice (Air, Tone, Decay, Width); again, the **Noise LFO** page (Drift, Wave, Rate, Target, as the chord's: Filter, a low-pass on the whole noise, Tone or Air); held, a nudge to the noise |
| GLO | The Setup page; held, saves (held on, the piece back to its recipe) |

Hold a layer's button (ENV, ARP, LFO, SEQ) for a nudge: its sound a small step somewhere new. Every
long press says what it did on a card in the middle of the screen.

DUB-1 also tells the [FM-1 Screen Reader](https://charlesvestal.github.io/fm1-reader/) what its
screen shows, over USB: the page and its knobs, a knob's value as it turns, the piece, the chord as
it moves, and what each button did.

The [manual](docs/MANUAL.md) has the rest, and the [cheat sheets](https://sbegueria.github.io/fm1-dub/cheatsheets/) a page for each part. [PLAN.md](PLAN.md) has the design, what's done and
what's still to do.

## Building and testing

The toolchain is the same as X0X's (see [BUILDING.md](BUILDING.md)):

```
tests/run_tests.sh      # the instrument, the platform pieces, the app in a simulator, the web builds
./build.sh              # build/amb.fwsc
python3 tools/fm1_install.py build/amb.fwsc     # install it over USB
```

`build/host/amb_host SCRIPT OUTDIR` runs the whole app on a computer from a script
(`tests/scenarios/*.amb`), with audio and screenshots coming out. `web/emu/build.sh` builds the
same code for the browser (the emulator); `web/app/build.sh` builds the engine alone into a web
app for phone and desktop. [EMULATOR.md](EMULATOR.md) says how to set these up on a Mac and how to
listen to a change.

## Credits

DUB-1 is a fork of [AMB-1](https://github.com/charlesvestal/fm1-amb) by Charles Vestal
(GPL-3.0-only): the engine's loops, echo chain, plate and pieces system, the UI and the platform
all come from there. The idea comes from AMB-1 and from dub techno in general: chords dissolving
in a tape echo over a steady kick. The reverb is Jon Dattorro's plate ("Effect Design, Part 1",
JAES 1997), as in [FoMni-1](https://github.com/charlesvestal/fm1-fomni); the granular cloud follows the
design of CLOUD in [JIANT](https://github.com/juanjiant-bit/JIANT) (GPL-3.0); the Euclidean rhythms
are Godfried Toussaint's reading of Bjorklund's algorithm; the noise machine (a crackle groove
ringing resonators tuned to the chord) follows Bahadirhan Kocer's "Using Resonators in Live". The platform (hardware layer, USB,
update loader, storage, installer) is [Felucca](https://github.com/hugelton/Felucca)'s by Leo
Kuroshita (Hügelton Instruments), by way of [X0X](https://github.com/charlesvestal/fm1-x0x),
FoMni-1 and AMB-1. The idea of running FM-1 firmware in the browser comes from
[Groove OS](https://www.groove-os.com/). GPL-3.0-only; see [LICENSING.md](LICENSING.md).

**The random chords.** When no chord slots are set, the chords come from a weighted random walk over
the degrees of the piece's scale, taken from AMB-1's `chord_change` (Charles Vestal) and re-weighted
for dub. Each chord is stacked in thirds from the scale, so it always fits the piece. When Change (the
Chord page's) says it's time, the walk moves to another degree. In a seven-note scale the weights are iv 3.0, VII
2.0, v and VI 1.5, III 1.0 and ii 0.4. Away from home it goes back to i 65% of the time, so the
music keeps returning to its tonic. A degree whose fifth is diminished (the half-diminished chord)
is never chosen; in a shorter scale every degree weighs the same. The voicing comes from dub
techno's chord-memory stab (Basic Channel, Maurizio), where one chord shape is played from every
key. The shape is kept and moved whole, either an open seventh (root, 5th, 7th, 10th) or a ninth
without its fifth. Its root lands nearest the one before, within a tritone and between C3 and B3,
so the stabs slide in parallel instead of jumping. Vary now and then swaps one shape for the
other. AMB-1's own walk weights the degrees differently (home and the near, strong moves first).
The code is `chord_change` and `voice_chord` in `firmware/src/dsp/amb.c`.

M-VAVE and FM-1 are trademarks of their owners. DUB-1 is not affiliated with M-VAVE or Hügelton
Instruments.
