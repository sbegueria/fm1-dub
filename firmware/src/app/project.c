/* SPDX-License-Identifier: GPL-3.0-only */
/* DUB-1 project: one flash object (OBJ_PROJ). A project of another format, or none, gives the
 * defaults. Part of the unity build, after app.h.
 *
 * The pieces (their recipes are the engine's, en_piece): each keeps its own settings as you leave it;
 * Tune, MIDI out and Song mode are global; so are the dub sirens' knobs (format 34). */
project_t proj;
app_scratch_t app_scratch;

const char *piece_name(int i) { return en_piece_name(i); }

static int global_par(int p) { return p == P_TUNE || p == P_MIDI || p == P_SMODE; }   /* (1.4.0: Song mode, the
                                                  * player's: GLO's second page) */

/* (1.3.0) a piece never changed takes today's recipe on an upgrade: one that matches one of its own recipes as they
 * have been (recipe_prints.h, from git: a hash over their values, Tune and MIDI out left out), on the scale its mode
 * gives, was left as it came, so it gets today's sound; one you changed keeps your settings */
#include "recipe_prints.h"
static uint32_t print_of(const int16_t *par, int n)
{
    uint32_t h = 2166136261u;
    int i;
    for (i = 0; i < n; i++) {
        uint16_t u = (uint16_t)par[i];
        if (global_par(i))
            continue;
        h = (h ^ (u & 0xFFu)) * 16777619u;
        h = (h ^ (u >> 8)) * 16777619u;
    }
    return h;
}
static int untouched_raw(const int16_t *par, int n, uint16_t pool, int piece)   /* as it was saved, n of them */
{
    int k;
    if ((unsigned)par[P_MODE] >= EN_NMODES || pool != EN_MODE_POOL[par[P_MODE]])
        return 0;                                /* (its scale changed by hand) */
    for (k = 0; k < RECIPE_PRINTS; k++)
        if (RECIPE_PRINT[k].n == n && RECIPE_PRINT[k].h[piece] == print_of(par, n))
            return 1;
    return 0;
}
static int untouched_now(const slot_t *sl, int piece)   /* as read: an old recipe's first ones, today's after them */
{
    int16_t rec[P_NPARAMS];
    uint16_t pool;
    int k, p;
    en_piece(piece, rec, &pool);
    if (sl->pool != pool)
        return 0;
    for (k = 0; k < RECIPE_PRINTS; k++) {
        const int nk = RECIPE_PRINT[k].n;
        if (nk > P_NPARAMS || RECIPE_PRINT[k].h[piece] != print_of(sl->par, nk))
            continue;
        for (p = nk; p < P_NPARAMS && (global_par(p) || sl->par[p] == rec[p]); p++)
            ;
        if (p == P_NPARAMS)
            return 1;
    }
    return 0;
}

static void recipe(int i, slot_t *s)
{
    en_piece(i, s->par, &s->pool);
    s->pings_ok = 0;
    s->screen = 0;
}

/* the projects before 1.3.0's noise (formats 8 to 10) had the parameters up to P_NCOL */
#define NPAR10 P_NCOL
_Static_assert(NPAR10 == 106, "formats 8-10 had 106 parameters");
/* DUB-1 1.0.0's project (format 8): the same, without the pings; read, and kept */
typedef struct {
    int16_t par[NPAR10];
    uint16_t pool;
} slot8_t;
typedef struct {
    uint32_t magic, format;
    int16_t par[NPAR10];
    uint16_t pool;
    uint8_t leds, piece, screen, rsv0;
    slot8_t slot[NPIECES];
    uint8_t rsv[16];
} project8_t;
/* DUB-1 1.1.0 to 1.3.0's chords (formats 9 and 10): today's, without the noise's five */
typedef struct {
    int16_t par[NPAR10];
    uint16_t pool;
    uint8_t pings[EN_PINGS_N];
    uint8_t pings_ok, screen;
} slot10_t;
typedef struct {
    uint32_t magic, format;
    int16_t par[NPAR10];
    uint16_t pool;
    uint8_t leds, piece, screen, rsv0;
    slot10_t slot[NPIECES];
    uint8_t rsv[16];
} project10_t;
/* DUB-1 1.3.0's noise and ladder (formats 11 and 12): today's, without Throws, Ghost, Notes and Hats level */
#define NPAR12 P_CTHROW
_Static_assert(NPAR12 == 111, "formats 11-12 had 111 parameters");
typedef struct {
    int16_t par[NPAR12];
    uint16_t pool;
    uint8_t pings[EN_PINGS_N];
    uint8_t pings_ok, screen;
} slot12_t;
typedef struct {
    uint32_t magic, format;
    int16_t par[NPAR12];
    uint16_t pool;
    uint8_t leds, piece, screen, rsv0;
    slot12_t slot[NPIECES];
    uint8_t rsv[16];
} project12_t;
/* DUB-1 1.3.0's pages (format 13): today's, without the chord's Ghost, the kick's density, the echo's Blur, Tone and
 * Width, the noise's Decay */
