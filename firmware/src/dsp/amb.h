/* SPDX-License-Identifier: GPL-3.0-only */
/* The instrument: a generative dub techno machine. Four layers draw on one pool of notes (a scale: the
 * root plus up to eleven more pitch classes) and one chord that moves now and then through it:
 *
 *   CHORD   the stab: four voices (two detuned saws each, or an FM keys) struck together on a Euclidean
 *           pattern off the beat, through a resonant low-pass that opens with each stab and sweeps with the
 *           arrangement; most of it goes to the echo. One voicing (an open minor 7, or a 9 without its fifth)
 *           shifted whole to each new root, as a chord memory. The chord moves every DRIFT bars, or holds.
 *   PINGS   short FM blips played by four loops of unequal length (13, 17, 23, 29 eighths), each holding a
 *           few notes of the chord that shift against the others; heavily echoed.
 *   BASS    a sub on the chord's root (now and then its fifth), on one of five grooves.
 *   DRUMS   a four-on-the-floor kick (it ducks the rest: DUCK), a shaker on the sixteenths (HITS sets how
 *           many) and an open hat off the beat.
 *
 * Over it all, the ARRANGEMENT: eight-bar phrases, each Full, a Break (the kick and the bass out, the filter
 * building), a Groove (fewer stabs) or Thin (no bass, no shaker); MOTION sets how much it changes: under 40 the
 * parts ridden out and back and the kick out a bar or two (Breathes), then Thin and Groove (Sections), from 70
 * Breaks too. Before the kick comes back, a fill. VARY turns the patterns and throws a stab into the echo now and then.
 * Then a tape echo (to ~5.6 s, two heads, a band-pass in its feedback that the arrangement sweeps, a
 * saturating feedback that may pass 100%), a short Echoplex, a granular CLOUD (after JIANT's) blooming into
 * Dattorro's plate, and a bed of vinyl crackle.
 * FREEZE (dub hold): the echo and the cloud stop listening and play what they hold for ever; the chord holds.
 *
 * Threads: en_render() runs in the audio interrupt. Everything else posts commands, which the render
 * drains at the start of a block; the state below marked (UI) is written by the render and only read
 * elsewhere. Float only, no libm: fastmath.h. */
#pragma once
#include <stdint.h>

#define EN_SR 44100.0f
#define EN_NLOOP 4                     /* ping loops */
#define EN_LOOP_MAX 29
#define EN_NDRUM 5                     /* kick, shaker, open hat, rim, clap */
#define EN_NPAD 4                      /* chord voices */

/* modes: pools relative to the root (bit i: root + i semitones) */
enum { MODE_IONIAN, MODE_DORIAN, MODE_PHRYGIAN, MODE_LYDIAN, MODE_MIXOLYDIAN, MODE_AEOLIAN, MODE_PENT_MAJ,
       MODE_PENT_MIN, MODE_HIRAJOSHI, MODE_IN, MODE_IWATO, MODE_YO, MODE_INSEN, EN_NMODES };   /* (1.4.0, format 28,
       KellyOffski's: five Japanese scales, five notes as the pentatonics) */
extern const char *const EN_MODE_NAME[EN_NMODES];
extern const uint16_t EN_MODE_POOL[EN_NMODES];
extern const char *const EN_NOTE_NAME[12];
extern const uint8_t EN_LOOP_LEN[EN_NLOOP];
extern const uint8_t EN_DRUM_N[EN_NDRUM];

