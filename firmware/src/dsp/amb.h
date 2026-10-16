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
 * building), a Groove (fewer stabs) or Thin (no bass, no shaker); MOTION sets how often it leaves Full. Before
 * the kick comes back, a fill. EVOLVE turns the patterns and throws a stab into the echo now and then.
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
       MODE_PENT_MIN, EN_NMODES };
extern const char *const EN_MODE_NAME[EN_NMODES];
extern const uint16_t EN_MODE_POOL[EN_NMODES];
extern const char *const EN_NOTE_NAME[12];
extern const uint8_t EN_LOOP_LEN[EN_NLOOP];
extern const uint8_t EN_DRUM_N[EN_NDRUM];

/* parameters: value ranges in en_param_info() */
enum {
    P_CHORD, P_PINGS, P_BASS, P_DRUMS,                /* layer levels */
    P_DENSITY, P_EVOLVE, P_DRIFT, P_HITS,             /* motion: the pings' notes, change, the chord's bars, the shaker */
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
    P_BALG, P_BFRAT, P_BFB, P_BFDEC,
    /* the chords (EDIT's page): eight slots, each a root (0..11, absolute), a kind (EN_CQ_*: 0 empty) and a type
     * (EN_CX_*); Play: chosen at random (0) or the slots in turn (1). An empty slot holds the chord before it */
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
/* performance (SEL's mode): layers muted (EN_M_*, a mask: set = muted), and the moves held now (EN_PF_*) */
enum { EN_M_CHORD = 1, EN_M_PINGS = 2, EN_M_BASS = 4, EN_M_KICK = 8, EN_M_HATS = 16 };
enum { EN_PF_THROW = 1, EN_PF_OPEN = 2, EN_PF_CLOSE = 4, EN_PF_KICKDROP = 8, EN_PF_ECHO = 16 };
void en_mute(uint32_t mask);
void en_perf(uint32_t flags);
/* the effects grid (REC held): the effects held now, a bit each (EN_FX_*), in the grid's order */
enum { EN_FX_THROW, EN_FX_TIME, EN_FX_TAPESTOP, EN_FX_SWELL, EN_FX_LOWPASS, EN_FX_HIGHPASS, EN_FX_WAH, EN_FX_PHONE,
       EN_FX_VERB, EN_FX_CRASH, EN_FX_FREEZE, EN_FX_WASH, EN_FX_STUTTER, EN_FX_GATE, EN_FX_SIREN, EN_FX_RISER, EN_NFX };
extern const char *const EN_FX_NAME[EN_NFX];
void en_fx(uint32_t mask);
#if BOARD_SMK37
/* The SMK-37 build: the pitch wheel on the tape, bend -127..127 (0 at rest): up runs it faster, the echo's time
 * shorter (to half), down slower (to twice), and the repeats bend as it eases there. Never saved. */
void en_warp(int bend);
#endif
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
extern volatile float en_arc;                  /* the filter's sweep, -1 dark .. +1 open (times Motion) */
extern volatile uint8_t en_pulse_rest;         /* the kick is out (a Break) */
extern volatile uint8_t en_pad_note[EN_NPAD];  /* the note each voice plays (MIDI) */
extern volatile float en_pad_amp[EN_NPAD];     /* and how loud */
extern volatile uint8_t en_section;            /* SEC_*: the phrase playing */
extern volatile uint8_t en_phrase_bar;         /* 0..7: its bar */
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
extern volatile uint16_t en_lane[6];           /* this bar as it plays: the stabs, the kick, the hats, the rim, the clap, the bass */

/* the pure parts, for the tests and the screen */
uint32_t en_euclid(int k, int n, int rot);     /* Bjorklund's pattern, as a bitmask (bit 0 = step 0) */
int en_scale(uint32_t pool, int *steps);       /* the pool's pitch classes (relative), ascending; count */
/* the chord on scale degree deg: its tones (MIDI pitch classes, absolute), 3 or 4 of them; count */
int en_chord_tones(uint32_t pool, int root, int deg, int *pcs);
/* the stab's voicing on degree deg: four intervals above the chord's root, ascending (shape 0: an open
 * 7th chord, root 5th 7th 10th; shape 1: root 7th 10th 14th, a 9th without its fifth) */
void en_voicing(uint32_t pool, int deg, int shape, int *iv);
const char *en_chord_name(const int *pcs, int n, char *buf);   /* "Em7", "Dsus4", "C" ... */
uint32_t en_groove(int g);                     /* the bass's pattern g: bit i, a note on step i */
int en_drift_bars(int v);                      /* Drift's value in bars; 0: the chord holds */
int en_kick_note(int root);                    /* the kick's note (MIDI) for the key's root: F1..B1 */
/* a layer's envelope as it sounds now (0 chord, 1 pings, 2 bass, 3 kick), for a sound page's drawing: n points
 * of its level (0..1) over *secs seconds, and of a second curve (the chord's filter envelope, the kick's
 * pitch; else 0) */
void en_env_shape(int layer, float *lvl, float *aux, int n, float *secs);
/* the FM pages: is the layer's voice FM now (0 chord, 1 pings, 2 bass), and its algorithm as it plays (0..8) */
int en_voice_fm(int layer);
#define EN_NPROG 8
enum { EN_CQ_EMPTY, EN_CQ_MAJOR, EN_CQ_MINOR, EN_CQ_SUS2, EN_CQ_SUS4, EN_CQ_DIM, EN_CQ_AUG, EN_NCQ };
enum { EN_CX_TRIAD, EN_CX_6, EN_CX_7, EN_CX_MAJ7, EN_CX_9, EN_CX_11, EN_CX_ADD9, EN_NCX };
extern const char *const EN_CQ_NAME[EN_NCQ], *const EN_CX_NAME[EN_NCX];
extern volatile int8_t en_prog_pos;            /* the slot the progression is on (0..7), -1: chosen at random */
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