#define NPAR13 P_CGHOST
_Static_assert(NPAR13 == 115, "format 13 had 115 parameters");
typedef struct {
    int16_t par[NPAR13];
    uint16_t pool;
    uint8_t pings[EN_PINGS_N];
    uint8_t pings_ok, screen;
} slot13_t;
typedef struct {
    uint32_t magic, format;
    int16_t par[NPAR13];
    uint16_t pool;
    uint8_t leds, piece, screen, rsv0;
    slot13_t slot[NPIECES];
    uint8_t rsv[16];
} project13_t;
/* DUB-1 1.3.0's echo and density (format 14): today's, without the Noise page's second and the hats' length */
#define NPAR14 P_NWIDTH
_Static_assert(NPAR14 == 121, "format 14 had 121 parameters");
typedef struct {
    int16_t par[NPAR14];
    uint16_t pool;
    uint8_t pings[EN_PINGS_N];
    uint8_t pings_ok, screen;
} slot14_t;
typedef struct {
    uint32_t magic, format;
    int16_t par[NPAR14];
    uint16_t pool;
    uint8_t leds, piece, screen, rsv0;
    slot14_t slot[NPIECES];
    uint8_t rsv[16];
} project14_t;
/* DUB-1 1.3.0's Noise space (format 15): today's, without the noise's LFO */
#define NPAR15 P_NLFO
_Static_assert(NPAR15 == 125, "format 15 had 125 parameters");
typedef struct {
    int16_t par[NPAR15];
    uint16_t pool;
    uint8_t pings[EN_PINGS_N];
    uint8_t pings_ok, screen;
} slot15_t;
typedef struct {
    uint32_t magic, format;
    int16_t par[NPAR15];
    uint16_t pool;
    uint8_t leds, piece, screen, rsv0;
    slot15_t slot[NPIECES];
    uint8_t rsv[16];
} project15_t;
/* DUB-1 1.3.0's noise LFO (format 16): today's, without the kick's rumble */
#define NPAR16 P_RUMBLE
_Static_assert(NPAR16 == 126, "format 16 had 126 parameters");
typedef struct {
    int16_t par[NPAR16];
    uint16_t pool;
    uint8_t pings[EN_PINGS_N];
    uint8_t pings_ok, screen;
} slot16_t;
typedef struct {
    uint32_t magic, format;
    int16_t par[NPAR16];
    uint16_t pool;
    uint8_t leds, piece, screen, rsv0;
    slot16_t slot[NPIECES];
    uint8_t rsv[16];
} project16_t;
/* DUB-1 1.4.0's test builds (formats 17 and 18): today's, without the LFOs' rates */
#define NPAR18 P_CLRATE
_Static_assert(NPAR18 == 127, "formats 17-18 had 127 parameters");
typedef struct {
    int16_t par[NPAR18];
    uint16_t pool;
    uint8_t pings[EN_PINGS_N];
    uint8_t pings_ok, screen;
} slot18_t;
typedef struct {
    uint32_t magic, format;
    int16_t par[NPAR18];
    uint16_t pool;
    uint8_t leds, piece, screen, rsv0;
    slot18_t slot[NPIECES];
    uint8_t rsv[16];
} project18_t;
/* DUB-1 1.4.0's test builds (format 19): today's, without the LFOs' waves */
#define NPAR19 P_CLWAVE
_Static_assert(NPAR19 == 129, "format 19 had 129 parameters");
typedef struct {
    int16_t par[NPAR19];
    uint16_t pool;
    uint8_t pings[EN_PINGS_N];
    uint8_t pings_ok, screen;
} slot19_t;
typedef struct {
    uint32_t magic, format;
    int16_t par[NPAR19];
    uint16_t pool;
    uint8_t leds, piece, screen, rsv0;
    slot19_t slot[NPIECES];
    uint8_t rsv[16];
} project19_t;
/* DUB-1 1.4.0's LFO pages (format 20): today's, without the chord's Vary and its LFO's Destination */
#define NPAR20 P_CVARY
_Static_assert(NPAR20 == 133, "format 20 had 133 parameters");
typedef struct {
    int16_t par[NPAR20];
    uint16_t pool;
    uint8_t pings[EN_PINGS_N];
    uint8_t pings_ok, screen;
} slot20_t;
typedef struct {
    uint32_t magic, format;
    int16_t par[NPAR20];
    uint16_t pool;
    uint8_t leds, piece, screen, rsv0;
    slot20_t slot[NPIECES];
    uint8_t rsv[16];
} project20_t;
/* DUB-1 1.4.0's Chord pages (format 21): today's, without the layers' Vary and the chord's Cutoff knob */
#define NPAR21 P_PVARY
_Static_assert(NPAR21 == 135, "format 21 had 135 parameters");
typedef struct {
    int16_t par[NPAR21];
    uint16_t pool;
    uint8_t pings[EN_PINGS_N];
    uint8_t pings_ok, screen;
} slot21_t;
typedef struct {
    uint32_t magic, format;
    int16_t par[NPAR21];
    uint16_t pool;
    uint8_t leds, piece, screen, rsv0;
    slot21_t slot[NPIECES];
    uint8_t rsv[16];
} project21_t;
/* DUB-1 1.4.0's layers' pages (format 22): today's, without the channel strips' sends and Low cuts */
#define NPAR22 P_CECHO
_Static_assert(NPAR22 == 140, "format 22 had 140 parameters");
typedef struct {
    int16_t par[NPAR22];
    uint16_t pool;
    uint8_t pings[EN_PINGS_N];
    uint8_t pings_ok, screen;
} slot22_t;
typedef struct {
    uint32_t magic, format;
    int16_t par[NPAR22];
    uint16_t pool;
    uint8_t leds, piece, screen, rsv0;
    slot22_t slot[NPIECES];
    uint8_t rsv[16];
} project22_t;
/* DUB-1 1.4.0's channel strips (format 23): today's, the drums one strip (no Kick strip: its level, sends, Low cut) */
#define NPAR23 P_KLVL
_Static_assert(NPAR23 == 153, "format 23 had 153 parameters");
typedef struct {
    int16_t par[NPAR23];
    uint16_t pool;
    uint8_t pings[EN_PINGS_N];
    uint8_t pings_ok, screen;
} slot23_t;
typedef struct {
    uint32_t magic, format;
    int16_t par[NPAR23];
    uint16_t pool;
    uint8_t leds, piece, screen, rsv0;
    slot23_t slot[NPIECES];
    uint8_t rsv[16];
} project23_t;
/* DUB-1 1.4.0's six strips (format 24): today's, without the noise LFO's Destination (P_NLDEST) */
#define NPAR24 P_NLDEST
_Static_assert(NPAR24 == 157, "format 24 had 157 parameters");
typedef struct {
    int16_t par[NPAR24];
    uint16_t pool;
    uint8_t pings[EN_PINGS_N];
    uint8_t pings_ok, screen;
} slot24_t;
typedef struct {
    uint32_t magic, format;
    int16_t par[NPAR24];
    uint16_t pool;
    uint8_t leds, piece, screen, rsv0;
    slot24_t slot[NPIECES];
    uint8_t rsv[16];
} project24_t;
/* DUB-1 1.4.0's noise LFO Destination (format 25): today's, without song structure (Song mode, Intro, Main, Outro,
 * Kind) */
#define NPAR25 P_SMODE
_Static_assert(NPAR25 == 158, "format 25 had 158 parameters");
typedef struct {
    int16_t par[NPAR25];
    uint16_t pool;
    uint8_t pings[EN_PINGS_N];
    uint8_t pings_ok, screen;
} slot25_t;
typedef struct {
    uint32_t magic, format;
    int16_t par[NPAR25];
    uint16_t pool;
    uint8_t leds, piece, screen, rsv0;
    slot25_t slot[NPIECES];
    uint8_t rsv[16];
} project25_t;
/* DUB-1 1.4.0's song structure (format 26): today's, without the dub mixer (Mixer) */
#define NPAR26 P_MIXER
_Static_assert(NPAR26 == 163, "format 26 had 163 parameters");
typedef struct {
    int16_t par[NPAR26];
    uint16_t pool;
    uint8_t pings[EN_PINGS_N];
    uint8_t pings_ok, screen;
} slot26_t;
typedef struct {
    uint32_t magic, format;
    int16_t par[NPAR26];
    uint16_t pool;
    uint8_t leds, piece, screen, rsv0;
    slot26_t slot[NPIECES];
    uint8_t rsv[16];
} project26_t;
/* DUB-1 1.4.0's dub mixer and strips' Filter (formats 27 and 28): today's, without the piece's looseness (Evolve's then);
 * their Vary Form's Evolve, the layers' Varys 0..100 over it */
#define NPAR28 P_LOOSE
_Static_assert(NPAR28 == 164, "formats 27 and 28 had 164 parameters");
typedef struct {
    int16_t par[NPAR28];
    uint16_t pool;
    uint8_t pings[EN_PINGS_N];
    uint8_t pings_ok, screen;
} slot28_t;
typedef struct {
    uint32_t magic, format;
    int16_t par[NPAR28];
    uint16_t pool;
    uint8_t leds, piece, screen, rsv0;
    slot28_t slot[NPIECES];
    uint8_t rsv[16];
} project28_t;
/* DUB-1 1.4.0's variety plan's step 1 (format 29): today's, without Drift, the LFOs' Trims and the throws' amount
 * (the LFOs' Depth and Drift and the Mixer's rides then) */
#define NPAR29 P_MOVE
_Static_assert(NPAR29 == 165, "format 29 had 165 parameters");
typedef struct {
    int16_t par[NPAR29];
    uint16_t pool;
    uint8_t pings[EN_PINGS_N];
    uint8_t pings_ok, screen;
} slot29_t;
typedef struct {
    uint32_t magic, format;
    int16_t par[NPAR29];
    uint16_t pool;
    uint8_t leds, piece, screen, rsv0;
    slot29_t slot[NPIECES];
    uint8_t rsv[16];
} project29_t;
/* DUB-1 1.4.0's formats 30 to 34 (the variety plan's steps 2-5, the Transition, the sirens' knobs): today's, without
 * the sweep's pace */
#define NPAR34 P_SWEEP
_Static_assert(NPAR34 == 169, "formats 30-34 had 169 parameters");
typedef struct {
    int16_t par[NPAR34];
    uint16_t pool;
    uint8_t pings[EN_PINGS_N];
    uint8_t pings_ok, screen;
} slot34_t;
typedef struct {
    uint32_t magic, format;
    int16_t par[NPAR34];
    uint16_t pool;
    uint8_t leds, piece, screen, values;
    slot34_t slot[NPIECES];
    uint8_t rsv[15];
    uint8_t trans;
    int8_t sir[EN_NSIR][EN_NSK];
} project34_t;
_Static_assert(sizeof(project_t) <= 3840u, "the project must fit one storage sector's payload");

static void take(const slot_t *s)                /* a piece's settings into play, the globals kept */
{
    int p;
    for (p = 0; p < P_NPARAMS; p++)
        if (!global_par(p))
            proj.par[p] = s->par[p];
    proj.pool = s->pool;
    proj.screen = ui_screen_ok(s->screen);       /* (each piece its own view; one gone, the ripples) */
}

static void keep(void)                           /* the piece playing, into its slot (with its ping loops) */
{
    int p;
    for (p = 0; p < P_NPARAMS; p++)
        proj.slot[proj.piece].par[p] = proj.par[p];
    proj.slot[proj.piece].pool = proj.pool;
    proj.slot[proj.piece].screen = proj.screen;
    en_pings_get(proj.slot[proj.piece].pings);
    proj.slot[proj.piece].pings_ok = 1;
}

