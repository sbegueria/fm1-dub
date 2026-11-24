/* SPDX-License-Identifier: GPL-3.0-only */
/* The film's stems (amb_host's `stems`): each of the screen's pictures alone, all the time, a video each, frame for
 * frame with the film, to lay full-screen under it in an edit. Each picture as its page draws it, without the header,
 * the knobs or any words, on the screen's ground (K_BG), 240 x 135 of it (16:9) blown up 8x, nearest (the screen's
 * own pixels: 1920 x 1080), H.264 through ffmpeg as it goes (nothing on disk but the videos).
 *
 * The UI a second time, compiled on its own: every static of ui.c here is this file's, a function's (the scope's
 * fading traces, the spectrum's falling columns, the tape's reels) or the file's (the ripples' rings, the Walk's
 * arrow), so drawing them never moves the screen's. What it reads: the engine's values (never written) and, each
 * of the screen's frames, a copy of the screen's own state where the screen takes the engine's (the meters, the
 * echo's peaks: read once, then zeroed), with its mutes and solos (amb_host.c's stems_main). The few engine values
 * the UI writes are this file's by name: the echo's peak (fed the screen's), the meters' (never taken here), the
 * spectrum's scratch, and the scope's and the spectrum's taps (the output, the whole, from the audio as it goes:
 * the screen's follow the page's layer). The ripples one picture a layer: their rings and specks kept a set each,
 * put in place for its turn. So with stems or without, the film and the WAV are the same, byte for byte.
 *
 *   stems DIR [NAME,NAME,..|all]   (before `film`) DIR/NAME.mp4 a picture each while the film records (filmstop)
 * AMB_STEMS="DIR [NAMES]": the same from the start, DIR as given (tools/make_night.sh, make_set.sh: STEMS=1).
 * AMB_STEMS_FF: ffmpeg's options for each (default STEMS_FF: 1920 x 1080 nearest, x264 veryfast, CRF 18). */
#define EN_HOST 1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#define __attribute__(x)

/* what the UI writes of the engine's, this copy's own (and the scope's and the spectrum's taps: the whole) */
#define en_echo_pk stems_echo_pk
#define en_meter stems_meter
#define en_meter_on stems_meter_on
#define en_duck_lo stems_duck_lo
#define en_view_layer stems_view_layer
#define en_scope stems_scope
#define en_scope_w stems_scope_w
#define en_spec stems_spec
#define en_spec_w stems_spec_w
#define app_scratch stems_scratch
/* (its entry points: the screen's are amb_host.c's) */
#define ui_init stems_ui_init
#define ui_frame stems_ui_frame
#define ui_input_only stems_ui_input_only
#define ui_dirty stems_ui_dirty
#define ui_say stems_ui_say
#define ui_splash stems_ui_splash

/* ------------------------------------------------------------ canvas --- */
/* (no panel: each picture taken from the canvas as drawn, draw_stem) */
static void lcd_sync(void) {}
static void lcd_fill(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint16_t c)
{
    (void)x, (void)y, (void)w, (void)h, (void)c;
}
static void lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *px)
{
    (void)x, (void)y, (void)w, (void)h, (void)px;
}
#define cv_text cv_text_words
#include "../firmware/src/gfx.c"
#undef cv_text
/* (no words: the picture alone; the pen moved on as if written) */
static int32_t cv_text(int32_t x, int32_t y, const felucca_font_t *f, const char *s, uint16_t c)
{
    (void)y, (void)c;
    return x + text_w(f, s);
}

/* ---------------------------------------------------------------- UI --- */
#include "../firmware/src/app/plat.h"
#include "../firmware/src/app/app.h"
static int ann_wire_send(const uint8_t *p, uint32_t n) { (void)p, (void)n; return 0; }
#include "../firmware/src/announce.c"
#include "../firmware/src/app/container.c"        /* (1.5.0: the set protocol's, its question never asked here) */
static int sxw_wire(const uint8_t *p, uint32_t n) { (void)p, (void)n; return 0; }
#include "../firmware/src/app/setwire.c"
#define EN_NO_CUES 1                                 /* (1.5.0, step 7: the cues played by the screen's UI only) */
#include "../firmware/src/app/ui.c"