/* parameters: value ranges in en_param_info() */
enum {
    P_CHORD, P_PINGS, P_BASS, P_DRUMS,                /* layer levels */
    P_DENSITY, P_VARY, P_DRIFT, P_HITS,               /* motion: the pings' notes, how much the loop varies (1.4.0, format
                                                       * 29: Vary, Evolve's place), the chord's bars, the shaker */
    P_ECHO, P_ETIME, P_REVERB, P_FEEDBACK,            /* space */
    P_CUTOFF, P_DECAY, P_PTONE, P_MOTION,             /* the stab's filter and length, the pings' tone, the arrangement */
    P_TUNE, P_MIDI, P_TEMPO, P_ROOT, P_MODE, P_POCT,  /* setup, panel encoders, the pings' octave */
    P_CVOICE,                                         /* the chord's voice (EN_CV_NAME) */
    P_BASSOCT, P_GROOVE, P_FIFTH,                     /* bass: its octave, its pattern, how often the fifth */
    P_SWING, P_HATS,                                  /* drums: swing, the hats' colour */
    P_STABS, P_CRACKLE, P_DUCK, P_SIZE,               /* stabs a bar, the vinyl, the kick's ducking, the plate */
    P_CLOUD,                                          /* the granular reverb */
    P_PVOICE, P_BVOICE, P_KIT,                        /* the pings' voice, the bass's, the drums' kit */
    /* the sound pages: each relative to the voice as it is (0), -50 .. +50 (the kick's tune: semitones) */
    P_CATT, P_CCOL, P_CRES, P_CENV,                   /* the chord: attack, colour, resonance, the filter's envelope */
    P_PTON, P_PRAT, P_PLEN, P_PBEND,                  /* the pings: tone, ratio, length, bend */
    P_BBITE, P_BSHAPE, P_BLEN, P_BSLIDE,              /* the bass: bite, shape, length, slide */
    P_KTUNE, P_KLEN, P_KPUNCH, P_HTONE,               /* the drums: the kick's tune, length, punch; the hats' tone */
    /* the FM pages (an FM voice): the algorithm (0: the voice's; 1..9), the upper operators' ratio, op 4's
     * feedback, the modulators' decay: relative, -50 .. +50 */
    P_CALG, P_CFRAT, P_CFB, P_CFDEC,
    P_PALG, P_PFRAT, P_PFB, P_PFDEC,
    P_BALG, P_BFRAT, P_BFB,
    P_CEDEC,                                          /* (format 36, REPORT-stabenv) the chord's filter envelope, how
                                                       * quickly it closes, -50..+50 around its tie to Decay (0: as
                                                       * before, a quarter of the stab's T60 and 20 ms; each step
                                                       * 0.08 octave of its time: x1/16 .. x16). Recipes only, on no
                                                       * page. (The bass's old LFO's slot, P_BFDEC, unread since 1.4.0:
                                                       * 0 in an older save) */
    /* the chords (EDIT's page): eight slots. A slot is one of three: a chord written (a kind, EN_CQ_*, not 0: its root
     * 0..11, absolute, and its type, EN_CX_*); the walk's (kind 0, root 1..7: a degree of the scale, plus one, so it
     * follows the key and the mode; type: its voicing, 0 or 1); or a gap (kind 0, root 0: the chord before it held).
     * Play: the walk's character in bits 0-1 (EN_WALK_*: Home, Wander, Free, each more varied; ALGORITHM on the page)
     * and Loop in bit 2 (EN_PLOOP; KNOB 4). Endless (Loop off): the chord playing is slot 1; on, all move up a slot and
     * the walk writes slot 8. Loop: the slots in turn, to the last that isn't a gap; nothing new */
    P_CR0, P_CR1, P_CR2, P_CR3, P_CR4, P_CR5, P_CR6, P_CR7,
    P_CQ0, P_CQ1, P_CQ2, P_CQ3, P_CQ4, P_CQ5, P_CQ6, P_CQ7,
    P_CX0, P_CX1, P_CX2, P_CX3, P_CX4, P_CX5, P_CX6, P_CX7,
    P_PMODE,
    /* the drums' pattern page: each lane's sixteen steps (bit i: step i, as int16) and its accents, its chance (%);
     * the accents' strength; Play: Auto (generated, as ever) or Pattern (these) */
    P_DPAT0, P_DPAT1, P_DPAT2, P_DPAT3, P_DPAT4,
    P_DACC0, P_DACC1, P_DACC2, P_DACC3, P_DACC4,
    P_DCH0, P_DCH1, P_DCH2, P_DCH3, P_DCH4,
    P_DACCENT, P_DMODE,
    /* the noise (SAVE's Noise page, 1.3.0): Colour, Grain, Room, Drift, -50..+50 around the piece's own (0), and a
     * character (EN_NCHAR_NAME: Vinyl, Tape, Dust, Room, Machine); Crackle (the Noise page's Level) is its level.
     * (Room is off the page: the recipes and the characters still set it) */
    P_NCOL, P_NGRAIN, P_NROOM, P_NDRIFT, P_NCHAR,
    /* 1.3.0's pages (format 13), 0..100: the stabs thrown into the echo (50: as Evolve and the arrangement throw
     * them; 0 never, 100 about three times as often); the pings' dry level against their sends (50: -6 dB, as the
     * sound pass left it; 0: +6 dB; 100: about -20 dB, almost all echo); the bass's notes in its groove (100: every
     * note; less, left out at random, the bar's first kept); the shaker and open hat against the kick (50: x1,
     * about +-12 dB at the ends) */
    P_CTHROW, P_PGHOST, P_BNOTES, P_HATSLVL,
    /* 1.3.0's echo and density (format 14), 0..100: the chord's dry against its sends (as the pings' Ghost; 50: -6 dB,
     * its echo x1.35); the kick's density (its shapes by phrase: 0 none .. 100 four on the floor, always); the echo's
     * blur (0 clean copies .. 50 each repeat a little more dissolved .. 100 a wash), its tone (darker .. brighter),
     * its width (0 mono .. 100 wide); the noise's Decay (how long its hits ring: 0 a knock .. 100 a hum) */
    P_CGHOST, P_KDENS, P_EBLUR, P_ETONE, P_EWIDTH, P_NTUNE,
    /* (format 15) the Noise page's second (SAVE again): its width (0 mono, 50 as it was, 100 wide), how much of it
     * into the plate and into the echo (50 as it was, 0 none, 100 twice); the hats' length (-50..+50 around the kit's) */
    P_NWIDTH, P_NPLATE, P_NECHO, P_HLEN,
    P_NLFO,                                           /* (1.3.0, format 16) the noise's LFO: its rings' tone, over a bar */
    P_RUMBLE,                                         /* (1.3.0, format 17) the kick's rumble */
    /* (1.4.0, format 19, REPORT10) the chord's LFO and the noise's: their rate (1, 2, 4, 8, 16, 32 bars, or Drift: a
     * slow random wander, a new place every 8-32 bars, glided to); their depth the Chord LFO and Noise LFO knobs.
     * (The pings' and the bass's LFO gone: P_PFDEC kept in the format, the engine leaves it be; P_BFDEC's slot, since
     * format 36, P_CEDEC) */
    P_CLRATE, P_NLRATE,
    P_CLWAVE, P_NLWAVE,                               /* (1.4.0, format 20, the user's: their wave's shape; format 30,
                                                       * the LFO pages' layout B, the user's: the slots of the Shape,
                                                       * now Wave, the wave's share against the wander, 0..100: 0 the
                                                       * wander alone, 50 half each, 100 the wave alone; the wave a sine) */
    P_CLWALK, P_NLWALK,                               /* (1.4.0, format 20, the user's: REPORT10's drift its own knob,
                                                       * always on, the LFO's wave over it) how far the slow random walk
                                                       * wanders: the chord's filter, the noise's */
    /* (1.4.0, format 21, the user's: the Chord's pages) the chord's Vary, 0..100: how often its stabs' pattern turns,
     * Evolve over it (50: as Evolve alone turned it; 0: still, the stabs all played; 100: three times as often); and
     * where its LFO and drift go (EN_CLDEST_NAME: Cutoff, Reso, Colour; ALGORITHM on the Chord LFO page). (Format 29:
     * Vary a trim on Form's Vary, -50..+50, -50 Hold: P_LOOSE's note) */
    P_CVARY, P_CLDEST,
    /* (1.4.0, format 22, the user's: the layers' pages) each layer's Vary, as the chord's: how often its part changes,
     * Evolve over it (50: as Evolve alone changed it; 0: still; 100: about three times as often): the pings' loops
     * (their notes moving, a pass sat out), the bass's plan (its phrase's figure, the octave note), the drums' (the
     * kick's shape, the shaker's steps, the clap), the noise's groove (the recipe's Change under it). And the chord's
     * Cutoff as a knob around the piece's own (-50..+50: 0 its recipe's, P_CUTOFF, now the base, on no knob; a step
     * 0.055 octave, as P_CUTOFF's). (Format 29: the Varys trims on Form's Vary, as the chord's) */
    P_PVARY, P_BVARY, P_DVARY, P_NVARY, P_CCUT,
    /* (1.4.0, format 23, the user's: the Arrangement page's channel strips, a part's white key held) each part's Echo
     * send and Plate send, 0..100: the part's own sends times a fader's share (50: as the piece sent before, 0: none,
     * 100: twice, +6 dB; the noise's are P_NECHO and P_NPLATE, as they were); the bass's echo none before (0); the
     * drums' the hats', the rim's and the clap's (the kick's none, and none now); then each part's Low cut, 0..100: a
     * gentle high-pass (6 dB an octave) before its sends, 0 off, ~20 Hz at 1 to ~600 Hz at 100 */
    P_CECHO, P_CPLATE, P_PECHO, P_PPLATE, P_BECHO, P_BPLATE, P_DECHO, P_DPLATE,
    P_CLOW, P_PLOW, P_BLOW, P_DLOW, P_NLOW,
    /* (1.4.0, format 24, the user's: the kick and the hats apart, six parts on white 1-6) the Drums strip split: P_DECHO,
     * P_DPLATE and P_DLOW now the Hats strip's (the shaker, the open hat, the rim and the clap: what the drums' were,
     * the kick never in them); the Kick strip's own: its level (x1 at 50, about +-12 dB at the ends, as Hats level:
     * the two faders apart, P_DRUMS over both, the Drums pages' OCT), its Echo and Plate sends (0: none, as before;
     * 100 a full send) and its Low cut (as the others', 0 off; the rumble its own, never cut) */
    P_KLVL, P_KECHO, P_KPLATE, P_KLOW,
    /* (1.4.0, format 25, the user's) where the noise's LFO and drift go (EN_NLDEST_NAME: Filter, its low-pass, as
     * before; Tone; Air; ALGORITHM on the Noise LFO page) */
    P_NLDEST,
    /* (1.4.0, format 26, the user's: song structure, build/design/SONG.md) Song mode, the player's as Tune (global, on
     * GLO's second page; EN_SMODE_NAME: Endless, as ever; Song, an intro, the main, an outro, then silence; Set, the
     * same, then the next piece, to the last); and each piece's song: its Intro, Main and Outro in phrases (eight bars a
     * step: the knobs show bars), and its intro's Kind (EN_SKIND_NAME: Ambient first, the noise and the chord's wash;
     * Pulse first, the kick and the bass) */
    P_SMODE, P_SINTRO, P_SMAIN, P_SOUTRO, P_SKIND,
    /* (1.4.0, format 27, the user's: the virtual dub mixer, the Arrangement page's KNOB 2) how much DUB-1 rides the
     * desk by itself, 0..100: 0 off (the music as it was, bit for bit); the parts' faders and sends, the chord's
     * filter and the echo's feedback moved slowly, off the phrase lines, around the knobs' values (never on them) */
    P_MIXER,                                     /* (format 31: retired, unread: its moves Motion's, its rides Drift's) */
    /* (1.4.0, format 29, the variety plan's step 1) how loose the playing is, 0..100: the stabs' own cutoffs, the hats'
     * and the bass's late notes, the throws; each piece's own (its recipe's), on no knob (Evolve's before: 2.5 times it,
     * to 100). (And P_VARY is Form's Vary, Evolve's place; P_CVARY, P_PVARY .. P_NVARY each layer's trim on it, -50..+50:
     * the layer's amount Vary plus its trim, 0..100; 0 follows Vary; -50 Hold, that layer never turns) */
    P_LOOSE,
    /* (1.4.0, format 30, the variety plan's step 2) Drift, Form's KNOB 4, 0..100: how much the sound moves, one amount
     * for all of it: the chord's and the noise's LFOs (each its wave and its random wander, in a fixed share), the
     * filter's slow arc and the echo's slow moves (from Motion), the dub mixer's slow rides (from the Mixer). 0 still;
     * 50 the classics' median (about an octave, two sweeps a minute, the levels a few dB); 100 Coldest Season.
     * (P_DRIFT, the name's first owner, is the Chord page's Change.) Then each LFO's Trim on it, -50..+50: x1 at 0,
     * still at -50, x2 at +50 (their old Depth and Drift, P_CFDEC, P_CLWALK, P_NLFO, P_NLWALK, kept in the format,
     * unread: format 30's step turned them into these) */
    P_MOVE, P_CLTRIM, P_NLTRIM,
    /* (format 30, the user's: Vary 0 no variation at all) the stabs' throws scale with the chord's amount (Form's Vary
     * plus its trim: none at 0 or Hold): as often as the looseness and Throws had them where the chord's amount is
     * this, 1..100 (each piece's own, its recipe's amount; on no knob; format 35: the amount itself, x 100, not the
     * knob's: en_vary_amount's, so its value stays across the new scale) */
    P_TVARY,
    /* (format 35, the variety plan's step 5b) the filter's slow sweep in Full, its pace: 1 a new place every 6-20 bars,
     * glided to over 3-7.5 (the records drift slower than the 8-bar loop; 8-32 left the still pieces' changes on the
     * phrase's line); 0 every 4-14, over 2-5, as before 35 (an older save's piece you changed keeps it). Each piece's
     * own, on no knob */
    P_SWEEP,
    P_NPARAMS
};
typedef struct {
    const char *name;
    int16_t lo, hi, def;
} en_param_t;
const en_param_t *en_param_info(int i);
void en_param_text(int i, int v, char *buf);   /* the value as the screen shows it (10 bytes) */