static void sir_defaults(void)                   /* (format 34) the sirens' knobs as the sirens were before them */
{
    int s, k;
    for (s = 0; s < EN_NSIR; s++)
        for (k = 0; k < EN_NSK; k++)
            proj.sir[s][k] = (int8_t)en_sir_def(s, k);
}
static void sir_clamp(void)                      /* (a value past its range: to its end) */
{
    int s, k, lo, hi;
    for (s = 0; s < EN_NSIR; s++)
        for (k = 0; k < EN_NSK; k++) {
            en_sir_range(s, k, &lo, &hi);
            if (proj.sir[s][k] < lo || proj.sir[s][k] > hi)
                proj.sir[s][k] = (int8_t)(proj.sir[s][k] < lo ? lo : hi);
        }
}

void project_defaults(void)
{
    int i;
    memset(&proj, 0, sizeof proj);
    proj.magic = PROJ_MAGIC;
    proj.format = PROJ_FORMAT;
    proj.rsv[0] = PROJ_WAVE_MARK;
    for (i = 0; i < NPIECES; i++)
        recipe(i, &proj.slot[i]);
    for (i = 0; i < P_NPARAMS; i++)
        proj.par[i] = en_param_info(i)->def;
    take(&proj.slot[0]);
    proj.leds = 1;
    sir_defaults();
}

static int pool_ok(uint32_t p)                   /* the root and at least two more notes */
{
    int n = 0, i;
    for (i = 0; i < 12; i++)
        n += (int)(p >> i & 1u);
    return (p & 1u) && !(p >> 12) && n >= 3;
}

static int pars_ok(const int16_t *par)
{
    int i;
    for (i = 0; i < P_NPARAMS; i++) {
        const en_param_t *p = en_param_info(i);
        if (par[i] < p->lo || par[i] > p->hi)
            return 0;
    }
    return 1;
}

static void pars_clamp(int16_t *par)            /* a value out of its (now narrower) range: to its end, not the whole
                                                  * project thrown away (1.3.0's one-sided knobs) */
{
    int i;
    for (i = 0; i < P_NPARAMS; i++) {
        const en_param_t *p = en_param_info(i);
        if (par[i] < p->lo)
            par[i] = p->lo;
        if (par[i] > p->hi)
            par[i] = p->hi;
    }
}
static int project_valid(void)                  /* an older format: the recipes again */
{
    int i;
    if (proj.magic != PROJ_MAGIC || !pool_ok(proj.pool) || !pars_ok(proj.par))
        return 0;
    if (proj.format != PROJ_FORMAT || proj.piece >= NPIECES)
        return 0;
    for (i = 0; i < NPIECES; i++)
        if (!pool_ok(proj.slot[i].pool) || !pars_ok(proj.slot[i].par))
            return 0;
    return 1;
}