volatile float stems_echo_pk, stems_meter[EN_NMETER], stems_duck_lo[2];
volatile uint8_t stems_meter_on, stems_view_layer;
volatile int16_t stems_scope[EN_SCOPE_N], stems_spec[EN_SPEC_N];
volatile uint32_t stems_scope_w, stems_spec_w;
app_scratch_t stems_scratch;
/* amb_host.c's: the screen's UI, its meters and its echo's peaks, into these (their sizes checked) */
void stems_main(void *u, size_t nu, void *m, size_t nm, void *e, size_t ne, uint8_t *ew);

/* ------------------------------------------------------------- stems --- */
#define SW 240                                   /* (16:9: 8x is 1920 x 1080) */
#define SH 135
#define STEMS_FF "-vf scale=1920:1080:flags=neighbor -c:v libx264 -preset veryfast -crf 18 -pix_fmt yuv420p"
enum { S_RIP, S_SCOPE, S_SPEC, S_TAPE, S_LFO, S_WALK, S_DESK, S_LANES, S_PAT };
/* each: its picture, the page it's drawn as, the rows shown (y0 .. y0 + SH - 1, the main area's: the header's 28
 * above), the columns (from x0; those left of xmin left out: the Walk's list of slots) */
static const struct {
    const char *name;
    uint8_t kind, view;
    int16_t y0, x0, xmin;
} STEM[] = {
    {"ripples", S_RIP, V_DUB, -19, 0, 0},        /* HOME's: the stabs' rings and their echoes, the bass's, the noise's
                                                  * specks, the ping loops going round (the screen's cut at the lanes:
                                                  * here the whole rings) */
    {"ripples-chord", S_RIP, V_CHORD, -19, 0, 0},   /* a layer's page's: its notes alone, in its colour */
    {"ripples-pings", S_RIP, V_TEXTURE, -19, 0, 0},
    {"ripples-bass", S_RIP, V_BASS, -19, 0, 0},
    {"ripples-drums", S_RIP, V_DRUMS, -19, 0, 0},
    {"specks", S_RIP, V_NSND, -19, 0, 0},        /* (the Noise pages': the noise's specks alone) */
    {"scope", S_SCOPE, V_DUB, -8, 0, 0},
    {"spectrum", S_SPEC, V_DUB, -6, 0, 0},
    {"tape", S_TAPE, V_DUB, -10, 0, 0},
    {"lfo", S_LFO, V_CLFO, -21, 0, 0},           /* the chord's LFO page's trace */
    {"lfo-noise", S_LFO, V_NLFO, -21, 0, 0},
    {"walk", S_WALK, V_WALK, 15, 46, 104},       /* the root round the circle of fifths, in the middle */
    {"meters", S_DESK, V_PERF, 21, 16, 0},       /* the Arrangement page's */
    {"lanes", S_LANES, V_DUB, 50, 0, 0},         /* the overview's seven rows, the bar's notes */
    {"lane-chord", S_LANES, V_CHORD, 54, 0, 0},  /* a layer's page's: its lane alone, large */
    {"lane-pings", S_LANES, V_TEXTURE, 55, 0, 0},
    {"lane-bass", S_LANES, V_BASS, 54, 0, 0},
    {"lane-drums", S_LANES, V_DRUMS, 54, 0, 0},
    {"lane-noise", S_LANES, V_NSND, 56, 0, 0},
    {"song", S_LANES, V_SONG, 47, 0, 0},         /* the Song page's: the song's shape, where it is */
    {"pattern", S_PAT, V_PAT, -9, 14, 0},        /* the Pattern page's steps (no lane chosen) */
};
#define NSTEM ((int)(sizeof STEM / sizeof STEM[0]))
static char st_dir[400];
static uint8_t st_on[NSTEM];
static int st_n;
static FILE *st_ff[NSTEM];
static uint16_t st_px[NSTEM][SW * SH];           /* each as last drawn (the screen's 20 a second; the film takes 30) */
static struct {                                  /* the ripples: a set a picture (S_RIP's) */
    uint8_t rip[sizeof rip], spk[sizeof spk];
    uint32_t stabs, bass, spk_r, spk_rng;
    int8_t src;
} st_rv[NSTEM];