/* commands (main loop -> render) */
void en_init(void);
void en_set(int param, int value);
void en_pool(uint32_t pool);                   /* the scale, relative to the root (bit 0 always set) */
void en_play(int on);                          /* it plays / rests (the tails ring on) */
void en_next(void);                            /* the next chord: on the next bar (now while resting) */
void en_freeze(int on);
void en_seed(uint32_t s);                      /* stir some entropy into the generators */
void en_panic(void);                           /* everything quiet, now */
void en_regrow(void);                          /* a new piece: fresh loops and patterns, the home chord */
uint32_t en_queue_w(void);
uint32_t en_queue_drops(void);                 /* (1.4.1) commands dropped, the queue full (PRESETS spun fast) */
#ifdef EN_HOST
int en_par_now(int p);
#endif                     /* (1.4.0) the commands posted so far (en_prog_q's count) */
uint32_t en_queue_left(void);                  /* (1.4.0) the commands still waiting (a piece's, spread over ~7 blocks) */
/* the ping loops, packed a byte a step (EN_PINGS_N: every loop's steps, in turn): the note offset in the low
 * nibble (15: a rest), the velocity in the high one (50 + 3 x v). A piece keeps them; en_pings gives them back
 * (0: none kept, the loops grown afresh), taken at once and again after each en_regrow */
