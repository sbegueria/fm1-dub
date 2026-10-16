/* SPDX-License-Identifier: GPL-3.0-only */
/* DUB-1 UI: the panel and the screen. Part of the unity build (after gfx.c and project.c); the host
 * simulator includes it too.
 *
 *   keys           the scale: each key adds or removes its note from the pool (the root stays)
 *   PLAY           plays / rests                   EDIT  tapped: the CHORDS page (eight slots: KNOB 1 the slot, the
 *                  keys its root, KNOB 2 its kind, KNOB 3 its type, KNOB 4 Play: Random or the slots); held: the next chord
 *   REC            tapped: DUB HOLD (the echo repeats what it holds, the chord holds); held: the EFFECTS grid, played
 *                  on the white keys while they're held             OCT- / OCT+   the pings' octave
 *   SEL            PERFORM: the keys mute the layers (white 1-5) and, held, make the dub moves (white 6-10)
 *   SELECT         the pages (with HOME held: the tempo)   ALGORITHM mode (loads a pool)   PRESETS the piece
 *   HOME ENV ARP LFO SEQ FX GLO   the pages: DUB, CHORD, TEXTURE, BASS, DRUMS, SPACE, SETUP
 *   KNOB 1-4       the page's four values
 *   SAVE           saves (it also saves by itself a few seconds after a change, while resting);
 *                  held 1.5 s: the piece back to its recipe
 * The SMK-37 build (BOARD_SMK37): the same, and the SMK's own controls (below, "SMK-37"). */
#include "../dsp/fastmath.h"

/* the pages, one a layer (a button each); SELECT steps through them in this order */
/* (past NPAGES, off SELECT's round and on no button of their own: V_MIX, PERFORM's knobs, the four layers'
 * levels; the layers' sound pages, the layer's button pressed again) */
enum { V_DUB, V_CHORD, V_TEXTURE, V_BASS, V_DRUMS, V_SPACE, V_SETUP, NPAGES, V_MIX = NPAGES, V_CSND, V_PSND, V_BSND,
       V_DSND, V_CFM, V_PFM, V_BFM, V_CHORDS, V_PAT, NVIEWS };
static const char *const VIEW_NAME[NVIEWS] = {"Dub", "Chord", "Texture", "Bass", "Drums", "Space", "Setup", "Mix",
                                              "Chord sound", "Ping sound", "Bass sound", "Drum sound",
                                              "Chord FM", "Ping FM", "Bass FM", "Chords", "Pattern"};
#define B_NONE 0xFFu
static const uint8_t VIEW_BTN[NVIEWS] = {B_HOME, B_ENV, B_ARP, B_LFO, B_SEQ, B_FX, B_GLO, B_NONE, B_NONE, B_NONE, B_NONE,
                                         B_NONE, B_NONE, B_NONE, B_NONE, B_NONE, B_NONE};
static int sound_of(int v) { return v == V_CHORD ? V_CSND : v == V_TEXTURE ? V_PSND : v == V_BASS ? V_BSND : v == V_DRUMS ? V_DSND : -1; }
static int fm_of(int v) { return v == V_CHORD ? V_CFM : v == V_TEXTURE ? V_PFM : v == V_BASS ? V_BFM : -1; }
static int parent_of(int v)
{
    return v == V_CSND || v == V_CFM ? V_CHORD : v == V_PSND || v == V_PFM ? V_TEXTURE : v == V_BSND || v == V_BFM ? V_BASS
         : v == V_DSND || v == V_PAT ? V_DRUMS : -1;
}
static int layer_of(int v) { v = parent_of(v) >= 0 ? parent_of(v) : v; return v == V_CHORD ? 0 : v == V_TEXTURE ? 1 : v == V_BASS ? 2 : 3; }
static int page_of(int v) { return parent_of(v) >= 0 ? parent_of(v) : v; }   /* a sound page: its layer's */
/* the four knobs of each page: a parameter, or one of these; and what the knob is called there */
enum { K_LEDS = 200, K_CPU, K_NONE, K_SLOT, K_CQ, K_CX,    /* (the Chords page: the slot, and its kind and type) */
       K_LANE, K_LCH };                                      /* (the Pattern page: the lane, and its chance) */
static const char *const LANE_NAME[EN_NDRUM] = {"Kick", "Shaker", "Open", "Rim", "Clap"};
_Static_assert(P_NPARAMS <= K_LEDS, "the knobs' own codes must come after the parameters");
static const uint16_t VIEW_KNOB[NVIEWS][4] = {
    {P_TEMPO, P_EVOLVE, P_MOTION, P_DRIFT},
    {P_CHORD, P_STABS, P_DECAY, P_CUTOFF},
    {P_PINGS, P_DENSITY, P_CLOUD, P_CRACKLE},
    {P_BASS, P_BASSOCT, P_GROOVE, P_FIFTH},
    {P_DRUMS, P_HITS, P_SWING, P_DUCK},
    {P_ECHO, P_ETIME, P_FEEDBACK, P_REVERB},
    {P_ROOT, P_TUNE, P_MIDI, K_LEDS},
    {P_CHORD, P_PINGS, P_BASS, P_DRUMS},
    {P_CATT, P_CCOL, P_CRES, P_CENV},
    {P_PTON, P_PRAT, P_PLEN, P_PBEND},
    {P_BBITE, P_BSHAPE, P_BLEN, P_BSLIDE},
    {P_KTUNE, P_KLEN, P_KPUNCH, P_HTONE},
    {P_CALG, P_CFRAT, P_CFB, P_CFDEC},
    {P_PALG, P_PFRAT, P_PFB, P_PFDEC},
    {P_BALG, P_BFRAT, P_BFB, P_BFDEC},
    {K_SLOT, K_CQ, K_CX, P_PMODE},
    {K_LANE, P_DACCENT, K_LCH, P_DMODE},
};
static const char *const KNOB_LABEL[NVIEWS][4] = {
    {"Tempo", "Evolve", "Motion", "Drift"},
    {"Level", "Stabs", "Decay", "Cutoff"},
    {"Pings", "Density", "Cloud", "Crackle"},
    {"Level", "Octave", "Groove", "Fifth"},
    {"Level", "Hits", "Swing", "Duck"},
    {"Echo", "Echo time", "Feedback", "Reverb"},
    {"Root", "Tune", "MIDI out", "Lights"},
    {"Chord", "Pings", "Bass", "Drums"},
    {"Attack", "Colour", "Reso", "Env"},
    {"Tone", "Ratio", "Length", "Bend"},
    {"Bite", "Shape", "Length", "Slide"},
    {"Kick tune", "Kick len", "Punch", "Hats"},
    {"Algo", "Ratio", "Feedback", "Mod dec"},
    {"Algo", "Ratio", "Feedback", "Mod dec"},
    {"Algo", "Ratio", "Feedback", "Mod dec"},
    {"Slot", "Chord", "Type", "Play"},
    {"Lane", "Accent", "Chance", "Play"},
};

/* the screen's colours: a dark room, as a dub techno sleeve (grey on black, a cold blue). The chord and
 * the scale in teal, the pings in a paler blue, the bass in brass, the drums in amber; dub hold in red.
 * Each layer has a dark tint for the shapes at rest. */
#define K_BG RGB(16, 20, 26)
#define K_PANEL RGB(36, 44, 56)
#define K_LINE RGB(64, 76, 94)
#define K_DIM RGB(146, 160, 176)
#define K_TEXT RGB(222, 228, 232)
#define K_ACC RGB(92, 196, 204)          /* the accent: the page, the knob being turned, a message's word */
#define K_BELL RGB(128, 170, 224)
#define K_BELL_T RGB(54, 76, 112)
#define K_PAD RGB(84, 200, 186)
#define K_PAD_T RGB(40, 96, 98)
#define K_PULSE RGB(236, 150, 84)
#define K_PULSE_T RGB(106, 72, 50)
#define K_BASS RGB(204, 180, 112)
#define K_BASS_T RGB(98, 86, 58)
#define K_FROST RGB(232, 92, 84)         /* dub hold */
#define K_FROST_T RGB(128, 56, 52)
#define K_RIM RGB(214, 124, 176)         /* the rim */
#define K_RIM_T RGB(108, 58, 86)
#define K_CLAP RGB(232, 206, 98)         /* the clap */
#define K_CLAP_T RGB(108, 98, 48)

static struct {
    uint8_t view;
    uint8_t perf, perf_from;           /* PERFORM (SEL): the keys play the mutes and the moves, the knobs
                                        * the mix; the page it was started from */
    uint32_t mutes, moves;             /* the layers muted (EN_M_*), the moves held (EN_PF_*) */
    uint32_t fx;                       /* the effects held (EN_FX_*), REC's grid */
    uint8_t slot, chords_from;         /* the Chords page: the slot edited; the page it was opened from */
    uint8_t lane;                      /* the Pattern page: the lane edited */
    uint8_t grid_said;
    uint32_t btn, keys, btn_used;
    uint32_t enc_t[NE];
    uint8_t enc_run[NE];               /* detents in the current quick run, per knob (accel) */
    uint32_t down_t[NB];               /* when each button went down */
    uint32_t turn_t;                   /* the last knob turn */
    uint8_t help;                      /* a button's card is up: the button + 1 */
    uint8_t reset_done;                /* SAVE held: the piece went back to its recipe this hold */
    uint8_t dirty;                     /* the project changed since the last save */
    uint32_t act_t, saved_t;
    char msg[2][24];
    uint32_t msg_until;
    int8_t touched;                    /* the knob turned last (0..3), shown bright for a moment */
    uint32_t touch_until;
    uint32_t frame;
    uint32_t sig[3];                   /* what each band shows: redrawn when it changes */
} ui;

/* the screen reader (announce.c; speak() below): a message went to it this frame, so speak() leaves what it
 * changed be; and the transport as last said (PLAY says its own: the engine takes it a block later) */
static uint8_t sp_said;
static char sp_buf[ANN_MAX + 1];                 /* (the screen reader's text, built below) */
static uint32_t sp_n;
static void sp_cat(const char *s);
static struct {
    uint8_t playing;
    uint32_t play_t;                   /* when PLAY (or the start) set it: the engine catches up within ms */
} sp_tp;
static void sp_piece(void);
static void msg_text(const char *a, const char *b, char *t)   /* the message as a reader gets it: "a b" */
{
    uint32_t i, k = 0;
    for (i = 0; i < 23u && a && a[i]; i++)
        t[k++] = a[i];
    if (b && b[0])
        t[k++] = ' ';
    for (i = 0; i < 23u && b && b[i]; i++)
        t[k++] = b[i];
    while (k && t[k - 1] == '.')               /* "UPDATE MODE IN 3..": the dots are for the eye */
        k--;
    t[k] = 0;
}
/* a message for a moment in the header; flags: how a screen reader gets it (0: the caller tells it) */
static void say_as(const char *a, const char *b, uint32_t flags)
{
    uint32_t i;
    char t[48];
    for (i = 0; i < 23u && a && a[i]; i++)
        ui.msg[0][i] = a[i];
    ui.msg[0][i] = 0;
    for (i = 0; i < 23u && b && b[i]; i++)
        ui.msg[1][i] = b[i];
    ui.msg[1][i] = 0;
    ui.msg_until = plat_ms() + 1300u;
    if (flags && ann_on()) {
        msg_text(a, b, t);
        ann_say(t, 0, flags);
        sp_said = 1;
    }
}
static void say(const char *a, const char *b) { say_as(a, b, ANN_NOW); }
/* something did not happen: a screen reader says it whole (what follows does not cut it off) */
static void warn(const char *a, const char *b) { say_as(a, b, ANN_NOW | ANN_KEEP); }
void ui_say(const char *a, const char *b) { say(a, b); }
#if BOARD_SMK37
static int smk_live(int k, int v);             /* a value as the engine gets it: the mod wheel adds to it */
static void smk_resend(void);
#endif

static void itoa_u(uint32_t v, char *b)
{
    char t[12];
    int n = 0;
    do {
        t[n++] = (char)('0' + v % 10u);
        v /= 10u;
    } while (v);
    while (n)
        *b++ = t[--n];
    *b = 0;
}

static void mark_dirty(void) { ui.dirty = 1; }
int ui_dirty(void) { return ui.dirty != 0; }

static int pool_count(uint32_t p)
{
    int n = 0, i;
    for (i = 0; i < 12; i++)
        n += (int)(p >> i & 1u);
    return n;
}
static uint32_t pool_abs(void)                 /* the pool as absolute pitch classes */
{
    uint32_t p = proj.pool, r = (uint32_t)proj.par[P_ROOT];
    return ((p << r) | (p >> (12u - r))) & 0xFFFu;
}
static const char *mode_name(void)
{
    return proj.pool == EN_MODE_POOL[proj.par[P_MODE]] ? EN_MODE_NAME[proj.par[P_MODE]] : "Custom";
}

/* ------------------------------------------------------------ values --- */
static int knob_get(int k)
{
    if (k < P_NPARAMS)
        return proj.par[k];
    if (k == K_LEDS)
        return proj.leds;
    if (k == K_CPU)
        return (int)plat_cpu_pct();
    if (k == K_SLOT)
        return ui.slot;
    if (k == K_CQ || k == K_CX)
        return proj.par[(k == K_CQ ? P_CQ0 : P_CX0) + ui.slot];
    if (k == K_LANE)
        return ui.lane;
    if (k == K_LCH)
        return proj.par[P_DCH0 + ui.lane];
    return 0;
}
static void knob_range(int k, int *lo, int *hi)
{
    if (k < P_NPARAMS) {
        *lo = en_param_info(k)->lo;
        *hi = en_param_info(k)->hi;
    } else {
        *lo = 0;
        *hi = k == K_LEDS ? 2 : k == K_CPU ? 100 : k == K_SLOT ? EN_NPROG - 1 : k == K_CQ ? EN_NCQ - 1 : k == K_CX ? EN_NCX - 1
            : k == K_LANE ? EN_NDRUM - 1 : k == K_LCH ? 100 : 0;
    }
}
static const char *knob_name(int k)
{
    if (k < P_NPARAMS)
        return en_param_info(k)->name;
    return k == K_LEDS ? "Lights" : k == K_CPU ? "CPU" : k == K_SLOT ? "Slot" : k == K_CQ ? "Chord" : k == K_CX ? "Type"
         : k == K_LANE ? "Lane" : k == K_LCH ? "Chance" : "";
}
static void knob_text(int k, char *b)
{
    int v = knob_get(k);
    if (k < P_NPARAMS) {
        en_param_text(k, v, b);
    } else if (k == K_LEDS) {                    /* On: the keys, and the unlit buttons glow; Keys: no glow */
        const char *n = v == 2 ? "Keys" : v ? "On" : "Off";
        while ((*b++ = *n++))
            ;
    } else if (k == K_CPU) {
        itoa_u((uint32_t)v, b);
        while (*b)
            b++;
        *b++ = '%';
        *b = 0;
    } else if (k == K_SLOT) {
        itoa_u((uint32_t)v + 1u, b);
    } else if (k == K_CQ || k == K_CX) {
        en_param_text((k == K_CQ ? P_CQ0 : P_CX0) + ui.slot, v, b);
    } else if (k == K_LANE) {
        const char *n = LANE_NAME[(unsigned)v < EN_NDRUM ? v : 0];
        while ((*b++ = *n++))
            ;
    } else if (k == K_LCH) {
        itoa_u((uint32_t)v, b);
        while (*b)
            b++;
        *b++ = '%';
        *b = 0;
    } else {
        b[0] = 0;
    }
}

static void knob_set(int k, int v)
{
    int lo, hi;
    if (k == K_CPU || k == K_NONE)
        return;
    knob_range(k, &lo, &hi);
    v = v < lo ? lo : v > hi ? hi : v;
    if (v == knob_get(k))
        return;
    if (k < P_NPARAMS) {
        proj.par[k] = (int16_t)v;
#if BOARD_SMK37
        en_set(k, smk_live(k, v));
#else
        en_set(k, v);
#endif
    } else if (k == K_LEDS) {
        proj.leds = (uint8_t)v;
    } else if (k == K_SLOT) {
        ui.slot = (uint8_t)v;
        return;                                  /* (only where the page looks: nothing saved) */
    } else if (k == K_CQ || k == K_CX) {
        knob_set((k == K_CQ ? P_CQ0 : P_CX0) + ui.slot, v);
        if (k == K_CQ && v && !proj.par[P_PMODE])
            knob_set(P_PMODE, 1);                /* a chord written: the slots play */
        return;
    } else if (k == K_LANE) {
        ui.lane = (uint8_t)v;
        return;
    } else if (k == K_LCH) {
        knob_set(P_DCH0 + ui.lane, v);
        return;
    }
    mark_dirty();
}

/* knob acceleration by speed (X0X's): a few clicks are one step each however quick, so small
 * corrections stay exact; a longer quick turn speeds up to 8 a detent, so half a turn sweeps 0-100.
 * Small ranges never accelerate. */
static uint8_t accel_off;              /* the host simulator's "spin": exact steps */
static int32_t accel(int role, int32_t s, int range)
{
    uint32_t now = plat_ms(), dt = now - ui.enc_t[role], a = (uint32_t)(s < 0 ? -s : s), m;
    ui.enc_t[role] = now;
    if (!a)
        return s;
    if (a > 1)
        dt /= a;                                      /* per detent */
    ui.enc_run[role] = (uint8_t)(dt < 60u ? (ui.enc_run[role] + a > 255u ? 255u : ui.enc_run[role] + a) : a);
    if (range <= 24 || accel_off || ui.enc_run[role] < 4u)
        return s;
    m = dt < 12u ? 8u : dt < 25u ? 5u : dt < 45u ? 3u : 1u;
    if (range < 100 && m > 3u)
        m = 3u;
    return s * (int32_t)m;
}

static void turn(int role, int k, int32_t e)
{
    int lo, hi;
    if (!e || k == K_NONE || k == K_CPU)
        return;
    knob_range(k, &lo, &hi);
    knob_set(k, knob_get(k) + accel(role, e, hi - lo));
}