static void from8(const project8_t *o)          /* 1.0.0's project: everything kept, the pings grown afresh */
{
    int i;
    memset(&proj, 0, sizeof proj);
    proj.magic = o->magic;
    proj.format = PROJ_FORMAT;
    memcpy(proj.par, o->par, sizeof o->par);
    proj.pool = o->pool;
    proj.leds = o->leds, proj.piece = o->piece, proj.screen = o->screen;
    for (i = 0; i < NPIECES; i++) {
        memcpy(proj.slot[i].par, o->slot[i].par, sizeof o->slot[i].par);
        proj.slot[i].pool = o->slot[i].pool;
    }
}
static void from10(const project10_t *o)        /* 1.1.0 to 1.3.0's chords: everything kept */
{
    int i;
    memset(&proj, 0, sizeof proj);
    proj.magic = o->magic;
    proj.format = PROJ_FORMAT;
    memcpy(proj.par, o->par, sizeof o->par);
    proj.pool = o->pool;
    proj.leds = o->leds, proj.piece = o->piece, proj.screen = o->screen;
    for (i = 0; i < NPIECES; i++) {
        memcpy(proj.slot[i].par, o->slot[i].par, sizeof o->slot[i].par);
        proj.slot[i].pool = o->slot[i].pool;
        memcpy(proj.slot[i].pings, o->slot[i].pings, sizeof proj.slot[i].pings);
        proj.slot[i].pings_ok = o->slot[i].pings_ok;
        proj.slot[i].screen = o->slot[i].screen;
    }
}
static void from12(const project12_t *o)        /* 1.3.0's noise and ladder (11, 12): everything kept */
{
    int i;
    memset(&proj, 0, sizeof proj);
    proj.magic = o->magic;
    proj.format = PROJ_FORMAT;
    memcpy(proj.par, o->par, sizeof o->par);
    proj.pool = o->pool;
    proj.leds = o->leds, proj.piece = o->piece, proj.screen = o->screen;
    for (i = 0; i < NPIECES; i++) {
        memcpy(proj.slot[i].par, o->slot[i].par, sizeof o->slot[i].par);
        proj.slot[i].pool = o->slot[i].pool;
        memcpy(proj.slot[i].pings, o->slot[i].pings, sizeof proj.slot[i].pings);
        proj.slot[i].pings_ok = o->slot[i].pings_ok;
        proj.slot[i].screen = o->slot[i].screen;
    }
}
static void from13(const project13_t *o)        /* 1.3.0's pages (13): everything kept */
{
    int i;
    memset(&proj, 0, sizeof proj);
    proj.magic = o->magic;
    proj.format = PROJ_FORMAT;
    memcpy(proj.par, o->par, sizeof o->par);
    proj.pool = o->pool;
    proj.leds = o->leds, proj.piece = o->piece, proj.screen = o->screen;
    for (i = 0; i < NPIECES; i++) {
        memcpy(proj.slot[i].par, o->slot[i].par, sizeof o->slot[i].par);
        proj.slot[i].pool = o->slot[i].pool;
        memcpy(proj.slot[i].pings, o->slot[i].pings, sizeof proj.slot[i].pings);
        proj.slot[i].pings_ok = o->slot[i].pings_ok;
        proj.slot[i].screen = o->slot[i].screen;
    }
}
static void from14(const project14_t *o)        /* 1.3.0's echo and density (14): everything kept */
{
    int i;
    memset(&proj, 0, sizeof proj);
    proj.magic = o->magic;
    proj.format = PROJ_FORMAT;
    memcpy(proj.par, o->par, sizeof o->par);
    proj.pool = o->pool;
    proj.leds = o->leds, proj.piece = o->piece, proj.screen = o->screen;
    for (i = 0; i < NPIECES; i++) {
        memcpy(proj.slot[i].par, o->slot[i].par, sizeof o->slot[i].par);
        proj.slot[i].pool = o->slot[i].pool;
        memcpy(proj.slot[i].pings, o->slot[i].pings, sizeof proj.slot[i].pings);
        proj.slot[i].pings_ok = o->slot[i].pings_ok;
        proj.slot[i].screen = o->slot[i].screen;
    }
}
static void from15(const project15_t *o)        /* 1.3.0's Noise space (15): everything kept */
{
    int i;
    memset(&proj, 0, sizeof proj);
    proj.magic = o->magic;
    proj.format = PROJ_FORMAT;
    memcpy(proj.par, o->par, sizeof o->par);
    proj.pool = o->pool;
    proj.leds = o->leds, proj.piece = o->piece, proj.screen = o->screen;
    for (i = 0; i < NPIECES; i++) {
        memcpy(proj.slot[i].par, o->slot[i].par, sizeof o->slot[i].par);
        proj.slot[i].pool = o->slot[i].pool;
        memcpy(proj.slot[i].pings, o->slot[i].pings, sizeof proj.slot[i].pings);
        proj.slot[i].pings_ok = o->slot[i].pings_ok;
        proj.slot[i].screen = o->slot[i].screen;
    }
}
static void from16(const project16_t *o)        /* 1.3.0's noise LFO (16): everything kept */
{
    int i;
    memset(&proj, 0, sizeof proj);
    proj.magic = o->magic;
    proj.format = PROJ_FORMAT;
    memcpy(proj.par, o->par, sizeof o->par);
    proj.pool = o->pool;
    proj.leds = o->leds, proj.piece = o->piece, proj.screen = o->screen;
    for (i = 0; i < NPIECES; i++) {
        memcpy(proj.slot[i].par, o->slot[i].par, sizeof o->slot[i].par);
        proj.slot[i].pool = o->slot[i].pool;
        memcpy(proj.slot[i].pings, o->slot[i].pings, sizeof proj.slot[i].pings);
        proj.slot[i].pings_ok = o->slot[i].pings_ok;
        proj.slot[i].screen = o->slot[i].screen;
    }
}
static void from18(const project18_t *o)        /* 1.4.0's test builds (17, 18): everything kept */
{
    int i;
    memset(&proj, 0, sizeof proj);
    proj.magic = o->magic;
    proj.format = PROJ_FORMAT;
    memcpy(proj.par, o->par, sizeof o->par);
    proj.pool = o->pool;
    proj.leds = o->leds, proj.piece = o->piece, proj.screen = o->screen;
    for (i = 0; i < NPIECES; i++) {
        memcpy(proj.slot[i].par, o->slot[i].par, sizeof o->slot[i].par);
        proj.slot[i].pool = o->slot[i].pool;
        memcpy(proj.slot[i].pings, o->slot[i].pings, sizeof proj.slot[i].pings);
        proj.slot[i].pings_ok = o->slot[i].pings_ok;
        proj.slot[i].screen = o->slot[i].screen;
    }
}
static void from19(const project19_t *o)        /* 1.4.0's test builds (19): everything kept */
{
    int i;
    memset(&proj, 0, sizeof proj);
    proj.magic = o->magic;
    proj.format = PROJ_FORMAT;
    memcpy(proj.par, o->par, sizeof o->par);
    proj.pool = o->pool;
    proj.leds = o->leds, proj.piece = o->piece, proj.screen = o->screen;
    for (i = 0; i < NPIECES; i++) {
        memcpy(proj.slot[i].par, o->slot[i].par, sizeof o->slot[i].par);
        proj.slot[i].pool = o->slot[i].pool;
        memcpy(proj.slot[i].pings, o->slot[i].pings, sizeof proj.slot[i].pings);
        proj.slot[i].pings_ok = o->slot[i].pings_ok;
        proj.slot[i].screen = o->slot[i].screen;
    }
}
static void from20(const project20_t *o)        /* 1.4.0's LFO pages (20): everything kept */
{
    int i;
    memset(&proj, 0, sizeof proj);
    proj.magic = o->magic;
    proj.format = PROJ_FORMAT;
    memcpy(proj.par, o->par, sizeof o->par);
    proj.pool = o->pool;
    proj.leds = o->leds, proj.piece = o->piece, proj.screen = o->screen;
    for (i = 0; i < NPIECES; i++) {
        memcpy(proj.slot[i].par, o->slot[i].par, sizeof o->slot[i].par);
        proj.slot[i].pool = o->slot[i].pool;
        memcpy(proj.slot[i].pings, o->slot[i].pings, sizeof proj.slot[i].pings);
        proj.slot[i].pings_ok = o->slot[i].pings_ok;
        proj.slot[i].screen = o->slot[i].screen;
    }
}
static void from21(const project21_t *o)        /* 1.4.0's Chord pages (21): everything kept */
{
    int i;
    memset(&proj, 0, sizeof proj);
    proj.magic = o->magic;
    proj.format = PROJ_FORMAT;
    memcpy(proj.par, o->par, sizeof o->par);
    proj.pool = o->pool;
    proj.leds = o->leds, proj.piece = o->piece, proj.screen = o->screen;
    for (i = 0; i < NPIECES; i++) {
        memcpy(proj.slot[i].par, o->slot[i].par, sizeof o->slot[i].par);
        proj.slot[i].pool = o->slot[i].pool;
        memcpy(proj.slot[i].pings, o->slot[i].pings, sizeof proj.slot[i].pings);
        proj.slot[i].pings_ok = o->slot[i].pings_ok;
        proj.slot[i].screen = o->slot[i].screen;
    }
}
static void from22(const project22_t *o)        /* 1.4.0's layers' pages (22): everything kept */
{
    int i;
    memset(&proj, 0, sizeof proj);
    proj.magic = o->magic;
    proj.format = PROJ_FORMAT;
    memcpy(proj.par, o->par, sizeof o->par);
    proj.pool = o->pool;
    proj.leds = o->leds, proj.piece = o->piece, proj.screen = o->screen;
    for (i = 0; i < NPIECES; i++) {
        memcpy(proj.slot[i].par, o->slot[i].par, sizeof o->slot[i].par);
        proj.slot[i].pool = o->slot[i].pool;
        memcpy(proj.slot[i].pings, o->slot[i].pings, sizeof proj.slot[i].pings);
        proj.slot[i].pings_ok = o->slot[i].pings_ok;
        proj.slot[i].screen = o->slot[i].screen;
    }
}
static void from23(const project23_t *o)        /* 1.4.0's channel strips (23): everything kept */
{
    int i;
    memset(&proj, 0, sizeof proj);
    proj.magic = o->magic;
    proj.format = PROJ_FORMAT;
    memcpy(proj.par, o->par, sizeof o->par);
    proj.pool = o->pool;
    proj.leds = o->leds, proj.piece = o->piece, proj.screen = o->screen;
    for (i = 0; i < NPIECES; i++) {
        memcpy(proj.slot[i].par, o->slot[i].par, sizeof o->slot[i].par);
        proj.slot[i].pool = o->slot[i].pool;
        memcpy(proj.slot[i].pings, o->slot[i].pings, sizeof proj.slot[i].pings);
        proj.slot[i].pings_ok = o->slot[i].pings_ok;
        proj.slot[i].screen = o->slot[i].screen;
    }
}
static void from24(const project24_t *o)        /* 1.4.0's six strips (24): everything kept */
{
    int i;
    memset(&proj, 0, sizeof proj);
    proj.magic = o->magic;
    proj.format = PROJ_FORMAT;
    memcpy(proj.par, o->par, sizeof o->par);
    proj.pool = o->pool;
    proj.leds = o->leds, proj.piece = o->piece, proj.screen = o->screen;
    for (i = 0; i < NPIECES; i++) {
        memcpy(proj.slot[i].par, o->slot[i].par, sizeof o->slot[i].par);
        proj.slot[i].pool = o->slot[i].pool;
        memcpy(proj.slot[i].pings, o->slot[i].pings, sizeof proj.slot[i].pings);
        proj.slot[i].pings_ok = o->slot[i].pings_ok;
        proj.slot[i].screen = o->slot[i].screen;
    }
}
static void from25(const project25_t *o)        /* 1.4.0's noise LFO Destination (25): everything kept */
{
    int i;
    memset(&proj, 0, sizeof proj);
    proj.magic = o->magic;
    proj.format = PROJ_FORMAT;
    memcpy(proj.par, o->par, sizeof o->par);
    proj.pool = o->pool;
    proj.leds = o->leds, proj.piece = o->piece, proj.screen = o->screen;
    for (i = 0; i < NPIECES; i++) {
        memcpy(proj.slot[i].par, o->slot[i].par, sizeof o->slot[i].par);
        proj.slot[i].pool = o->slot[i].pool;
        memcpy(proj.slot[i].pings, o->slot[i].pings, sizeof proj.slot[i].pings);
        proj.slot[i].pings_ok = o->slot[i].pings_ok;
        proj.slot[i].screen = o->slot[i].screen;
    }
}
static void from26(const project26_t *o)        /* 1.4.0's song structure (26): everything kept */
{
    int i;
    memset(&proj, 0, sizeof proj);
    proj.magic = o->magic;
    proj.format = PROJ_FORMAT;
    memcpy(proj.par, o->par, sizeof o->par);
    proj.pool = o->pool;
    proj.leds = o->leds, proj.piece = o->piece, proj.screen = o->screen;
    for (i = 0; i < NPIECES; i++) {
        memcpy(proj.slot[i].par, o->slot[i].par, sizeof o->slot[i].par);
        proj.slot[i].pool = o->slot[i].pool;
        memcpy(proj.slot[i].pings, o->slot[i].pings, sizeof proj.slot[i].pings);
        proj.slot[i].pings_ok = o->slot[i].pings_ok;
        proj.slot[i].screen = o->slot[i].screen;
    }
}
static void from28(const project28_t *o)        /* 1.4.0's dub mixer and Filter (27, 28): everything kept */
{
    int i;
    memset(&proj, 0, sizeof proj);
    proj.magic = o->magic;
    proj.format = PROJ_FORMAT;
    memcpy(proj.par, o->par, sizeof o->par);
    proj.pool = o->pool;
    proj.leds = o->leds, proj.piece = o->piece, proj.screen = o->screen;
    for (i = 0; i < NPIECES; i++) {
        memcpy(proj.slot[i].par, o->slot[i].par, sizeof o->slot[i].par);
        proj.slot[i].pool = o->slot[i].pool;
        memcpy(proj.slot[i].pings, o->slot[i].pings, sizeof proj.slot[i].pings);
        proj.slot[i].pings_ok = o->slot[i].pings_ok;
        proj.slot[i].screen = o->slot[i].screen;
    }
}
static void from29(const project29_t *o)        /* the variety plan's step 1 (29): everything kept */
{
    int i;
    memset(&proj, 0, sizeof proj);
    proj.magic = o->magic;
    proj.format = PROJ_FORMAT;
    memcpy(proj.par, o->par, sizeof o->par);
    proj.pool = o->pool;
    proj.leds = o->leds, proj.piece = o->piece, proj.screen = o->screen;
    for (i = 0; i < NPIECES; i++) {
        memcpy(proj.slot[i].par, o->slot[i].par, sizeof o->slot[i].par);
        proj.slot[i].pool = o->slot[i].pool;
        memcpy(proj.slot[i].pings, o->slot[i].pings, sizeof proj.slot[i].pings);
        proj.slot[i].pings_ok = o->slot[i].pings_ok;
        proj.slot[i].screen = o->slot[i].screen;
    }
}
static void from34(const project34_t *o)        /* (30 to 34) everything kept, the format too (read on below as it
                                                  * is); the sweep's pace from the recipes (newpar) */
{
    int i;
    memset(&proj, 0, sizeof proj);
    proj.magic = o->magic;
    proj.format = o->format;
    memcpy(proj.par, o->par, sizeof o->par);
    proj.pool = o->pool;
    proj.leds = o->leds, proj.piece = o->piece, proj.screen = o->screen, proj.values = o->values;
    for (i = 0; i < NPIECES; i++) {
        memcpy(proj.slot[i].par, o->slot[i].par, sizeof o->slot[i].par);
        proj.slot[i].pool = o->slot[i].pool;
        memcpy(proj.slot[i].pings, o->slot[i].pings, sizeof proj.slot[i].pings);
        proj.slot[i].pings_ok = o->slot[i].pings_ok;
        proj.slot[i].screen = o->slot[i].screen;
    }
    memcpy(proj.rsv, o->rsv, sizeof proj.rsv);
    proj.trans = o->trans;
    memcpy(proj.sir, o->sir, sizeof proj.sir);
}
static int16_t trim28(int ev, int v)            /* before 29: a layer's Vary (0..100: Evolve times 0..1 to 50, to 3 at
                                                  * 100) as today's trim on Form's Vary (Evolve's value, kept): the
                                                  * amount it gave, Evolve x factor, as Vary + trim; 0 Hold */
{
    const float f = v <= 50 ? (float)v / 50.0f : 1.0f + (float)(v - 50) / 25.0f;
    int t;
    if (v <= 0)
        return -50;
    t = (int)((float)ev * (f - 1.0f) + 1000.5f) - 1000;   /* (rounded, half up: as the recipes were turned) */
    return (int16_t)(t < -49 ? -49 : t > 50 ? 50 : t);
}
static void vary28(int16_t *par, int old)       /* before 29: Evolve is Form's Vary as it was; the Varys it had, trims;
                                                  * those it hadn't (before 21, 22) 0, as Evolve alone turned the layers
                                                  * then (step 5b; step 5 had the recipes' trims of the time) */
{
    static const uint8_t VP[4] = {P_PVARY, P_BVARY, P_DVARY, P_NVARY};
    int k;
    par[P_CVARY] = (int16_t)(old >= 21 ? trim28(par[P_VARY], par[P_CVARY]) : 0);
    for (k = 0; k < 4; k++)
        par[VP[k]] = (int16_t)(old >= 22 ? trim28(par[P_VARY], par[VP[k]]) : 0);
}
static void lowcut27(int16_t *par)               /* (27's Low cut, 1..100: ~20 Hz .. ~600 Hz, 4.907 octaves over 99 steps;
                                                  * the Filter's high-pass, +1..+50: 20 Hz .. 2.5 kHz, 6.966 over 49) */
{
    static const uint8_t LC[6] = {P_CLOW, P_PLOW, P_BLOW, P_DLOW, P_NLOW, P_KLOW};
    int k;
    for (k = 0; k < 6; k++) {
        const int v = par[LC[k]];
        par[LC[k]] = (int16_t)(v <= 0 ? 0 : 1 + (int)((float)(v - 1) * (4.907f * 49.0f / (99.0f * 6.966f)) + 0.5f));
    }
}
static void cut21(int16_t *par, int piece)      /* before 22: the chord's Cutoff the knob itself; now the knob around
                                                  * the piece's own (its recipe's, P_CUTOFF): what you'd set, kept as
                                                  * the knob's offset from it (as far as the knob goes) */
{
    int16_t rec[P_NPARAMS];
    uint16_t pool;
    int d;
    en_piece(piece, rec, &pool);
    d = par[P_CUTOFF] - rec[P_CUTOFF];
    par[P_CCUT] = (int16_t)(d < -50 ? -50 : d > 50 ? 50 : d);
    par[P_CUTOFF] = rec[P_CUTOFF];
}
static void groove11(int16_t *par)              /* before 12: the groove a pattern (0..4: Drone, Offbeat, Dotted,
                                                 * Roller, Sync), now the knob's (Drone, Offbeat, Dotted, Sync, Roller) */
{
    static const int8_t TO[5] = {0, 1, 2, 4, 3};
    par[P_GROOVE] = TO[(unsigned)par[P_GROOVE] < 5u ? par[P_GROOVE] : 1];
    par[P_BNOTES] = 100;
}
static void groove12(int16_t *par)              /* 12: the groove a ladder (0 Drone .. 8 Roller, each step a pattern
                                                 * and how many of its notes), now a pattern and Notes */
{
    static const int8_t PAT[9] = {0, 1, 1, 2, 2, 3, 3, 4, 4};   /* (Drone, Thin off, Offbeat, Thin dot, Dotted, Thin
                                                                 * sync, Sync, Thin roll, Roller: the knob's patterns) */
    static const int8_t DENS[9] = {100, 55, 100, 60, 100, 65, 100, 60, 100};
    unsigned g = (unsigned)par[P_GROOVE] < 9u ? (unsigned)par[P_GROOVE] : 2u;
    par[P_GROOVE] = PAT[g];
    par[P_BNOTES] = DENS[g];
}
static void newpar(int from)                     /* the parameters a format lacked (from on), from the recipes */
{
    int16_t rec[P_NPARAMS];
    uint16_t pool;
    int i, p;
    for (i = 0; i < NPIECES; i++) {
        en_piece(i, rec, &pool);
        for (p = from; p < P_NPARAMS; p++)
            proj.slot[i].par[p] = rec[p];
    }
    for (p = from; p < P_NPARAMS; p++)
        proj.par[p] = proj.slot[proj.piece < NPIECES ? proj.piece : 0].par[p];
}
static void noise10(void)                       /* before 11: each piece's noise as its recipe has it */
{
    int16_t rec[P_NPARAMS];
    uint16_t pool;
    int i, p;
    for (i = 0; i < NPIECES; i++) {
        en_piece(i, rec, &pool);
        for (p = P_NCOL; p < NPAR12; p++)
            proj.slot[i].par[p] = rec[p];
    }
    for (p = P_NCOL; p < NPAR12; p++)
        proj.par[p] = proj.slot[proj.piece < NPIECES ? proj.piece : 0].par[p];
}