#define EN_PINGS_N 82
void en_pings_get(uint8_t *b);
void en_pings(const uint8_t *b);
/* performance (SEL's mode): layers muted (EN_M_*, a mask: set = muted), and the moves held now (EN_PF_*) */
enum { EN_M_CHORD = 1, EN_M_PINGS = 2, EN_M_BASS = 4, EN_M_KICK = 8, EN_M_HATS = 16, EN_M_NOISE = 32 };   /* (the noise:
                                                * OCT- + OCT+ on its page; its level kept) */
enum { EN_PF_THROW = 1, EN_PF_OPEN = 2, EN_PF_CLOSE = 4, EN_PF_KICKDROP = 8, EN_PF_ECHO = 16, EN_PF_HATTHROW = 32,
       EN_PF_PTHROW = 64, EN_PF_BTHROW = 128, EN_PF_KTHROW = 256 };   /* (1.4.0, the moves rethink: a throw a part, on
                                                  * its black key held: the chord's EN_PF_THROW, every stab thrown; the
                                                  * pings', the bass's, the hats', their echo sends pushed; the user's,
                                                  * zzq: the kick's for the noise's, its echo send pushed) */
/* (KICKDROP: Drop, the kick and the bass. HATTHROW, 1.4.0, the user's: the hats, the rim and the clap thrown into the
 * echo while held, the kick never; their tails left to ring) */
void en_mute(uint32_t mask);
void en_perf(uint32_t flags);
/* the master actions (HOME's and PERFORM's knobs), -50..50 each, 0: the piece as it is; for playing, never saved.
 * Filter: down a low-pass on the music (Shut's), up a resonant high-pass (Open's), the kick left out; Dub: the
 * echo's send and feedback; Energy: Density, Hits and Stabs; Space: the reverb, the cloud and the plate's size */
enum { EN_MAC_FILTER, EN_MAC_DUB, EN_MAC_ENERGY, EN_MAC_SPACE, EN_NMAC };
void en_macro(int i, int v);
/* (1.4.0) song structure: HOME held + PLAY, the outro from the next phrase (in any Song mode: a way to land a piece by
 * hand); again, in it or while it waits: back to the main (Endless: on as ever). The song's place for the screen and
 * the app: EN_SG_*, and the bars its main, outro and silence start on (since PLAY; 0 0: no outro coming). The parts
 * (EN_NPART: the chord, the pings, the bass, the kick, the hats with the rim and the clap, the noise: EN_M_*'s order):
 * where each comes into an intro (a share of it, by Kind) and starts to leave an outro */
#define EN_NPART 6
enum { EN_SG_OFF, EN_SG_INTRO, EN_SG_MAIN, EN_SG_OUTRO, EN_SG_END };
#define EN_NSMODE 3
#define EN_NSKIND 2
extern const char *const EN_SMODE_NAME[EN_NSMODE], *const EN_SKIND_NAME[EN_NSKIND];
void en_outro(void);
/* (1.4.0, the user's: Transition, the Song mode page's KNOB 2, Set mode alone; build/design/SET2.md) how a Set goes
 * from one piece to the next: LP as ever (the song ends, PLAY stops, the tails ring out, the next piece after a wait);
 * Segue (no stop: the next piece from the bar after the outro, the echo and the plate ringing on into it); Mix (over
 * the last phrase the tempo and the echo's time glide to the next piece's; on the downbeat its pulse, the kick and the
 * noise, under the old tails; its bass a bar on, its chord, pings and hats two). en_trans: the mode and the next
 * piece's tempo and echo time (tempo 0: no next piece: it rests, as LP), as the app has them; en_tr_cue: the outro's
 * last bar reached, the old parts gone (the app then sets the next piece and says en_trans_go); the next piece then
 * starts on the next downbeat past the outro, its song from the top */
enum { EN_TR_LP, EN_TR_SEGUE, EN_TR_MIX, EN_NTRANS };
extern const char *const EN_TRANS_NAME[EN_NTRANS];
void en_trans(int mode, int next_tempo, int next_etime);
void en_trans_go(void);
extern volatile uint8_t en_tr_cue;
extern volatile float en_bpm;                  /* the tempo heard now (a Mix's glide; else Tempo's), for tests and logs */
extern volatile uint8_t en_song_ph;
extern volatile uint32_t en_song_b0, en_song_bo, en_song_be;
extern volatile uint8_t en_song_ask;
/* (1.4.0) the dub mixer (format 31: its rides Drift's, its moves Motion's) as heard now: each part's level ride (dB, a move out and back in it), its echo and
 * plate sends' (dB); the move under way (0 none, 1 a part out and back, 2 an echo swell); the moves begun and those on
 * a phrase's first bar (for the tests) */