static void rv_in(int s)
{
    memcpy(rip, st_rv[s].rip, sizeof rip);
    memcpy(spk, st_rv[s].spk, sizeof spk);
    rip_stabs = st_rv[s].stabs, rip_bass = st_rv[s].bass, spk_r = st_rv[s].spk_r, spk_rng = st_rv[s].spk_rng;
    rip_src = st_rv[s].src;
}
static void rv_out(int s)
{
    memcpy(st_rv[s].rip, rip, sizeof rip);
    memcpy(st_rv[s].spk, spk, sizeof spk);
    st_rv[s].stabs = rip_stabs, st_rv[s].bass = rip_bass, st_rv[s].spk_r = spk_r, st_rv[s].spk_rng = spk_rng;
    st_rv[s].src = rip_src;
}

/* stems DIR [NAMES]: the pictures named (a comma between; all if none, or "all"); the number armed (-1: a name
 * unknown) */
int stems_arm(const char *dir, const char *names)
{
    const char *p;
    int s;
    snprintf(st_dir, sizeof st_dir, "%s", dir);
    st_n = 0;
    for (s = 0; s < NSTEM; s++) {
        const size_t n = strlen(STEM[s].name);
        p = names;
        st_on[s] = !names || !names[0] || !strcmp(names, "all");
        while (p && *p && !st_on[s]) {
            if (!strncmp(p, STEM[s].name, n) && (p[n] == ',' || !p[n]))
                st_on[s] = 1;
            p = strchr(p, ',');
            p = p ? p + 1 : 0;
        }
        rv_out(s);                               /* (each ripples' set from the rings as they are: none, at boot) */
        st_n += st_on[s];
    }
    for (p = names; p && *p && strcmp(names, "all");) {   /* (a name of none: said) */
        const char *e = strchr(p, ',');
        const size_t n = e ? (size_t)(e - p) : strlen(p);
        for (s = 0; s < NSTEM && (strlen(STEM[s].name) != n || strncmp(p, STEM[s].name, n)); s++)
            ;
        if (s == NSTEM) {
            fprintf(stderr, "stems: no picture %.*s (", (int)n, p);
            for (s = 0; s < NSTEM; s++)
                fprintf(stderr, "%s%s", s ? " " : "", STEM[s].name);
            fprintf(stderr, ")\n");
            st_n = 0;
            return -1;
        }
        p = e ? e + 1 : 0;
    }
    return st_n;
}

/* the film started: an ffmpeg a picture, fed as the film goes */
void stems_open(void)
{
    const char *ff = getenv("AMB_STEMS_FF") ? getenv("AMB_STEMS_FF") : STEMS_FF;
    char cmd[1024];
    int s, i;
    for (s = 0; s < NSTEM; s++) {
        if (!st_on[s] || st_ff[s])
            continue;
        for (i = 0; i < SW * SH; i++)            /* (the ground till its first drawing: a launch screen's run draws none) */
            st_px[s][i] = swap16(K_BG);
        snprintf(cmd, sizeof cmd, "ffmpeg -loglevel error -y -f rawvideo -pix_fmt rgb565be -s %dx%d -r 30 -i - %s "
                 "-movflags +faststart '%s/%s.mp4'", SW, SH, ff, st_dir, STEM[s].name);
        st_ff[s] = popen(cmd, "w");
        if (!st_ff[s])
            fprintf(stderr, "stems: cannot run ffmpeg for %s\n", STEM[s].name);
    }
}
void stems_close(void)
{
    int s;
    for (s = 0; s < NSTEM; s++)
        if (st_ff[s]) {
            pclose(st_ff[s]);                    /* (it waits for the video's end) */
            st_ff[s] = 0;
        }
}
/* a film frame: each picture as last drawn */
void stems_frame(void)
{
    int s;
    for (s = 0; s < NSTEM; s++)
        if (st_ff[s])
            fwrite(st_px[s], 2, SW * SH, st_ff[s]);
}