static void nz17(void)                          /* before 18: the noise's Density and Air, -50..50 (the character's
                                                 * offset on them), as 0..100 */
{
    int i;
    proj.par[P_NGRAIN] = (int16_t)en_nz_dens18(proj.par[P_NGRAIN], proj.par[P_NCHAR]);
    proj.par[P_NROOM] = (int16_t)en_nz_air18(proj.par[P_NROOM], proj.par[P_NCHAR]);
    for (i = 0; i < NPIECES; i++) {
        proj.slot[i].par[P_NGRAIN] = (int16_t)en_nz_dens18(proj.slot[i].par[P_NGRAIN], proj.slot[i].par[P_NCHAR]);
        proj.slot[i].par[P_NROOM] = (int16_t)en_nz_air18(proj.slot[i].par[P_NROOM], proj.slot[i].par[P_NCHAR]);
    }
}

static void nlfo18(void)                        /* before 19: the noise's LFO deeper a step (0.67 of the tone at 100,
                                                 * now 0.27): as deep as it was */
{
    int i;
    proj.par[P_NLFO] = (int16_t)en_nlfo18(proj.par[P_NLFO]);
    for (i = 0; i < NPIECES; i++)
        proj.slot[i].par[P_NLFO] = (int16_t)en_nlfo18(proj.slot[i].par[P_NLFO]);
}