/* ------------------------------------------------------------- the pool --- */
static void toggle_note(int pc)                 /* a key: its note in or out of the scale */
{
    int rel = (pc - proj.par[P_ROOT] + 12) % 12;
    uint32_t p = proj.pool ^ (1u << rel);
    if (rel == 0) {
        warn("THE ROOT STAYS", "(SETUP)");
        return;
    }
    if (pool_count(p) < 3) {
        warn("3 NOTES", "AT LEAST");
        return;
    }
    proj.pool = (uint16_t)p;
    en_pool(p);
    say(p >> rel & 1u ? "ADDED" : "REMOVED", EN_NOTE_NAME[pc]);
    mark_dirty();
}

static void set_mode(int m)
{
    m = (m % EN_NMODES + EN_NMODES) % EN_NMODES;
    proj.par[P_MODE] = (int16_t)m;
    proj.pool = EN_MODE_POOL[m];
    en_set(P_MODE, m);
    en_pool(proj.pool);
    say("MODE", EN_MODE_NAME[m]);
    mark_dirty();
}

/* the voices: ALGORITHM on a layer's page steps through that layer's (round: after the last, the first) */
static int view_voice(int v)
{
    v = page_of(v);
    return v == V_CHORD ? P_CVOICE : v == V_TEXTURE ? P_PVOICE : v == V_BASS ? P_BVOICE : v == V_DRUMS ? P_KIT : -1;
}
static void set_voice(int p, int v)
{
    const en_param_t *pi = en_param_info(p);
    char t[12];
    int n = pi->hi - pi->lo + 1;
    v = ((v - pi->lo) % n + n) % n + pi->lo;
    proj.par[p] = (int16_t)v;
    en_set(p, v);
    en_param_text(p, v, t);
    say(p == P_KIT ? "KIT" : "VOICE", t);
    mark_dirty();
}

static void set_piece(int i)                    /* the piece playing kept as it is; piece i as it was left */
{
    project_piece(i);
    project_apply();
#if BOARD_SMK37
    smk_resend();
#endif
    en_seed(plat_ms());
    en_regrow();
    say_as("PIECE", piece_name(proj.piece), 0);
    sp_piece();                                  /* a screen reader: the piece with its key and tempo */
    mark_dirty();
}

/* ----------------------------------------------------------- PERFORM --- */
/* SEL's mode: the white keys from F3 up. The first five mute and bring back a layer (a press each); the next
 * five are dub moves, held as long as the key is: every stab thrown into the echo, the filter pushed open,
 * pushed shut, the kick dropped, the echo pushed (all in, feeding back). The black keys and the rest do
 * nothing; MIDI notes in still change the scale */
static const uint8_t PERF_KEY[10] = {0, 2, 4, 6, 7, 9, 11, 12, 14, 16};   /* white keys 1..10, as key numbers */
static const char *const PERF_NAME[10] = {"CHORD", "PINGS", "BASS", "KICK", "HATS", "THROW", "OPEN", "SHUT",
                                           "KICK DROP", "ECHO"};
static void perf_key(uint32_t key, int down)
{
    int i;
    for (i = 0; i < 10 && PERF_KEY[i] != key; i++)
        ;
    if (i >= 10)
        return;
    if (i < 3) {                                 /* the chord, pings, bass: their Mix knob to 0 or 100 */
        static const uint8_t LEVEL[3] = {P_CHORD, P_PINGS, P_BASS};
        int v;
        if (!down)
            return;
        v = proj.par[LEVEL[i]] > 0 ? 0 : 100;
        knob_set(LEVEL[i], v);
        if (ui.view == V_MIX) {                  /* the knob lit, as if turned */
            ui.touched = (int8_t)i;
            ui.touch_until = plat_ms() + 1200u;
        }
        say(PERF_NAME[i], v ? "ON" : "OFF");
        return;
    }
    if (i < 5) {                                 /* the kick, the hats (one Drums knob for both): a mute */
        if (!down)
            return;
        ui.mutes ^= 1u << i;
        en_mute(ui.mutes);
        /* the Drums knob follows: 0 once both are out, 100 when one comes back (from 0) */
        if ((ui.mutes & (EN_M_KICK | EN_M_HATS)) == (EN_M_KICK | EN_M_HATS))
            knob_set(P_DRUMS, 0);
        else if (proj.par[P_DRUMS] == 0 || !(ui.mutes >> i & 1u))
            knob_set(P_DRUMS, 100);
        if (ui.view == V_MIX) {
            ui.touched = 3;
            ui.touch_until = plat_ms() + 1200u;
        }
        say(PERF_NAME[i], ui.mutes >> i & 1u ? "OFF" : "ON");
        return;
    }
    if (down)
        ui.moves |= 1u << (i - 5);
    else
        ui.moves &= ~(1u << (i - 5));
    en_perf(ui.moves);
    if (down)
        say(PERF_NAME[i], 0);
}

static void set_view(int v);
static void perf_set(int on)                     /* on: the mixer on the knobs; off: back to the page */
{
    ui.perf = (uint8_t)on;
    if (on) {
        if (ui.view != V_MIX)
            ui.perf_from = ui.view;
        set_view(V_MIX);
    } else {
        if (ui.moves) {                          /* the moves let go; the mutes stay */
            ui.moves = 0;
            en_perf(0);
        }
        if (ui.view == V_MIX)
            set_view(ui.perf_from);
    }
    say("PERFORM", on ? "ON" : "OFF");
}

/* ------------------------------------------------------- the effects grid --- */
/* REC held: the white keys from F3 up are the sixteen effects (four groups of four: the echo, the filter,
 * the space, rhythm and noise), each as long as its key is held, even once REC is let go. A tap on REC alone
 * is still dub hold (REC's release does nothing once a key played) */
static const uint8_t FX_KEY[EN_NFX] = {0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26};
#define GRID_HOLD 300u                   /* ms of REC held before the grid shows */
static int grid_up(void) { return ui.fx || ((ui.btn >> B_REC & 1u) && plat_ms() - ui.down_t[B_REC] >= GRID_HOLD); }
static int fx_key(uint32_t key, int down)        /* 1: the key was an effect's */
{
    int e;
    for (e = 0; e < EN_NFX && FX_KEY[e] != key; e++)
        ;
    if (e >= EN_NFX)
        return 0;
    if (down) {
        ui.fx |= 1u << e;
        ui.btn_used |= 1u << B_REC;              /* (REC's release: no dub hold now) */
        say(EN_FX_NAME[e], 0);
    } else {
        ui.fx &= ~(1u << e);
    }
    en_fx(ui.fx);
    return 1;
}

/* ----------------------------------------------------------- the chords --- */
/* EDIT tapped: the Chords page, eight slots. KNOB 1 the slot, the keys its root (any note: an empty slot
 * becomes a minor chord on it), KNOB 2 its kind, KNOB 3 its type, KNOB 4 Play: Random (the chords chosen as
 * ever) or the slots in turn, a chord every Drift bars. An empty slot between two holds the one before; after
 * the last filled one, round again. EDIT again, or any page's button: back. EDIT held: the next chord */
#define NEXT_HOLD 500u
static int prog_count(void)                      /* how far the slots run: to the last filled one */
{
    int i, n = 0;
    for (i = 0; i < EN_NPROG; i++)
        if (proj.par[P_CQ0 + i])
            n = i + 1;
    return n;
}
static const char *slot_label(int i, char *nm)
{
    return en_chord_label(proj.par[P_CR0 + i], proj.par[P_CQ0 + i], proj.par[P_CX0 + i], nm);
}
static void set_view(int v);
static void slot_key(uint32_t key)               /* the Chords page: a key, the slot's root */
{
    char nm[16];
    knob_set(P_CR0 + ui.slot, (int)((5u + key) % 12u));
    if (!proj.par[P_CQ0 + ui.slot])
        knob_set(K_CQ, EN_CQ_MINOR);
    if (!proj.par[P_PMODE])
        knob_set(P_PMODE, 1);
    say(slot_label(ui.slot, nm), 0);
}
static void edit_released(uint32_t held)
{
    if (held >= NEXT_HOLD) {                     /* held: the next chord */
        en_next();
        say("NEXT CHORD", en_playing ? "ON THE BAR" : 0);
    } else if (ui.view == V_CHORDS) {            /* tapped: the Chords page, or back from it */
        set_view(ui.chords_from);
    } else {
        ui.chords_from = (uint8_t)ui.view;
        set_view(V_CHORDS);
    }
}

/* ---------------------------------------------------------- the pattern --- */
/* the Drums button a third time: the Pattern page. KNOB 1 the lane (kick, shaker, open hat, rim), the white keys
 * its sixteen steps (a key, a step: on or off), a black key held with a white one: that step accented; KNOB 2
 * the accents' strength, KNOB 3 the lane's chance, KNOB 4 Play: Auto (generated, as ever) or Pattern. The first
 * edit takes the pattern over from what Auto plays. The arrangement still takes lanes out (a Break, a fill) */
static const uint8_t STEP_KEY[16] = {0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26};   /* the white keys */
static uint32_t lane_steps(int d) { return proj.par[P_DMODE] ? (uint32_t)(uint16_t)proj.par[P_DPAT0 + d] : en_drum_pat[d]; }
static void pattern_key(uint32_t key, uint32_t keys)
{
    int st, d = ui.lane, acc;
    uint32_t m, black = 0;
    char t[24];
    for (st = 0; st < 16 && STEP_KEY[st] != key; st++)
        ;
    if (st >= 16)
        return;                                  /* (a black key: an accent's, with a white one) */
    for (m = 0; m < 16; m++)
        keys &= ~(1u << STEP_KEY[m]);
    black = keys;
    if (!proj.par[P_DMODE]) {                    /* the first edit: Auto's pattern, taken over */
        int k;
        for (k = 0; k < EN_NDRUM; k++)
            knob_set(P_DPAT0 + k, (int)(int16_t)(uint16_t)en_drum_pat[k]);
        knob_set(P_DMODE, 1);
    }
    m = (uint32_t)(uint16_t)proj.par[P_DPAT0 + d];
    acc = (int)((uint32_t)(uint16_t)proj.par[P_DACC0 + d] >> st & 1u);
    if (black) {                                 /* accent it (on, too), or not */
        knob_set(P_DACC0 + d, (int)(int16_t)(uint16_t)((uint32_t)(uint16_t)proj.par[P_DACC0 + d] ^ (1u << st)));
        knob_set(P_DPAT0 + d, (int)(int16_t)(uint16_t)(m | 1u << st));
        acc = !acc;
    } else {
        knob_set(P_DPAT0 + d, (int)(int16_t)(uint16_t)(m ^ 1u << st));
    }
    t[0] = 0;
    itoa_u((uint32_t)st + 1u, t);
    say_as(LANE_NAME[d], 0, 0);
    if (ann_on()) {
        sp_n = 0;
        sp_buf[0] = 0;
        sp_cat("Step ");
        sp_cat(t);
        sp_cat(black ? (acc ? " accented" : " not accented") : (proj.par[P_DPAT0 + d] >> st & 1 ? " on" : " off"));
        ann_say(sp_buf, 0, ANN_NOW);
        sp_said = 1;
    }
}

#if BOARD_SMK37
/* ------------------------------------------------------------- SMK-37 --- */
/* The SMK-37's controls beyond the FM-1's (plat.h SMK section; the FM-1's keep their DUB-1 meaning):
 *   pads           the 16 white keys F3..G5, lit as the FM-1's keys are, in colour. As the scale (and the Chords
 *                  page's roots) low notes on the bottom row; where the white keys are a row of sixteen (the effects
 *                  grid, the Pattern page's steps, PERFORM's 1-10) the top row is 1-8 and the bottom row 9-16, as
 *                  the screen reads
 *   keys           F3..G5 the FM-1's 27; C3..E3 and G#5..C6 more of the scale (on the Chords page, the root too)
 *   KNOB BANK      the picker on the pads (and the screen): the top row the eight pieces, the bottom row the eight
 *                  modes; KNOB BANK again: back
 *   FADER BANK     what the four faders set: A the levels (as the Mix page), B the space, C the shape (Cutoff,
 *                  Decay, Density, Hits). A fader takes over once it moves; the fader card shows the bank
 *   < >            the previous / next page (as SELECT)
 *   pitch wheel    the tape: up faster (the echo's time down to half), down slower (up to twice); the repeats
 *                  bend as it gets there
 *   mod wheel      the filter: the chord's Cutoff opened on top of its knob */
#define SMK_NBANK 3
#define SMK_CARD_MS 1800u
static const uint8_t SMK_FADER_PAR[SMK_NBANK][4] = {{P_CHORD, P_PINGS, P_BASS, P_DRUMS},
                                                    {P_ECHO, P_ETIME, P_FEEDBACK, P_REVERB},
                                                    {P_CUTOFF, P_DECAY, P_DENSITY, P_HITS}};
static const char *const SMK_FADER_LABEL[SMK_NBANK][4] = {{"Chord", "Pings", "Bass", "Drums"},
                                                          {"Echo", "Echo time", "Feedback", "Reverb"},
                                                          {"Cutoff", "Decay", "Density", "Hits"}};
static const char *const SMK_BANK_NAME[SMK_NBANK] = {"A LEVELS", "B SPACE", "C SHAPE"};
static struct {
    uint32_t sbtn;                     /* the four extra buttons held (SB_*) */
    uint8_t fbank;                     /* the fader bank, 0..2 */
    uint8_t flast[4];                  /* each fader's last value, 0xFF: not read yet (takes over once it moves) */
    int8_t ftouch;                     /* the fader moved last (the card shows it), -1 none */
    uint8_t pick;                      /* KNOB BANK: the picker up */
    uint32_t card_until;               /* the fader card */
    int32_t pitch, modw;               /* the wheels: pitch -127..127, mod 0..127 */
} smk;
static void smk_reset(void)
{
    memset(&smk, 0, sizeof smk);
    memset(smk.flast, 0xFF, sizeof smk.flast);
    smk.ftouch = -1;
}
static int smk_fcard(void) { return plat_ms() < smk.card_until; }

static int smk_live(int k, int v)               /* Cutoff: the mod wheel opens it the rest of the way */
{
    int lo, hi;
    if (k != P_CUTOFF || !smk.modw)
        return v;
    knob_range(k, &lo, &hi);
    return v + (hi - v) * (int)smk.modw / 127;
}
static void smk_resend(void) { en_set(P_CUTOFF, smk_live(P_CUTOFF, proj.par[P_CUTOFF])); }

/* the pads' layout: the effects grid (REC held) first, then the picker, then the rows of sixteen */
static int smk_layout(uint32_t btn)
{
    if ((btn >> B_REC & 1u) || ui.fx)
        return SMK_PADS_GRID;
    if (smk.pick)
        return SMK_PADS_OWN;
    return ui.perf || ui.view == V_PAT ? SMK_PADS_GRID : SMK_PADS_PIANO;
}

static void smk_page(int d)                      /* < >: as SELECT */
{
    int v = page_of(ui.view) + d;
    if (ui.view == V_MIX)
        v = (int)ui.perf_from + d;
    if (ui.view == V_CHORDS)
        v = (int)ui.chords_from + d;
    if (v >= 0 && v < NPAGES)
        set_view(v);
}

static void smk_input(uint32_t btn)
{
    uint32_t ph = plat_pad_hits(), sb = plat_smk_buttons(), sp = sb & ~smk.sbtn, now = plat_ms(), i;
    uint64_t kh = plat_keybed_hits();
    int32_t pitch, modw;
    smk.sbtn = sb;
    for (i = 0; i < SMK_NPADS; i++)              /* the picker: the top row the pieces, the bottom row the modes */
        if (smk.pick && (ph >> i & 1u)) {
            if (i >= 8u)
                set_mode((int)i - 8);
            else if ((int)i != proj.piece)
                set_piece((int)i);
        }
    if (sp >> SB_KNOB_BANK & 1u) {               /* KNOB BANK: the picker, up or put away */
        smk.pick = (uint8_t)!smk.pick;
        smk.card_until = 0;                      /* (one card at a time) */
        say("PADS", smk.pick ? "PIECE + MODE" : "PLAY");
    }
    if (sp >> SB_FADER_BANK & 1u) {              /* FADER BANK: the next bank, on the fader card */
        smk.fbank = (uint8_t)((smk.fbank + 1u) % SMK_NBANK);
        memset(smk.flast, 0xFF, sizeof smk.flast);
        smk.ftouch = -1;
        smk.card_until = now + SMK_CARD_MS;
        say("FADERS", SMK_BANK_NAME[smk.fbank]);
    }
    if (sp >> SB_LEFT & 1u)
        smk_page(-1);
    if (sp >> SB_RIGHT & 1u)
        smk_page(1);
    if (sp)
        ui.turn_t = now;                         /* (a help card: put away) */
    if (!(btn >> B_REC & 1u) && !ui.fx && !ui.perf && ui.view != V_PAT)
        for (i = 0; i < SMK_NKEYBED; i++)        /* the outer keys (C3..E3, G#5..C6): their notes, as the keys */
            if ((i < 5u || i >= 32u) && (kh >> i & 1u)) {
                if (ui.view == V_CHORDS)
                    slot_key((i % 12u + 7u) % 12u);   /* (the key of the same note: key 0 is F3) */
                else
                    toggle_note((int)(i % 12u));
            }
    for (i = 0; i < 4u; i++) {                   /* the faders: a value each, once a fader moves */
        int v = plat_fader((int)i), k = SMK_FADER_PAR[smk.fbank][i], lo, hi, j;
        if (v < 0)
            continue;
        if (smk.flast[i] == 0xFFu) {
            smk.flast[i] = (uint8_t)v;
            continue;
        }
        if (v == smk.flast[i])
            continue;
        smk.flast[i] = (uint8_t)v;
        knob_range(k, &lo, &hi);
        knob_set(k, lo + (v * (hi - lo) + 63) / 127);
        smk.ftouch = (int8_t)i;
        smk.card_until = now + SMK_CARD_MS;
        for (j = 0; j < 4; j++)                  /* the page's own knob of it, lit as if turned */
            if (VIEW_KNOB[ui.view][j] == k) {
                ui.touched = (int8_t)j;
                ui.touch_until = now + 1200u;
            }
        ui.act_t = now;
    }
    if (plat_wheels(&pitch, &modw)) {
        char t[8];
        if (pitch != smk.pitch) {                /* the tape: the echo's time, in % of what it is set to */
            uint32_t pc = (uint32_t)(100.0f * fm_exp2f((float)pitch * (-1.0f / 127.0f)) + 0.5f), n;
            smk.pitch = pitch;
            en_warp((int)pitch);
            itoa_u(pc, t);
            for (n = 0; t[n]; n++)
                ;
            t[n++] = '%';
            t[n] = 0;
            say_as("ECHO TIME", t, 0);
        }
        if (modw != smk.modw) {                  /* the filter */
            smk.modw = modw;
            smk_resend();
            t[0] = '+';
            itoa_u((uint32_t)(modw * 100 / 127), t + 1);
            say_as("CUTOFF", t, 0);
        }
        ui.act_t = now;
    }
    if (sb || kh || ph)
        ui.act_t = now;
    plat_smk_pads(smk_layout(btn));
}
#endif

