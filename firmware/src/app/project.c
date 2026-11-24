/* SPDX-License-Identifier: GPL-3.0-only */
/* DUB-1 project: (1.5.0, format 37) the WORK object in the frame store (storage.c's fs_*, through plat.h), its
 * bytes a container (container.c); before, one flash object (OBJ_PROJ, formats 8 to 36): read at the first boot of
 * 1.5.0 and migrated, never written again. A project of another format, or none, gives the defaults. Part of the
 * unity build, after app.h.
 *
 * The pieces (their recipes are the engine's, en_piece): each keeps its own settings as you leave it, and (37) its
 * name, a set's; Tune, MIDI out and Song mode are global; so are the dub sirens' knobs (format 34). */
project_t proj;
app_scratch_t app_scratch;
#include "container.c"                           /* (1.5.0) the container's codec: a stored project's bytes */

const char *piece_name(int i)                    /* (format 37) its name, a set's; none: its recipe's */
{
    const int k = (unsigned)i < NPIECES ? i : 0;
    return proj.slot[k].name[0] ? proj.slot[k].name : en_piece_name(proj.slot[k].recipe);
}
int piece_recipe(int i) { return proj.slot[(unsigned)i < NPIECES ? i : 0].recipe; }   /* (format 37) the recipe piece i
                                                  * came from (a set may put any in any slot) */

/* (format 37) a name as the screens and the reader take it (names.h: the characters, and no wider than the header's
 * tag holds beside its phrase), then NULs to the field's end (n bytes). 1: good */
#include "names.h"
_Static_assert(NAME_CHARS == PROJ_NAME, "names.h's backstop is the project's field");
static int name_ok(const char *f, uint32_t n)
{
    uint32_t px;
    return name_fits(f, n, &px);
}
static void name_take(char *d, const char *f)   /* a field checked by name_ok into a name */
{
    uint32_t i;
    for (i = 0; i < PROJ_NAME && f[i]; i++)
        d[i] = f[i];
    d[i] = 0;
}

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
    s->recipe = (uint8_t)i;
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
_Static_assert(sizeof(project36_t) <= 3840u, "1.4's project fit one storage sector's payload");
_Static_assert(sizeof(project_t) <= sizeof(float) * 2u * EN_SPEC_N, "(1.5.0) the project decoded in the scratch, which "
               "the spectrum's FFT sizes: no RAM more");

static void take_to(project_t *p, const slot_t *s)   /* a piece's settings into play, the globals kept */
{
    int i;
    for (i = 0; i < P_NPARAMS; i++)
        if (!global_par(i))
            p->par[i] = s->par[i];
    p->pool = s->pool;
    p->screen = ui_screen_ok(s->screen);         /* (each piece its own view; one gone, the ripples) */
}
static void take(const slot_t *s) { take_to(&proj, s); }

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