static void walk19(int16_t *par)                 /* before 20: Drift a rate (6); now its own knob, the wave over it */
{
    if (par[P_CLRATE] >= EN_LFO_DRIFT)
        par[P_CLWALK] = par[P_CFDEC], par[P_CFDEC] = 0, par[P_CLRATE] = 2;
    if (par[P_NLRATE] >= EN_LFO_DRIFT)
        par[P_NLWALK] = par[P_NLFO], par[P_NLFO] = 0, par[P_NLRATE] = 2;
}

static void gaps9(int16_t *par)                  /* before 10: an empty slot is a gap (its root, left as it was, is not
                                                  * a degree of the walk's) */
{
    int i;
    for (i = 0; i < EN_NPROG; i++)
        if (!par[P_CQ0 + i])
            par[P_CR0 + i] = 0, par[P_CX0 + i] = 0;
    par[P_PMODE] = par[P_PMODE] ? EN_PLOOP | EN_WALK_WANDER : EN_WALK_WANDER;   /* Prog: Loop; Random: the walk, now Wander */
}

static void shape30(int16_t *par)                /* (format 30, step 2's test builds) a Shape as a Wave */
{
    par[P_CLWAVE] = (int16_t)(par[P_CLWAVE] == 4 ? 0 : 50);
    par[P_NLWAVE] = (int16_t)(par[P_NLWAVE] == 4 ? 0 : 50);
}