/* ------------------------------------------------------------ buttons --- */
static void set_view(int v)
{
    ui.view = (uint8_t)v;
    ui.touched = -1;
}

static void button(int b)
{
    int v;
    if (sound_of(ui.view) >= 0 && b == VIEW_BTN[ui.view]) {   /* the layer's button again: its sound page */
        set_view(sound_of(ui.view));
        return;
    }
    if (parent_of(ui.view) >= 0 && b == VIEW_BTN[parent_of(ui.view)]) {   /* again: an FM voice's FM page; then back */
        int pv = parent_of(ui.view);
        if (ui.view == sound_of(pv) && fm_of(pv) >= 0)   /* (on a voice that isn't FM, the page says so) */
            set_view(fm_of(pv));
        else if (ui.view == sound_of(pv) && pv == V_DRUMS)
            set_view(V_PAT);                     /* the drums: their pattern */
        else
            set_view(pv);
        return;
    }
    if (b == B_HOME && ui.view == V_DUB) {       /* HOME again: the middle of the screen, the next way */
        static const char *const SCREEN_NAME[3] = {"RIPPLES", "SCOPE", "SPECTRUM"};
        proj.screen = (uint8_t)((proj.screen + 1u) % 3u);
        mark_dirty();
        say("SCREEN", SCREEN_NAME[proj.screen]);
        return;
    }
    for (v = 0; v < NPAGES; v++)                 /* a page's button: its page */
        if (VIEW_BTN[v] == b) {
            set_view(v);
            return;
        }
    switch (b) {
    case B_SEL:
        perf_set(!ui.perf);
        break;
    case B_EDIT:                                 /* (its release: edit_released, in input) */
        break;
    case B_PLAY:
        sp_tp.playing = !en_playing;
        sp_tp.play_t = plat_ms();
        if (en_playing) {
            en_play(0);
            say("RESTING", 0);
        } else {
            en_seed(plat_ms());
            en_play(1);
            say("PLAYING", 0);
        }
        break;
    case B_REC:
        if (plat_ms() - ui.down_t[B_REC] >= GRID_HOLD)
            break;                               /* held long enough for the grid: only a tap is dub hold */
        say("DUB HOLD", en_frozen ? "OFF" : "ON");
        en_freeze(!en_frozen);                   /* the engine takes it at its next block */
        break;
    case B_OCTDN: case B_OCTUP:
        knob_set(P_POCT, proj.par[P_POCT] + (b == B_OCTUP ? 1 : -1));
        {
            char t[8];
            en_param_text(P_POCT, proj.par[P_POCT], t);
            say("PING OCTAVE", t);
        }
        break;
    case B_SAVE:
        if (project_save() == 0) {
            ui.dirty = 0;
            say("SAVED", 0);
        } else {
            warn("NOT SAVED", "FLASH ERROR");
        }
        break;
    default: break;
    }
}

/* MIDI in (USB and the TRS jack): channel 1 notes add or remove their note from the scale; clock and
 * start / stop drive the grid */
static void midi_in(void)
{
    uint32_t pkt;
    while (plat_midi_in(&pkt)) {
        uint32_t st = (pkt >> 8) & 0xFFu, n = (pkt >> 16) & 0x7Fu, v = pkt >> 24;
        if (st == 0xF8u || st == 0xFAu || st == 0xFBu || st == 0xFCu)
            en_clock(st == 0xF8u ? EN_CLK_TICK : st == 0xFAu ? EN_CLK_START : st == 0xFBu ? EN_CLK_CONTINUE : EN_CLK_STOP);
        else if (st == 0x90u && v)
            toggle_note((int)(n % 12u));
    }
}

/* HELP CARDS (X0X's): hold one button alone for HELP_HOLD and a card says what it does. A key, a
 * turn or another button puts it away; letting go after its card does nothing else. PLAY acts on
 * its press, and SEL has no job here: neither has a card. */
#define HELP_HOLD 900u
#define RESET_HOLD 1500u
static void help_tick(uint32_t btn, uint32_t keys)
{
    uint32_t now = plat_ms();
    int i;
    if (ui.help) {
        uint32_t m = 1u << (ui.help - 1);
        if (btn != m || keys || (int32_t)(ui.turn_t - ui.down_t[ui.help - 1]) > 0)
            ui.help = 0;
        return;
    }
    if (!btn || (btn & (btn - 1u)) || keys)
        return;
    for (i = 0; i < NB; i++)
        if (btn == 1u << i && i != B_PLAY && i != B_REC && i != B_EDIT && !(ui.btn_used & btn) && now - ui.down_t[i] >= HELP_HOLD &&
            (int32_t)(ui.turn_t - ui.down_t[i]) <= 0) {
            ui.help = (uint8_t)(i + 1);
            ui.btn_used |= btn;                  /* its release now does nothing */
        }
}

static void input(void)
{
#if BOARD_SMK37
    uint32_t btn = plat_buttons(), keys, ch, i;
    int32_t e;
    smk_input(btn);                              /* (the pads' layout first: plat_keys reads the pads in it) */
    keys = plat_keys();
#else
    uint32_t btn = plat_buttons(), keys = plat_keys(), ch, i;
    int32_t e;
#endif
    ch = btn ^ ui.btn;
    ui.btn = btn;
    for (i = 0; i < NB; i++) {
        uint32_t m = 1u << i;
        if (!(ch & m))
            continue;
        if (btn & m) {
            ui.btn_used &= ~m;
            ui.down_t[i] = plat_ms();
            if (i == B_SAVE)
                ui.reset_done = 0;
            if (i == B_PLAY)                   /* transport on the press */
                button((int)i);
        } else if (i == B_EDIT) {               /* EDIT: the Chords page (a tap), the next chord (held) */
            edit_released(plat_ms() - ui.down_t[B_EDIT]);
        } else if (!(ui.btn_used & m) && i != B_PLAY) {
            button((int)i);
        }
    }
    ch = keys ^ ui.keys;
    ui.keys = keys;
    for (i = 0; i < NKEYS; i++)
        if (ch >> i & 1u) {
            int down = (int)(keys >> i & 1u);
            if (((btn >> B_REC & 1u) && down) || (!down && ui.fx))
                if (fx_key(i, down))
                    continue;                    /* REC held: the effects (and their keys let go) */
            if (btn >> B_REC & 1u)
                continue;                        /* (REC held: the other keys do nothing) */
            if (ui.view == V_PAT && !ui.perf) {  /* the Pattern page: the white keys, the lane's steps */
                if (down)
                    pattern_key(i, keys);
                continue;
            }
            if (ui.view == V_CHORDS && !ui.perf) {   /* the Chords page: the keys, the slot's root */
                if (down)
                    slot_key(i);
                continue;
            }
            if (ui.perf)
                perf_key(i, (int)(keys >> i & 1u));
            else if (keys >> i & 1u)
                toggle_note((int)((5u + i) % 12u));   /* key 0 is F3 */
        }
    if (btn || keys || ch)
        ui.act_t = plat_ms();
    midi_in();
    if ((e = plat_enc(EN_SELECT)) != 0) {        /* SELECT: the pages; with HOME held, the tempo */
        if (btn >> B_HOME & 1u) {
            turn(EN_SELECT, P_TEMPO, e);
            ui.btn_used |= 1u << B_HOME;
            if (ui.view == V_DUB) {              /* Tempo is the Dub page's first knob */
                ui.touched = 0;
                ui.touch_until = plat_ms() + 1200u;
            } else {
                char t[8];
                itoa_u((uint32_t)proj.par[P_TEMPO], t);
                say("TEMPO", t);
            }
        } else {
            int v = page_of(ui.view) + (e > 0 ? 1 : -1);
            if (ui.view == V_MIX)                /* from the mixer: SELECT goes on from the page it came from */
                v = (int)ui.perf_from + (e > 0 ? 1 : -1);
            if (ui.view == V_CHORDS)
                v = (int)ui.chords_from + (e > 0 ? 1 : -1);
            if (v >= 0 && v < NPAGES)
                set_view(v);
        }
        ui.turn_t = plat_ms();
    }
    if ((e = plat_enc(EN_ALGO)) != 0) {         /* on a layer's page: its voice; elsewhere: the mode */
        int vp = view_voice(ui.view);
        if (vp >= 0)
            set_voice(vp, proj.par[vp] + (e > 0 ? 1 : -1));
        else
            set_mode(proj.par[P_MODE] + (e > 0 ? 1 : -1));
        ui.turn_t = plat_ms();
    }
    if ((e = plat_enc(EN_PRESET)) != 0) {
        set_piece(proj.piece + (e > 0 ? 1 : -1));
        ui.turn_t = plat_ms();
    }
    if ((btn >> B_SAVE & 1u) && !ui.reset_done && plat_ms() - ui.down_t[B_SAVE] > RESET_HOLD) {
        ui.reset_done = 1;                     /* held: the piece back to its recipe (not saved) */
        ui.btn_used |= 1u << B_SAVE;
        ui.help = 0;
        project_piece_reset();
        project_apply();
#if BOARD_SMK37
        smk_resend();
#endif
        en_regrow();
        say("RESET", piece_name(proj.piece));
        mark_dirty();
    }
    for (i = 0; i < 4; i++)
        if ((e = plat_enc(EN_K1 + (int)i)) != 0) {
            if (grid_up())
                continue;                        /* (the grid on the screen: the knobs rest) */
            turn(EN_K1 + (int)i, VIEW_KNOB[ui.view][i], e);
            ui.touched = (int8_t)i;
            ui.touch_until = plat_ms() + 1200u;
            ui.act_t = ui.turn_t = plat_ms();
        }
    help_tick(btn, keys);
}

/* ------------------------------------------------------------- screen --- */
/* three bands: the header (28 px), the main screen (144), the knobs (68) */
#define HDR_H 28
#define MAIN_Y 28
#define MAIN_H 144
#define KNB_Y 172
#define KNB_H 68

static uint32_t hash(uint32_t h, uint32_t v) { return (h ^ v) * 16777619u; }

/* a rectangle with rounded corners (radius r, up to 6) */
static void cv_round(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint16_t c)
{
    static const uint8_t IN[7][6] = {{0}, {1}, {1, 0}, {2, 1, 0}, {2, 1, 0, 0}, {3, 2, 1, 0, 0}, {4, 2, 1, 1, 0, 0}};
    int32_t j;
    if (r > 6)
        r = 6;
    if (r > h / 2)
        r = h / 2;
    cv_rect(x, y + r, w, h - 2 * r, c);
    for (j = 0; j < r; j++) {
        int32_t d = IN[r][j];
        cv_rect(x + d, y + j, w - 2 * d, 1, c);
        cv_rect(x + d, y + h - 1 - j, w - 2 * d, 1, c);
    }
}

/* a filled circle of radius r (0..6) centred on (cx, cy): HW[r][d] is the half width d rows from the middle */
static void cv_disc(int32_t cx, int32_t cy, int32_t r, uint16_t c)
{
    static const uint8_t HW[7][7] = {{0}, {1, 0}, {2, 1, 0}, {3, 2, 2, 0}, {4, 4, 3, 2, 0}, {5, 5, 4, 4, 3, 0},
                                     {6, 6, 5, 5, 4, 3, 0}};
    int32_t j;
    if (r <= 0) {
        cv_rect(cx, cy, 1, 1, c);
        return;
    }
    if (r > 6)
        r = 6;
    for (j = -r + 1; j < r; j++) {
        int32_t w = HW[r][j < 0 ? -j : j];
        cv_rect(cx - w, cy + j, 2 * w + 1, 1, c);
    }
}

static void text_c(int32_t cx, int32_t y, const felucca_font_t *f, const char *s, uint16_t c)
{
    cv_text(cx - text_w(f, s) / 2, y, f, s, c);
}

/* anti-aliased drawing (X0X's): a pixel blended over the canvas, a disc, and a knob's ring */
static void px_blend(int32_t x, int32_t y, uint16_t c, uint32_t a)
{
    uint16_t *q;
    uint32_t bg;
    y += cv_oy;
    if ((uint32_t)x >= cv_w || y < cv_y0 || y >= cv_y1 || !a)
        return;
    q = &cv_px[(uint32_t)y * cv_w + (uint32_t)x];
    if (a >= 16u) {
        *q = swap16(c);
        return;
    }
    bg = swap16(*q);
    *q = swap16((uint32_t)(((((c >> 11) * a + (bg >> 11) * (16u - a)) >> 4) << 11) |
                           (((((c >> 5) & 63u) * a + ((bg >> 5) & 63u) * (16u - a)) >> 4) << 5) |
                           (((c & 31u) * a + (bg & 31u) * (16u - a)) >> 4)));
}

static void disc(float cx, float cy, float r, uint16_t c)
{
    int32_t x0 = (int32_t)fm_floorf(cx - r - 1.0f), x1 = (int32_t)(cx + r + 1.0f);
    int32_t y0 = (int32_t)fm_floorf(cy - r - 1.0f), y1 = (int32_t)(cy + r + 1.0f), x, y;
    for (y = y0; y <= y1; y++)
        for (x = x0; x <= x1; x++) {
            float dx = (float)x + 0.5f - cx, dy = (float)y + 0.5f - cy, d = fm_sqrtf(dx * dx + dy * dy);
            px_blend(x, y, c, (uint32_t)(fm_clampf(r - d + 0.5f, 0.0f, 1.0f) * 16.0f + 0.5f));
        }
}

/* atan2 in turns, 0..1 from +x going clockwise on screen (y down) */
static float turns(float y, float x)
{
    float ax = fm_fabsf(x), ay = fm_fabsf(y), a = ax < ay ? ax / (ay + 1e-9f) : ay / (ax + 1e-9f), s = a * a, r;
    r = ((-0.0464964749f * s + 0.15931422f) * s - 0.327622764f) * s * a + a;   /* atan on [0, 1] */
    if (ay > ax)
        r = 1.57079637f - r;
    if (x < 0.0f)
        r = FM_PI - r;
    if (y < 0.0f)
        r = FM_TWO_PI - r;
    return r * (1.0f / FM_TWO_PI);
}

/* a knob: a 270-degree track from 7:30 to 4:30, anti-aliased, round ends; the value filled from
 * the start (or from the top, for a centred value) and a round handle */
static void arc(int32_t cx, int32_t cy, int32_t r, float v, uint16_t track, uint16_t fill, uint16_t bg, int bipolar)
{
    const float th = 3.4f, a0 = 0.375f, sweep = 0.75f;
    float ro = (float)r, ri = ro - th, rm = ro - th * 0.5f, fx = (float)cx + 0.5f, fy = (float)cy + 0.5f, lo, hi;
    int32_t x, y;
    v = fm_clampf(v, 0.0f, 1.0f);
    lo = bipolar ? (v < 0.5f ? v : 0.5f) : 0.0f;
    hi = bipolar ? (v < 0.5f ? 0.5f : v) : v;
    for (y = -r - 1; y <= r + 1; y++)
        for (x = -r - 1; x <= r + 1; x++) {
            float dx = (float)x, dy = (float)y, d = fm_sqrtf(dx * dx + dy * dy), t, a;
            a = fm_clampf(ro + 0.5f - d, 0.0f, 1.0f) * fm_clampf(d - ri + 0.5f, 0.0f, 1.0f);
            if (a <= 0.0f)
                continue;
            t = turns(dy, dx) - a0;
            if (t < 0.0f)
                t += 1.0f;
            t /= sweep;
            if (t > 1.0f)
                continue;
            px_blend(cx + x, cy + y, (t >= lo && t <= hi) ? fill : track, (uint32_t)(a * 16.0f + 0.5f));
        }
    {
        float e0 = a0 * FM_TWO_PI, e1 = (a0 + sweep) * FM_TWO_PI, ev = (a0 + sweep * v) * FM_TWO_PI;
        float el = (a0 + sweep * lo) * FM_TWO_PI;
        disc(fx + fm_cosf(e0) * rm - 0.5f, fy + fm_sinf(e0) * rm - 0.5f, th * 0.5f, lo <= 0.0f && hi > 0.0f ? fill : track);
        disc(fx + fm_cosf(e1) * rm - 0.5f, fy + fm_sinf(e1) * rm - 0.5f, th * 0.5f, hi >= 1.0f ? fill : track);
        if (hi > lo)
            disc(fx + fm_cosf(el) * rm - 0.5f, fy + fm_sinf(el) * rm - 0.5f, th * 0.5f, fill);
        disc(fx + fm_cosf(ev) * rm - 0.5f, fy + fm_sinf(ev) * rm - 0.5f, th * 0.5f + 1.3f, bg);
        disc(fx + fm_cosf(ev) * rm - 0.5f, fy + fm_sinf(ev) * rm - 0.5f, th * 0.5f + 0.6f, fill);
    }
}