/* the output, as the engine's taps take it on the screen's whole (en_render's l + r after the soft clip): the scope
 * an eighth of the rate, the spectrum a half, each the mean of its samples */
void stems_audio(const int32_t *blk, uint32_t n)
{
    static float sc_acc, sp_acc;
    static uint32_t ph;
    uint32_t k;
    if (!st_n)
        return;
    for (k = 0; k < n; k++) {
        const float v = (float)blk[2 * k] * (1.0f / 8300000.0f) + (float)blk[2 * k + 1] * (1.0f / 8300000.0f);
        sc_acc += v;
        sp_acc += v;
        if (ph & 1u) {
            stems_spec[stems_spec_w & (EN_SPEC_N - 1u)] = (int16_t)(fm_clampf(sp_acc, -4.0f, 4.0f) * 8000.0f);
            stems_spec_w++;
            sp_acc = 0.0f;
        }
        if (++ph >= 8u) {
            ph = 0;
            stems_scope[stems_scope_w & (EN_SCOPE_N - 1u)] = (int16_t)(fm_clampf(sc_acc, -16.0f, 16.0f) * 2000.0f);
            sc_acc = 0.0f;
            stems_scope_w++;
        }
    }
}

static void draw_stem(int s, uint32_t now)
{
    int32_t x, y;
    cv_begin(SW, SH, K_BG);
    cv_oy = -STEM[s].y0;
    switch (STEM[s].kind) {
    case S_RIP:                                  /* (as draw_main's, unclipped) */
        if (rip_src < 0 || rip_src == 4) {
            specks_take(now);
            draw_specks(now);
        }
        draw_ripples(now);
        if (rip_src < 0 || rip_src == 1)
            draw_orbits();
        break;
    case S_SCOPE:
        draw_scope();
        break;
    case S_SPEC:
        draw_spec();
        break;
    case S_TAPE:
        draw_tape(now);
        break;
    case S_LFO:
        draw_lfo(STEM[s].view == V_NLFO);
        break;
    case S_WALK:
        draw_walk();
        break;
    case S_DESK:
        draw_desk();
        break;
    case S_LANES:
        draw_lanes();
        break;
    default:
        draw_pattern();
        break;
    }
    for (y = 0; y < SH; y++)
        for (x = 0; x < SW; x++) {
            const int32_t cx = x + STEM[s].x0;
            st_px[s][y * SW + x] = cx < SW && cx >= STEM[s].xmin ? cv_px[y * SW + cx] : swap16(K_BG);
        }
}

/* each of the screen's frames (after ui_frame): the screen's state taken; the ripples' rings and the Walk's arrow
 * moved on, as the screen's every frame; every third, as the screen draws, each picture drawn */
void stems_tick(void)
{
    const uint32_t now = plat_ms();
    uint8_t w;
    float pk;
    int s, draw;
    if (!st_n)
        return;
    stems_main(&ui, sizeof ui, &mt, sizeof mt, epk, sizeof epk, &epk_w);
    w = epk_w;
    pk = epk[(w + EPK_N - 1u) % EPK_N].pk;
    wk_root_seen(now);
    draw = (ui.frame - 1u) % 3u == 0u;           /* (ui_frame counted it: the frame the screen drew on) */
    for (s = 0; s < NSTEM; s++) {
        if (!st_on[s])
            continue;
        ui.view = STEM[s].view;
        ui.lane = 0xFFu;
        if (STEM[s].kind == S_RIP) {             /* (its rings in place; the echo's peak the screen took, again: the
                                                  * peaks as the screen's) */
            rv_in(s);
            epk_w = (uint8_t)((w + EPK_N - 1u) % EPK_N);
            stems_echo_pk = pk;
            echo_take(now);
            ripples_take(now);
        }
        if (draw)
            draw_stem(s, now);
        if (STEM[s].kind == S_RIP)
            rv_out(s);
    }
}