extern volatile float en_mx_db[EN_NPART], en_mx_snd[EN_NPART][2];
extern volatile uint32_t en_mx_moves, en_mx_line;
extern volatile uint8_t en_mx_ev;
/* (format 31, the variety plan's step 3) Motion's moves begun so far: a part ridden out and back, an echo swell, the
 * kick dropped out a bar or two; the kick's drop-out now (the bars it's still out, 0: in). For the tests and the logs */
enum { EN_MO_OUT, EN_MO_SWELL, EN_MO_KICK, EN_MO_N };
extern volatile uint32_t en_mo_count[EN_MO_N];
extern volatile uint8_t en_mo_kick;
extern volatile float en_fader[EN_NPART];       /* each part's fader as heard now, 0..1 (for tests and the screen) */
float en_song_in(int kind, int part);
float en_song_out(int part);
void en_lock(int on);                          /* PERFORM's Lock: each phrase keeps the kick's shape, the noise's groove,
                                                * the drums' and the bass's plan, the shaker's pattern */
/* the effects grid (REC held): the effects held now, a bit each (EN_FX_*), in the grid's order */
enum { EN_FX_THROW, EN_FX_TIME, EN_FX_TAPESTOP, EN_FX_SWELL, EN_FX_LOWPASS, EN_FX_HIGHPASS, EN_FX_WAH, EN_FX_PHONE,
       EN_FX_VERB, EN_FX_CRASH, EN_FX_FREEZE, EN_FX_WASH, EN_FX_STUTTER, EN_FX_GATE, EN_FX_SIREN, EN_FX_RISER, EN_NFX };
extern const char *const EN_FX_NAME[EN_NFX];
void en_fx(uint32_t mask);
/* (1.4.0, the user's) the dub sirens, the Arrangement page's four right-most black keys: one voice, into the echo as
 * the grid's Siren. Wail, Hi-lo and Laser tuned to the key (the root, the fifth, the octave); Whoop out of tune on
 * purpose (a box's: its own pitch, not the key's). Wail and Hi-lo follow the tempo, Whoop and Laser don't. EN_SIR_TAP (a press): the short one, about a bar, played to its end; EN_SIR_HOLD (still held past a
 * third of a second): the slow one, from where it is, looping; EN_SIR_OFF (a held one let go): it fades, the echo's
 * tails ringing. A new one takes over from the one sounding. Not saved */
enum { EN_SIR_WAIL, EN_SIR_WHOOP, EN_SIR_HILO, EN_SIR_LASER, EN_NSIR };
enum { EN_SIR_OFF, EN_SIR_TAP, EN_SIR_HOLD };
void en_siren(int siren, int act);
extern volatile uint8_t en_sir_on;              /* the siren sounding (its gate open): its number + 1; 0 none */
extern volatile float en_sir_st, en_sir_hz;     /* its pitch now: semitones over the root as voiced, Hz (for tests) */
/* (1.4.0, format 34, the user's) each siren's four knobs, KNOB 1-4 while its key is held (a tap uses them too): Pitch
 * (the tuned ones: semitones, -12..+12, on the key's notes; Whoop: tenths of a semitone, -120..+120, free), Rate (a
 * step of EN_SIR_NV's note values, Wail's cycle and Hi-lo's swap held, or of EN_SIR_HZ, Whoop's whoops and the Laser's
 * warble), Depth (semitones, 1..24: how far it swings; on the key's notes, but Whoop), Echo (0..100: its send, 50 as
 * it was; past 50 the echo's feedback pushed too, as a throw's). Kept by the UI (proj.sir, global), sent with
 * en_sir_set; en_sir_def's values the sirens as they were before (Whoop's new) */
enum { EN_SK_PITCH, EN_SK_RATE, EN_SK_DEPTH, EN_SK_ECHO, EN_NSK };
void en_sir_set(int siren, int knob, int v);
int en_sir_def(int siren, int knob);
void en_sir_range(int siren, int knob, int *lo, int *hi);
int en_sir_eff(int siren, int knob, int v);      /* the value as it plays (the tuned ones' Pitch and Depth on the key's
                                                * notes, as the scale is now) */
int en_sir_step(int siren, int knob, int v, int d);   /* v turned d clicks (the tuned ones' Pitch and Depth: a note of
                                                * the key a click) */
void en_sir_text(int siren, int knob, int v, char *b);   /* "+3", "1/8", "2.5 Hz", "12 st", "70" */
extern const char *const EN_SIR_NAME[EN_NSIR];  /* "Wail", "Whoop", "Hi-lo", "Laser" */
/* MIDI clock in (as FoMni): ticks drive the 1/16 grid (6 a step) while they keep coming */
enum { EN_CLK_TICK, EN_CLK_START, EN_CLK_CONTINUE, EN_CLK_STOP };
void en_clock(int msg);

/* the render (audio ISR): n stereo frames, 24-bit in int32; gain Q12 (the MASTER pot) */
void en_render(int32_t *out_lr, uint32_t n, uint32_t gain_q12);

/* MIDI out, from the render: the app sends it (USB); status, data 1, data 2 */
void en_midi_out(uint32_t st, uint32_t d1, uint32_t d2);

/* the voices: each layer's sounds (ALGORITHM on the layer's page) */
#define EN_NCV 7
#define EN_NPV 5
#define EN_NBV 5
#define EN_NKIT 5
extern const char *const EN_CV_NAME[EN_NCV], *const EN_PV_NAME[EN_NPV], *const EN_BV_NAME[EN_NBV],
    *const EN_KIT_NAME[EN_NKIT];

/* the arrangement's sections */
enum { SEC_FULL, SEC_BREAK, SEC_GROOVE, SEC_THIN, EN_NSEC };
extern const char *const EN_SEC_NAME[EN_NSEC];