static void caps(char *d, const char *s, int n)  /* s in capitals, at most n - 1 characters */
{
    int i;
    for (i = 0; i < n - 1 && s[i]; i++)
        d[i] = (char)(s[i] >= 'a' && s[i] <= 'z' ? s[i] - 32 : s[i]);
    d[i] = 0;
}

static uint16_t mix(uint16_t a, uint16_t b, int t)   /* t 0..16: a -> b */
{
    int r, g, bl;
    if (t <= 0)
        return a;
    if (t >= 16)
        return b;
    r = ((a >> 11) * (16 - t) + (b >> 11) * t) / 16;
    g = (((a >> 5) & 63) * (16 - t) + ((b >> 5) & 63) * t) / 16;
    bl = ((a & 31) * (16 - t) + (b & 31) * t) / 16;
    return (uint16_t)(r << 11 | g << 5 | bl);
}
static int q16(float x) { return x <= 0.0f ? 0 : x >= 1.0f ? 16 : (int)(x * 16.0f + 0.5f); }

static void tag(int32_t *x, const char *s, uint16_t bg, uint16_t fg)
{
    int32_t w = text_w(&FONT_XS, s) + 10;
    cv_round(*x, 6, w, 16, 6, bg);
    cv_text(*x + 5, 7, &FONT_XS, s, fg);
    *x += w + 4;
}

/* the battery (dxsloop's thresholds on the divider's raw reading): three bars, 591 up, 561, 531; on USB
 * (powering and charging it: there is no charger line) the bars fill one, two, three in turn */
static int batt_bars(void)
{
    int32_t b = plat_battery();
    return b >= 591 ? 3 : b >= 561 ? 2 : b >= 531 ? 1 : 0;
}
static int batt_pct(void)                        /* a rough charge, 521 .. 601 */
{
    int32_t p = (plat_battery() - 521) * 100 / 80;
    return p < 0 ? 0 : p > 100 ? 100 : (int)p;
}
static void draw_battery(int32_t bx)
{
    int lvl = plat_usb_power() ? 1 + (int)((plat_ms() / 600u) % 3u) : batt_bars(), k;
    uint16_t c = !plat_usb_power() && lvl <= 1 ? K_FROST : K_DIM, o = !plat_usb_power() && !lvl ? K_FROST : K_DIM;
    cv_rect(bx, 9, 15, 1, o);                    /* (empty: the outline red) */
    cv_rect(bx, 17, 15, 1, o);
    cv_rect(bx, 9, 1, 9, o);
    cv_rect(bx + 14, 9, 1, 9, o);
    cv_rect(bx + 15, 11, 2, 5, o);
    for (k = 0; k < lvl; k++)
        cv_rect(bx + 2 + k * 4, 11, 3, 5, c);
}

/* the header: the piece in a tag, its root and mode; on the right, the tempo with a slow beat light
 * (a play triangle while resting) and a dot while there is something unsaved. Messages take its place
 * for a moment. */
static void draw_header(void)
{
    char t[24];
    int msg = plat_ms() < ui.msg_until;
    uint32_t h = hash(hash(hash(2166136261u, proj.piece | (uint32_t)proj.par[P_ROOT] << 8 | (uint32_t)proj.pool << 12),
                           en_playing | ui.dirty << 4 | (uint32_t)msg << 5 | (uint32_t)en_ext << 6 | (uint32_t)proj.par[P_MODE] << 8),
                      (uint32_t)proj.par[P_TEMPO] | (uint32_t)batt_bars() << 29 | (uint32_t)plat_usb_power() << 31 |
                      (plat_usb_power() ? (plat_ms() / 600u) % 3u << 7 : 0u) | (ui.view == V_SETUP ? plat_cpu_pct() << 8 | plat_cpu_peak() << 15 |
                                                                           (plat_xruns() & 0x1FFu) << 22 : 0u));
    if (msg)
        h = hash(hash(h, (uint32_t)ui.msg[0][0] << 8 | ui.msg[0][1]), ui.msg_until);
    if (en_playing)
        h = hash(h, (en_bars / 4u) & 1u);
    if (h == ui.sig[0])
        return;
    ui.sig[0] = h;
    cv_begin(240, HDR_H, K_BG);
    if (msg) {
        int32_t x = cv_text(10, 6, &FONT_B, ui.msg[0], K_TEXT);
        cv_text(x + 8, 6, &FONT_B, ui.msg[1], K_ACC);
    } else {
        int32_t x = 232, w;
        if (plat_battery() >= 0 || plat_usb_power()) {   /* the battery, at the right edge */
            draw_battery(219);
            x = 213;
        }
        if (ui.dirty) {                            /* unsaved */
            cv_round(x - 5, 12, 5, 5, 2, K_DIM);
            x -= 11;
        }
        if (en_ext) {
            t[0] = 'E';
            t[1] = 'X';
            t[2] = 'T';
            t[3] = 0;
        } else {
            itoa_u((uint32_t)proj.par[P_TEMPO], t);
        }
        if (ui.view == V_SETUP) {                 /* Setup: the audio load (now/peak), and any dropouts ("!n") */
            char c[24] = "CPU ";
            uint32_t k;
            itoa_u(plat_cpu_pct(), c + 4);
            for (k = 0; c[k]; k++)
                ;
            c[k++] = '/';
            itoa_u(plat_cpu_peak(), c + k);
            if (plat_xruns()) {
                for (; c[k]; k++)
                    ;
                c[k++] = ' ', c[k++] = '!';
                itoa_u(plat_xruns() > 999u ? 999u : plat_xruns(), c + k);
            }
            x -= text_w(&FONT_XS, c);
            cv_text(x, 8, &FONT_XS, c, K_DIM);
            x -= 10;
        }
        x -= text_w(&FONT_S, t);
        cv_text(x, 6, &FONT_S, t, en_playing ? K_TEXT : K_DIM);
        x -= 16;
        if (en_playing) {                         /* a slow beat light */
            int on = ((en_bars / 4u) & 1u) == 0u;
            cv_round(x, 9, 10, 10, 5, on ? K_PAD : K_PAD_T);
        } else {
            int32_t j;                            /* resting: a play triangle */
            for (j = 0; j < 5; j++)
                cv_rect(x + 2 + j, 9 + j, 1, 11 - 2 * j, K_DIM);
        }
        w = text_w(&FONT_B, piece_name(proj.piece)) + 12;   /* the piece, in a coral tag */
        cv_round(4, 4, w, 20, 6, K_TEXT);
        cv_text(10, 6, &FONT_B, piece_name(proj.piece), K_BG);
        if (ui.view != V_SETUP) {                /* (Setup: the room is the CPU's; the root is its first knob) */
            w = cv_text(w + 10, 8, &FONT_XS, EN_NOTE_NAME[proj.par[P_ROOT]], K_TEXT);
            cv_text(w + 3, 8, &FONT_XS, mode_name(), K_DIM);
        }
    }
    cv_blit(0, 0);
}

/* the main screen: echo ripples. Each stab drops a ring in the middle that spreads and fades; the echo's
 * repeats follow it, a ring each echo time, each as loud as the echo keeps of the one before (a throw: wider
 * and brighter; dub hold: red, and the repeats don't die). The chord's name in the middle. Around it: the
 * section and its eight bars (top left), the way to the next chord (top right), the four layers' levels
 * (left), the kick's ducking (right), and the bar as it plays, a lane a layer (below: stabs, kick, hats,
 * bass), the step playing lit */
#define CX 120
#define CY 48
#define RIP_N 10
#define RIP_R 46                                 /* a ring is gone by this radius */
#define RIP_V 0.04f                              /* px a millisecond: ~1.3 s across */
static struct {
    uint32_t t0;
    float amp;
    uint8_t thr, on;
} rip[RIP_N];
static uint32_t rip_stabs;

static const char *chord_now(char *nm)          /* the chord's name, as the screen shows it ("Em7") */
{
    int pcs[EN_NPAD + 1], k;
    if (en_chord_slot >= 0 && proj.par[P_CQ0 + en_chord_slot])   /* a slot's: its own name */
        return slot_label(en_chord_slot, nm);
    pcs[0] = en_chord_root;
    for (k = 0; k < EN_NPAD; k++)
        pcs[k + 1] = en_chord[k] % 12;
    return en_chord_name(pcs, EN_NPAD + 1, nm);
}

static void ripples_take(uint32_t now)          /* a new stab: a new ripple (into the oldest slot if all are busy) */
{
    uint32_t n = en_stabs;
    int i, k = 0;
    if (n == rip_stabs)
        return;
    rip_stabs = n;
    if (en_frozen || en_echo_in < 0.01f)        /* nothing new goes into a held echo */
        return;
    for (i = 0; i < RIP_N; i++) {
        if (!rip[i].on) {
            k = i;
            break;
        }
        if (rip[i].t0 < rip[k].t0)
            k = i;
    }
    rip[k].t0 = now;
    rip[k].thr = en_stab_thrown;
    rip[k].amp = fm_minf(en_stab_vel * (0.45f + 0.55f * en_echo_in) * (en_stab_thrown ? 1.5f : 1.0f), 1.0f);
    rip[k].on = 1;
}

static void circle(int32_t cx, int32_t cy, int32_t r, uint16_t c)   /* an outline, one pixel (midpoint) */
{
    int32_t x = r, y = 0, e = 1 - r;
    while (x >= y) {
        cv_pset(cx + x, cy + y, c), cv_pset(cx - x, cy + y, c), cv_pset(cx + x, cy - y, c), cv_pset(cx - x, cy - y, c);
        cv_pset(cx + y, cy + x, c), cv_pset(cx - y, cy + x, c), cv_pset(cx + y, cy - x, c), cv_pset(cx - y, cy - x, c);
        y++;
        if (e < 0) {
            e += 2 * y + 1;
        } else {
            x--;
            e += 2 * (y - x) + 1;
        }
    }
}

static int draw_ripples(uint32_t now)           /* 1: something still moves */
{
    const float ms = en_echo_ms > 20.0f ? en_echo_ms : 400.0f, g = en_echo_g, life = (float)RIP_R / RIP_V;
    int i, live = 0;
    for (i = 0; i < RIP_N; i++) {
        float age = (float)(now - rip[i].t0), a0;
        int k, k0, seen = 0;
        if (!rip[i].on)
            continue;
        k0 = age > life ? (int)((age - life) / ms) + 1 : 0;   /* the repeats already gone */
        a0 = rip[i].amp;
        for (k = 0; k < k0 && a0 > 0.0f; k++)
            a0 *= g;
        for (k = k0; k < k0 + 8; k++, a0 *= g) {
            float t = age - (float)k * ms, r, a;
            uint16_t c;
            if (t < 0.0f)
                break;
            r = 5.0f + t * RIP_V;
            a = a0 * (1.0f - r / (float)RIP_R);
            if (r >= (float)RIP_R || a < 0.05f)
                continue;
            seen = 1;
            c = mix(K_BG, en_frozen ? K_FROST : rip[i].thr ? K_ACC : K_PAD, q16(a * 2.0f));
            circle(CX, CY, (int32_t)r, c);       /* two pixels wide (a throw: three) */
            circle(CX, CY, (int32_t)r + 1, c);
            if (rip[i].thr)
                circle(CX, CY, (int32_t)r + 2, c);
        }
        if (!seen && (en_frozen ? 0 : a0 < 0.05f || g <= 0.0f) && age > 200.0f)
            rip[i].on = 0;                       /* its last repeat has faded */
        else
            live = 1;
    }
    return live;
}

static void line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t c);

static void draw_orbits(void)                    /* the ping loops, around the ripples: a dot each, going round */
{
    int i;
    for (i = 0; i < EN_NLOOP; i++) {             /* (a loop a step a turn's step; the loops of unequal length) */
        float a = 6.2831853f * (float)en_loop_pos[i] / (float)EN_LOOP_LEN[i] - 1.5707963f + (float)i * 1.9f,
              rr = 24.0f + 6.0f * (float)i, g = en_loop_glow[i];
        int32_t x = CX + (int32_t)(rr * fm_cosf(a) + 0.5f), y = CY + (int32_t)(rr * fm_sinf(a) + 0.5f);
        if (!en_playing)
            g = 0.0f;
        cv_disc(x, y, g > 0.5f ? 3 : 2, mix(mix(K_BELL_T, K_BELL, 5), K_BELL, q16(g * 1.3f)));
    }
}

/* the middle as a scope: the output (mono), from a rising zero crossing so it stands, a thick line with a
 * glow over the two traces before it, fading; its height following the level shown (the gain rises for a
 * quiet moment) */
#define SC_X 56
#define SC_W 128
#define SC_Y 58
#define SC_H 26
static void draw_scope(void)
{
    static int16_t s[EN_SCOPE_N];
    static int8_t was[2][SC_W];                  /* the last two traces, fading (a phosphor's) */
    static float gain = 4.0f;
    const uint16_t c = en_frozen ? K_FROST : K_PAD, gl = mix(K_BG, c, 5), ph[2] = {mix(K_BG, c, 6), mix(K_BG, c, 3)};
    uint32_t w = en_scope_w;
    int i, t0 = 0, m = 1;
    int32_t x;
    for (i = 0; i < EN_SCOPE_N; i++)             /* (the oldest first) */
        s[i] = en_scope[(w + (uint32_t)i) & (EN_SCOPE_N - 1u)];
    for (i = 2; i < EN_SCOPE_N - SC_W; i++)
        if (s[i - 1] < 0 && s[i] >= 0 && s[i + 1] - s[i - 2] > 200) {
            t0 = i;
            break;
        }
    for (x = 0; x < SC_W; x++)
        m = s[t0 + x] > m ? s[t0 + x] : -s[t0 + x] > m ? -s[t0 + x] : m;
    gain += (fm_minf(0.85f * 32000.0f / (float)(m > 600 ? m : 600), 30.0f) - gain) * 0.25f;
    for (x = 0; x <= SC_W; x += 4)               /* the graticule: the middle dotted, ticks along the edges */
        cv_rect(SC_X + x, SC_Y, 1, 1, K_LINE);
    for (x = 0; x <= SC_W; x += SC_W / 8) {
        cv_rect(SC_X + x, SC_Y - SC_H - 2, 1, 3, K_LINE);
        cv_rect(SC_X + x, SC_Y + SC_H, 1, 3, K_LINE);
    }
    for (i = 1; i >= 0; i--)                     /* the traces before, older dimmer */
        for (x = 1; x < SC_W; x++)
            line(SC_X + x - 1, SC_Y - was[i][x - 1], SC_X + x, SC_Y - was[i][x], ph[i]);
    for (x = 0; x < SC_W; x++) {
        float v = (float)s[t0 + x] * (1.0f / 32000.0f) * gain;
        was[1][x] = was[0][x];
        was[0][x] = (int8_t)(fm_clampf(v, -1.0f, 1.0f) * (float)SC_H + (v < 0.0f ? -0.5f : 0.5f));
    }
    for (x = 1; x < SC_W; x++) {                 /* the trace: a glow, then two pixels bright */
        int32_t y0 = SC_Y - was[0][x - 1], y1 = SC_Y - was[0][x];
        line(SC_X + x - 1, y0 - 1, SC_X + x, y1 - 1, gl);
        line(SC_X + x - 1, y0 + 2, SC_X + x, y1 + 2, gl);
    }
    for (x = 1; x < SC_W; x++) {
        int32_t y0 = SC_Y - was[0][x - 1], y1 = SC_Y - was[0][x];
        line(SC_X + x - 1, y0, SC_X + x, y1, c);
        line(SC_X + x - 1, y0 + 1, SC_X + x, y1 + 1, c);
    }
}

/* the middle as a spectrum (a Winamp's): the output's last 512 samples at 22 kHz, windowed and transformed,
 * in 18 bands a third of an octave or more wide, 43 Hz to 11 kHz; each a column of segments, teal low, brass,
 * amber, red at the top, rising at once and falling slowly, a cap held at its peak a moment */
#define SP_NB 18
#define SP_X 58
#define SP_BOT 86
#define SP_SEG 18                                /* segments a column, 3 px each */
static void fft512(float *re, float *im)         /* in place, radix 2 */
{
    int n = EN_SPEC_N, i, j = 0, len, k;
    for (i = 1; i < n; i++) {                    /* the bit-reversed order */
        int b = n >> 1;
        for (; j & b; b >>= 1)
            j ^= b;
        j ^= b;
        if (i < j) {
            float t = re[i];
            re[i] = re[j], re[j] = t;
            t = im[i];
            im[i] = im[j], im[j] = t;
        }
    }
    for (len = 2; len <= n; len <<= 1) {
        float a = -6.2831853f / (float)len, wr = fm_cosf(a), wi = fm_sinf(a);
        for (i = 0; i < n; i += len) {
            float cr = 1.0f, ci = 0.0f;
            for (k = 0; k < len / 2; k++) {
                int p = i + k, q = p + len / 2;
                float tr = re[q] * cr - im[q] * ci, ti = re[q] * ci + im[q] * cr, nr;
                re[q] = re[p] - tr, im[q] = im[p] - ti;
                re[p] += tr, im[p] += ti;
                nr = cr * wr - ci * wi;
                ci = cr * wi + ci * wr;
                cr = nr;
            }
        }
    }
}