static void sir_defaults(project_t *p)           /* (format 34) the sirens' knobs as the sirens were before them */
{
    int s, k;
    for (s = 0; s < EN_NSIR; s++)
        for (k = 0; k < EN_NSK; k++)
            p->sir[s][k] = (int8_t)en_sir_def(s, k);
}
static void sir_clamp(project_t *p)              /* (a value past its range: to its end) */
{
    int s, k, lo, hi;
    for (s = 0; s < EN_NSIR; s++)
        for (k = 0; k < EN_NSK; k++) {
            en_sir_range(s, k, &lo, &hi);
            if (p->sir[s][k] < lo || p->sir[s][k] > hi)
                p->sir[s][k] = (int8_t)(p->sir[s][k] < lo ? lo : hi);
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
    sir_defaults(&proj);
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
static int project_valid(const project_t *p)    /* an older format: the recipes again */
{
    int i;
    if (p->magic != PROJ_MAGIC || !pool_ok(p->pool) || !pars_ok(p->par))
        return 0;
    if (p->format != PROJ_FORMAT || p->piece >= NPIECES || p->setlen > NPIECES)
        return 0;
    for (i = 0; i < NPIECES; i++)
        if (!pool_ok(p->slot[i].pool) || !pars_ok(p->slot[i].par) || p->slot[i].recipe >= NPIECES)
            return 0;
    return 1;
}

static void from8(project_t *d, const project8_t *o)          /* 1.0.0's project: everything kept, the pings grown afresh */
{
    int i;
    memset(d, 0, sizeof *d);
    d->magic = o->magic;
    d->format = o->format;
    memcpy(d->par, o->par, sizeof o->par);
    d->pool = o->pool;
    d->leds = o->leds, d->piece = o->piece, d->screen = o->screen;
    for (i = 0; i < NPIECES; i++) {
        memcpy(d->slot[i].par, o->slot[i].par, sizeof o->slot[i].par);
        d->slot[i].pool = o->slot[i].pool;
    }
}
static void from10(project_t *d, const project10_t *o)        /* 1.1.0 to 1.3.0's chords: everything kept */
{
    int i;
    memset(d, 0, sizeof *d);
    d->magic = o->magic;
    d->format = o->format;
    memcpy(d->par, o->par, sizeof o->par);
    d->pool = o->pool;
    d->leds = o->leds, d->piece = o->piece, d->screen = o->screen;
    for (i = 0; i < NPIECES; i++) {
        memcpy(d->slot[i].par, o->slot[i].par, sizeof o->slot[i].par);
        d->slot[i].pool = o->slot[i].pool;
        memcpy(d->slot[i].pings, o->slot[i].pings, sizeof d->slot[i].pings);
        d->slot[i].pings_ok = o->slot[i].pings_ok;
        d->slot[i].screen = o->slot[i].screen;
    }
}
static void from12(project_t *d, const project12_t *o)        /* 1.3.0's noise and ladder (11, 12): everything kept */
{
    int i;
    memset(d, 0, sizeof *d);
    d->magic = o->magic;
    d->format = o->format;
    memcpy(d->par, o->par, sizeof o->par);
    d->pool = o->pool;
    d->leds = o->leds, d->piece = o->piece, d->screen = o->screen;
    for (i = 0; i < NPIECES; i++) {
        memcpy(d->slot[i].par, o->slot[i].par, sizeof o->slot[i].par);
        d->slot[i].pool = o->slot[i].pool;
        memcpy(d->slot[i].pings, o->slot[i].pings, sizeof d->slot[i].pings);
        d->slot[i].pings_ok = o->slot[i].pings_ok;
        d->slot[i].screen = o->slot[i].screen;
    }
}
static void from13(project_t *d, const project13_t *o)        /* 1.3.0's pages (13): everything kept */
{
    int i;
    memset(d, 0, sizeof *d);
    d->magic = o->magic;
    d->format = o->format;
    memcpy(d->par, o->par, sizeof o->par);
    d->pool = o->pool;
    d->leds = o->leds, d->piece = o->piece, d->screen = o->screen;
    for (i = 0; i < NPIECES; i++) {
        memcpy(d->slot[i].par, o->slot[i].par, sizeof o->slot[i].par);
        d->slot[i].pool = o->slot[i].pool;
        memcpy(d->slot[i].pings, o->slot[i].pings, sizeof d->slot[i].pings);
        d->slot[i].pings_ok = o->slot[i].pings_ok;
        d->slot[i].screen = o->slot[i].screen;
    }
}
static void from14(project_t *d, const project14_t *o)        /* 1.3.0's echo and density (14): everything kept */
{
    int i;
    memset(d, 0, sizeof *d);
    d->magic = o->magic;
    d->format = o->format;
    memcpy(d->par, o->par, sizeof o->par);
    d->pool = o->pool;
    d->leds = o->leds, d->piece = o->piece, d->screen = o->screen;
    for (i = 0; i < NPIECES; i++) {
        memcpy(d->slot[i].par, o->slot[i].par, sizeof o->slot[i].par);
        d->slot[i].pool = o->slot[i].pool;
        memcpy(d->slot[i].pings, o->slot[i].pings, sizeof d->slot[i].pings);
        d->slot[i].pings_ok = o->slot[i].pings_ok;
        d->slot[i].screen = o->slot[i].screen;
    }
}
static void from15(project_t *d, const project15_t *o)        /* 1.3.0's Noise space (15): everything kept */
{
    int i;
    memset(d, 0, sizeof *d);
    d->magic = o->magic;
    d->format = o->format;
    memcpy(d->par, o->par, sizeof o->par);
    d->pool = o->pool;
    d->leds = o->leds, d->piece = o->piece, d->screen = o->screen;
    for (i = 0; i < NPIECES; i++) {
        memcpy(d->slot[i].par, o->slot[i].par, sizeof o->slot[i].par);
        d->slot[i].pool = o->slot[i].pool;
        memcpy(d->slot[i].pings, o->slot[i].pings, sizeof d->slot[i].pings);
        d->slot[i].pings_ok = o->slot[i].pings_ok;
        d->slot[i].screen = o->slot[i].screen;
    }
}
static void from16(project_t *d, const project16_t *o)        /* 1.3.0's noise LFO (16): everything kept */
{
    int i;
    memset(d, 0, sizeof *d);
    d->magic = o->magic;
    d->format = o->format;
    memcpy(d->par, o->par, sizeof o->par);
    d->pool = o->pool;
    d->leds = o->leds, d->piece = o->piece, d->screen = o->screen;
    for (i = 0; i < NPIECES; i++) {
        memcpy(d->slot[i].par, o->slot[i].par, sizeof o->slot[i].par);
        d->slot[i].pool = o->slot[i].pool;
        memcpy(d->slot[i].pings, o->slot[i].pings, sizeof d->slot[i].pings);
        d->slot[i].pings_ok = o->slot[i].pings_ok;
        d->slot[i].screen = o->slot[i].screen;
    }
}
static void from18(project_t *d, const project18_t *o)        /* 1.4.0's test builds (17, 18): everything kept */
{
    int i;
    memset(d, 0, sizeof *d);
    d->magic = o->magic;
    d->format = o->format;
    memcpy(d->par, o->par, sizeof o->par);
    d->pool = o->pool;
    d->leds = o->leds, d->piece = o->piece, d->screen = o->screen;
    for (i = 0; i < NPIECES; i++) {
        memcpy(d->slot[i].par, o->slot[i].par, sizeof o->slot[i].par);
        d->slot[i].pool = o->slot[i].pool;
        memcpy(d->slot[i].pings, o->slot[i].pings, sizeof d->slot[i].pings);
        d->slot[i].pings_ok = o->slot[i].pings_ok;
        d->slot[i].screen = o->slot[i].screen;
    }
}
static void from19(project_t *d, const project19_t *o)        /* 1.4.0's test builds (19): everything kept */
{
    int i;
    memset(d, 0, sizeof *d);
    d->magic = o->magic;
    d->format = o->format;
    memcpy(d->par, o->par, sizeof o->par);
    d->pool = o->pool;
    d->leds = o->leds, d->piece = o->piece, d->screen = o->screen;
    for (i = 0; i < NPIECES; i++) {
        memcpy(d->slot[i].par, o->slot[i].par, sizeof o->slot[i].par);
        d->slot[i].pool = o->slot[i].pool;
        memcpy(d->slot[i].pings, o->slot[i].pings, sizeof d->slot[i].pings);
        d->slot[i].pings_ok = o->slot[i].pings_ok;
        d->slot[i].screen = o->slot[i].screen;
    }
}
static void from20(project_t *d, const project20_t *o)        /* 1.4.0's LFO pages (20): everything kept */
{
    int i;
    memset(d, 0, sizeof *d);
    d->magic = o->magic;
    d->format = o->format;
    memcpy(d->par, o->par, sizeof o->par);
    d->pool = o->pool;
    d->leds = o->leds, d->piece = o->piece, d->screen = o->screen;
    for (i = 0; i < NPIECES; i++) {
        memcpy(d->slot[i].par, o->slot[i].par, sizeof o->slot[i].par);
        d->slot[i].pool = o->slot[i].pool;
        memcpy(d->slot[i].pings, o->slot[i].pings, sizeof d->slot[i].pings);
        d->slot[i].pings_ok = o->slot[i].pings_ok;
        d->slot[i].screen = o->slot[i].screen;
    }
}
static void from21(project_t *d, const project21_t *o)        /* 1.4.0's Chord pages (21): everything kept */
{
    int i;
    memset(d, 0, sizeof *d);
    d->magic = o->magic;
    d->format = o->format;
    memcpy(d->par, o->par, sizeof o->par);
    d->pool = o->pool;
    d->leds = o->leds, d->piece = o->piece, d->screen = o->screen;
    for (i = 0; i < NPIECES; i++) {
        memcpy(d->slot[i].par, o->slot[i].par, sizeof o->slot[i].par);
        d->slot[i].pool = o->slot[i].pool;
        memcpy(d->slot[i].pings, o->slot[i].pings, sizeof d->slot[i].pings);
        d->slot[i].pings_ok = o->slot[i].pings_ok;
        d->slot[i].screen = o->slot[i].screen;
    }
}
static void from22(project_t *d, const project22_t *o)        /* 1.4.0's layers' pages (22): everything kept */
{
    int i;
    memset(d, 0, sizeof *d);
    d->magic = o->magic;
    d->format = o->format;
    memcpy(d->par, o->par, sizeof o->par);
    d->pool = o->pool;
    d->leds = o->leds, d->piece = o->piece, d->screen = o->screen;
    for (i = 0; i < NPIECES; i++) {
        memcpy(d->slot[i].par, o->slot[i].par, sizeof o->slot[i].par);
        d->slot[i].pool = o->slot[i].pool;
        memcpy(d->slot[i].pings, o->slot[i].pings, sizeof d->slot[i].pings);
        d->slot[i].pings_ok = o->slot[i].pings_ok;
        d->slot[i].screen = o->slot[i].screen;
    }
}
static void from23(project_t *d, const project23_t *o)        /* 1.4.0's channel strips (23): everything kept */
{
    int i;
    memset(d, 0, sizeof *d);
    d->magic = o->magic;
    d->format = o->format;
    memcpy(d->par, o->par, sizeof o->par);
    d->pool = o->pool;
    d->leds = o->leds, d->piece = o->piece, d->screen = o->screen;
    for (i = 0; i < NPIECES; i++) {
        memcpy(d->slot[i].par, o->slot[i].par, sizeof o->slot[i].par);
        d->slot[i].pool = o->slot[i].pool;
        memcpy(d->slot[i].pings, o->slot[i].pings, sizeof d->slot[i].pings);
        d->slot[i].pings_ok = o->slot[i].pings_ok;
        d->slot[i].screen = o->slot[i].screen;
    }
}
static void from24(project_t *d, const project24_t *o)        /* 1.4.0's six strips (24): everything kept */
{
    int i;
    memset(d, 0, sizeof *d);
    d->magic = o->magic;
    d->format = o->format;
    memcpy(d->par, o->par, sizeof o->par);
    d->pool = o->pool;
    d->leds = o->leds, d->piece = o->piece, d->screen = o->screen;
    for (i = 0; i < NPIECES; i++) {
        memcpy(d->slot[i].par, o->slot[i].par, sizeof o->slot[i].par);
        d->slot[i].pool = o->slot[i].pool;
        memcpy(d->slot[i].pings, o->slot[i].pings, sizeof d->slot[i].pings);
        d->slot[i].pings_ok = o->slot[i].pings_ok;
        d->slot[i].screen = o->slot[i].screen;
    }
}
static void from25(project_t *d, const project25_t *o)        /* 1.4.0's noise LFO Destination (25): everything kept */
{
    int i;
    memset(d, 0, sizeof *d);
    d->magic = o->magic;
    d->format = o->format;
    memcpy(d->par, o->par, sizeof o->par);
    d->pool = o->pool;
    d->leds = o->leds, d->piece = o->piece, d->screen = o->screen;
    for (i = 0; i < NPIECES; i++) {
        memcpy(d->slot[i].par, o->slot[i].par, sizeof o->slot[i].par);
        d->slot[i].pool = o->slot[i].pool;
        memcpy(d->slot[i].pings, o->slot[i].pings, sizeof d->slot[i].pings);
        d->slot[i].pings_ok = o->slot[i].pings_ok;
        d->slot[i].screen = o->slot[i].screen;
    }
}
static void from26(project_t *d, const project26_t *o)        /* 1.4.0's song structure (26): everything kept */
{
    int i;
    memset(d, 0, sizeof *d);
    d->magic = o->magic;
    d->format = o->format;
    memcpy(d->par, o->par, sizeof o->par);
    d->pool = o->pool;
    d->leds = o->leds, d->piece = o->piece, d->screen = o->screen;
    for (i = 0; i < NPIECES; i++) {
        memcpy(d->slot[i].par, o->slot[i].par, sizeof o->slot[i].par);
        d->slot[i].pool = o->slot[i].pool;
        memcpy(d->slot[i].pings, o->slot[i].pings, sizeof d->slot[i].pings);
        d->slot[i].pings_ok = o->slot[i].pings_ok;
        d->slot[i].screen = o->slot[i].screen;
    }
}
static void from28(project_t *d, const project28_t *o)        /* 1.4.0's dub mixer and Filter (27, 28): everything kept */
{
    int i;
    memset(d, 0, sizeof *d);
    d->magic = o->magic;
    d->format = o->format;
    memcpy(d->par, o->par, sizeof o->par);
    d->pool = o->pool;
    d->leds = o->leds, d->piece = o->piece, d->screen = o->screen;
    for (i = 0; i < NPIECES; i++) {
        memcpy(d->slot[i].par, o->slot[i].par, sizeof o->slot[i].par);
        d->slot[i].pool = o->slot[i].pool;
        memcpy(d->slot[i].pings, o->slot[i].pings, sizeof d->slot[i].pings);
        d->slot[i].pings_ok = o->slot[i].pings_ok;
        d->slot[i].screen = o->slot[i].screen;
    }
}
static void from29(project_t *d, const project29_t *o)        /* the variety plan's step 1 (29): everything kept */
{
    int i;
    memset(d, 0, sizeof *d);
    d->magic = o->magic;
    d->format = o->format;
    memcpy(d->par, o->par, sizeof o->par);
    d->pool = o->pool;
    d->leds = o->leds, d->piece = o->piece, d->screen = o->screen;
    for (i = 0; i < NPIECES; i++) {
        memcpy(d->slot[i].par, o->slot[i].par, sizeof o->slot[i].par);
        d->slot[i].pool = o->slot[i].pool;
        memcpy(d->slot[i].pings, o->slot[i].pings, sizeof d->slot[i].pings);
        d->slot[i].pings_ok = o->slot[i].pings_ok;
        d->slot[i].screen = o->slot[i].screen;
    }
}
static void from34(project_t *d, const project34_t *o)        /* (30 to 34) everything kept, the format too (read on below as it
                                                  * is); the sweep's pace from the recipes (newpar) */
{
    int i;
    memset(d, 0, sizeof *d);
    d->magic = o->magic;
    d->format = o->format;
    memcpy(d->par, o->par, sizeof o->par);
    d->pool = o->pool;
    d->leds = o->leds, d->piece = o->piece, d->screen = o->screen, d->values = o->values;
    for (i = 0; i < NPIECES; i++) {
        memcpy(d->slot[i].par, o->slot[i].par, sizeof o->slot[i].par);
        d->slot[i].pool = o->slot[i].pool;
        memcpy(d->slot[i].pings, o->slot[i].pings, sizeof d->slot[i].pings);
        d->slot[i].pings_ok = o->slot[i].pings_ok;
        d->slot[i].screen = o->slot[i].screen;
    }
    memcpy(d->rsv, o->rsv, sizeof d->rsv);
    d->trans = o->trans;
    memcpy(d->sir, o->sir, sizeof d->sir);
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
static void newpar(int16_t *par, int piece, int from)   /* the parameters a format lacked (from on), from the
                                                  * piece's recipe */
{
    int16_t rec[P_NPARAMS];
    uint16_t pool;
    int p;
    en_piece(piece, rec, &pool);
    for (p = from; p < P_NPARAMS; p++)
        par[p] = rec[p];
}
static void noise10(int16_t *par, int piece)    /* before 11: the piece's noise as its recipe has it */
{
    int16_t rec[P_NPARAMS];
    uint16_t pool;
    int p;
    en_piece(piece, rec, &pool);
    for (p = P_NCOL; p < NPAR12; p++)
        par[p] = rec[p];
}

static void nz17(int16_t *par)                  /* before 18: the noise's Density and Air, -50..50 (the character's
                                                 * offset on them), as 0..100 */
{
    par[P_NGRAIN] = (int16_t)en_nz_dens18(par[P_NGRAIN], par[P_NCHAR]);
    par[P_NROOM] = (int16_t)en_nz_air18(par[P_NROOM], par[P_NCHAR]);
}

static void nlfo18(int16_t *par)                /* before 19: the noise's LFO deeper a step (0.67 of the tone at 100,
                                                 * now 0.27): as deep as it was */
{
    par[P_NLFO] = (int16_t)en_nlfo18(par[P_NLFO]);
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

/* (1.5.0) the upgrade a piece at a time: what project_load did to a whole project (every slot, then the playing
 * values, step by step), now slot_upgrade on one piece, the same steps in the same order, so a piece read from a
 * container (a stored set, a .dub1 file, made by an older DUB-1) is brought to today's exactly as a 1.4 save is */
typedef struct {
    uint32_t fmt;                                /* the save's format (8 .. PROJ_FORMAT) */
    int npar;                                    /* the parameters it had (npar_of) */
    int wave;                                    /* (format 30) PROJ_WAVE_MARK there: its LFOs' Wave in layout B */
} upg_t;

static int npar_of(uint32_t f)                   /* the parameters format f had; 0: no format DUB-1 wrote */
{
    return f < 8u ? 0 : f <= 10u ? NPAR10 : f <= 12u ? NPAR12 : f == 13u ? NPAR13 : f == 14u ? NPAR14 : f == 15u ? NPAR15
         : f == 16u ? NPAR16 : f <= 18u ? NPAR18 : f == 19u ? NPAR19 : f == 20u ? NPAR20 : f == 21u ? NPAR21
         : f == 22u ? NPAR22 : f == 23u ? NPAR23 : f == 24u ? NPAR24 : f == 25u ? NPAR25 : f == 26u ? NPAR26
         : f <= 28u ? NPAR28 : f == 29u ? NPAR29 : f <= 34u ? NPAR34 : f <= 36u ? NPAR36 : f <= PROJ_FORMAT ? P_NPARAMS : 0;
}

/* one piece's values from format u->fmt to today's: a slot's (sl, its par) or the playing ones (sl 0: no "never
 * changed" for them); piece: the recipe its new values come from. 1: the piece was never changed (it takes today's
 * recipe: proj_finish). (project_load's `old`: the formats before 30, 8 read as 9; 30 on, today's layout) */
static int par_upgrade(int16_t *par, const slot_t *sl, int piece, const upg_t *u)
{
    const uint32_t f = u->fmt;
    const int old = f == 8u ? 9 : f < 30u ? (int)f : 0;
    int fresh = 0;
    if (old && sl)                               /* (the save as it was, its own recipes then) */
        fresh = untouched_raw(par, u->npar, sl->pool, piece);
    if (u->npar < P_NPARAMS)                     /* (the new ones, from the recipes) */
        newpar(par, piece, u->npar);
    if (sl && (f == 30u || f == 31u))            /* (30, 31: today's layout; the pieces never changed known first, by
                                                  * their recipes then) */
        fresh = untouched_now(sl, piece);
    if (f == 30u && !u->wave) {                  /* (a step 2 test build's, zzo's or zzp's: the LFOs' Shape in the
                                                  * Wave's slots; the Shape Wander, the wander alone, Wave 0, the waves
                                                  * half each, 50; a piece never changed, known by that build's recipe,
                                                  * takes today's) */
        if (sl)
            fresh = untouched_raw(par, NPAR34, sl->pool, piece);
        shape30(par);
    }
    if (old == 19)                               /* (19: Drift a rate; now its own knob) */
        walk19(par);
    if (old == 9)                                /* (8 and 9: the chords' slots as 10 has them) */
        gaps9(par);
    if (old && old <= 10)                        /* (before 11: the noise's own, from the recipes) */
        noise10(par, piece);
    if (old && old <= 11)                        /* (before 12: the groove a pattern in the engine's order) */
        groove11(par);
    else if (old == 12)                          /* (12: the groove a ladder step) */
        groove12(par);
    if (old >= 11) {                             /* (before 18: the pieces never changed known first, by their recipes
                                                  * then; then the noise's Density and Air converted) */
        if (sl)
            fresh |= untouched_now(sl, piece);
        if (old < 18)
            nz17(par);
    }
    if (old >= 16)                               /* (before 19, the noise's LFO saved: as deep as it was) */
        nlfo18(par);
    if (old && old < 22) {                       /* (before 22: the chord's Cutoff as an offset from the piece's own;
                                                  * the pieces never changed known first: they take today's) */
        if (sl && old >= 11)
            fresh |= untouched_now(sl, piece);
        if (!fresh)
            cut21(par, piece);
    }
    if (old == 23)                               /* (23: the drums' Low cut on the kick too; now the Hats strip's alone:
                                                  * the kick keeps it, as its own strip's) */
        par[P_KLOW] = par[P_DLOW];
    if (old && old < 28)                         /* (before 28: each strip's Low cut, 0..100; now its Filter, centred,
                                                  * the high-pass its right: the same cutoff, as near as a step goes) */
        lowcut27(par);
    if (old && old < 29)                         /* (before 29: Evolve Form's Vary as it was, the layers' Varys trims on
                                                  * it, as near as they sounded; the looseness the recipe's) */
        vary28(par, old);
    if (old && old < 30)                         /* (before 30: the LFOs' Depth and Drift and the Mixer's rides as Drift
                                                  * and the LFOs' Trims, as near as they sounded) */
        en_move29(par);
    if (old || f == 30u) {                       /* (before 31: Motion the arrangement only, the Mixer's moves in it, as
                                                  * near as they sounded; a piece as a recipe before 31 had it, all of
                                                  * it: never changed) */
        if (sl)
            fresh |= untouched_now(sl, piece);
        en_motion30(par);
    }
    if (old || f == 30u || f == 31u) {           /* (before 32: the knobs whose steps did nothing rescaled, as near as
                                                  * they sounded, over what the save had: the rest from today's
                                                  * recipes, left be) */
        if (sl)
            fresh |= untouched_now(sl, piece);
        en_zones31(par, f >= 30u ? P_NPARAMS : u->npar);
    }
    if (f < 35u) {                               /* (before 35: Vary and its trims on the new scale, as near as they
                                                  * sounded; a piece changed keeps the sweep's old pace) */
        if (sl)
            fresh |= untouched_now(sl, piece);
        en_vary34(par);
        par[P_SWEEP] = 0;
    }
    if (f < 36u) {                               /* (before 36, REPORT-stabenv: Env decay, P_CEDEC, in the bass's old
                                                  * LFO's slot, unread since 1.4.0: 0, the filter's envelope tied to
                                                  * Decay as it was; Env amount as saved) */
        if (sl)
            fresh |= untouched_now(sl, piece);
        par[P_CEDEC] = 0;
    }
    return fresh;
}
static int slot_upgrade(slot_t *s, int piece, const upg_t *u) { return par_upgrade(s->par, s, piece, u); }

/* the ranges as they are now; a piece never changed takes today's recipe (its view kept); the sirens' knobs */
static void proj_finish(project_t *p, const uint8_t *fresh, uint32_t fmt)
{
    int i;
    p->format = PROJ_FORMAT;
    p->rsv[0] = PROJ_WAVE_MARK;
    if (p->trans >= EN_NTRANS || fmt < 33u)      /* (before 33: LP, as they played) */
        p->trans = 0;
    p->screen = ui_screen_ok(p->screen);         /* (1.4.0: the echo trail and the fifths gone: the ripples) */
    for (i = 0; i < NPIECES; i++)
        p->slot[i].screen = ui_screen_ok(p->slot[i].screen);
    pars_clamp(p->par);
    for (i = 0; i < NPIECES; i++)
        pars_clamp(p->slot[i].par);
    for (i = 0; i < NPIECES; i++)                /* (never changed: today's recipe, its view kept; (37) its own recipe,
                                                  * a set may have moved it) */
        if (fresh[i] || untouched_now(&p->slot[i], p->slot[i].recipe)) {
            uint8_t sc = p->slot[i].screen;
            recipe(p->slot[i].recipe, &p->slot[i]);
            p->slot[i].screen = sc;
            if (i == p->piece)
                take_to(p, &p->slot[i]);
        }
    if (fmt < 34u)                               /* (before 34: no sirens' knobs saved, whatever the layout: theirs as
                                                  * the sirens were) */
        sir_defaults(p);
    sir_clamp(p);
}

static void proj_upgrade(project_t *p, const upg_t *u)   /* every piece, then the playing values, then the ranges */
{
    uint8_t fresh[NPIECES];
    int i;
    for (i = 0; i < NPIECES; i++)
        fresh[i] = (uint8_t)slot_upgrade(&p->slot[i], p->slot[i].recipe, u);
    par_upgrade(p->par, 0, p->slot[p->piece < NPIECES ? p->piece : 0].recipe, u);
    proj_finish(p, fresh, u->fmt);
}

static void from36(project_t *d, const project36_t *o)   /* (35, 36: 1.4's last layout) everything kept */
{
    int i;
    memset(d, 0, sizeof *d);
    d->magic = o->magic, d->format = o->format;
    memcpy(d->par, o->par, sizeof o->par);
    d->pool = o->pool, d->leds = o->leds, d->piece = o->piece, d->screen = o->screen, d->values = o->values;
    for (i = 0; i < NPIECES; i++) {
        memcpy(d->slot[i].par, o->slot[i].par, sizeof o->slot[i].par);
        d->slot[i].pool = o->slot[i].pool;
        memcpy(d->slot[i].pings, o->slot[i].pings, sizeof d->slot[i].pings);
        d->slot[i].pings_ok = o->slot[i].pings_ok;
        d->slot[i].screen = o->slot[i].screen;
    }
    memcpy(d->rsv, o->rsv, sizeof d->rsv);
    d->trans = o->trans;
    memcpy(d->sir, o->sir, sizeof d->sir);
}

/* a 1.4 save (OBJ_PROJ: formats 8 to 36, n bytes of it in b) read as it was laid out into d, nothing converted yet:
 * 0, and *u what proj_upgrade needs; -1: not one (no project, or a layout too short for its format) */
static int proj_parse_old(const void *b, int n, project_t *d, upg_t *u)
{
    const project36_t *const o = b;
    const uint32_t f = n >= 8 && o->magic == PROJ_MAGIC ? o->format : 0u;
#define FITS(t) (n >= (int)sizeof(t))
    if (f == 8u && FITS(project8_t))
        from8(d, b);
    else if ((f == 9u || f == 10u) && FITS(project10_t))
        from10(d, b);
    else if ((f == 11u || f == 12u) && FITS(project12_t))
        from12(d, b);
    else if (f == 13u && FITS(project13_t))
        from13(d, b);
    else if (f == 14u && FITS(project14_t))
        from14(d, b);
    else if (f == 15u && FITS(project15_t))
        from15(d, b);
    else if (f == 16u && FITS(project16_t))
        from16(d, b);
    else if ((f == 17u || f == 18u) && FITS(project18_t))   /* (17: as 18 laid out; the noise's Density and Air
                                                  * converted in the upgrade) */
        from18(d, b);
    else if (f == 19u && FITS(project19_t))
        from19(d, b);
    else if (f == 20u && FITS(project20_t))
        from20(d, b);
    else if (f == 21u && FITS(project21_t))
        from21(d, b);
    else if (f == 22u && FITS(project22_t))
        from22(d, b);
    else if (f == 23u && FITS(project23_t))
        from23(d, b);
    else if (f == 24u && FITS(project24_t))
        from24(d, b);
    else if (f == 25u && FITS(project25_t))
        from25(d, b);
    else if (f == 26u && FITS(project26_t))
        from26(d, b);
    else if ((f == 27u || f == 28u) && FITS(project28_t))   /* (27, 28: as 28 laid out; 27's strips' Low cut, now
                                                  * their Filter) */
        from28(d, b);
    else if (f == 29u && FITS(project29_t))
        from29(d, b);
    else if (f >= 30u && f <= 34u)               /* (30 to 34: 34's layout, a parameter less: the sweep's pace from the
                                                  * recipes; 32's saved short, without the sirens' knobs: zeros, their
                                                  * defaults in the upgrade) */
        from34(d, b);
    else if (f == 35u || f == 36u)
        from36(d, b);
    else
        return -1;
#undef FITS
    {
        int i;
        for (i = 0; i < NPIECES; i++)            /* (before 37: slot i recipe i) */
            d->slot[i].recipe = (uint8_t)i;
    }
    u->fmt = f, u->npar = npar_of(f), u->wave = f == 30u && d->rsv[0] == PROJ_WAVE_MARK;
    return 0;
}

/* ------------------------------------------------------------------------------------- the project's bytes --- */
/* (1.5.0) the project as a container (container.c): what the frame store's WORK holds, and a .dub1 file. Written in
 * two passes (counted, then for real: the total in the header first), read with every value checked and brought to
 * today's (slot_upgrade), into a project_t that is taken whole or not at all */
#ifdef EN_HOST
#define D1C_SRC D1C_SRC_HOST
#else
#define D1C_SRC D1C_SRC_DEVICE
#endif
static uint32_t work_mig;                        /* (META) OBJ_PROJ's seq when 1.4's save was migrated: the downgrade
                                                  * check's (0: none) */
static uint32_t work_saves;                      /* (META) the saves (a count) */
static uint8_t work_sets;                        /* (the header's D1C_F_SETS) the sets the FM-1 comes with offered on
                                                  * this unit (first_sets): once, never again */
static uint32_t work_rflags;                     /* (work_read) the header's flags of the WORK it read */

/* ------------------------------------------------------------------------------ the cues (1.5.0, step 7) --- */
/* A piece's cue list stays where it was read (a CUES chunk in the WORK frame, or in a stored set's frame after APPLY):
 * proj.cues[i] says where. Only the playing piece's list comes into RAM (ui.c's player, cues_read); a WORK save copies
 * each list from where it is into the new WORK (checked again on the way: a list that no longer reads, its frame
 * reused, is dropped, never written broken) and then points there. 64 cues a piece at most, 8 bytes each */
static int cue_ok(const d1c_cue_t *q)            /* a cue's op and its arg, val and len as that op takes them (app.h) */
{
    const int v = q->val, a = q->arg;
    int lo, hi;
    switch (q->op) {
    case CUE_MUTE:
    case CUE_SOLO: return a < EN_NPART && (v == 0 || v == 1) && !q->len;
    case CUE_MOVE: return a < CUE_NMOVE && !v && q->len;
    case CUE_FX: return a < EN_NFX && !v && q->len;
    case CUE_SIREN: return a < EN_NSIR && !v;
    case CUE_SIRK:
        if (a >= EN_NSIR * EN_NSK)
            return 0;
        en_sir_range(a / EN_NSK, a % EN_NSK, &lo, &hi);
        return v >= lo && v <= hi;
    case CUE_MACRO: return a < EN_NMAC && v >= -50 && v <= 50;
    case CUE_KNOB: return a < P_NPARAMS && !global_par(a) && v >= en_param_info(a)->lo && v <= en_param_info(a)->hi;
    case CUE_OUTRO:
    case CUE_NEXT: return !a && !v && !q->len;
    case CUE_LOCK:
    case CUE_HOLD: return !a && (v == 0 || v == 1) && !q->len;
    default: return 0;
    }
}
#define CUE_SRC_MEM 0x80u
#ifdef EN_HOST
static uint8_t cue_mem[PLAT_FS_MAX];             /* (the host's: a container read from memory, setin, dub1in, the web's
                                                  * hear a set) its copy, its cues read from here; cue_gen the copy's
                                                  * count, so a list of an older one is never read from a newer */
static uint8_t cue_gen;
#endif
static uint8_t dec_src;                          /* (proj_decode) where the container being read is (cuesrc_t's src) */
static int cue_get(const cuesrc_t *s, uint32_t i, uint8_t *b, uint32_t n)   /* cues i .. i+n-1's bytes: 0 */
{
#ifdef EN_HOST
    if (s->src & CUE_SRC_MEM) {
        const uint32_t at = s->off + D1C_CUE * i;
        if ((s->src & 0x7Fu) != cue_gen || at + D1C_CUE * n > sizeof cue_mem)
            return -1;
        memcpy(b, cue_mem + at, D1C_CUE * n);
        return 0;
    }
#endif
    if (!s->src || (s->src & CUE_SRC_MEM))
        return -1;
    return plat_fs_read((int)s->src - 1, s->off + D1C_CUE * i, b, D1C_CUE * n);
}
/* a list where it is, read and checked (each cue, the order): its count, 0 none or no longer readable. q: the cues
 * (up to CUE_MAX), or 0 (only checked, 8 at a time) */
static uint32_t cues_read(const cuesrc_t *s, d1c_cue_t *q)
{
    uint8_t b[8 * D1C_CUE];
    uint32_t i, k, j, last = 0;
    d1c_cue_t c;
    if (!s->src || !s->n || s->n > CUE_MAX)
        return 0;
    for (i = 0; i < s->n; i += k) {
        k = s->n - i > 8u ? 8u : s->n - i;
        if (cue_get(s, i, b, k))
            return 0;
        for (j = 0; j < k; j++) {
            d1c_cue_in(b + D1C_CUE * j, &c);
            if (c.pos < last || !cue_ok(&c))
                return 0;
            last = c.pos;
            if (q)
                q[i + j] = c;
        }
    }
    return s->n;
}
static uint16_t cue_at[NPIECES];                 /* (proj_emit) where each piece's cues went in the container written, */
static uint8_t cue_nw[NPIECES];                  /* and how many (0: none) */
static void cues_emit(d1c_w_t *w, const project_t *p, uint32_t n)   /* the CUES chunks of pieces 0 .. n-1 */
{
    uint8_t b[8 * D1C_CUE];
    uint32_t i, j, k, c;
    for (i = 0; i < NPIECES; i++)
        cue_nw[i] = 0;
    for (i = 0; i < n; i++) {
        if (!(c = cues_read(&p->cues[i], 0)))
            continue;
        d1c_cues_head(w, i, c, 0);
        cue_at[i] = (uint16_t)w->len, cue_nw[i] = (uint8_t)c;
        for (j = 0; j < c; j += k) {
            k = c - j > 8u ? 8u : c - j;
            if (cue_get(&p->cues[i], j, b, k))
                w->err = D1C_EIO;
            d1c_bytes(w, b, D1C_CUE * k);
        }
    }
}
static void cues_moved(project_t *p, int f)      /* (a WORK just committed in frame f) the lists now there */
{
    int i;
    for (i = 0; i < NPIECES; i++) {
        p->cues[i].src = cue_nw[i] ? (uint8_t)(f + 1) : 0u;
        p->cues[i].n = cue_nw[i], p->cues[i].off = cue_nw[i] ? cue_at[i] : 0u;
    }
}

/* the container of project p as format u->fmt with u->npar parameters (today's for a save; an old save's own, when the
 * host wraps one as it was: project_wrapcheck): a WORK (kind FS_WORK, all eight, GLOB), or (the host's, step 4) a SET
 * of its first n pieces, GLOB or not */
static void proj_emit(d1c_w_t *w, const project_t *p, const upg_t *u, uint32_t total, uint32_t kind, uint32_t n,
                      int glob)
{
    d1c_hdr_t h;
    d1c_meta_t m;
    d1c_glob_t g;
    d1c_piece_t pc;
    uint32_t i;
    memset(&h, 0, sizeof h);
    h.total = total, h.format = u->fmt, h.kind = (uint16_t)kind, h.npieces = (uint16_t)n;
    h.flags = (u->wave ? D1C_F_WAVE : 0u) | (kind == FS_WORK && p->cues_off ? D1C_F_NOCUES : 0u) |
              (kind == FS_WORK && work_sets ? D1C_F_SETS : 0u);
    d1c_header(w, &h);
    memset(&m, 0, sizeof m);
    d1c_name_set(m.name, p->name);
    d1c_name_set(m.fw, EN_VERSION);
    m.source = D1C_SRC, m.migrated = work_mig, m.saves = work_saves;
    d1c_meta(w, &m);
    memset(&g, 0, sizeof g);
    g.piece = p->piece, g.leds = p->leds, g.screen = p->screen, g.values = p->values, g.trans = p->trans;
    g.nsir = EN_NSIR, g.nsk = EN_NSK, g.setlen = p->setlen, g.pool = p->pool, g.npar = (uint16_t)u->npar;
    memcpy(g.sir, p->sir, sizeof p->sir);
    if (glob)
        d1c_glob(w, &g, p->par);
    for (i = 0; i < n; i++) {
        const slot_t *s = &p->slot[i];
        memset(&pc, 0, sizeof pc);
        pc.index = (uint8_t)i, pc.kind = D1C_FULL, pc.recipe = s->recipe, pc.screen = s->screen;
        pc.pings_ok = s->pings_ok, pc.nparams = (uint16_t)u->npar, pc.pool = s->pool, pc.npings = EN_PINGS_N;
        d1c_name_set(pc.name, s->name);
        d1c_piece(w, &pc, s->pings, s->par);
    }
    cues_emit(w, p, n);                          /* (step 7) */
    d1c_end(w);
}
static void work_emit(d1c_w_t *w, const project_t *p, const upg_t *u, uint32_t total)
{
    proj_emit(w, p, u, total, FS_WORK, NPIECES, 1);
}
static uint32_t work_len(const project_t *p, const upg_t *u)   /* (the counting pass) */
{
    d1c_w_t w;
    d1c_w_init(&w, 0, 0);
    work_emit(&w, p, u, 0);
    return w.len;
}

/* a container (r, opened: its shape and CRC good) into p: every chunk read, a piece's values brought from the
 * container's format to today's, every value in its range, the pieces all there. 0, or < 0 and p means nothing (the
 * caller takes p whole, or not at all: never half applied). meta: META's fields (zeros without one).
 * base 0: a WORK (the globals and all eight pieces). (step 4) base a project: a SET on it (p starts as *base): its
 * pieces into slots 0 .. n-1 (1 to 8 of them, each its own recipe), the rest of base's left as they were, Set mode
 * ending at its last (setlen); from its GLOB, if it has one, Song mode, the Transition, the sirens' knobs and Tune,
 * not MIDI out nor the player's (the lights, the knobs' numbers: the decisions); its first piece playing */
static int proj_decode(const d1c_r_t *r, project_t *p, d1c_meta_t *meta, const project_t *base)
{
    uint32_t at = 0, id, pay, len, seen = 0, npc = 0, k, cseen = 0;
    uint8_t fresh[NPIECES], full[NPIECES];
    int16_t gp[P_NPARAMS];                       /* (a set's GLOB: its playing values, Tune and Song mode taken) */
    int16_t *const gpar = base ? gp : p->par;
    int rc, i, glob = 0, hasmeta = 0;
    upg_t u;
    d1c_piece_t pc;
    d1c_glob_t g;
    if (base)
        *p = *base;
    else
        memset(p, 0, sizeof *p);
    p->name[0] = 0;
    memset(meta, 0, sizeof *meta);
    memset(full, 0, sizeof full);
    if (r->h.kind != (base ? FS_SET : FS_WORK))
        return D1C_ESHAPE;
    if (r->h.format > PROJ_FORMAT)
        return D1C_ENEWER;                       /* ("made for a newer DUB-1": META's version says which) */
    if (!npar_of(r->h.format))
        return D1C_ESHAPE;
    u.fmt = r->h.format, u.npar = npar_of(u.fmt), u.wave = u.fmt == 30u && (r->h.flags & D1C_F_WAVE);
    p->magic = PROJ_MAGIC, p->format = u.fmt;
    if (base && (r->h.npieces < 1u || r->h.npieces > NPIECES))
        return D1C_ESHAPE;
    while (d1c_next(r, &at, &id, &pay, &len)) {
        if (id == D1C_META) {
            if (hasmeta++)
                return D1C_ESHAPE;
            if ((rc = d1c_read_meta(r, pay, len, meta)) != 0)
                return rc;
            if (!name_ok(meta->name, D1C_NAME))
                return D1C_EBAD;
            name_take(p->name, meta->name);
        } else if (id == D1C_GLOB) {
            if (glob++)
                return D1C_ESHAPE;
            if ((rc = d1c_read_glob(r, pay, len, &g)) != 0)
                return rc;
            if (g.npar != u.npar)
                return g.npar > P_NPARAMS ? D1C_ENEWER : D1C_ESHAPE;
            if (g.nsir != EN_NSIR || g.nsk != EN_NSK)
                return D1C_ESHAPE;
            if ((rc = d1c_read_i16s(r, g.par_at, gpar, g.npar)) != 0)
                return rc;
            if (g.setlen > NPIECES)
                return D1C_EBAD;
            if (!base)
                p->piece = g.piece, p->leds = g.leds, p->screen = g.screen, p->values = g.values, p->pool = g.pool,
                p->setlen = g.setlen;
            p->trans = g.trans;
            memcpy(p->sir, g.sir, sizeof p->sir);
        } else if (id == D1C_PIEC) {
            slot_t *s;
            if ((rc = d1c_read_piece(r, pay, len, &pc)) != 0)
                return rc;
            if (pc.index >= NPIECES || (seen >> pc.index & 1u) || pc.recipe >= NPIECES)
                return D1C_EBAD;
            if (!name_ok(pc.name, D1C_NAME))
                return D1C_EBAD;
            seen |= 1u << pc.index, npc++;
            s = &p->slot[pc.index];
            if (pc.kind == D1C_FULL) {
                if (pc.nparams != u.npar)
                    return pc.nparams > P_NPARAMS ? D1C_ENEWER : D1C_ESHAPE;
                if (pc.npings != EN_PINGS_N || pc.pings_ok > 1u)
                    return D1C_EBAD;
                if (d1c_get(r, pc.pings_at, s->pings, EN_PINGS_N) || d1c_read_i16s(r, pc.data_at, s->par, pc.nparams))
                    return D1C_EIO;
                s->pool = pc.pool, s->pings_ok = pc.pings_ok, s->screen = pc.screen, s->recipe = pc.recipe;
                full[pc.index] = 1;
            } else {                             /* (today's recipe; a DELTA's changes on it: today's format only, an older
                                                  * one's base can't be known) */
                if (u.fmt != PROJ_FORMAT || pc.npings)
                    return D1C_ESHAPE;
                recipe(pc.recipe, s);
                s->pings_ok = 0;
                s->screen = pc.screen;
                for (k = 0; pc.kind == D1C_DELTA && k < pc.nparams; k++) {
                    int16_t v[2];
                    if (d1c_read_i16s(r, pc.data_at + 4u * k, v, 2))
                        return D1C_EIO;
                    if ((uint16_t)v[0] >= P_NPARAMS)
                        return D1C_EBAD;
                    s->par[(uint16_t)v[0]] = v[1];
                }
            }
            name_take(s->name, pc.name);
        } else if (id == D1C_CUES) {             /* (step 7) a piece's cues: each checked; where they are kept, not them */
            d1c_cues_t c;
            d1c_cue_t q;
            if (u.fmt < 37u)
                return D1C_ESHAPE;               /* (none before 37: a cue's values are 37's) */
            if ((rc = d1c_read_cues(r, pay, len, &c)) != 0)
                return rc;
            if (c.flags || c.index >= NPIECES || (cseen >> c.index & 1u))
                return D1C_EBAD;
            for (k = 0; k < c.count; k++) {
                if (d1c_read_cue(r, &c, k, &q))
                    return D1C_EIO;
                if (!cue_ok(&q))
                    return D1C_EBAD;
            }
            cseen |= 1u << c.index;
            p->cues[c.index].src = dec_src, p->cues[c.index].n = c.count, p->cues[c.index].off = (uint16_t)c.cues_at;
        }                                        /* (another id: skipped, a newer writer's) */
    }
    if (npc != r->h.npieces || seen != (1u << npc) - 1u || (!base && (!glob || npc != NPIECES)))
        return D1C_ESHAPE;                       /* (WORK: the globals and all eight; a set: slots 0 .. n-1) */
    if (cseen & ~seen)
        return D1C_ESHAPE;                       /* (cues for a piece it doesn't have) */
    for (i = 0; i < NPIECES; i++)                /* (its pieces without cues: none, whatever the slot had) */
        if ((seen >> i & 1u) && !(cseen >> i & 1u))
            memset(&p->cues[i], 0, sizeof p->cues[i]);
    if (!base)
        p->cues_off = (uint8_t)!!(r->h.flags & D1C_F_NOCUES);
    for (i = 0; i < NPIECES; i++)                /* (each by its own recipe: (37) a set may put any piece in any slot) */
        fresh[i] = (uint8_t)(full[i] && slot_upgrade(&p->slot[i], p->slot[i].recipe, &u));
    if (glob)
        par_upgrade(gpar, 0, p->slot[!base && p->piece < NPIECES ? p->piece : 0].recipe, &u);
    if (base) {                                  /* (a set: its first piece playing; Tune and Song mode its, if it says) */
        p->piece = 0;
        take_to(p, &p->slot[0]);
        p->setlen = (uint8_t)(npc < NPIECES ? npc : 0u);
        if (glob)
            p->par[P_TUNE] = gp[P_TUNE], p->par[P_SMODE] = gp[P_SMODE];
    }
    proj_finish(p, fresh, u.fmt);
    return project_valid(p) ? 0 : D1C_EBAD;
}

/* ------------------------------------------------------------------------------------------------- loading --- */
static const char *proj_note;                    /* (the boot) what the screen says once of the save it took */
static int fs_get(void *ctx, uint32_t off, void *dst, uint32_t n) { return plat_fs_read(*(const int *)ctx, off, dst, n); }
static int fs_put_cb(void *ctx, const void *p, uint32_t n)
{
    (void)ctx;
    return plat_fs_put(p, n);
}

/* a WORK container (r opened) into p: 0, *meta its META */
static int work_decode(const d1c_r_t *r, project_t *p, d1c_meta_t *meta)
{
    return proj_decode(r, p, meta, 0);
}
/* WORK's version in frame f (len bytes) decoded into p: 0, *meta its META */
static int work_read(int f, uint32_t len, project_t *p, d1c_meta_t *meta)
{
    d1c_r_t r;
    int rc = d1c_open(&r, fs_get, &f, len);
    dec_src = (uint8_t)(f + 1);
    work_rflags = rc ? 0u : r.h.flags;
    return rc ? rc : work_decode(&r, p, meta);
}

/* project p as a new version of WORK: counted, a frame opened (may_erase 0, playing: only one erased ahead), written
 * a page at a time, committed (read back, the header last). 0, or the store's error (PLAT_FS_BUSY: none erased) */
static int work_write(project_t *p, int may_erase)
{
    static const upg_t U = {PROJ_FORMAT, P_NPARAMS, 0};
    d1c_w_t w;
    uint32_t total;
    int rc, f;
    work_saves++;
    total = work_len(p, &U);
    if ((rc = plat_fs_begin(FS_WORK, 0, total, may_erase)) != 0)
        return rc;
    f = plat_fs_open();
    d1c_w_init(&w, fs_put_cb, 0);
    work_emit(&w, p, &U, total);
    if (w.err || w.len != total) {
        plat_fs_abort();
        return w.err ? w.err : -3;
    }
    if ((rc = plat_fs_commit()) == 0)
        cues_moved(p, f);                        /* (step 7) the cue lists read from the new WORK from now on */
    return rc;
}

/* 1.4's save (OBJ_PROJ, formats 8 to 36) into proj, converted as project_load always did: 0; else the defaults, -1 */
static int project_load_obj(void)
{
    project36_t *const pbuf = &app_scratch.old;  /* (the boot's buffer, lent by the scratch: app.h) */
    upg_t u;
    int n;
    memset(pbuf, 0, sizeof *pbuf);
    n = plat_store_load(OBJ_PROJ, pbuf, sizeof *pbuf);
    if (proj_parse_old(pbuf, n, &proj, &u) == 0) {
        proj_upgrade(&proj, &u);
        if (project_valid(&proj))
            return 0;
    }
    project_defaults();
    return -1;
}

/* (1.5.0, the user's 2026-11-23) the sets the FM-1 comes with (the Sets page's .dub1 files in sets/: tools/gen_sets.py),
 * stored once on a unit: at the boot, stopped (before the audio starts), after WORK is read, into the first empty set
 * slots in their order (a slot used never written; fewer empty than sets: those that fit). Each stored as one sent from
 * the computer is after its yes (setwire.c: a frame begun, the bytes put, read back, checked whole as loading it would
 * take it, the header last): the same bytes, its cues too (kept; played only where DUB1_CUES). One whose very bytes
 * are stored already in some slot (sent from the Sets page, or a boot cut before the flag was saved) not stored again.
 * Then WORK saved with D1C_F_SETS: never again on this unit, a set deleted stays deleted. The flash failing (a frame,
 * an erase, a program, the read back; WORK's save): no flag, tried again at the next boot (once a boot), the sets
 * stored so far kept. Not after a failed boot (the guard's). How many were stored, said once (proj_note) */
#include "first_sets.h"
static int first_there(uint32_t k)               /* set k's bytes stored already in some slot: 1 */
{
    uint8_t b[256];
    uint32_t ix, len, off, n;
    int f;
    for (ix = 0; ix < PLAT_FS_SETS; ix++) {
        if ((f = plat_fs_find(FS_SET, ix, 0, &len)) < 0 || len != first_set_len[k])
            continue;
        for (off = 0; off < len; off += n) {
            n = len - off < sizeof b ? len - off : (uint32_t)sizeof b;
            if (plat_fs_read(f, off, b, n) || memcmp(b, first_set[k] + off, n))
                break;
        }
        if (off >= len)
            return 1;
    }
    return 0;
}
/* set k into slot ix: 0 stored; 1 not one this DUB-1 takes (never will be: skipped); < 0 the flash (nothing stored) */
static int first_put(uint32_t k, uint32_t ix)
{
    uint8_t b[256];                              /* (the image's bytes through RAM: plat_fs_put's) */
    char nm[PROJ_NAME + 1];
    const uint32_t len = first_set_len[k];
    uint32_t off, n, np;
    int f, rc;
    if ((rc = plat_fs_begin(FS_SET, ix, len, 1)) != 0)
        return rc;
    for (off = 0; off < len; off += n) {
        n = len - off < sizeof b ? len - off : (uint32_t)sizeof b;
        memcpy(b, first_set[k] + off, n);
        if ((rc = plat_fs_put(b, n)) != 0) {
            plat_fs_abort();
            return rc;
        }
    }
    if ((f = plat_fs_finish()) < 0)              /* (the frame left free) */
        return f;
    if (project_set_check(f, len, nm, &np)) {
        plat_fs_abort();
        return 1;
    }
    return plat_fs_seal();
}
static void first_sets(void)
{
    static char note[16];
    uint32_t k, ix = 0, added = 0;
    int rc;
    for (k = 0; k < FIRST_SETS_N; k++) {
        if (first_there(k))
            continue;
        while (ix < PLAT_FS_SETS && plat_fs_has(FS_SET, ix))
            ix++;
        if (ix >= PLAT_FS_SETS)
            break;                               /* (no empty slot: the rest not stored) */
        if ((rc = first_put(k, ix)) < 0)
            return;                              /* (the flash: no flag, the next boot again) */
        added += rc == 0, ix += rc == 0;
    }
    work_sets = 1;
    if (work_write(&proj, 1)) {
        work_sets = 0;
        return;
    }
    if (added) {
        note[0] = (char)('0' + added);
        memcpy(note + 1, added > 1 ? " SETS ADDED" : " SET ADDED", added > 1 ? 12 : 11);
        proj_note = note;
    }
}

/* (1.5.0, STORAGE-1.5.0.md 5.2) the boot: the frames' headers; WORK's newest good version (after a failed boot, the
 * one before it: a save that crashes the decoder can't keep the unit failing); 1.4's save instead when WORK has none,
 * or when 1.4.x saved since WORK took it (its seq above META's migrated: their newer edits win), migrated and saved
 * as WORK at once (not after a failed boot); else the defaults. Then two frames erased ahead (the boot is stopped) */
int project_load(void)
{
    project_t *const w = &app_scratch.load;      /* (decoded here, checked, then taken whole) */
    const int guard = plat_boot_failed();
    uint32_t obj, len, nth;
    int f, got = 0, rc = 0;
    d1c_meta_t m;
    proj_note = 0;
    plat_fs_scan();
    obj = plat_store_seq(OBJ_PROJ);
    for (nth = guard ? 1u : 0u; !got && (f = plat_fs_find(FS_WORK, 0, nth, &len)) >= 0; nth++)
        got = work_read(f, len, w, &m) == 0;     /* (one that doesn't read: the one before) */
    work_sets = got && (work_rflags & D1C_F_SETS);   /* (the flag WORK carries, whichever branch below) */
    if (got && obj <= m.migrated) {
        proj = *w;
        work_mig = m.migrated, work_saves = m.saves;
        if (guard)
            proj_note = "OLDER SAVE LOADED";
    } else if (obj && project_load_obj() == 0) {  /* (the first boot of 1.5.0, or 1.4.x saved since) */
        work_mig = obj, work_saves = 0;
        if (guard)
            proj_note = "OLDER SAVE LOADED";
        else
            work_write(&proj, 1);                /* (once, as it was: 1.4's save stays, never written) */
    } else if (got) {                            /* (1.4's newer save there but unreadable: WORK as it was) */
        proj = *w;
        work_mig = m.migrated, work_saves = m.saves;
    } else {
        project_defaults();
        work_mig = obj, work_saves = 0;
        if (guard && plat_fs_find(FS_WORK, 0, 0, &len) >= 0)
            proj_note = "SAVE NOT LOADED";
        rc = -1;
    }
    if (!guard && !work_sets)
        first_sets();                            /* (once a unit: the sets it comes with; not after a failed boot) */
    if (!guard)
        plat_fs_prepare();
    return rc;
}

const char *project_note(void)
{
    const char *n = proj_note;
    proj_note = 0;
    return n;
}

int project_save(void)
{
    int rc;
    if (plat_fs_open() >= 0)
        return PLAT_FS_HELD;                     /* (a set coming in, or waiting for its yes: saved after) */
    keep();
    proj.rsv[0] = PROJ_WAVE_MARK;
    rc = work_write(&proj, !en_playing);         /* (playing: into a frame erased ahead, pages only; none: BUSY, saved
                                                  * when stopped, autosave) */
    if (rc == 0 && !en_playing)
        plat_fs_prepare();                       /* (stopped: the next one's frame erased now) */
    return rc;
}

void project_idle(void)
{
    if (!en_playing)
        plat_fs_prepare();
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
    recipe(proj.slot[proj.piece].recipe, &s);    /* ((37) its own recipe: a set may have put it in another slot) */
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

/* ------------------------------------------------------------------------------- the sets (1.5.0, step 4) --- */
/* the set protocol's side (setwire.c): what a stored set is, taking one into the slots, WORK's bytes as they are */

/* a set's version in frame f (len bytes: stored, or one coming in, finished and still headerless) checked whole, as
 * taking it would take it: its shape and CRC, every piece and value (decoded onto the project as it is, into the
 * scratch, then dropped). 0, its name (META's; empty: none) and pieces; else D1C_* */
int project_set_check(int f, uint32_t len, char *name, uint32_t *npieces)
{
    d1c_r_t r;
    d1c_meta_t m;
    int rc = d1c_open(&r, fs_get, &f, len);
    dec_src = (uint8_t)(f + 1);
    if (rc || (rc = proj_decode(&r, &app_scratch.load, &m, &proj)) != 0)
        return rc;
    name_take(name, m.name);
    *npieces = r.h.npieces;
    return 0;
}

/* (LIST, DELETE's question) a set's name and pieces, its shape and CRC only (not decoded): 0; else D1C_* */
int project_set_info(int f, uint32_t len, char *name, uint32_t *npieces)
{
    d1c_r_t r;
    d1c_meta_t m;
    uint32_t at = 0, id, pay, n;
    int rc = d1c_open(&r, fs_get, &f, len);
    if (rc)
        return rc;
    if (r.h.kind != FS_SET)
        return D1C_ESHAPE;
    name[0] = 0;
    *npieces = r.h.npieces;
    while (d1c_next(&r, &at, &id, &pay, &n))
        if (id == D1C_META) {
            if ((rc = d1c_read_meta(&r, pay, n, &m)) != 0)
                return rc;
            if (name_ok(m.name, D1C_NAME))
                name_take(name, m.name);
            break;
        }
    return 0;
}

/* (the Set page's Store: the set loaded, after a boot) a set's name, cheaply: META's field where a set's writers put it
 * (the first chunk, after the header), no CRC (only to match the pieces' name; what is stored or loaded is checked
 * whole as ever): 0; else -1 (not there, not a name) */
int project_set_name(int f, uint32_t len, char *name)
{
    uint8_t b[D1C_HDR + 8u + D1C_NAME];
    name[0] = 0;
    if (len < sizeof b || plat_fs_read(f, 0, b, sizeof b) || d1c_le(b, 4) != D1C_MAGIC || d1c_le(b + 16, 2) != FS_SET ||
        d1c_le(b + D1C_HDR, 4) != D1C_META || !name_ok((const char *)b + D1C_HDR + 8u, D1C_NAME))
        return -1;
    name_take(name, (const char *)b + D1C_HDR + 8u);
    return 0;
}

/* (the user's, 2026-11-23: the Set page's KNOB 3, Store) the pieces as they are now (the piece playing kept into its
 * slot first, as a save does), pieces 1 .. n of them, as a set named name, into set ix: the bytes a set's file has
 * (proj_emit's SET: META, GLOB (the set's Mode, Transition, sirens and Tune; MIDI out, Lights and Values in it but never
 * taken from a set), the pieces whole, their cues), written as one sent from the computer is: a frame begun (the old
 * version kept till the new one counts), the bytes put, read back, checked whole as loading it would take it; left
 * headerless for the question (plat_fs_seal: it counts; plat_fs_abort: it doesn't). The frame (>= 0), *len its length;
 * else < 0: the flash (PLAT_FS_*, the store's), or D1C_* (not one this DUB-1 would take: never expected) */
int project_set_write(uint32_t ix, uint32_t n, const char *name, uint32_t *len)
{
    static const upg_t U = {PROJ_FORMAT, P_NPARAMS, 0};
    char was[PROJ_NAME + 1], nm[PROJ_NAME + 1];
    d1c_w_t w;
    uint32_t total, np, i;
    int rc, f;
    keep();
    memcpy(was, proj.name, sizeof was);          /* (the set's name in META for the writing: the project's own after) */
    for (i = 0; i < PROJ_NAME && name[i]; i++)
        proj.name[i] = name[i];
    proj.name[i] = 0;
    d1c_w_init(&w, 0, 0);
    proj_emit(&w, &proj, &U, 0, FS_SET, n, 1);
    total = w.len;
    if ((rc = plat_fs_begin(FS_SET, ix, total, 1)) == 0) {
        d1c_w_init(&w, fs_put_cb, 0);
        proj_emit(&w, &proj, &U, total, FS_SET, n, 1);
        if ((rc = w.err ? w.err : w.len != total ? -3 : 0) != 0)
            plat_fs_abort();
    }
    memcpy(proj.name, was, sizeof was);
    if (rc)
        return rc;
    if ((f = plat_fs_finish()) < 0)              /* (the last page, read back; the frame left free) */
        return f;
    if ((rc = project_set_check(f, total, nm, &np)) != 0) {
        plat_fs_abort();
        return rc;
    }
    *len = total;
    return f;
}

/* (APPLY's yes) the set in frame f into the slots, on the project as it is (the piece playing kept into its slot
 * first): decoded into the scratch, checked, then taken whole, and WORK saved as it is (not through project_save: the
 * engine still holds the old piece's loops). 0: taken and saved; 1: taken, not saved (the caller marks it unsaved);
 * < 0 not taken */
int project_set_take(int f, uint32_t len)
{
    d1c_r_t r;
    d1c_meta_t m;
    int rc = d1c_open(&r, fs_get, &f, len);
    dec_src = (uint8_t)(f + 1);                  /* (its cues read from its frame till WORK is saved) */
    keep();
    if (rc || (rc = proj_decode(&r, &app_scratch.load, &m, &proj)) != 0)
        return rc;
    proj = app_scratch.load;
    if (work_write(&proj, 1))
        return 1;
    plat_fs_prepare();
    return 0;
}

/* (Decisions 4: a restore's pieces) a WORK's version in frame f (len bytes, coming in, finished and headerless) decoded
 * whole into the scratch, as the boot reads WORK: 0, its name (META's); else D1C_*. Then project_work_take, with
 * nothing between (the scratch is shared) */
static int8_t work_in = -1;                      /* (a restore's pieces) the frame they came in: its cues read from it */
int project_work_check(int f, uint32_t len, char *name)
{
    d1c_meta_t m;
    int rc = work_read(f, len, &app_scratch.load, &m);
    work_in = (int8_t)f;
    if (rc)
        return rc;
    name_take(name, m.name);
    return 0;
}

/* the WORK project_work_check decoded, taken whole (the frame it came in dropped first), and saved as this device's
 * WORK: its own META (the 1.4 save's seq it migrated, its count of saves), never the backup's. 0 taken and saved, 1
 * taken not saved (the caller marks it unsaved) */
int project_work_take(void)
{
    int rc;
    proj = app_scratch.load;
    plat_fs_keep(work_in);                       /* (step 7: the frame they came in, dropped, not taken for the save: its
                                                  * cues are copied from it) */
    rc = work_write(&proj, 1);
    plat_fs_keep(-1);
    work_in = -1;
    if (rc) {
        memset(proj.cues, 0, sizeof proj.cues);  /* (not saved: that frame is free now, its cues not kept) */
        return 1;
    }
    plat_fs_prepare();
    return 0;
}

/* (LIST, READ) WORK's bytes as a save would write them now: the total length, and the n bytes from off into dst (off 0:
 * the piece playing kept into its slot first, as a save does; every chunk then the same project) */
typedef struct {
    uint8_t *d;
    uint32_t off, n, at;
} win_t;
static int win_put(void *ctx, const void *p, uint32_t n)
{
    win_t *w = ctx;
    const uint8_t *b = p;
    uint32_t i;
    for (i = 0; i < n; i++, w->at++)
        if (w->at >= w->off && w->at < w->off + w->n)
            w->d[w->at - w->off] = b[i];
    return 0;
}
uint32_t project_work_bytes(uint32_t off, uint8_t *dst, uint32_t n)
{
    static const upg_t U = {PROJ_FORMAT, P_NPARAMS, 0};
    d1c_w_t w;
    win_t win = {dst, off, n, 0};
    uint32_t total;
    if (n && off == 0u)
        keep();
    total = work_len(&proj, &U);
    if (n) {
        d1c_w_init(&w, win_put, &win);
        work_emit(&w, &proj, &U, total);
    }
    return total;
}

#ifdef EN_HOST
/* ------------------------------------------------------------------------------- the host's checks (1.5.0) --- */
typedef struct {
    uint8_t *b;
    uint32_t n, cap;
} pmem_t;
static int pmem_put(void *ctx, const void *p, uint32_t n)
{
    pmem_t *m = ctx;
    if (m->n + n > m->cap)
        return -1;
    memcpy(m->b + m->n, p, n);
    m->n += n;
    return 0;
}
static int pmem_get(void *ctx, uint32_t off, void *dst, uint32_t n)
{
    memcpy(dst, (const uint8_t *)ctx + off, n);
    return 0;
}
/* project p as a container in b (format u->fmt, its parameters): the length, 0 if it didn't fit */
static uint32_t proj_to_mem(const project_t *p, const upg_t *u, uint8_t *b, uint32_t cap)
{
    d1c_w_t w;
    pmem_t m = {b, 0, cap};
    const uint32_t total = work_len(p, u);
    d1c_w_init(&w, pmem_put, &m);
    work_emit(&w, p, u, total);
    return w.err || m.n != total ? 0u : total;
}
/* (step 5: the web emulator's "hear a set first") a SET's bytes into the slots as APPLY's yes takes a stored one (the
 * piece playing kept into its slot, decoded onto the project into the scratch, taken whole, WORK written): 0 taken
 * and saved, 1 taken not saved, < 0 not taken (D1C_*) */
static void cue_mem_take(const uint8_t *b, uint32_t n)   /* (a container read from memory: its copy, a new count) */
{
    memcpy(cue_mem, b, n < sizeof cue_mem ? n : sizeof cue_mem);
    cue_gen = (uint8_t)((cue_gen + 1u) & 0x7Fu);
    dec_src = (uint8_t)(CUE_SRC_MEM | cue_gen);
}
static int project_set_mem(const uint8_t *b, uint32_t n)
{
    d1c_r_t r;
    d1c_meta_t m;
    int rc = d1c_open(&r, pmem_get, (void *)b, n);
    if (rc)
        return rc;
    cue_mem_take(b, n);
    keep();
    if ((rc = proj_decode(&r, &app_scratch.load, &m, &proj)) != 0)
        return rc;
    proj = app_scratch.load;
    if (work_write(&proj, 1))
        return 1;
    plat_fs_prepare();
    return 0;
}
static int proj_from_mem(const uint8_t *b, uint32_t n, project_t *p)   /* (a WORK's bytes) 0: p holds it */
{
    d1c_r_t r;
    d1c_meta_t m;
    int rc = d1c_open(&r, pmem_get, (void *)b, n);
    if (rc)
        return rc;
    cue_mem_take(b, n);
    return work_decode(&r, p, &m);
}
/* two projects the same, field by field (not rsv: no meaning since 31; a WORK writes no rsv) */
static int proj_same(const project_t *a, const project_t *b, char *why, size_t nw)
{
    int i;
#define DIFF(c, what) if (c) { snprintf(why, nw, "%s", what); return 0; }
    DIFF(a->magic != b->magic || a->format != b->format, "magic or format")
    DIFF(memcmp(a->par, b->par, sizeof a->par), "the playing values")
    DIFF(a->pool != b->pool || a->leds != b->leds || a->piece != b->piece || a->screen != b->screen ||
         a->values != b->values || a->trans != b->trans || a->setlen != b->setlen, "the globals")
    DIFF(memcmp(a->sir, b->sir, sizeof a->sir), "the sirens' knobs")
    DIFF(strcmp(a->name, b->name), "the set's name")
    DIFF(a->cues_off != b->cues_off, "the cues' switch")
    for (i = 0; i < NPIECES; i++) {             /* (step 7) each piece's cue list, wherever each is */
        static d1c_cue_t qa[CUE_MAX], qb[CUE_MAX];
        const uint32_t na = cues_read(&a->cues[i], qa), nb = cues_read(&b->cues[i], qb);
        if (na != nb || memcmp(qa, qb, na * sizeof qa[0])) {
            snprintf(why, nw, "piece %d's cues", i);
            return 0;
        }
    }
    for (i = 0; i < NPIECES; i++) {
        const slot_t *x = &a->slot[i], *y = &b->slot[i];
        if (memcmp(x->par, y->par, sizeof x->par) || x->pool != y->pool || memcmp(x->pings, y->pings, sizeof x->pings) ||
            x->pings_ok != y->pings_ok || x->screen != y->screen || strcmp(x->name, y->name) || x->recipe != y->recipe) {
            snprintf(why, nw, "piece %d", i);
            return 0;
        }
    }
#undef DIFF
    return 1;
}
/* (step 2's test) the save in OBJ_PROJ through both paths: read as project_load reads it; and wrapped as it was (its
 * own format and parameters, nothing converted) into a container, read back as a set would be. The same project, or
 * both refused. 0: the same (*fmt the save's format) */
static int project_wrapcheck(uint32_t *fmt, char *why, size_t nw)
{
    static project36_t raw;
    static project_t a, rp, b;
    static uint8_t buf[PLAT_FS_MAX];
    upg_t u;
    uint32_t n;
    int len, va, vb;
    memset(&raw, 0, sizeof raw);
    len = plat_store_load(OBJ_PROJ, &raw, sizeof raw);
    *fmt = len >= 8 ? raw.format : 0u;
    if (proj_parse_old(&raw, len, &rp, &u)) {
        snprintf(why, nw, "not a save project_load reads");
        return 1;
    }
    a = rp;
    proj_upgrade(&a, &u);
    va = project_valid(&a);
    n = proj_to_mem(&rp, &u, buf, sizeof buf);
    vb = n && proj_from_mem(buf, n, &b) == 0;
    if (va != vb) {
        snprintf(why, nw, "%s", va ? "the container refused" : "the container read, project_load's not");
        return 1;
    }
    return va && !proj_same(&a, &b, why, nw);
}
/* (step 2's fuzz) today's project as a container, mutated n times: never a fault (ASan's build), never read when its
 * CRC was not mended, valid when read; the project itself never touched. 0: all held */
static uint32_t fz_s = 1u;
static uint32_t fz_rnd(void)
{
    fz_s ^= fz_s << 13;
    fz_s ^= fz_s >> 17;
    fz_s ^= fz_s << 5;
    return fz_s;
}
static uint32_t fz_cues(const uint8_t *m, uint32_t L, uint32_t *span)   /* (step 7) where the CUES chunks start (0:
                                                  * none), *span their bytes to END */
{
    uint32_t at = D1C_HDR;
    while (at + 8u <= L) {
        const uint32_t id = d1c_le(m + at, 4), len = d1c_le(m + at + 4, 4);
        if (id == D1C_END || len > L)
            break;
        if (id == D1C_CUES) {
            *span = L - 12u - at;
            return at;
        }
        at += 8u + ((len + 3u) & ~3u);
    }
    return 0;
}
static int project_fuzz(uint32_t n, uint32_t seed, uint32_t *nread, char *why, size_t nw)
{
    static uint8_t a[PLAT_FS_MAX], m[PLAT_FS_MAX + 64];
    static project_t snap, t;
    upg_t u = {PROJ_FORMAT, P_NPARAMS, 0};
    uint32_t len, k, i;
#define RND() fz_rnd()
    const uint32_t L = proj_to_mem(&proj, &u, a, sizeof a);
    snap = proj;
    fz_s = seed | 1u;
    *nread = 0;
    if (!L || proj_from_mem(a, L, &t)) {
        snprintf(why, nw, "today's project doesn't read back");
        return 1;
    }
    for (k = 0; k < n; k++) {
        const uint32_t op = RND() % 9u, mend = RND() % 2u;
        uint32_t span = 0, c0 = fz_cues(a, L, &span);
        int rc;
        memcpy(m, a, L);
        len = L;
        if (op >= 7u && c0) {                    /* (step 7: a cue list's byte, or two: its head, a pos, an op, a value) */
            m[c0 + RND() % span] = (uint8_t)(RND() % 2 ? m[c0 + RND() % span] + 1u : RND());
            if (op == 8u)
                m[c0 + RND() % span] ^= (uint8_t)(1u << (RND() % 8u));
        } else if (op == 0)
            m[RND() % L] ^= (uint8_t)(1u << (RND() % 8u));
        else if (op == 1)
            for (i = 0; i < 1u + RND() % 6u; i++)
                m[RND() % L] = (uint8_t)RND();
        else if (op == 2)
            len = RND() % L;
        else if (op == 3) {
            const uint32_t x = 4u * (1u + RND() % 16u);
            for (i = 0; i < x; i++)
                m[L + i] = (uint8_t)RND();
            len = L + x;
        } else if (op == 4) {                    /* (a value: a parameter, a count, a pool, a name's byte) */
            const uint32_t at = 32u + RND() % (L - 44u);
            m[at] = (uint8_t)(RND() % 3 ? m[at] + 1u : RND());
        } else if (op == 5) {                    /* (a chunk's length, or the header's total) */
            const uint32_t at = RND() % 2 ? 8u : 32u + 4u * (RND() % ((L - 44u) / 4u)), v = RND() % 4 ? RND() % 600u : RND();
            m[at] = (uint8_t)v, m[at + 1] = (uint8_t)(v >> 8), m[at + 2] = (uint8_t)(v >> 16), m[at + 3] = (uint8_t)(v >> 24);
        } else {                                 /* (the format) */
            m[12] = (uint8_t)(RND() % 45u);
        }
        if (mend && len >= 12u) {                /* (END's CRC made right: what's left is the decoder's to refuse) */
            const uint32_t c = ~d1c_crc_up(0xFFFFFFFFu, m, len - 12u);
            m[len - 4] = (uint8_t)c, m[len - 3] = (uint8_t)(c >> 8), m[len - 2] = (uint8_t)(c >> 16), m[len - 1] = (uint8_t)(c >> 24);
        }
        rc = proj_from_mem(m, len, &t);
        if (!rc) {
            (*nread)++;
            if (!mend && !(len == L && !memcmp(m, a, L))) {
                snprintf(why, nw, "mutation %u read with its CRC not mended", k);
                return 1;
            }
            if (!project_valid(&t)) {
                snprintf(why, nw, "mutation %u read, not valid", k);
                return 1;
            }
        }
    }
#undef RND
    if (!proj_same(&proj, &snap, why, nw))
        return 1;
    return 0;
}
/* (step 3) after a boot: WORK's newest version, read, is the project in RAM; and the project written and read again,
 * the same (a WORK reads back as itself). 0: so */
static int project_workcheck(char *why, size_t nw)
{
    static project_t t;
    static uint8_t buf[PLAT_FS_MAX];
    static const upg_t U = {PROJ_FORMAT, P_NPARAMS, 0};
    d1c_meta_t m;
    uint32_t len, n;
    const int f = plat_fs_find(FS_WORK, 0, 0, &len);
    if (f < 0 || work_read(f, len, &t, &m)) {
        snprintf(why, nw, "%s", f < 0 ? "no WORK" : "WORK doesn't read");
        return 1;
    }
    if (!proj_same(&proj, &t, why, nw))
        return 1;
    n = proj_to_mem(&proj, &U, buf, sizeof buf);
    if (!n || proj_from_mem(buf, n, &t)) {
        snprintf(why, nw, "the project doesn't read back");
        return 1;
    }
    return !proj_same(&proj, &t, why, nw);
}
/* (step 3's fuzz, through the flash and the boot) today's project made the reference (saved, booted); then n times a
 * mutated copy of it written as WORK's newest version (the frame's CRC good, the container's maybe not), and a boot:
 * the project is the mutated one exactly when it reads (*took), else the version before, exactly: never half
 * applied. Run with no 1.4 save in OBJ_PROJ (the downgrade check would take that). 0: all held */
static int project_fuzzboot(uint32_t n, uint32_t seed, uint32_t *took, char *why, size_t nw)
{
    static project_t ref, t;
    static uint8_t a[PLAT_FS_MAX], m[PLAT_FS_MAX + 64];
    static const upg_t U = {PROJ_FORMAT, P_NPARAMS, 0};
    uint32_t L, k, i, len;
    *took = 0;
    if (plat_store_seq(OBJ_PROJ)) {
        snprintf(why, nw, "a 1.4 save is there (OBJ_PROJ): run it without");
        return 1;
    }
    if (work_write(&proj, 1) || project_load()) {
        snprintf(why, nw, "today's project not saved and read");
        return 1;
    }
    ref = proj;
    L = proj_to_mem(&ref, &U, a, sizeof a);
    fz_s = seed | 1u;
    for (k = 0; k < n; k++) {
        const uint32_t op = fz_rnd() % 7u, mend = fz_rnd() % 2u;
        uint32_t span = 0, c0 = fz_cues(a, L, &span);
        int ok;
        memcpy(m, a, L);
        len = L;
        if (op == 6u && c0)                      /* (step 7: a byte of the cue lists) */
            m[c0 + fz_rnd() % span] = (uint8_t)(fz_rnd() % 2 ? m[c0 + fz_rnd() % span] + 1u : fz_rnd());
        else if (op == 0)
            m[fz_rnd() % L] ^= (uint8_t)(1u << (fz_rnd() % 8u));
        else if (op == 1)
            for (i = 0; i < 1u + fz_rnd() % 6u; i++)
                m[fz_rnd() % L] = (uint8_t)fz_rnd();
        else if (op == 2)
            len = 1u + fz_rnd() % (L - 1u);
        else if (op == 3) {
            const uint32_t at = 32u + fz_rnd() % (L - 44u);
            m[at] = (uint8_t)(fz_rnd() % 3 ? m[at] + 1u : fz_rnd());
        } else if (op == 4) {
            const uint32_t at = 32u + 4u * (fz_rnd() % ((L - 44u) / 4u)), v = fz_rnd() % 600u;
            m[at] = (uint8_t)v, m[at + 1] = (uint8_t)(v >> 8), m[at + 2] = 0, m[at + 3] = 0;
        } else {
            m[16] = (uint8_t)(fz_rnd() % 6u);    /* (the kind) */
        }
        if (mend && len >= 12u) {
            const uint32_t c = ~d1c_crc_up(0xFFFFFFFFu, m, len - 12u);
            m[len - 4] = (uint8_t)c, m[len - 3] = (uint8_t)(c >> 8), m[len - 2] = (uint8_t)(c >> 16), m[len - 1] = (uint8_t)(c >> 24);
        }
        ok = proj_from_mem(m, len, &t) == 0;
        if (plat_fs_begin(FS_WORK, 0, len, 1) || plat_fs_put(m, len) || plat_fs_commit()) {
            snprintf(why, nw, "mutation %u not written", k);
            return 1;
        }
        memset(&proj, 0, sizeof proj);
        project_load();
        if (ok)
            (*took)++;
        if (!proj_same(&proj, ok ? &t : &ref, why, nw)) {
            char w2[64];
            snprintf(w2, sizeof w2, "%s", why);
            snprintf(why, nw, "mutation %u (op %u, %s): %s", k, op, ok ? "read" : "refused", w2);
            return 1;
        }
        if (work_write(&ref, 1)) {               /* (the reference the newest again) */
            snprintf(why, nw, "the reference not saved again");
            return 1;
        }
    }
    proj = ref;
    return 0;
}
#endif