/* state for the screen (UI) */
extern volatile uint8_t en_playing, en_frozen, en_ext;
extern volatile uint8_t en_chord[EN_NPAD];     /* the chord's notes (MIDI), as the voices play them */
extern volatile uint8_t en_chord_root;         /* the chord's root, MIDI pitch class 0..11 */
extern volatile uint8_t en_chord_n;            /* its distinct tones: 3 or 4 */
extern volatile float en_drift;                /* 0..1: how far to the next chord (0 while it holds) */
extern volatile float en_pad_level, en_bass_level;   /* the stab's and the bass's envelopes, 0..1 */
extern volatile uint8_t en_loop_pos[EN_NLOOP];
extern volatile int8_t en_loop_note[EN_NLOOP][EN_LOOP_MAX];   /* -1 rest, else the slot's offset */
extern volatile float en_loop_glow[EN_NLOOP];  /* 0..1: a ping just sounded from this loop */
extern volatile uint8_t en_drum_pos[EN_NDRUM];
extern volatile uint32_t en_drum_pat[EN_NDRUM];/* bit i: a hit on step i (all 16 steps) */
extern volatile float en_drum_glow[EN_NDRUM];
extern volatile uint32_t en_bars;              /* sixteenths played since start (for tests) */
extern volatile float en_arc;                  /* the filter's sweep, -1 dark .. +1 open (times the slow movement's depth:
                                                  * the chord's Drift and LFO Depth; 1.4.0, the variety study) */
extern volatile uint8_t en_pulse_rest;         /* the kick is out (a Break) */
extern volatile uint8_t en_pad_note[EN_NPAD];  /* the note each voice plays (MIDI) */
extern volatile float en_pad_amp[EN_NPAD];     /* and how loud */
extern volatile uint8_t en_section;            /* SEC_*: the phrase playing */
extern volatile uint8_t en_phrase_bar;         /* 0..7: its bar */
extern volatile uint8_t en_change_next;        /* the next phrase brings a change (1 the plan, 2 the kick, 4 the section;
                                                * decided on its last bar) */
extern volatile uint32_t en_stab_pat;          /* the stabs' pattern this bar (bit i: step i) */
extern volatile uint32_t en_throws;            /* stabs thrown into the echo so far (for tests) */
/* for the screen's ripples and lanes */
extern volatile uint32_t en_stabs;             /* stabs struck so far */
extern volatile float en_stab_vel;             /* the last one's strength, 0..1 */
extern volatile uint8_t en_stab_thrown;        /* the last one was thrown */
extern volatile float en_echo_ms, en_echo_g;   /* the echo's time; the level a repeat keeps of the last (1: held) */
extern volatile float en_echo_in;              /* how much of the stab goes into the echo, 0..1 */
extern volatile float en_duck;                 /* the ducking now, 0 .. 1 (full depth) */
/* the output, for the screen's scope and spectrum: the last 256 samples (mono, a mean of each eight), and the
 * last 512 (mono, a mean of each two: 22 kHz), each with where the next goes */
#define EN_SCOPE_N 256
#define EN_SPEC_N 512
extern volatile int16_t en_scope[EN_SCOPE_N], en_spec[EN_SPEC_N];
extern volatile uint32_t en_scope_w, en_spec_w;
/* what the scope and spectrum listen to: 0 the whole output, 1..5 one layer alone, dry (the chord, the pings, the
 * bass, the drums, the noise), for a layer's page; and each layer's notes struck so far, the last one's strength */
extern volatile uint8_t en_view_layer;
extern volatile uint32_t en_hits[5];
extern volatile float en_hit_vel[5];
extern volatile float en_echo_pk;               /* the echo's output, its peak since the screen last took it */
/* (1.4.0, the Arrangement page's meters) each part's peak since the screen last took it (it sets them back to 0): the
 * chord's, the pings', the bass's, the kick's, the hats' (the shaker, the open hat, the rim, the clap), the noise's
 * (1.4.0: the drums' split, the user's six parts), each its dry as it goes into the
 * mix (before the duck), every 4th sample, only while en_meter_on; and the master's, every sample, always (the clip
 * light): after the gain, going into the output's soft clip (1.0: full scale; over it, squashed). In the output's
 * units, the MASTER pot's gain in. And the duck's deepest gain since taken (set back to 1): on the music, on the bass */
enum { EN_MT_CHORD, EN_MT_PINGS, EN_MT_BASS, EN_MT_KICK, EN_MT_HATS, EN_MT_NOISE, EN_MT_MASTER, EN_NMETER };
extern volatile float en_meter[EN_NMETER];
extern volatile uint8_t en_meter_on;
extern volatile float en_duck_lo[2];
/* the noise's events, for the screen's specks: a ring of the last few (kind: 0 a click, 1 a pop, 2 a dust grain; its
 * strength as heard, 0..255; its side, 0 left .. 255 right), en_nev_w where the next goes */
/* the audio's load, the last block's time against its budget, in % (set by the device's audio handler; 0 elsewhere):
 * over 80, the engine sheds for a moment what is least heard (the quietest tails, the cloud's new grains), so a crowded moment doesn't overrun */
extern volatile uint8_t en_load;
#define EN_NEV_N 16
extern volatile uint8_t en_nev_k[EN_NEV_N], en_nev_s[EN_NEV_N], en_nev_p[EN_NEV_N];
extern volatile uint32_t en_nev_w;
extern volatile uint16_t en_lane[6];
extern volatile float en_bass_gate;              /* (1.4.0, the bass's page) how long its notes hold, in sixteenths */
extern volatile int8_t en_bass_octn;             /* which of this bar's bass notes is an octave up (0..; -1: none) */
extern volatile uint16_t en_lane_sh, en_lane_op;   /* (1.4.0, the Drums page) the shaker's and the open hat's apart */
extern volatile uint16_t en_lane_png;            /* (1.4.0, the overview) the pings this bar: the sixteenths one falls on */
extern volatile uint8_t en_kick_v[16], en_hat_v[16];   /* each step's strength as it will play (0..255): the ghosts low */
extern volatile uint32_t en_nz_pat;             /* (1.4.0, the Noise page) its groove: two bars of sixteenths */
extern volatile uint8_t en_nz_vel[32], en_nz_step;   /* each hit's strength (a ghost low); the step playing (0..31) */
int en_nz_air18(int old, int character);          /* (format 18) the noise's Air before 18 (-50..50) as 0..100 */
int en_nz_dens18(int old, int character);         /* (format 18) the noise's Density before 18 (-50..50) as 0..100 */
/* (1.4.0) the LFO pages: the chord's (0) and the noise's (1) LFO as heard, a sixteenth at a time, the last 16 bars;
 * each -127..127 of its depth's reach (its knob: 127 at 100, a sine or a drift alike); en_lfo_w the sixteenths
 * written so far (the next goes at en_lfo_w % EN_LFO_N) */