static void draw_spec(void)
{
    static const uint16_t EDGE[SP_NB + 1] = {1, 2, 3, 4, 5, 6, 7, 9, 12, 16, 22, 30, 40, 55, 75, 102, 138, 188, 256};
    static float re[EN_SPEC_N], im[EN_SPEC_N], lv[SP_NB], cap[SP_NB];
    static uint8_t hold[SP_NB];
    uint32_t w = en_spec_w;
    int i, b, g;
    for (i = 0; i < EN_SPEC_N; i++) {            /* (the oldest first, a Hann window) */
        float h = 0.5f - 0.5f * fm_cosf(6.2831853f * (float)i * (1.0f / (float)(EN_SPEC_N - 1)));
        re[i] = (float)en_spec[(w + (uint32_t)i) & (EN_SPEC_N - 1u)] * (1.0f / 32000.0f) * h;
        im[i] = 0.0f;
    }
    fft512(re, im);
    for (b = 0; b < SP_NB; b++) {
        float pw = 1e-9f, db, f;
        int32_t x = SP_X + b * 7, n;
        for (i = EDGE[b]; i < EDGE[b + 1]; i++)
            pw += re[i] * re[i] + im[i] * im[i];
        db = 3.0103f * fm_log2f(pw) - 42.1f + 1.5f * (float)b;   /* dB of full scale, tilted up 1.5 dB a band */
        f = fm_clampf((db + 56.0f) * (1.0f / 48.0f), 0.0f, 1.0f);
        if (!en_playing)
            f = 0.0f;
        lv[b] = f > lv[b] ? f : lv[b] - 0.04f;   /* up at once, down slowly */
        if (lv[b] >= cap[b]) {
            cap[b] = lv[b];
            hold[b] = 12;                        /* (frames: ~0.6 s) */
        } else if (hold[b]) {
            hold[b]--;
        } else {
            cap[b] -= 0.025f;
        }
        n = (int32_t)(lv[b] * (float)SP_SEG + 0.5f);
        for (g = 0; g < SP_SEG; g++) {
            uint16_t c = g >= SP_SEG - 3 ? K_FROST : g >= SP_SEG * 2 / 3 ? K_PULSE : g >= SP_SEG / 3 ? K_BASS : K_PAD;
            if (en_frozen)
                c = K_FROST;
            cv_rect(x, SP_BOT - 3 * g - 2, 6, 2, g < n ? c : g == 0 ? K_LINE : K_BG);
        }
        if (cap[b] > 0.03f)
            cv_rect(x, SP_BOT - 3 * (int32_t)(cap[b] * (float)SP_SEG + 0.5f) - 1, 6, 1, K_TEXT);
    }
}

/* a sound page: its layer's envelope (its level; the chord's filter envelope, the pings' bend, the kick's
 * pitch, dimmer), over the time it lasts */
static void draw_env(int layer_view)
{
    float lv[48], ax[48], secs;
    int i, layer = layer_view == V_CHORD ? 0 : layer_view == V_TEXTURE ? 1 : layer_view == V_BASS ? 2 : 3;
    int32_t x0 = 58, w = 124, y0 = 8, h = 76;
    char t[12];
    en_env_shape(layer, lv, ax, 48, &secs);
    cv_rect(x0, y0 + h, w + 4, 1, K_LINE);
    cv_rect(x0, y0, 1, h, K_LINE);
    for (i = 0; i < 48; i++) {
        int32_t x = x0 + 2 + i * w / 47, ya = y0 + h - (int32_t)(ax[i] * (float)h), yl = y0 + h - (int32_t)(lv[i] * (float)h);
        if (ax[i] > 0.01f)
            cv_rect(x, ya, 2, 2, K_LINE);
        cv_rect(x, yl, 2, y0 + h - yl, mix(K_BG, K_PAD, 5));
        cv_rect(x, yl, 2, 2, K_PAD);
    }
    itoa_u((uint32_t)(secs * 1000.0f + 0.5f), t);
    {
        uint32_t k = 0;
        while (t[k])
            k++;
        t[k++] = ' ', t[k++] = 'm', t[k++] = 's', t[k] = 0;
    }
    cv_text(x0 + w - text_w(&FONT_XS, t), y0 + h + 1, &FONT_XS, t, K_DIM);
}

/* an FM page: the voice's algorithm, as the FM synths draw it: the operators as boxes, each modulator above
 * the one it modulates, the carriers on the bottom line (they're heard), op 4's feedback a loop */
static const int8_t ALG_XY[EN_FM_NALG][4][2] = {   /* each op's place (x in half steps from the middle, y up) */
    {{0, 0}, {0, 1}, {0, 2}, {0, 3}},   {{0, 0}, {0, 1}, {-1, 2}, {1, 2}}, {{0, 0}, {-1, 1}, {-1, 2}, {1, 1}},
    {{0, 0}, {-1, 1}, {1, 1}, {1, 2}},  {{-1, 0}, {-1, 1}, {1, 0}, {1, 1}}, {{-2, 0}, {0, 0}, {2, 0}, {0, 1}},
    {{-2, 0}, {0, 0}, {2, 0}, {2, 1}},  {{-3, 0}, {-1, 0}, {1, 0}, {3, 0}}, {{0, 0}, {0, 1}, {0, -9}, {0, -9}},
};
static const uint16_t ALG_EDGE[EN_FM_NALG] = {   /* bit 4 * from + to (ops 0..3): from modulates to */
    1u << (4 * 1 + 0) | 1u << (4 * 2 + 1) | 1u << (4 * 3 + 2), 1u << (4 * 1 + 0) | 1u << (4 * 2 + 1) | 1u << (4 * 3 + 1),
    1u << (4 * 1 + 0) | 1u << (4 * 2 + 1) | 1u << (4 * 3 + 0), 1u << (4 * 1 + 0) | 1u << (4 * 2 + 0) | 1u << (4 * 3 + 2),
    1u << (4 * 1 + 0) | 1u << (4 * 3 + 2),                     1u << (4 * 3 + 0) | 1u << (4 * 3 + 1) | 1u << (4 * 3 + 2),
    1u << (4 * 3 + 2),                                         0u,
    1u << (4 * 1 + 0),
};
static void line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t c)   /* Bresenham */
{
    int32_t dx = x1 > x0 ? x1 - x0 : x0 - x1, dy = y1 > y0 ? y0 - y1 : y1 - y0, sx = x0 < x1 ? 1 : -1,
            sy = y0 < y1 ? 1 : -1, e = dx + dy;
    for (;;) {
        cv_pset(x0, y0, c);
        if (x0 == x1 && y0 == y1)
            break;
        if (2 * e >= dy) {
            e += dy;
            x0 += sx;
        }
        if (2 * e <= dx) {
            e += dx;
            y0 += sy;
        }
    }
}
static void draw_alg(int layer)
{
    static const char *const D[4] = {"1", "2", "3", "4"};
    int alg = en_fm_alg(layer), k, f, t;
    int32_t px[4], py[4];
    char tt[12];
    if (!en_voice_fm(layer)) {                   /* the FM voices, to choose with ALGORITHM */
        char vn[12], t[24];
        en_param_text(view_voice(ui.view), proj.par[view_voice(ui.view)], vn);
        caps(t, vn, 12);
        text_c(120, 28, &FONT_B, t, K_TEXT);
        text_c(120, 48, &FONT_XS, "IS NOT AN FM VOICE", K_DIM);
        text_c(120, 66, &FONT_XS, "ALGORITHM, FOR ONE:", K_DIM);
        text_c(120, 80, &FONT_XS, layer == 0 ? "KEYS, BELL" : "PUNCH, RUBBER, PLUCK, GROWL", K_ACC);
        return;
    }
    for (k = 0; k < 4; k++) {
        px[k] = 120 + ALG_XY[alg][k][0] * 17;
        py[k] = 80 - ALG_XY[alg][k][1] * 18;
    }
    for (f = 0; f < 4; f++)                      /* the connections, under the boxes */
        for (t = 0; t < 4; t++)
            if (ALG_EDGE[alg] >> (4 * f + t) & 1u)
                line(px[f], py[f] + 7, px[t], py[t] - 7, K_DIM);
    for (k = 0; k < 4; k++) {
        int carrier = ALG_XY[alg][k][1] == 0;
        if (ALG_XY[alg][k][1] < 0)
            continue;                            /* (two operators: 3 and 4 unused) */
        if (carrier)
            line(px[k], py[k] + 7, px[k], 91, K_PAD);
        cv_round(px[k] - 7, py[k] - 7, 15, 15, 3, carrier ? K_PAD : K_BELL);
        text_c(px[k] + 1, py[k] - 7, &FONT_XS, D[k], K_BG);
    }
    if (alg != 8) {                              /* op 4's feedback */
        line(px[3] + 7, py[3] - 3, px[3] + 11, py[3] - 3, K_DIM);
        line(px[3] + 11, py[3] - 3, px[3] + 11, py[3] - 10, K_DIM);
        line(px[3] + 11, py[3] - 10, px[3], py[3] - 10, K_DIM);
        line(px[3], py[3] - 10, px[3], py[3] - 7, K_DIM);
    }
    cv_rect(62, 91, 117, 1, K_PAD);
    tt[0] = 'A', tt[1] = 'L', tt[2] = 'G', tt[3] = 'O', tt[4] = ' ';
    itoa_u((uint32_t)(alg + 1), tt + 5);
    if (alg == 8) {
        tt[5] = '2', tt[6] = '-', tt[7] = 'O', tt[8] = 'P', tt[9] = 0;
    }
    text_c(120, -1, &FONT_XS, tt, K_DIM);
}

static void draw_meters(void)                    /* left: the four layers */
{
    static const uint16_t C[4] = {K_PAD, K_BELL, K_BASS, K_PULSE}, T[4] = {K_PAD_T, K_BELL_T, K_BASS_T, K_PULSE_T};
    float lv[4], bm = 0.0f, dm = 0.0f;
    int i;
    for (i = 0; i < EN_NLOOP; i++)
        bm = fm_maxf(bm, en_loop_glow[i]);
    for (i = 0; i < EN_NDRUM; i++)
        dm = fm_maxf(dm, en_drum_glow[i]);
    lv[0] = en_pad_level * (float)proj.par[P_CHORD] * 0.01f;
    lv[1] = bm * (float)proj.par[P_PINGS] * 0.01f;
    lv[2] = en_bass_level * (float)proj.par[P_BASS] * 0.01f;
    lv[3] = dm * (float)proj.par[P_DRUMS] * 0.01f;
    for (i = 0; i < 4; i++) {
        int32_t x = 8 + i * 11, h = (int32_t)(fm_clampf(lv[i], 0.0f, 1.0f) * 52.0f + 0.5f);
        cv_round(x, 32, 8, 56, 3, T[i]);
        if (h > 0)
            cv_round(x, 32 + 56 - h - 2, 8, h + 2, 3, C[i]);
    }
}

static void draw_duck(void)                      /* right: the kick's ducking, falling from the top */
{
    int32_t h = (int32_t)(fm_clampf(en_duck * (float)proj.par[P_DUCK] * 0.01f, 0.0f, 1.0f) * 52.0f + 0.5f);
    cv_round(220, 32, 8, 56, 3, K_PULSE_T);
    if (h > 0)
        cv_round(220, 32, 8, h + 2, 3, K_PULSE);
    cv_text(216 - text_w(&FONT_XS, "DUCK"), 76, &FONT_XS, "DUCK", K_DIM);
}

static void draw_lanes(void)                     /* below: the bar, a lane a layer */
{
    static const uint16_t C[6] = {K_PAD, K_PULSE, K_TEXT, K_RIM, K_CLAP, K_BASS},
                          T[6] = {K_PAD_T, K_PULSE_T, K_LINE, K_RIM_T, K_CLAP_T, K_BASS_T};
    static const int8_t DR[6] = {-1, 0, 1, 3, 4, -1};   /* each lane's drum (its chance) */
    int l, i, k, pos = en_drum_pos[0];
    float ghost[16];
    if (en_playing)                              /* the step playing: a column */
        cv_round(38 + pos * 11, 94, 10, 50, 2, K_PANEL);
    for (i = 0; i < 16; i++)
        ghost[i] = 0.0f;
    if (!en_ext && proj.par[P_TEMPO] > 0) {      /* the stabs' echoes: where each repeat lands, as faint as it is */
        float e = en_echo_ms * (float)proj.par[P_TEMPO] * (1.0f / 15000.0f), g = en_echo_in;   /* (in sixteenths) */
        for (i = 0; i < 16; i++)
            if (en_lane[0] >> i & 1u) {
                float a = g;
                for (k = 1; k <= 6; k++) {
                    int q = (int)((float)i + (float)k * e + 0.5f) % 16;
                    a *= en_echo_g;
                    if (a < 0.06f)
                        break;
                    if (a > ghost[q])
                        ghost[q] = a;
                }
            }
    }
    for (l = 0; l < 6; l++) {                    /* (in the layers' colours: stabs, kick, hats, rim, clap, bass) */
        uint32_t p = en_lane[l];
        int32_t y = 96 + l * 8;
        int maybe = DR[l] >= 0 && (proj.par[P_DMODE] ? proj.par[P_DCH0 + DR[l]] < 50 : DR[l] == 4);
        cv_round(29, y + 1, 5, 4, 1, C[l]);
        for (i = 0; i < 16; i++) {
            int32_t x = 40 + i * 11;
            if (p >> i & 1u && maybe) {          /* a hit that only sometimes comes: a ring; filled when it does */
                uint16_t c = en_playing && i == pos && en_drum_glow[DR[l]] > 0.5f ? K_TEXT : mix(T[l], C[l], 10);
                if (en_playing && i == pos && en_drum_glow[DR[l]] > 0.5f) {
                    cv_round(x - 1, y - 1, 8, 8, 3, c);
                } else {
                    cv_rect(x + 1, y, 4, 1, c), cv_rect(x + 1, y + 5, 4, 1, c);
                    cv_rect(x, y + 1, 1, 4, c), cv_rect(x + 5, y + 1, 1, 4, c);
                }
            } else if (p >> i & 1u) {
                cv_round(x, y, 6, 6, 2, en_playing && i == pos ? C[l] : mix(T[l], C[l], 10));
            } else if (l == 0 && ghost[i] > 0.0f) {   /* an echo's repeat: a ring; as the playhead hits it, a flash */
                if (en_playing && i == pos) {
                    cv_round(x - 1, y - 1, 8, 8, 3, K_TEXT);
                } else {
                    uint16_t c = mix(K_BG, K_PAD, 9 + q16(ghost[i]) * 7 / 16);
                    cv_rect(x + 1, y, 4, 1, c), cv_rect(x + 1, y + 5, 4, 1, c);
                    cv_rect(x, y + 1, 1, 4, c), cv_rect(x + 5, y + 1, 1, 4, c);
                }
            } else if (i & 3) {
                cv_rect(x + 2, y + 2, 2, 2, K_LINE);
            } else {
                cv_rect(x + 2, y + 2, 2, 2, K_DIM);
            }
        }
    }
}

/* a button's card (help_tick): its name, then what it does, in the garden's place. A line starting with a
 * space goes on from the one before (a screen reader joins them: sp_line) */
#define HELP_LINES 4
static const char *const BTN_LABEL[NB] = {"FX", "SEL", "ENV", "LFO", "EDIT", "GLO", "HOME", "SAVE", "ARP", "SEQ",
                                          "PLAY", "REC", "OCT-", "OCT+"};
static int help_lines(int b, const char *l[HELP_LINES])
{
    int n = 0;
#define L(t) (l[n++] = (t))
    switch (b) {
    case B_HOME: L("DUB"); L("TEMPO, EVOLVE, MOTION, DRIFT"); L("+ SELECT: TEMPO"); L("AGAIN: RIPPLES, SCOPE, SPECTRUM"); break;
    case B_ENV: L("THE CHORD: LEVEL, STABS,"); L(" DECAY, CUTOFF"); L("ALGORITHM: ITS VOICE"); L("AGAIN: SOUND; AGAIN: FM"); break;
    case B_ARP: L("TEXTURE: PINGS, DENSITY,"); L(" CLOUD, CRACKLE"); L("ALGORITHM: THE PINGS' VOICE"); L("AGAIN: SOUND; AGAIN: FM"); break;
    case B_LFO: L("THE BASS"); L("LEVEL, OCTAVE, GROOVE, FIFTH"); L("ALGORITHM: ITS VOICE"); L("AGAIN: SOUND; AGAIN: FM"); break;
    case B_SEQ: L("THE DRUMS"); L("LEVEL, HITS, SWING, DUCK"); L("ALGORITHM: THE KIT"); L("AGAIN: SOUND; AGAIN: PATTERN"); break;
    case B_FX: L("THE SPACE"); L("ECHO, ECHO TIME, FEEDBACK,"); L("REVERB"); break;
    case B_GLO: L("SETUP"); L("ROOT, TUNE, MIDI OUT, LIGHTS"); break;
    case B_EDIT: L("TAP: THE CHORDS (8 SLOTS:"); L(" THE KEYS, THE SLOT'S ROOT)"); L("HOLD: THE NEXT CHORD"); break;
#if BOARD_SMK37
    case B_SEL: L("PERFORM: KNOBS MIX; PADS 1-5"); L(" MUTE CHORD PINGS BASS KICK HATS;"); L("6-10 HELD: THROW OPEN SHUT"); L(" KICK DROP ECHO"); break;
    case B_REC: L("TAP: DUB HOLD (THE ECHO"); L(" REPEATS WHAT IT HOLDS)"); L("HOLD: THE EFFECTS GRID,"); L(" PLAYED ON THE PADS"); break;
#else
    case B_SEL: L("PERFORM: KNOBS MIX; KEYS 1-5"); L(" MUTE CHORD PINGS BASS KICK HATS;"); L("6-10 HELD: THROW OPEN SHUT"); L(" KICK DROP ECHO"); break;
    case B_REC: L("TAP: DUB HOLD (THE ECHO"); L(" REPEATS WHAT IT HOLDS)"); L("HOLD: THE EFFECTS GRID,"); L(" PLAYED ON THE WHITE KEYS"); break;
#endif
    case B_OCTDN: L("THE PINGS AN OCTAVE DOWN"); L("WITH OCT+, 5 S: UPDATE MODE"); break;
    case B_OCTUP: L("THE PINGS AN OCTAVE UP"); L("WITH OCT-, 5 S: UPDATE MODE"); break;
    case B_SAVE: L("SAVES ALL EIGHT PIECES"); L("KEEP HOLDING: THE PIECE"); L(" BACK TO ITS RECIPE"); break;
    default: break;
    }
#undef L
    return n;
}