int project_load(void)
{
    project_t *const pbuf = &app_scratch.load;   /* (the boot's buffer, lent by the scratch: app.h) */
    int n, old = 0, i, old30 = 0, old31 = 0;
    uint32_t fmt_in;                             /* (format 34: the save's own format, for the sirens' knobs) */
    uint8_t fresh[NPIECES];                      /* (a piece never changed: today's recipe) */
    memset(fresh, 0, sizeof fresh);
    memset(pbuf, 0, sizeof *pbuf);
    n = plat_store_load(OBJ_PROJ, pbuf, sizeof *pbuf);
    fmt_in = n >= 8 && pbuf->magic == PROJ_MAGIC ? pbuf->format : 0u;
    if (n >= 8 && pbuf->magic == PROJ_MAGIC && pbuf->format == 8u && n >= (int)sizeof(project8_t)) {
        for (i = 0; i < NPIECES; i++) {
            const project8_t *o = (const project8_t *)(const void *)pbuf;
            fresh[i] = (uint8_t)untouched_raw(o->slot[i].par, NPAR10, o->slot[i].pool, i);
        }
        from8((const project8_t *)(const void *)pbuf);
        old = 9;
    } else if (n >= 8 && pbuf->magic == PROJ_MAGIC && (pbuf->format == 9u || pbuf->format == 10u) &&
               n >= (int)sizeof(project10_t)) {
        old = (int)pbuf->format;
        for (i = 0; i < NPIECES; i++) {
            const project10_t *o = (const project10_t *)(const void *)pbuf;
            fresh[i] = (uint8_t)untouched_raw(o->slot[i].par, NPAR10, o->slot[i].pool, i);
        }
        from10((const project10_t *)(const void *)pbuf);
    } else if (n >= 8 && pbuf->magic == PROJ_MAGIC && (pbuf->format == 11u || pbuf->format == 12u) &&
               n >= (int)sizeof(project12_t)) {
        old = (int)pbuf->format;
        for (i = 0; i < NPIECES; i++) {
            const project12_t *o = (const project12_t *)(const void *)pbuf;
            fresh[i] = (uint8_t)untouched_raw(o->slot[i].par, NPAR12, o->slot[i].pool, i);
        }
        from12((const project12_t *)(const void *)pbuf);
    } else if (n >= 8 && pbuf->magic == PROJ_MAGIC && pbuf->format == 16u && n >= (int)sizeof(project16_t)) {
        old = 16;
        for (i = 0; i < NPIECES; i++) {
            const project16_t *o = (const project16_t *)(const void *)pbuf;
            fresh[i] = (uint8_t)untouched_raw(o->slot[i].par, NPAR16, o->slot[i].pool, i);
        }
        from16((const project16_t *)(const void *)pbuf);
    } else if (n >= 8 && pbuf->magic == PROJ_MAGIC && pbuf->format == 15u && n >= (int)sizeof(project15_t)) {
        old = 15;
        for (i = 0; i < NPIECES; i++) {
            const project15_t *o = (const project15_t *)(const void *)pbuf;
            fresh[i] = (uint8_t)untouched_raw(o->slot[i].par, NPAR15, o->slot[i].pool, i);
        }
        from15((const project15_t *)(const void *)pbuf);
    } else if (n >= 8 && pbuf->magic == PROJ_MAGIC && pbuf->format == 14u && n >= (int)sizeof(project14_t)) {
        old = 14;
        for (i = 0; i < NPIECES; i++) {
            const project14_t *o = (const project14_t *)(const void *)pbuf;
            fresh[i] = (uint8_t)untouched_raw(o->slot[i].par, NPAR14, o->slot[i].pool, i);
        }
        from14((const project14_t *)(const void *)pbuf);
    } else if (n >= 8 && pbuf->magic == PROJ_MAGIC && pbuf->format == 13u && n >= (int)sizeof(project13_t)) {
        old = 13;
        for (i = 0; i < NPIECES; i++) {
            const project13_t *o = (const project13_t *)(const void *)pbuf;
            fresh[i] = (uint8_t)untouched_raw(o->slot[i].par, NPAR13, o->slot[i].pool, i);
        }
        from13((const project13_t *)(const void *)pbuf);
    } else if (n >= 8 && pbuf->magic == PROJ_MAGIC && (pbuf->format == 17u || pbuf->format == 18u) &&
               n >= (int)sizeof(project18_t)) {
        old = (int)pbuf->format;                   /* (17: as 18 laid out; the noise's Density and Air converted below) */
        for (i = 0; i < NPIECES; i++) {
            const project18_t *o = (const project18_t *)(const void *)pbuf;
            fresh[i] = (uint8_t)untouched_raw(o->slot[i].par, NPAR18, o->slot[i].pool, i);
        }
        from18((const project18_t *)(const void *)pbuf);
    } else if (n >= 8 && pbuf->magic == PROJ_MAGIC && pbuf->format == 19u && n >= (int)sizeof(project19_t)) {
        old = 19;
        for (i = 0; i < NPIECES; i++) {
            const project19_t *o = (const project19_t *)(const void *)pbuf;
            fresh[i] = (uint8_t)untouched_raw(o->slot[i].par, NPAR19, o->slot[i].pool, i);
        }
        from19((const project19_t *)(const void *)pbuf);
    } else if (n >= 8 && pbuf->magic == PROJ_MAGIC && pbuf->format == 20u && n >= (int)sizeof(project20_t)) {
        old = 20;
        for (i = 0; i < NPIECES; i++) {
            const project20_t *o = (const project20_t *)(const void *)pbuf;
            fresh[i] = (uint8_t)untouched_raw(o->slot[i].par, NPAR20, o->slot[i].pool, i);
        }
        from20((const project20_t *)(const void *)pbuf);
    } else if (n >= 8 && pbuf->magic == PROJ_MAGIC && pbuf->format == 21u && n >= (int)sizeof(project21_t)) {
        old = 21;
        for (i = 0; i < NPIECES; i++) {
            const project21_t *o = (const project21_t *)(const void *)pbuf;
            fresh[i] = (uint8_t)untouched_raw(o->slot[i].par, NPAR21, o->slot[i].pool, i);
        }
        from21((const project21_t *)(const void *)pbuf);
    } else if (n >= 8 && pbuf->magic == PROJ_MAGIC && pbuf->format == 22u && n >= (int)sizeof(project22_t)) {
        old = 22;
        for (i = 0; i < NPIECES; i++) {
            const project22_t *o = (const project22_t *)(const void *)pbuf;
            fresh[i] = (uint8_t)untouched_raw(o->slot[i].par, NPAR22, o->slot[i].pool, i);
        }
        from22((const project22_t *)(const void *)pbuf);
    } else if (n >= 8 && pbuf->magic == PROJ_MAGIC && pbuf->format == 23u && n >= (int)sizeof(project23_t)) {
        old = 23;
        for (i = 0; i < NPIECES; i++) {
            const project23_t *o = (const project23_t *)(const void *)pbuf;
            fresh[i] = (uint8_t)untouched_raw(o->slot[i].par, NPAR23, o->slot[i].pool, i);
        }
        from23((const project23_t *)(const void *)pbuf);
    } else if (n >= 8 && pbuf->magic == PROJ_MAGIC && pbuf->format == 24u && n >= (int)sizeof(project24_t)) {
        old = 24;
        for (i = 0; i < NPIECES; i++) {
            const project24_t *o = (const project24_t *)(const void *)pbuf;
            fresh[i] = (uint8_t)untouched_raw(o->slot[i].par, NPAR24, o->slot[i].pool, i);
        }
        from24((const project24_t *)(const void *)pbuf);
    } else if (n >= 8 && pbuf->magic == PROJ_MAGIC && pbuf->format == 25u && n >= (int)sizeof(project25_t)) {
        old = 25;
        for (i = 0; i < NPIECES; i++) {
            const project25_t *o = (const project25_t *)(const void *)pbuf;
            fresh[i] = (uint8_t)untouched_raw(o->slot[i].par, NPAR25, o->slot[i].pool, i);
        }
        from25((const project25_t *)(const void *)pbuf);
    } else if (n >= 8 && pbuf->magic == PROJ_MAGIC && pbuf->format == 26u && n >= (int)sizeof(project26_t)) {
        old = 26;
        for (i = 0; i < NPIECES; i++) {
            const project26_t *o = (const project26_t *)(const void *)pbuf;
            fresh[i] = (uint8_t)untouched_raw(o->slot[i].par, NPAR26, o->slot[i].pool, i);
        }
        from26((const project26_t *)(const void *)pbuf);
    } else if (n >= 8 && pbuf->magic == PROJ_MAGIC && (pbuf->format == 27u || pbuf->format == 28u) &&
               n >= (int)sizeof(project28_t)) {
        old = (int)pbuf->format;                   /* (27, 28: as 28 laid out; 27's strips' Low cut, now their Filter) */
        for (i = 0; i < NPIECES; i++) {
            const project28_t *o = (const project28_t *)(const void *)pbuf;
            fresh[i] = (uint8_t)untouched_raw(o->slot[i].par, NPAR28, o->slot[i].pool, i);
        }
        from28((const project28_t *)(const void *)pbuf);
    } else if (n >= 8 && pbuf->magic == PROJ_MAGIC && pbuf->format == 29u && n >= (int)sizeof(project29_t)) {
        old = 29;
        for (i = 0; i < NPIECES; i++) {
            const project29_t *o = (const project29_t *)(const void *)pbuf;
            fresh[i] = (uint8_t)untouched_raw(o->slot[i].par, NPAR29, o->slot[i].pool, i);
        }
        from29((const project29_t *)(const void *)pbuf);
    } else {
        if (n >= 8 && pbuf->magic == PROJ_MAGIC && pbuf->format >= 30u && pbuf->format <= 34u) {   /* (30 to 34:
                                                  * 34's layout, a parameter less: the sweep's pace from the recipes,
                                                  * then as it was for a piece changed, below) */
            from34((const project34_t *)(const void *)pbuf);
            newpar(NPAR34);
            if (proj.format == 34u)
                proj.format = PROJ_FORMAT;
        } else {
            proj = *pbuf;
        }
        if (n >= 8 && proj.magic == PROJ_MAGIC && proj.format == 30u) {   /* (30: today's layout; its Motion and Mixer
                                                  * as 31's below, the pieces never changed known first, by their
                                                  * recipes then) */
            old30 = 1;
            for (i = 0; i < NPIECES; i++)
                fresh[i] = (uint8_t)untouched_now(&proj.slot[i], i);
            proj.format = PROJ_FORMAT;
        }
        if (n >= 8 && proj.magic == PROJ_MAGIC && proj.format == 32u) {   /* (32: today's layout, without the
                                                  * Transition: LP) */
            proj.trans = 0;
            proj.format = PROJ_FORMAT;
        }
        if (n >= 8 && proj.magic == PROJ_MAGIC && proj.format == 31u) {   /* (31: today's layout; the knobs with dead
                                                  * steps as 32's below, the pieces never changed known first, by their
                                                  * recipes then) */
            old31 = 1;
            for (i = 0; i < NPIECES; i++)
                fresh[i] = (uint8_t)untouched_now(&proj.slot[i], i);
            proj.format = PROJ_FORMAT;
        }
        if (n >= 8 && proj.magic == PROJ_MAGIC && proj.format == 32u)   /* (32: today's layout, the sirens' knobs
                                                  * after it unsaved: their defaults, below) */
            proj.format = PROJ_FORMAT;
        if (n >= 8 && proj.magic == PROJ_MAGIC && proj.format == 33u)   /* (33: the Transition's test build: today's
                                                  * layout, its Transition kept, the sirens' knobs after it unsaved:
                                                  * their defaults, below) */
            proj.format = PROJ_FORMAT;
        if (n >= 8 && proj.magic == PROJ_MAGIC && proj.format == 35u)   /* (35: today's layout; Env decay's slot the
                                                  * bass's old LFO's, unread: 0, below) */
            proj.format = PROJ_FORMAT;
        if (old30 && proj.rsv[0] != PROJ_WAVE_MARK) {
            /* (a step 2 test build's, zzo's or zzp's: the LFOs' Shape in the Wave's slots; the Shape Wander, the
             * wander alone, Wave 0, the waves half each, 50; a piece never changed, known by that build's recipe,
             * takes today's below) */
            for (i = 0; i < NPIECES; i++)
                fresh[i] = (uint8_t)untouched_raw(proj.slot[i].par, NPAR34, proj.slot[i].pool, i);
            shape30(proj.par);
            for (i = 0; i < NPIECES; i++)
                shape30(proj.slot[i].par);
            proj.rsv[0] = PROJ_WAVE_MARK;
        }
    }
    if (old && old < 30)                                     /* (the new ones, from the recipes) */
        newpar(old == 29 ? NPAR29 : old >= 27 ? NPAR28 : old == 26 ? NPAR26 : old == 25 ? NPAR25 : old == 24 ? NPAR24 : old == 23 ? NPAR23 : old == 22 ? NPAR22 : old == 21 ? NPAR21 : old == 20 ? NPAR20 : old == 19 ? NPAR19 : old >= 17 ? NPAR18 : old == 16 ? NPAR16 : old == 15 ? NPAR15 : old == 14 ? NPAR14 : old == 13 ? NPAR13
                                                                                                            : NPAR12);
    if (old == 19) {                             /* (19: Drift a rate; now its own knob) */
        walk19(proj.par);
        for (i = 0; i < NPIECES; i++)
            walk19(proj.slot[i].par);
    }
    if (old == 9) {                              /* (8 and 9: the chords' slots as 10 has them) */
        gaps9(proj.par);
        for (i = 0; i < NPIECES; i++)
            gaps9(proj.slot[i].par);
    }
    if (old && old <= 10)                        /* (before 11: the noise's own, from the recipes) */
        noise10();
    if (old && old <= 11) {                      /* (before 12: the groove a pattern in the engine's order) */
        groove11(proj.par);
        for (i = 0; i < NPIECES; i++)
            groove11(proj.slot[i].par);
    } else if (old == 12) {                      /* (12: the groove a ladder step) */
        groove12(proj.par);
        for (i = 0; i < NPIECES; i++)
            groove12(proj.slot[i].par);
    }
    if (old >= 11) {                             /* (before 18: the pieces never changed known first, by their recipes
                                                  * then; then the noise's Density and Air converted) */
        for (i = 0; i < NPIECES; i++)
            fresh[i] = (uint8_t)(fresh[i] || untouched_now(&proj.slot[i], i));
        if (old < 18)
            nz17();
    }
    if (old >= 16)                               /* (before 19, the noise's LFO saved: as deep as it was; the pieces
                                                  * never changed take today's recipe below) */
        nlfo18();
    if (old && old < 22) {                       /* (before 22: the chord's Cutoff as an offset from the piece's own;
                                                  * the pieces never changed known first, by their recipes then: they
                                                  * take today's below) */
        for (i = 0; i < NPIECES; i++) {
            if (old >= 11)
                fresh[i] = (uint8_t)(fresh[i] || untouched_now(&proj.slot[i], i));
            if (!fresh[i])
                cut21(proj.slot[i].par, i);
        }
        cut21(proj.par, proj.piece < NPIECES ? proj.piece : 0);
    }
    if (old == 23) {                             /* (23: the drums' Low cut on the kick too; now the Hats strip's alone:
                                                  * the kick keeps it, as its own strip's) */
        proj.par[P_KLOW] = proj.par[P_DLOW];
        for (i = 0; i < NPIECES; i++)
            proj.slot[i].par[P_KLOW] = proj.slot[i].par[P_DLOW];
    }
    if (old && old < 28) {                       /* (before 28: each strip's Low cut, 0..100; now its Filter, centred,
                                                  * the high-pass its right: the same cutoff, as near as a step goes) */
        lowcut27(proj.par);
        for (i = 0; i < NPIECES; i++)
            lowcut27(proj.slot[i].par);
    }
    if (old && old < 29) {                       /* (before 29: Evolve Form's Vary as it was, the layers' Varys trims on
                                                  * it, as near as they sounded; the looseness the recipe's, above) */
        vary28(proj.par, old);
        for (i = 0; i < NPIECES; i++)
            vary28(proj.slot[i].par, old);
    }
    if (old && old < 30) {                       /* (before 30: the LFOs' Depth and Drift and the Mixer's rides as Drift
                                                  * and the LFOs' Trims, as near as they sounded; the throws' amount the
                                                  * chord's then; the pieces never changed take today's recipe below) */
        en_move29(proj.par);
        for (i = 0; i < NPIECES; i++)
            en_move29(proj.slot[i].par);
    }
    if (old || old30) {                          /* (before 31: Motion the arrangement only, the Mixer's moves in it,
                                                  * as near as they sounded; the pieces never changed take today's
                                                  * recipe below) */
        for (i = 0; i < NPIECES; i++)            /* (a piece as a recipe before 31 had it, all of it: never changed) */
            fresh[i] = (uint8_t)(fresh[i] || untouched_now(&proj.slot[i], i));
        en_motion30(proj.par);
        for (i = 0; i < NPIECES; i++)
            en_motion30(proj.slot[i].par);
    }
    if (old || old30 || old31) {                 /* (before 32: the knobs whose steps did nothing rescaled, as near as
                                                  * they sounded; the pieces never changed take today's recipe below) */
        const int sv = !old ? P_NPARAMS : old >= 30 ? P_NPARAMS : old == 29 ? NPAR29 : old >= 27 ? NPAR28 : old == 26 ? NPAR26
                     : old == 25 ? NPAR25 : old == 24 ? NPAR24 : old == 23 ? NPAR23 : old == 22 ? NPAR22 : old == 21 ? NPAR21
                     : old == 20 ? NPAR20 : old == 19 ? NPAR19 : old >= 17 ? NPAR18 : old == 16 ? NPAR16 : old == 15 ? NPAR15
                     : old == 14 ? NPAR14 : old == 13 ? NPAR13 : old >= 11 ? NPAR12 : NPAR10;   /* (what the save had; the
                                                  * rest from today's recipes, newpar's and noise10's, left be) */
        for (i = 0; i < NPIECES; i++)            /* (a piece as a recipe before 32 had it, all of it: never changed) */
            fresh[i] = (uint8_t)(fresh[i] || untouched_now(&proj.slot[i], i));
        en_zones31(proj.par, sv);
        for (i = 0; i < NPIECES; i++)
            en_zones31(proj.slot[i].par, sv);
    }
    if (fmt_in >= 8u && fmt_in < 35u) {          /* (before 35: Vary and its trims on the new scale, as near as they
                                                  * sounded; a piece changed keeps the sweep's old pace; the pieces
                                                  * never changed known first, by their recipes then: today's below) */
        for (i = 0; i < NPIECES; i++)
            fresh[i] = (uint8_t)(fresh[i] || untouched_now(&proj.slot[i], i));
        en_vary34(proj.par);
        proj.par[P_SWEEP] = 0;
        for (i = 0; i < NPIECES; i++) {
            en_vary34(proj.slot[i].par);
            proj.slot[i].par[P_SWEEP] = 0;
        }
    }
    if (fmt_in >= 8u && fmt_in < 36u) {          /* (before 36, REPORT-stabenv: Env decay, P_CEDEC, in the bass's old
                                                  * LFO's slot, unread since 1.4.0: 0, the filter's envelope tied to
                                                  * Decay as it was; Env amount as saved. A piece never changed takes
                                                  * today's recipe below, its new envelope with it) */
        for (i = 0; i < NPIECES; i++)
            fresh[i] = (uint8_t)(fresh[i] || untouched_now(&proj.slot[i], i));
        proj.par[P_CEDEC] = 0;
        for (i = 0; i < NPIECES; i++)
            proj.slot[i].par[P_CEDEC] = 0;
    }
    if (n >= 8 && proj.magic == PROJ_MAGIC && proj.format == PROJ_FORMAT) {   /* (the ranges as they are now) */
        if (proj.trans >= EN_NTRANS || old || old30 || old31)
            proj.trans = 0;                      /* (before 33: LP, as they played) */
        proj.screen = ui_screen_ok(proj.screen);   /* (1.4.0: the echo trail and the fifths gone: the ripples) */
        for (i = 0; i < NPIECES; i++)
            proj.slot[i].screen = ui_screen_ok(proj.slot[i].screen);
        pars_clamp(proj.par);
        for (i = 0; i < NPIECES; i++)
            pars_clamp(proj.slot[i].par);
        for (i = 0; i < NPIECES; i++)            /* (never changed: today's recipe, its view kept) */
            if (fresh[i] || untouched_now(&proj.slot[i], i)) {
                uint8_t sc = proj.slot[i].screen;
                recipe(i, &proj.slot[i]);
                proj.slot[i].screen = sc;
                if (i == proj.piece)
                    take(&proj.slot[i]);
            }
    }
    if (fmt_in < 34u)                            /* (before 34: no sirens' knobs saved, whatever the layout: theirs as
                                                  * the sirens were) */
        sir_defaults();
    sir_clamp();
    if (n >= 8 && project_valid())
        return 0;
    project_defaults();
    return -1;
}

int project_save(void)
{
    keep();
    proj.rsv[0] = PROJ_WAVE_MARK;
    return plat_store_save(OBJ_PROJ, &proj, sizeof proj);
}

void project_piece(int i)
{
    keep();
    proj.piece = (uint8_t)((i % NPIECES + NPIECES) % NPIECES);
    take(&proj.slot[proj.piece]);
}

void project_piece_reset(void)
{
    slot_t s;
    recipe(proj.piece, &s);
    s.screen = proj.screen;                      /* (the view stays as it is) */
    take(&s);
    proj.slot[proj.piece].pings_ok = 0;          /* (its loops grown afresh too) */
}

void project_apply(void)                         /* (then en_regrow keeps the piece's loops, if it has them) */
{
    int i;
    const slot_t *s = &proj.slot[proj.piece];
    for (i = 0; i < P_NPARAMS; i++)
        en_set(i, proj.par[i]);
    for (i = 0; i < EN_NSIR * EN_NSK; i++)       /* (format 34: the sirens' knobs, global) */
        en_sir_set(i / EN_NSK, i % EN_NSK, proj.sir[i / EN_NSK][i % EN_NSK]);
    en_pool(proj.pool);
    en_pings(s->pings_ok ? s->pings : 0);
}