#define EN_LFO_N 256
#define EN_LFO_DRIFT 6                           /* (before 20, a rate: Drift; then its own knob, P_CLWALK; format 30,
                                                  * a Shape) */
extern volatile int8_t en_lfo_tr[2][EN_LFO_N];
extern volatile int8_t en_lfo_wv[2][EN_LFO_N], en_lfo_wd[2][EN_LFO_N];   /* (the LFO pages' picture) the wave's
                                                  * part and the wander's, apart, as en_lfo_tr (its sum, as heard) */
float en_lfo_wave_part(int k, uint32_t s);       /* the wave's part of LFO k (0 the chord's) at step s, its reach's units */
float en_lfo_reach_of(int k);                    /* LFO k's reach either way now (the chord's octaves, the noise's 0..2) */
float en_chord_cut(void);                        /* the chord's cutoff from its knobs, Hz (before its envelope) */
extern volatile uint8_t en_lfo_solo;             /* (for tests) 1: an LFO's wave alone, no wander (format 30) */
extern volatile uint32_t en_lfo_w;
extern volatile float en_lfo_now, en_lfo_reach;   /* (1.4.0, the user's) the chord's LFO and drift now, and how far
                                                   * they reach either way, in octaves of its Cutoff (a 5.5th of the
                                                   * knob's sweep an octave: Reso's and Colour's alike, its Destination's
                                                   * knob shows them) */
#define EN_NCLDEST 3                             /* (1.4.0) the chord's LFO's destinations: its Cutoff, Reso, Colour */
enum { EN_CLD_CUTOFF, EN_CLD_RESO, EN_CLD_COLOUR };
extern const char *const EN_CLDEST_NAME[EN_NCLDEST];
#define EN_NNLDEST 3                             /* (1.4.0) the noise's LFO's destinations: its Filter, Tone, Air */
enum { EN_NLD_FILTER, EN_NLD_TONE, EN_NLD_AIR };
extern const char *const EN_NLDEST_NAME[EN_NNLDEST];
extern volatile float en_nlfo_at[EN_NNLDEST];    /* (1.4.0, for tests) what the noise's LFO puts on each now: the
                                                  * Filter its swing (0..2 of the reach), Tone -1..1 and Air 0..1 of
                                                  * their knob */
extern volatile float en_nlfo_now, en_nlfo_reach;   /* (1.4.0) the noise's LFO and drift now and how far they reach
                                                  * (0..2: a quarter of Tone's or Air's sweep a unit, its moving knob) */
extern volatile uint32_t en_stab_turns;          /* (1.4.0) the stabs' pattern's turns so far (for tests: Vary) */
extern volatile uint32_t en_vary_turns[4];       /* (1.4.0, for tests) the pings', the bass's, the drums' and the noise's
                                                  * changes so far (their Vary) */
extern volatile float en_lfo_at[EN_NCLDEST];     /* (1.4.0, for tests) what the chord's LFO puts on each destination
                                                  * now: the Cutoff in octaves, Reso and Colour -1..1 of their knob */
extern volatile uint32_t en_lfo_picks[2];        /* the drifts' new places so far (for tests) */
int en_lfo_bars(int v);                          /* a rate's bars (0: Drift) */
float en_filter_hz(int v);                       /* a strip's Filter (-50..+50): its cutoff, Hz (0: off) */
int en_nlfo18(int old);                          /* (format 19) a Noise LFO before 19 as 19's (its depth kept) */
void en_motion30(int16_t *par);                  /* (format 31) a piece before 31 as 31's: its Motion and Mixer as
                                                  * Motion, the arrangement only */
float en_tone_at(int base, int t);               /* (format 32) a sound page's Tone (-50..50) around the piece's own
                                                  * (0..100): the room either side */
void en_zones31(int16_t *par, int saved);        /* (format 32) a piece before 32 as 32's: the knobs with dead steps
                                                  * rescaled (Stabs a count; Density, Hits and the noise's faded in;
                                                  * the Tones the room either side; the Echo's Tone), as near as they
                                                  * sounded; saved: the parameters the save had (the rest today's) */
float en_vary_amount(int v);                     /* (format 35) Form's Vary, or a layer's amount (Vary plus its trim),
                                                  * 0..100 on the knob: how much it turns, 0..1 (the classics' median
                                                  * at 50: 0.08) */
void en_vary34(int16_t *par);                    /* (format 35) a piece before 35 as 35's: its Vary and the trims on it
                                                  * on the new scale, each layer's amount as near as it was */
void en_move29(int16_t *par);                    /* (format 30) a piece before 30 as 30's: its LFOs' Depth and Drift and
                                                  * its Mixer as Drift and the LFOs' Trims (and a Shape Drift where the
                                                  * wave was off), as near as they sounded */
extern volatile float en_nz_t60;                 /* how long a hit rings, to -60 dB, seconds */
extern volatile float en_stab_t60;              /* (1.4.0, the chord's page) a stab's length to -60 dB, seconds */
extern volatile uint16_t en_stab_thr;            /* this bar's stabs thrown so far (bit a step) */
extern volatile uint8_t en_stab_v[16];           /* this bar's stabs' strength as struck (0..255; 0: not yet) */
extern volatile int8_t en_bass_off[16];          /* each groove step's note, in semitones from the root's (the fifth
                                                  * above or below, the octave note up): this bar's */           /* this bar as it plays: the stabs, the kick, the hats, the rim, the clap, the bass */