static void draw_help(void)
{
    const char *l[HELP_LINES];
    int n = help_lines(ui.help - 1, l), k;
    cv_round(16, 14, 208, 116, 6, K_PANEL);
    cv_round(16, 14, 208, 26, 6, K_TEXT);
    cv_rect(16, 34, 208, 6, K_TEXT);
    text_c(120, 18, &FONT_B, BTN_LABEL[ui.help - 1], K_BG);
    for (k = 0; k < n; k++)
        text_c(120, 56 + k * 20, &FONT_S, l[k] + (l[k][0] == ' '), K_TEXT);
}

/* the effects grid, in the main screen's place while REC is held: four rows of four, the first of each lit
 * as the group's (its key lit too), the ones held filled */
static void grid_band(int32_t oy)                 /* the whole screen, in two bands of 240 x 120 */
{
    static const char *const GROUP[4] = {"ECHO", "FILTER", "SPACE", "RHYTHM"};
    static const char *const CELL[EN_NFX] = {"Throw", "Time x2", "Stop", "Swell", "Low", "High", "Wah", "Phone",
                                             "Verb", "Crash", "Freeze", "Wash", "Stutter", "Gate", "Siren", "Riser"};
    int r, c;
    cv_begin(240, 120, K_BG);
    text_c(120, 2 - oy, &FONT_S, "EFFECTS", K_ACC);
#if BOARD_SMK37
    text_c(120, 20 - oy, &FONT_XS, "HOLD A PAD    TAP REC: DUB HOLD", K_DIM);
#else
    text_c(120, 20 - oy, &FONT_XS, "HOLD A WHITE KEY    TAP REC: DUB HOLD", K_DIM);
#endif
    for (r = 0; r < 4; r++) {
        int32_t yr = 38 + r * 50;
        cv_text(6, yr - oy, &FONT_XS, GROUP[r], K_DIM);
        for (c = 0; c < 4; c++) {
            int e = r * 4 + c, on = (int)(ui.fx >> e & 1u);
            int32_t x = 4 + c * 59, y = yr + 13 - oy;
            char t[12];
            cv_round(x, y, 55, 33, 5, on ? K_ACC : c == 0 ? K_PANEL : mix(K_BG, K_PANEL, 8));
            if (c == 0 && !on)                   /* (the first of a group: its key is lit) */
                cv_rect(x + 4, y + 29, 47, 2, K_ACC);
            caps(t, CELL[e], sizeof t);
            text_c(x + 28, y + 8, &FONT_S, t, on ? K_BG : K_TEXT);
        }
    }
    cv_blit(0, (uint32_t)oy);
}

static void draw_grid(void)                      /* REC held: the effects, the whole screen */
{
    uint32_t h = hash(0x47524944u, ui.fx);
    if (h == ui.sig[1] && ui.sig[0] == 0x47524944u)
        return;
    ui.sig[0] = ui.sig[2] = 0x47524944u;         /* (the header and the knobs drawn again after) */
    ui.sig[1] = h;
    grid_band(0);
    grid_band(120);
}

/* the Pattern page: the four lanes, sixteen steps each, the lane edited lit, the accents taller, the step
 * playing a column */
static void draw_pattern(void)
{
    static const uint16_t C[EN_NDRUM] = {K_PULSE, K_TEXT, K_DIM, K_RIM, K_CLAP};
    static const char *const SH[EN_NDRUM] = {"KICK", "SHKR", "OPEN", "RIM", "CLAP"};
    int l, i, pos = en_drum_pos[0];
    text_c(120, -1, &FONT_XS, proj.par[P_DMODE] ? "PLAY: THE PATTERN" : "PLAY: AUTO (A KEY TAKES IT OVER)",
           proj.par[P_DMODE] ? K_ACC : K_DIM);
    if (en_playing)
        cv_round(37 + pos * 12, 15, 11, 86, 2, K_PANEL);   /* (five lanes, 17 px apart) */
    for (l = 0; l < EN_NDRUM; l++) {
        uint32_t p = lane_steps(l), a = proj.par[P_DMODE] ? (uint32_t)(uint16_t)proj.par[P_DACC0 + l] : 0u;
        int32_t y = 17 + l * 17, sel = l == ui.lane;
        int maybe = proj.par[P_DMODE] ? proj.par[P_DCH0 + l] < 50 : l == 4;   /* (a chance under half: rings) */
        if (sel)
            cv_round(2, y - 2, 236, 17, 4, mix(K_BG, K_PANEL, 10));
        cv_text(5, y + 1, &FONT_XS, SH[l], sel ? K_TEXT : K_DIM);
        for (i = 0; i < 16; i++) {
            int32_t x = 38 + i * 12;
            if (p >> i & 1u) {
                int ac = (int)(a >> i & 1u);
                uint16_t c = en_playing && i == pos ? K_TEXT : sel ? C[l] : mix(K_BG, C[l], 9);
                int32_t yy = ac ? y : y + 3, hh = ac ? 13 : 7;
                if (maybe) {                     /* a step that only sometimes plays: a ring */
                    cv_rect(x + 1, yy, 7, 1, c), cv_rect(x + 1, yy + hh - 1, 7, 1, c);
                    cv_rect(x, yy + 1, 1, hh - 2, c), cv_rect(x + 8, yy + 1, 1, hh - 2, c);
                } else {
                    cv_round(x, yy, 9, hh, 2, c);
                }
            } else {
                cv_rect(x + 4, y + 6, 2, 2, (i & 3) ? K_LINE : K_DIM);
            }
        }
    }
#if BOARD_SMK37
    text_c(120, 104, &FONT_XS, "PADS: STEPS   BLACK KEY + PAD: ACCENT", K_DIM);
#else
    text_c(120, 104, &FONT_XS, "WHITE: STEPS   BLACK + WHITE: ACCENT", K_DIM);
#endif
}

/* the Chords page: the eight slots, the one edited outlined, the one playing lit; an empty slot inside the
 * progression shows that the chord before it goes on */
static void draw_slots(void)
{
    int i, n = prog_count(), play = proj.par[P_PMODE] && n;
    char nm[16];
    text_c(120, -1, &FONT_XS, play ? "PLAY: THE SLOTS IN TURN" : "PLAY: RANDOM (THE SLOTS WAIT)", play ? K_ACC : K_DIM);
    for (i = 0; i < EN_NPROG; i++) {
        int32_t x = 12 + (i & 3) * 55, y = 18 + (i >> 2) * 42;
        int filled = proj.par[P_CQ0 + i] != 0, now = play && i == en_prog_pos, sel = i == ui.slot;
        if (sel)
            cv_round(x - 2, y - 2, 54, 38, 5, K_ACC);
        cv_round(x, y, 50, 34, 4, now ? K_PAD : filled ? K_PANEL : mix(K_BG, K_PANEL, 6));
        {
            char d[2];
            d[0] = (char)('1' + i), d[1] = 0;
            cv_text(x + 3, y + 1, &FONT_XS, d, now ? K_BG : K_DIM);
        }
        if (filled) {
            const char *t = slot_label(i, nm);
            text_c(x + 25, y + 11, text_w(&FONT_B, t) <= 46 ? &FONT_B : &FONT_XS, t, now ? K_BG : K_TEXT);
        } else if (i < n) {
            text_c(x + 25, y + 13, &FONT_XS, "(HOLDS)", now ? K_BG : K_DIM);
        }
    }
    text_c(120, 104, &FONT_XS, "KEYS: THE SLOT'S ROOT   EDIT: BACK", K_DIM);
}

#if BOARD_SMK37
/* SMK-37: the cards in the main screen's place. The fader card: the bank's four values as four faders, in the
 * knob strip's columns (the faders sit in a row too), the banks above. The picker: the pieces over their row of
 * pads, the modes over theirs */
static void smk_draw_fcard(void)
{
    static const uint16_t LAYER[4] = {K_PAD, K_BELL, K_BASS, K_PULSE};
    int i, now_touch = smk.ftouch >= 0;
    int32_t x = 10;
    for (i = 0; i < SMK_NBANK; i++)              /* the banks: the one the faders set, bright */
        tag(&x, SMK_BANK_NAME[i], i == smk.fbank ? K_TEXT : K_PANEL, i == smk.fbank ? K_BG : K_DIM);
    for (i = 0; i < 4; i++) {
        int k = SMK_FADER_PAR[smk.fbank][i], lo, hi, v, hot = now_touch && smk.ftouch == i;
        int32_t cx = i * 60 + 30, h;
        uint16_t fill = smk.fbank == 0 ? LAYER[i] : hot ? K_ACC : K_PAD;
        char t[16];
        if (i)
            cv_rect(i * 60, 30, 1, 108, K_LINE);
        if (hot)
            cv_round(i * 60 + 3, 27, 54, 114, 6, K_PANEL);
        knob_range(k, &lo, &hi);
        v = knob_get(k);
        caps(t, SMK_FADER_LABEL[smk.fbank][i], sizeof t);
        text_c(cx, 30, &FONT_XS, t, hot ? K_TEXT : K_DIM);
        h = hi > lo ? (int32_t)((v - lo) * 70 / (hi - lo)) : 0;
        cv_round(cx - 4, 48, 8, 74, 3, K_LINE);
        if (h > 0)
            cv_round(cx - 4, 48 + 70 - h, 8, h + 4, 3, fill);
        cv_round(cx - 9, 48 + 70 - h - 1, 18, 6, 2, hot ? K_ACC : K_TEXT);   /* the fader's cap */
        knob_text(k, t);
        text_c(cx, 124, &FONT_XS, t, hot ? K_ACC : K_TEXT);
    }
}

static const char *const SMK_MODE_SHORT[EN_NMODES] = {"ION", "DOR", "PHR", "LYD", "MIX", "AEO", "P+", "P-"};
static int smk_mode_now(void) { return proj.pool == EN_MODE_POOL[proj.par[P_MODE]] ? proj.par[P_MODE] : -1; }
static void smk_draw_picker(void)
{
    int i, mode = smk_mode_now();
    int32_t x;
    x = cv_text(10, 2, &FONT_XS, "PIECE", K_DIM);
    cv_text(x + 8, 0, &FONT_B, piece_name(proj.piece), K_TEXT);
    x = cv_text(10, 64, &FONT_XS, "MODE", K_DIM);
    cv_text(x + 8, 62, &FONT_B, mode_name(), K_TEXT);
    for (i = 0; i < 16; i++) {                   /* as the pads: the pieces on the top row, the modes below */
        int row = i / 8, on = row ? i - 8 == mode : i == proj.piece;
        int32_t cx = 4 + (i % 8) * 29, cy = row ? 84 : 22;
        char t[4];
        caps(t, row ? SMK_MODE_SHORT[i - 8] : piece_name(i), sizeof t);
        cv_round(cx, cy, 27, 30, 5, on ? (row ? K_PAD : K_TEXT) : K_PANEL);
        text_c(cx + 13, cy + 9, &FONT_XS, t, on ? K_BG : K_DIM);
    }
    text_c(120, 124, &FONT_XS, "THE PADS PICK    KNOB BANK: BACK", K_DIM);
}

/* 1 = a card has the main screen's place this frame (drawn when it changed) */
static int smk_card(void)
{
    uint32_t h;
    int i;
    if (smk_fcard()) {
        h = hash(0x46414452u, smk.fbank | (uint32_t)(smk.ftouch + 1) << 4);
        for (i = 0; i < 4; i++)
            h = hash(h, (uint32_t)knob_get(SMK_FADER_PAR[smk.fbank][i]));
        if (h != ui.sig[1]) {
            ui.sig[1] = h;
            cv_begin(240, MAIN_H, K_BG);
            smk_draw_fcard();
            cv_blit(0, MAIN_Y);
        }
        return 1;
    }
    if (smk.pick) {
        h = hash(hash(0x5049434Bu, proj.piece | (uint32_t)proj.par[P_MODE] << 8), proj.pool);
        if (h != ui.sig[1]) {
            ui.sig[1] = h;
            cv_begin(240, MAIN_H, K_BG);
            smk_draw_picker();
            cv_blit(0, MAIN_Y);
        }
        return 1;
    }
    return 0;
}

/* the pads' lights (leds()): what the FM-1's key LEDs show, in colour, each pad of a group lit at least dim. The
 * scale: out of it dark orange, in it light blue, the root a brighter blue, the chord's tones as it plays bright
 * white-blue (dub hold: red). The effects grid: each group in its colour, its first pad brighter (the FM-1 lights its key), held white.
 * The Pattern page: the lane's steps in the lane's colour (accents brighter), the step playing white. PERFORM: the
 * layers in their colours (dim when out), the moves red while held. The Chords page: the slot's root. The picker:
 * the piece and the mode lit. Lights Off: the scale dark (the rest stays) */
static void smk_pad(int p, uint32_t r, uint32_t g, uint32_t b, uint32_t lv)   /* lv: 0..256 of r g b */
{
    r = r * lv >> 8;
    g = g * lv >> 8;
    b = b * lv >> 8;
    plat_pad_rgb(p, (uint8_t)(r > 255u ? 255u : r), (uint8_t)(g > 255u ? 255u : g), (uint8_t)(b > 255u ? 255u : b));
}
#define SMK_DIM 72u                                /* a pad of a group at rest: clearly lit, well under the others */
static const uint8_t SMK_RGB_SCALE[3] = {60, 230, 205}, SMK_RGB_HOLD[3] = {240, 80, 70};
/* three states told apart by colour, not only by level: off dark orange, on light blue, active bright white-blue */
static const uint8_t SMK_OFF[3] = {110, 38, 0}, SMK_ON[3] = {40, 120, 210}, SMK_ROOT[3] = {70, 170, 255},
                     SMK_ACT[3] = {190, 230, 255};
#define SMK_STATE(p, c) smk_pad((p), (c)[0], (c)[1], (c)[2], 256u)
static void smk_pads(void)
{
    static const uint8_t GROUP[4][3] = {{60, 200, 230}, {70, 110, 255}, {170, 100, 240}, {250, 150, 40}};
    int w, layout = smk_layout(ui.btn), grid = layout == SMK_PADS_GRID;
    if (layout == SMK_PADS_OWN) {                /* the picker */
        int mode = smk_mode_now();
        for (w = 0; w < SMK_NPADS; w++) {
            int on = w < 8 ? w == proj.piece : w - 8 == mode;
            SMK_STATE(w, on ? SMK_ACT : SMK_ON);
        }
        return;
    }
    for (w = 0; w < SMK_NPADS; w++) {            /* white key w on its pad */
        int p = grid ? w : w < 8 ? w + 8 : w - 8, key = STEP_KEY[w];
        uint32_t pc = (5u + (uint32_t)key) % 12u;
        if ((ui.btn >> B_REC & 1u) || ui.fx) {   /* the effects grid */
            const uint8_t *c = GROUP[w / 4];
            if (ui.fx >> w & 1u)
                smk_pad(p, 230, 230, 230, 256u);
            else
                smk_pad(p, c[0], c[1], c[2], (w & 3) ? SMK_DIM : 128u);
        } else if (ui.perf) {                    /* PERFORM: 1-5 the layers, 6-10 the moves */
            if (w < 3) {
                static const uint8_t LEVEL[3] = {P_CHORD, P_PINGS, P_BASS};
                SMK_STATE(p, proj.par[LEVEL[w]] > 0 ? SMK_ON : SMK_OFF);
            } else if (w < 5) {
                SMK_STATE(p, ui.mutes >> w & 1u ? SMK_OFF : SMK_ON);
            } else if (w < 10) {
                if (ui.moves >> (w - 5) & 1u)
                    smk_pad(p, SMK_RGB_HOLD[0], SMK_RGB_HOLD[1], SMK_RGB_HOLD[2], 256u);
                else
                    SMK_STATE(p, SMK_ON);
            } else {
                smk_pad(p, 0, 0, 0, 0u);
            }
        } else if (ui.view == V_PAT) {           /* the Pattern page: the lane's steps */
            uint32_t st = lane_steps(ui.lane) >> w & 1u;
            uint32_t ac = proj.par[P_DMODE] ? (uint32_t)(uint16_t)proj.par[P_DACC0 + ui.lane] >> w & 1u : 0u;
            if (en_playing && w == en_drum_pos[0])
                SMK_STATE(p, SMK_ACT);
            else
                SMK_STATE(p, st ? (ac ? SMK_ROOT : SMK_ON) : SMK_OFF);
        } else if (ui.view == V_CHORDS) {        /* the Chords page: the slot's root */
            int on = proj.par[P_CQ0 + ui.slot] && pc == (uint32_t)proj.par[P_CR0 + ui.slot];
            SMK_STATE(p, on ? SMK_ACT : SMK_OFF);
        } else if (!proj.leds) {
            smk_pad(p, 0, 0, 0, 0u);
        } else {                                 /* the scale; the chord's tones as it plays */
            uint32_t ap = pool_abs(), in = ap >> pc & 1u, root = pc == (uint32_t)proj.par[P_ROOT];
            int i, tone = 0;
            for (i = 0; en_playing && i < EN_NPAD; i++)
                tone |= en_chord[i] && en_chord[i] % 12u == pc;
            if (tone && en_pad_level > 0.02f) {
                if (en_frozen)
                    smk_pad(p, SMK_RGB_HOLD[0], SMK_RGB_HOLD[1], SMK_RGB_HOLD[2], 256u);
                else
                    SMK_STATE(p, SMK_ACT);
            } else {
                SMK_STATE(p, !in ? SMK_OFF : root ? SMK_ROOT : SMK_ON);
            }
        }
    }
}
#endif

static void draw_main(void)
{
    uint32_t now = plat_ms(), h = hash(2166136261u, (uint32_t)en_frozen << 16 | proj.piece << 24 | (uint32_t)en_playing << 17), i;
    static uint8_t moving;
    if (ui.frame % 3u)                           /* 20 frames a second is plenty */
        return;
#if BOARD_SMK37
    if (!grid_up() && !ui.help && smk_card())    /* the fader card or the picker */
        return;
#endif
    if (ui.view == V_PAT) {                      /* the Pattern page: the steps */
        h = hash(hash(0x50415454u, ui.lane | (uint32_t)proj.par[P_DMODE] << 4 | (uint32_t)en_drum_pos[0] << 8), en_playing);
        for (i = 0; i < EN_NDRUM; i++)
            h = hash(hash(h, lane_steps((int)i)), (uint32_t)(uint16_t)proj.par[P_DACC0 + i]);
        if (h != ui.sig[1]) {
            ui.sig[1] = h;
            cv_begin(240, MAIN_H, K_BG);
            draw_pattern();
            cv_blit(0, MAIN_Y);
        }
        return;
    }
    if (ui.view == V_CHORDS) {                   /* the Chords page: the slots */
        h = hash(hash(0x43484F52u, ui.slot | (uint32_t)proj.par[P_PMODE] << 4), (uint32_t)en_prog_pos + 1u);
        for (i = 0; i < EN_NPROG; i++)
            h = hash(h, (uint32_t)proj.par[P_CR0 + i] | (uint32_t)proj.par[P_CQ0 + i] << 4 | (uint32_t)proj.par[P_CX0 + i] << 8);
        if (h != ui.sig[1]) {
            ui.sig[1] = h;
            cv_begin(240, MAIN_H, K_BG);
            draw_slots();
            cv_blit(0, MAIN_Y);
        }
        return;
    }
    if (grid_up()) {                             /* REC held: the effects grid (ui_frame draws it, whole) */
        if (!ui.grid_said && ann_on()) {         /* the grid in words: the white keys' effects, a group at a time */
            int e;
            sp_n = 0;
            sp_buf[0] = 0;
            sp_cat("EFFECTS GRID. White keys");
            for (e = 0; e < EN_NFX; e++) {
                sp_cat(e == 0 ? ": " : (e & 3) ? ", " : "; ");
                sp_cat(EN_FX_NAME[e]);
            }
            ann_say(sp_buf, 0, ANN_NOW);
            ui.grid_said = 1;
        }
        return;
    }
    ui.grid_said = 0;
    if (ui.help) {
        h = hash(0x48454C50u, ui.help);
        if (h != ui.sig[1]) {
            ui.sig[1] = h;
            cv_begin(240, MAIN_H, K_BG);
            draw_help();
            cv_blit(0, MAIN_Y);
        }
        return;
    }
    ripples_take(now);
    for (i = 0; i < EN_NPAD; i++)
        h = hash(h, en_chord[i]);
    for (i = 0; i < 6; i++)
        h = hash(h, en_lane[i] | (uint32_t)proj.par[i < 4 ? i : 0] << 16);
    h = hash(h, en_drum_pos[0] | (uint32_t)q16(en_duck) << 4 | (uint32_t)q16(en_pad_level) << 9 | (uint32_t)q16(en_bass_level) << 14);
    h = hash(h, (uint32_t)(ui.view == V_SETUP ? plat_battery() + 1 : 0) | (uint32_t)(en_prog_pos + 1) << 12);
    h = hash(h, (uint32_t)(en_drift * 40.0f) | en_section << 8 | (uint32_t)en_phrase_bar << 12 | (uint32_t)ui.perf << 16);
    for (i = 0; i < EN_NLOOP; i++)
        h = hash(h, (uint32_t)q16(en_loop_glow[i]) | (uint32_t)en_loop_pos[i] << 8);
    for (i = 0; i < EN_NDRUM; i++)
        h = hash(h, (uint32_t)q16(en_drum_glow[i]));
    h = hash(h, ui.view);
    if (parent_of(ui.view) >= 0) {               /* a sound or FM page: redrawn as its values move */
        for (i = 0; i < 4; i++)
            h = hash(h, (uint32_t)(proj.par[VIEW_KNOB[ui.view][i]] + 64));
        h = hash(h, (uint32_t)proj.par[view_voice(ui.view)] | (uint32_t)proj.par[P_DECAY] << 8 | (uint32_t)proj.par[P_GROOVE] << 16);
    }
    if (h == ui.sig[1] && !moving)
        return;
    ui.sig[1] = h;
    cv_begin(240, MAIN_H, K_BG);
    if (ui.view >= V_CFM) {                      /* an FM page: the algorithm */
        draw_alg(layer_of(ui.view));
        moving = 0;
    } else if (parent_of(ui.view) >= 0) {        /* a sound page: the layer's envelope instead of the ripples */
        draw_env(parent_of(ui.view));
        moving = 1;
    } else if (proj.screen % 3u == 1u) {         /* (HOME again: a scope, a spectrum, or the ripples) */
        draw_scope();
        moving = 1;
    } else if (proj.screen % 3u == 2u) {
        draw_spec();
        moving = 1;
    } else {
        moving = (uint8_t)draw_ripples(now);
        draw_orbits();
    }
    if (parent_of(ui.view) < 0 && proj.screen % 3u) {   /* the chord: over the scope or the spectrum, small */
        char nm[16];
        chord_now(nm);
        text_c(CX, 1, &FONT_S, nm, en_frozen ? K_FROST : K_TEXT);
    } else if (parent_of(ui.view) < 0) {         /* the chord, in the middle */
        char nm[16];
        chord_now(nm);
        if (text_w(&FONT_B, nm) <= 44)
            text_c(CX, CY - 8, &FONT_B, nm, en_frozen ? K_FROST : K_TEXT);
        else
            text_c(CX, CY - 6, &FONT_XS, nm, en_frozen ? K_FROST : K_TEXT);
        if (proj.par[P_PMODE] && en_prog_pos >= 0 && prog_count()) {   /* the slots: where they are ("2/4") */
            char t[8];
            t[0] = (char)('1' + en_prog_pos), t[1] = '/', t[2] = (char)('0' + prog_count()), t[3] = 0;
            text_c(CX, CY + 8, &FONT_XS, t, K_DIM);
        }
    }
    {   /* top left: the phrase's section and its eight bars; under it, the mode the keys are in */
        char t[12];
        int b;
        caps(t, EN_SEC_NAME[en_section], sizeof t);
        cv_text(8, 2, &FONT_XS, t, en_section == SEC_FULL ? K_DIM : K_ACC);
        for (b = 0; b < 8; b++)
            cv_round(9 + b * 5, 22, 3, 3, 1, en_playing && b <= en_phrase_bar ? K_DIM : K_LINE);
    }
    if (ui.perf)
        cv_text(232 - text_w(&FONT_XS, "PERFORM"), 18, &FONT_XS, "PERFORM", K_ACC);
    if (en_frozen) {                             /* the way to the next chord, top right (dub hold: hold) */
        cv_text(232 - text_w(&FONT_XS, "hold"), 2, &FONT_XS, "hold", K_FROST);
    } else if (en_drift_bars(proj.par[P_DRIFT])) {
        int32_t w = 30, f = (int32_t)(en_drift * (float)w + 0.5f);
        cv_round(232 - w, 8, w, 6, 3, K_LINE);
        if (f > 0)
            cv_round(232 - w, 8, f < 6 ? 6 : f, 6, 3, K_PAD);
    }
    draw_meters();
    draw_duck();
    draw_lanes();
    if (ui.view == V_SETUP && (plat_battery() >= 0 || plat_usb_power())) {   /* the battery: on USB, or ~% (raw) */
        char t[24] = "BATT ";
        uint32_t k = 5;
        if (plat_usb_power()) {
            t[k++] = 'U', t[k++] = 'S', t[k++] = 'B', t[k] = 0;
        } else {
            itoa_u((uint32_t)batt_pct(), t + k);
            while (t[k])
                k++;
            t[k++] = '%', t[k] = 0;
        }
        if (plat_battery() >= 0) {               /* (and the raw reading, dim) */
            t[k++] = ' ', t[k++] = ' ', t[k++] = 'r', t[k++] = 'a', t[k++] = 'w', t[k++] = ' ';
            itoa_u((uint32_t)plat_battery(), t + k);
        }
        text_c(120, -1, &FONT_XS, t, K_DIM);
    }
    cv_blit(0, MAIN_Y);
}

/* the knob strip (X0X's): a rule with the page's name and five page dots at its right end; under
 * it a cell a knob: its name, a ring (or pips, for a switch of a few positions), the value. The
 * knob being turned is drawn on a tint. Only redrawn when what it shows changes (the rings cost a
 * square root and an arctangent per pixel). */
static void draw_knobs(void)
{
    uint32_t h = hash(2166136261u, ui.view | (uint32_t)ui.touched << 8), i;
    if (view_voice(ui.view) >= 0)
        h = hash(h, (uint32_t)proj.par[view_voice(ui.view)]);
    int now_touch = plat_ms() < ui.touch_until;
    for (i = 0; i < 4; i++)
        h = hash(h, (uint32_t)knob_get(VIEW_KNOB[ui.view][i]));
    h = hash(h, (uint32_t)now_touch);
    if (h == ui.sig[2])
        return;
    ui.sig[2] = h;
    cv_begin(240, KNB_H, K_BG);
    {   /* the rule; at its right end, where you are: the page, then the dots */
        char t[12];
        int k;
        int32_t xd = 237 - NPAGES * 6, xt;
        int32_t x0 = 4;
        if (view_voice(ui.view) >= 0) {          /* a layer's page: its voice, at the rule's start */
            char vn[12];
            en_param_text(view_voice(ui.view), proj.par[view_voice(ui.view)], vn);
            caps(t, vn, sizeof t);
            x0 = cv_text(6, -1, &FONT_XS, t, K_DIM) + 5;
        }
        caps(t, VIEW_NAME[ui.view], sizeof t);
        xt = xd - 5 - text_w(&FONT_XS, t);
        cv_rect(x0, 5, xt - 5 - x0, 1, K_LINE);
        cv_text(xt, -1, &FONT_XS, t, K_ACC);
        for (k = 0; k < NPAGES; k++)
            cv_round(xd + k * 6, 3, 4, 4, 2, k == page_of(ui.view) ? K_ACC : K_LINE);
    }
    for (i = 0; i < 4; i++) {
        int k = VIEW_KNOB[ui.view][i], lo, hi, v, hot = now_touch && ui.touched == (int)i;
        int32_t x = (int32_t)i * 60, cx = x + 30;
        char t[16];
        static const uint16_t MIXC[4] = {K_PAD, K_BELL, K_BASS, K_PULSE};   /* (the Mix page: the layers' colours) */
        uint16_t bg = hot ? K_PANEL : K_BG, fill = ui.view == V_MIX ? MIXC[i] : hot ? K_ACC : K_PAD;
        if (k == K_NONE)
            continue;
        if (i)
            cv_rect(x, 16, 1, 46, K_LINE);
        if (hot)
            cv_round(x + 3, 11, 54, 57, 6, K_PANEL);
        knob_range(k, &lo, &hi);
        v = knob_get(k);
        caps(t, KNOB_LABEL[ui.view][i], sizeof t);
        text_c(cx, 12, &FONT_XS, t, hot ? K_TEXT : K_DIM);
        if (k != K_CPU && hi - lo < 6) {         /* a switch: its positions as pips */
            int n = hi - lo + 1, j, pw = n > 4 ? 6 : 9, gap = 3, w0 = n * (pw + gap) - gap;
            for (j = 0; j < n; j++)
                cv_round(cx - w0 / 2 + j * (pw + gap), 33, pw, 8, 2, j == v - lo ? fill : K_LINE);
        } else if (k != K_CPU) {
            arc(cx, 37, 12, hi > lo ? (float)(v - lo) / (float)(hi - lo) : 0.0f, K_LINE, fill, bg, lo < 0);
        }
        knob_text(k, t);
        text_c(cx, 50, &FONT_B, t, hot ? K_ACC : K_TEXT);
    }
    cv_blit(0, KNB_Y);
}

static void leds(void)
{
    uint32_t b = 0, k = 0, i, ap = pool_abs();
    if (VIEW_BTN[page_of(ui.view)] != B_NONE && (parent_of(ui.view) < 0 || (plat_ms() / 300u) & 1u))
        b |= 1u << VIEW_BTN[page_of(ui.view)];   /* (a sound page: its layer's button blinks) */
    if (en_playing)
        b |= 1u << B_PLAY;
    if (en_frozen)
        b |= 1u << B_REC;
    if (ui.view == V_PAT && !ui.perf && !grid_up()) {   /* the Pattern page: the lane's steps on the white keys */
        uint32_t p = lane_steps(ui.lane);
        for (i = 0; i < 16; i++)
            if (p >> i & 1u)
                k |= 1u << STEP_KEY[i];
    } else if (ui.view == V_CHORDS && !ui.perf && !grid_up()) {   /* the Chords page: the slot's root (if it has one) */
        if (proj.par[P_CQ0 + ui.slot])
            for (i = 0; i < NKEYS; i++)
                if ((5u + i) % 12u == (uint32_t)proj.par[P_CR0 + ui.slot])
                    k |= 1u << i;
    } else if (grid_up()) {                      /* the grid: each group's first key, and the effects held */
        for (i = 0; i < EN_NFX; i++)
            if (!(i & 3) || (ui.fx >> i & 1u))
                k |= 1u << FX_KEY[i];
    } else if (ui.perf) {                        /* PERFORM: the layers playing, the moves held */
        b |= 1u << B_SEL;
        static const uint8_t LEVEL[3] = {P_CHORD, P_PINGS, P_BASS};
        for (i = 0; i < 3; i++)
            if (proj.par[LEVEL[i]] > 0)
                k |= 1u << PERF_KEY[i];
        for (i = 3; i < 5; i++)
            if (!(ui.mutes >> i & 1u))
                k |= 1u << PERF_KEY[i];
        for (i = 0; i < 5; i++)
            if (ui.moves >> i & 1u)
                k |= 1u << PERF_KEY[5 + i];
    } else if (proj.leds) {
        for (i = 0; i < NKEYS; i++)
            if (ap >> ((5u + i) % 12u) & 1u)
                k |= 1u << i;
    }
    plat_glow(proj.leds == 1);
#if BOARD_SMK37
    plat_smk_leds((uint32_t)smk.pick << SB_KNOB_BANK | (uint32_t)(smk.fbank != 0) << SB_FADER_BANK);
    smk_pads();
#endif
    plat_leds(b, k);
}

/* AUTOSAVE: a few seconds after a change, while resting and quiet, nothing touched (a flash erase
 * silences the audio for a moment: never while the garden grows; SAVE saves then) */
#define AUTOSAVE_QUIET 4000u
static void autosave(void)
{
    uint32_t now = plat_ms();
    if (!ui.dirty || en_playing || ui.btn || ui.keys || now - ui.act_t < AUTOSAVE_QUIET || en_pad_level > 0.01f)
        return;
    if (project_save() == 0) {
        ui.dirty = 0;
        ui.saved_t = now;
    }
}

/* ------------------------------------------------------ screen reader --- */
/* The screen as text for a screen reader on the computer (announce.c; fm1-reader): a page once it settles,
 * with its knobs; a knob's value as it turns; a help card as it comes up; the chord as it moves (queued: the
 * garden moves it by itself, every 20 s at the quickest); growing and resting from MIDI, and a MIDI clock
 * arriving or going; what a button did (say). WHERE (the reader asks, or it has just started listening) is
 * the whole screen in two: the page, its knobs and the header, then the garden (the chord, the scale).
 * Nothing is built while no reader listens. */
static struct {
    uint8_t view, help, ext, started;
    int16_t val[4];
    uint32_t chord;                            /* the chord's name, hashed */
} sp;
static char sp_buf[ANN_MAX + 1];
static uint32_t sp_n;
/* the chord names (en_chord_name's, after the root) in words: "m" and "M7" differ only in capitals, which
 * the words file ignores */
static const char *const CHORD_Q[][2] = {
    {"", "major"}, {"m", "minor"}, {"7", "7"}, {"M7", "major 7"}, {"m7", "minor 7"}, {"6", "6"},
    {"m6", "minor 6"}, {"add9", "add 9"}, {"madd9", "minor add 9"}, {"aug", "augmented"}, {"dim", "diminished"},
    {"dim7", "diminished 7"}, {"m7b5", "half diminished"}, {"mM7", "minor major 7"}, {"sus2", "suspended 2"},
    {"sus4", "suspended 4"}, {"7sus4", "7 suspended 4"}, {"5", "5"},
};
#define NCHORD_Q (sizeof CHORD_Q / sizeof CHORD_Q[0])

static void sp_cat(const char *s)
{
    while (*s && sp_n < ANN_MAX)
        sp_buf[sp_n++] = *s++;
    sp_buf[sp_n] = 0;
}
static void sp_int(uint32_t v)
{
    char t[12];
    itoa_u(v, t);
    sp_cat(t);
}
static int sp_same(const char *a, const char *b)
{
    while (*a && *a == *b)
        a++, b++;
    return *a == *b;
}

/* a value as it is said: as shown */
static void sp_value(int k)
{
    char t[16];
    knob_text(k, t);
    sp_cat(t);
}
static void sp_knob(int view, int i)           /* "Stabs 30": the name the page gives it, then its value */
{
    sp_cat(KNOB_LABEL[view][i]);
    sp_cat(" ");
    sp_value(VIEW_KNOB[view][i]);
}

static void sp_chord_q(int root, const char *q)   /* "E minor 7" */
{
    uint32_t i;
    sp_cat(EN_NOTE_NAME[root % 12]);
    sp_cat(" ");
    for (i = 0; i < NCHORD_Q; i++)
        if (sp_same(q, CHORD_Q[i][0])) {
            sp_cat(CHORD_Q[i][1]);
            return;
        }
    sp_cat(q);
}
static void sp_chord(void)                     /* the chord in the middle of the garden */
{
    char nm[16];
    const char *q = chord_now(nm), *r = EN_NOTE_NAME[en_chord_root % 12u];
    while (*r && *q == *r)
        q++, r++;
    sp_chord_q(en_chord_root % 12u, q);
}