/* the pure parts, for the tests and the screen */
uint32_t en_euclid(int k, int n, int rot);     /* Bjorklund's pattern, as a bitmask (bit 0 = step 0) */
int en_scale(uint32_t pool, int *steps);       /* the pool's pitch classes (relative), ascending; count */
/* the chord on scale degree deg: its tones (MIDI pitch classes, absolute), 3 or 4 of them; count */
int en_chord_tones(uint32_t pool, int root, int deg, int *pcs);
/* the stab's voicing on degree deg: four intervals above the chord's root, ascending (shape 0: an open
 * 7th chord, root 5th 7th 10th; shape 1: root 7th 10th 14th, a 9th without its fifth) */
void en_voicing(uint32_t pool, int deg, int shape, int *iv);
const char *en_chord_name(const int *pcs, int n, char *buf);   /* "Em7", "Dsus4", "C" ... */
uint32_t en_groove(int g);                     /* the bass's pattern g (the engine's: Drone, Offbeat, Dotted, Roller,
                                                * Sync): bit i, a note on step i */
int en_groove_pat(int knob);                   /* the Groove knob (Drone, Offbeat, Dotted, Sync, Roller): its pattern */
int en_drift_bars(int v);                      /* Drift's value in bars; 0: the chord holds */
int en_kick_note(int root);                    /* the kick's note (MIDI) for the key's root: F1..B1 */
/* a layer's envelope as it sounds now (0 chord, 1 pings, 2 bass, 3 kick), for a sound page's drawing: n points
 * of its level (0..1) over *secs seconds, and of a second curve (the chord's filter envelope, the kick's
 * pitch; else 0) */
void en_env_shape(int layer, float *lvl, float *aux, int n, float *secs);
/* (format 36, the Chord filter page's picture) the stab as it plays now: its filter envelope's depth (octaves over the
 * cutoff at the strike: the voice's, times Env's 0..2), its time to -60 dB (s: Decay's, Env decay's), the strike's
 * time constant (s) and the stab's length to -60 dB (s) */
void en_chord_env(float *oct, float *ft, float *att, float *t60);
/* the FM pages: is the layer's voice FM now (0 chord, 1 pings, 2 bass), and its algorithm as it plays (0..8) */
int en_voice_fm(int layer);
#define EN_NPROG 8
enum { EN_CQ_EMPTY, EN_CQ_MAJOR, EN_CQ_MINOR, EN_CQ_SUS2, EN_CQ_SUS4, EN_CQ_DIM, EN_CQ_AUG, EN_NCQ };
enum { EN_CX_TRIAD, EN_CX_6, EN_CX_7, EN_CX_MAJ7, EN_CX_9, EN_CX_11, EN_CX_ADD9, EN_NCX };
extern const char *const EN_CQ_NAME[EN_NCQ], *const EN_CX_NAME[EN_NCX];
extern volatile int8_t en_prog_pos;            /* the slot the progression is on (0..7; Endless: 0), -1: none */
/* the slots as the engine has them (it writes the walk's, and moves them up in Endless): root, kind, type each, and
 * a count that changes when they do (the UI takes them into the project) */
extern volatile int8_t en_prog_slot[EN_NPROG][3];
extern volatile uint32_t en_prog_gen;
extern volatile uint32_t en_prog_q;            /* (1.4.0) the commands read when they were (en_queue_w's count): a
                                               * piece's, spread over blocks, publishes them half its, half the last's */
/* a walk slot's chord as written (to edit it): its root (absolute), kind and type, from the scale's degree */
void en_walk_chord(uint32_t pool, int root, int deg, int *r, int *q, int *x);
void en_ping_nudge(void);                      /* a nudge (ARP held): one note of one ping loop, another */
void en_prog_rotate(void);                     /* Loop to Endless (by hand): the slot playing up to slot 1 */
void en_prog_rewalk(void);                     /* the walk's character turned (by hand): its slots walked again, all but
                                                * the one playing (yours and the gaps kept) */
enum { EN_WALK_HOME, EN_WALK_WANDER, EN_WALK_FREE, EN_NWALK, EN_PLOOP = 4 };
extern const char *const EN_WALK_NAME[EN_NWALK];
#define EN_NNCHAR 5
extern const char *const EN_NCHAR_NAME[EN_NNCHAR];   /* the noise's characters */
extern volatile int8_t en_chord_slot;          /* the slot whose chord sounds, -1: a chord of the scale */
int en_deg_of(uint32_t pool, int rel);         /* the scale degree for a note above the root (nearest in the pool) */
void en_chord_iv(int kind, int type, int *iv); /* a slot's voicing: four intervals above its root, ascending */
const char *en_chord_label(int root, int kind, int type, char *buf);   /* "Fm7", "Bbsus4", "Eb6" ("-": empty) */
int en_fm_alg(int layer);
#define EN_FM_NALG 9

/* the pieces: recipes for every parameter (Tune and MIDI out included, but they are the player's) */
#define EN_NPIECES 8
const char *en_piece_name(int i);
void en_piece(int i, int16_t *par, uint16_t *pool);

#if !defined(EN_HOST) && !defined(EN_PROF)    /* (1.4.0) on the FM-1 always: the Setup page's peak by stage */
#define EN_PROF 1
#endif
#ifdef EN_PROF                                 /* the render's stages, timed: 0 drain, 1 control, 2 the faders, the mixer and the
                                                  * LFOs, 3 stabs, 4 bass, 5 pings, 6 widen, 7 drums, 8 noise, 9 fx, 10 rumble,
                                                  * 11 the mix loop (echo, cloud, plate, master), 12 the steps' events
                                                  * (the platform's: en_prof_ticks, a free-running counter) */
#define EN_PROF_N 13
uint32_t en_prof_ticks(void);
extern volatile uint32_t en_prof[EN_PROF_N];
#endif