/* the page: its name and its knobs; all: the header too (the piece, its key, the tempo, playing or resting) */
static void sp_page(int all)
{
    int i;
    sp_n = 0;
    sp_buf[0] = 0;
    sp_cat(VIEW_NAME[ui.view]);
    sp_cat(" page");
    if (view_voice(ui.view) >= 0) {
        char vn[12];
        en_param_text(view_voice(ui.view), proj.par[view_voice(ui.view)], vn);
        sp_cat(view_voice(ui.view) == P_KIT ? ", kit " : ", voice ");
        sp_cat(vn);
    }
    for (i = 0; i < 4; i++) {
        sp_cat(i ? ", " : ": ");
        sp_knob(ui.view, i);
    }
    if (!all)
        return;
    sp_cat(". Piece ");
    sp_cat(piece_name(proj.piece));
    sp_cat(", ");
    sp_cat(EN_NOTE_NAME[proj.par[P_ROOT]]);
    sp_cat(" ");
    sp_cat(mode_name());
    if (en_ext) {
        sp_cat(", tempo EXT");
    } else if (ui.view != V_DUB) {             /* (the Dub page's first knob says it) */
        sp_cat(", tempo ");
        sp_int((uint32_t)proj.par[P_TEMPO]);
    }
    sp_cat(sp_tp.playing ? ", playing" : ", resting");
    if (ui.dirty)
        sp_cat(", not saved yet");
    if (ui.view == V_SETUP) {
        sp_cat(". CPU ");
        sp_int(plat_cpu_pct());
        sp_cat("%, peak ");
        sp_int(plat_cpu_peak());
        sp_cat("%");
        if (plat_usb_power()) {
            sp_cat(", on USB power");
        } else if (plat_battery() >= 0) {
            sp_cat(", battery about ");
            sp_int((uint32_t)batt_pct());
            sp_cat("%");
        }
        if (plat_xruns()) {
            sp_cat(", ");
            sp_int(plat_xruns());
            sp_cat(" dropouts");
        }
    }
}

/* the main screen: the chord and how far to the next, the section, the scale from the root up ("F and F
 * sharp": one letter's two notes joined, so they are heard as two) */
static void sp_garden(void)
{
    uint32_t ap = pool_abs();
    int i, last = -1;
    sp_n = 0;
    sp_buf[0] = 0;
    sp_cat("Chord ");
    sp_chord();
    if (en_frozen) {
        sp_cat(", dub hold");
    } else if (en_drift_bars(proj.par[P_DRIFT])) {
        sp_cat(", next chord ");
        sp_int((uint32_t)(fm_clampf(en_drift, 0.0f, 1.0f) * 100.0f + 0.5f));
        sp_cat("% of the way");
    } else {
        sp_cat(", held");
    }
    sp_cat(". Section ");
    sp_cat(EN_SEC_NAME[en_section]);
    sp_cat(", bar ");
    sp_int((uint32_t)en_phrase_bar + 1u);
    sp_cat(". Scale");
    for (i = 0; i < 12; i++) {
        int pc = (proj.par[P_ROOT] + i) % 12;
        if (ap >> pc & 1u) {
            sp_cat(last < 0 ? " " : EN_NOTE_NAME[last][0] == EN_NOTE_NAME[pc][0] ? " and " : ", ");
            sp_cat(EN_NOTE_NAME[pc]);
            last = pc;
        }
    }
    if (en_pulse_rest && proj.par[P_DRUMS])
        sp_cat(". Kick out");
}

/* a help card: its button, then its lines ("ENV button. THE CHORD. LEVEL, STABS, DECAY, CUTOFF") */
static void sp_line(const char *s)
{
    char p = sp_n ? sp_buf[sp_n - 1] : 0;
    if (*s == ' ')                             /* it goes on from the line before */
        s++;
    else if (p != ',' && p != ':')
        sp_cat(".");
    sp_cat(" ");
    sp_cat(s);
}
static void sp_help(void)
{
    const char *l[HELP_LINES];
    int n = help_lines(ui.help - 1, l), k;
    sp_n = 0;
    sp_buf[0] = 0;
    sp_cat(BTN_LABEL[ui.help - 1]);
    sp_cat(" button");
    for (k = 0; k < n; k++)
        sp_line(l[k]);
}

static void sp_piece_text(void)                /* "PIECE Chamber, A Aeolian, tempo 118" */
{
    sp_n = 0;
    sp_buf[0] = 0;
    sp_cat("PIECE ");
    sp_cat(piece_name(proj.piece));
    sp_cat(", ");
    sp_cat(EN_NOTE_NAME[proj.par[P_ROOT]]);
    sp_cat(" ");
    sp_cat(mode_name());
    sp_cat(", tempo ");
    sp_int((uint32_t)proj.par[P_TEMPO]);
}
static void sp_piece(void)
{
    if (!ann_on())
        return;
    sp_piece_text();
    ann_say(sp_buf, 0, ANN_NOW);
    sp_said = 1;
}

static uint32_t sp_chord_sig(void)
{
    char nm[16];
    const char *s = chord_now(nm);
    uint32_t h = 2166136261u;
    while (*s)
        h = hash(h, (uint8_t)*s++);
    return h;
}
static void sp_snap(void)
{
    int i;
    for (i = 0; i < 4; i++)
        sp.val[i] = (int16_t)knob_get(VIEW_KNOB[ui.view][i]);
    sp.view = ui.view;
    sp.help = ui.help;
    sp.ext = en_ext;
    sp.chord = sp_chord_sig();
}

static void speak(void)
{
    uint32_t now = plat_ms(), c;
    int where = ann_where(), i, changed = -1;
    if (!ann_on()) {
        sp.started = 0;
        sp_said = 0;
        return;
    }
    if (!sp.started || where) {                       /* everything on the screen, in two */
        sp.started = 1;
        sp_snap();
        if (ui.help) {
            sp_help();
            ann_say(sp_buf, 0, ANN_NOW);
            sp_page(1);
        } else {
            sp_page(1);
            ann_say(sp_buf, 0, ANN_NOW);
            sp_garden();
        }
        ann_say(sp_buf, 0, 0);
        sp_said = 0;
        return;
    }
    if (sp_said) {                                    /* a button said what it did (and what it changed) */
        sp_said = 0;
        sp_snap();
        return;
    }
    if (en_playing != sp_tp.playing && now - sp_tp.play_t > 150u) {   /* MIDI start / stop */
        sp_tp.playing = en_playing;
        ann_say(en_playing ? "PLAYING" : "RESTING", 0, 0);
    }
    if (en_ext != sp.ext) {                           /* a MIDI clock arrived, or stopped */
        sp.ext = en_ext;
        sp_n = 0;
        sp_cat("Tempo ");
        if (en_ext)
            sp_cat("EXT");
        else
            sp_int((uint32_t)proj.par[P_TEMPO]);
        ann_say(sp_buf, 0, 0);
    }
    if (ui.help != sp.help) {                         /* a help card: at once */
        sp.help = ui.help;
        if (ui.help) {
            sp_help();
            ann_say(sp_buf, 0, ANN_NOW);
            return;
        }
    }
    if (ui.view != sp.view) {                         /* a new page: said when it settles */
        sp_snap();
        sp_page(0);
        ann_later(sp_buf, ANN_NOW, 200u, now);
        return;
    }
    for (i = 0; i < 4; i++) {                         /* a knob turning */
        int v = knob_get(VIEW_KNOB[ui.view][i]);
        if (v != sp.val[i]) {
            sp.val[i] = (int16_t)v;
            if (changed < 0)
                changed = i;
        }
    }
    if (changed >= 0) {
        sp_n = 0;
        sp_knob(ui.view, changed);
        ann_later(sp_buf, ANN_NOW, 120u, now);
    }
    c = sp_chord_sig();
    if (c != sp.chord) {                              /* the chord moved: queued, after what is being said */
        sp.chord = c;
        sp_n = 0;
        sp_cat("Chord ");
        sp_chord();
        ann_say(sp_buf, 0, 0);
    }
}

#ifdef EN_HOST
/* every page, knob value, help card, chord and message as a screen reader gets them (the "speakdump"
 * command): KIND <tab> the text sent (fm1-reader/tools/audit.mjs puts it in words with web/words/ and checks it) */
static void sp_print(const char *kind, const char *t) { printf("%s\t%s\n", kind, t); }
static void speak_dump(void)
{
    static const char *const MSG[][2] = {{"UPDATE MODE IN", "3.."}, {"UPDATE MODE", "CANCELLED"},
                                         {"NEXT CHORD", "ON THE BAR"}, {"NEXT CHORD", 0}, {"RESTING", 0},
                                         {"PLAYING", 0}, {"DUB HOLD", "ON"}, {"DUB HOLD", "OFF"}, {"SAVED", 0},
                                         {"PERFORM", "ON"}, {"PERFORM", "OFF"}};
    static const char *const WARN[][2] = {{"THE ROOT STAYS", "(SETUP)"}, {"3 NOTES", "AT LEAST"},
                                          {"NOT SAVED", "FLASH ERROR"}};
    project_t keep = proj;
    uint8_t keep_view = ui.view, keep_help = ui.help, keep_dirty = ui.dirty, keep_play = sp_tp.playing;
    uint8_t keep_ext = en_ext, keep_frozen = en_frozen, keep_rest = en_pulse_rest, keep_sec = en_section;
    int v, i, x, lo, hi;
    char t[48];
    for (v = 0; v < NVIEWS; v++) {
        ui.view = (uint8_t)v;
        sp_page(0);
        sp_print("SCREEN", sp_buf);
        for (i = 0; i < 4; i++) {
            int k = VIEW_KNOB[v][i];
            knob_range(k, &lo, &hi);
            for (x = lo; x <= hi; x++) {
                if (k < P_NPARAMS)
                    proj.par[k] = (int16_t)x;
                else if (k == K_SLOT)
                    ui.slot = (uint8_t)x;
                else if (k == K_CQ || k == K_CX)
                    proj.par[(k == K_CQ ? P_CQ0 : P_CX0) + ui.slot] = (int16_t)x;
                else if (k == K_LANE)
                    ui.lane = (uint8_t)x;
                else if (k == K_LCH)
                    proj.par[P_DCH0 + ui.lane] = (int16_t)x;
                else
                    proj.leds = (uint8_t)x;
                sp_n = 0;
                sp_knob(v, i);
                sp_print("KNOB", sp_buf);
            }
            proj = keep;
        }
    }
    for (x = 0; x < 8; x++) {                         /* WHERE: playing, resting, unsaved, a clock, held, custom */
        ui.view = (uint8_t)(x * 3 % NPAGES);
        sp_tp.playing = (uint8_t)(x & 1);
        ui.dirty = (uint8_t)(x >> 1 & 1);
        en_ext = (uint8_t)(x == 3);
        en_frozen = (uint8_t)(x == 5);
        en_pulse_rest = (uint8_t)(x == 2);
        en_section = (uint8_t)(x % EN_NSEC);
        proj.pool = x == 6 ? 0xFFFu : x == 7 ? 0x91u : keep.pool;
        proj.par[P_ROOT] = (int16_t)(x * 5 % 12);
        sp_page(1);
        sp_print("WHERE", sp_buf);
        sp_garden();
        sp_print("WHERE", sp_buf);
    }
    proj = keep;
    ui.dirty = keep_dirty;
    sp_tp.playing = keep_play;
    en_ext = keep_ext;
    en_frozen = keep_frozen;
    en_pulse_rest = keep_rest;
    en_section = keep_sec;
    ui.view = keep_view;
    for (i = 0; i < NB; i++) {
        const char *l[HELP_LINES];
        if (!help_lines(i, l))
            continue;
        ui.help = (uint8_t)(i + 1);
        sp_help();
        sp_print("HELP", sp_buf);
    }
    ui.help = keep_help;
    for (x = 0; x < 12; x++)
        for (i = 0; i < (int)NCHORD_Q; i++) {
            sp_n = 0;
            sp_cat("Chord ");
            sp_chord_q(x, CHORD_Q[i][0]);
            sp_print("CHORD", sp_buf);
        }
    for (i = 0; i < (int)(sizeof MSG / sizeof MSG[0]); i++) {
        msg_text(MSG[i][0], MSG[i][1], t);
        sp_print("MSG", t);
    }
    for (i = 0; i < (int)(sizeof WARN / sizeof WARN[0]); i++) {
        msg_text(WARN[i][0], WARN[i][1], t);
        sp_print("WARN", t);
    }
    for (x = 0; x < 12; x++) {
        msg_text("ADDED", EN_NOTE_NAME[x], t);
        sp_print("MSG", t);
        msg_text("REMOVED", EN_NOTE_NAME[x], t);
        sp_print("MSG", t);
    }
    for (x = 0; x < 10; x++) {
        msg_text(PERF_NAME[x], x < 5 ? "OFF" : 0, t);
        sp_print("MSG", t);
        if (x < 5) {
            msg_text(PERF_NAME[x], "ON", t);
            sp_print("MSG", t);
        }
    }
    for (x = 0; x < EN_NMODES; x++) {
        msg_text("MODE", EN_MODE_NAME[x], t);
        sp_print("MSG", t);
    }
    for (x = -2; x <= 2; x++) {
        char o[8];
        en_param_text(P_POCT, x, o);
        msg_text("PING OCTAVE", o, t);
        sp_print("MSG", t);
    }
    for (x = 90; x <= 140; x += 25) {
        char o[8];
        itoa_u((uint32_t)x, o);
        msg_text("TEMPO", o, t);
        sp_print("MSG", t);
    }
    for (x = 0; x < NPIECES; x++) {
        project_piece(x);
        sp_piece_text();
        sp_print("MSG", sp_buf);
        msg_text("RESET", piece_name(x), t);
        sp_print("MSG", t);
    }
    proj = keep;
    sp_print("MSG", "Tempo EXT");
    sp_print("MSG", "Tempo 122");
}
#endif

void ui_init(void)
{
    memset(&ui, 0, sizeof ui);
#if BOARD_SMK37
    smk_reset();
#endif
    ui.touched = -1;
    lcd_fill(0, 0, 240, 240, K_BG);
    if (!en_playing)
        en_play(1);                              /* it plays from the start */
    sp_tp.playing = 1;
    sp_tp.play_t = plat_ms();
}

void ui_frame(void)
{
    input();
    autosave();
    if (grid_up()) {                             /* REC held: the effects, the whole screen */
        if (!(ui.frame % 3u))
            draw_grid();
        draw_main();                             /* (it says the grid to a screen reader) */
    } else {
        draw_header();
        draw_main();
        draw_knobs();
    }
    leds();
    speak();
    ann_tick(plat_ms());
    ui.frame++;
}

void ui_input_only(void) { input(); }

/* the launch screen, while the FM-1 starts, as a delay throw: the name struck (a ring spreads from it),
 * then, every four frames as an echo's time, a repeat thrown out of it to its place along the line, each
 * smaller and fainter, in the layers' colours (the chord's teal, the pings' blue, the bass's brass, the
 * drums' amber), each with its own ring; then the line under it and the version. Frame f of UI_SPLASH_N,
 * drawn whole (two bands of 240 x 120) */
static void splash_band(int f, int32_t oy)
{
    static const felucca_font_t *const F[5] = {&FONT_L, &FONT_L, &FONT_M, &FONT_B, &FONT_S};
    static const int32_t FH[5] = {32, 32, 24, 16, 16}, YC = 70;
    const uint16_t C[5] = {K_TEXT, K_PAD, K_BELL, K_BASS, K_PULSE};
    int32_t w[5], x[5], k;
    for (k = 0; k < 5; k++)
        w[k] = text_w(F[k], "DUB-1");
    x[0] = 120 - w[0] / 2;                        /* the name in the middle; the repeats down the screen */
    cv_begin(240, 120, K_BG);
    for (k = 0; k < 5; k++) {                    /* the rings: one a strike, in its colour */
        int born = 1 + k * 4, age = f - born;
        int32_t r = 12 + age * 8;
        float a = 1.0f - (float)r / 140.0f;
        if (age < 0 || a <= 0.0f)
            continue;
        circle(120, YC - oy, r, mix(K_BG, k ? C[k] : K_PAD, q16(a * (k ? 0.8f : 0.6f))));
    }
    for (k = 4; k >= 1; k--) {                   /* the repeats: thrown out of the name, falling to their places */
        int born = 1 + k * 4, age = f - born;
        int32_t yt = YC + 14 + k * 26, y;
        float t;
        if (age < 0)
            continue;
        t = fm_minf((float)age / 3.0f, 1.0f);
        t = t * (2.0f - t);                      /* (eased) */
        y = YC + (int32_t)((float)(yt - YC) * t);
        cv_text(120 - w[k] / 2, y - FH[k] / 2 - oy, F[k], "DUB-1", mix(K_BG, C[k], (int)(16.0f * t) - 2 * k));
    }
    if (f >= 1)                                  /* the name, struck */
        cv_text(x[0], YC - FH[0] / 2 - oy, F[0], "DUB-1", mix(K_BG, K_TEXT, f >= 3 ? 16 : 6 * f));
    if (f >= 20)
#if BOARD_SMK37
        text_c(120, 204 - oy, &FONT_S, "Dub techno for the SMK-37", mix(K_BG, K_ACC, f >= 23 ? 16 : (f - 19) * 4));
#else
        text_c(120, 204 - oy, &FONT_S, "Dub techno for the FM-1", mix(K_BG, K_ACC, f >= 23 ? 16 : (f - 19) * 4));
#endif
    if (f >= 22)
        text_c(120, 224 - oy, &FONT_XS, "DUB-1 " EN_VERSION, K_DIM);
    cv_blit(0, (uint32_t)oy);
}

void ui_splash(int f)
{
    splash_band(f, 0);
    splash_band(f, 120);
}
