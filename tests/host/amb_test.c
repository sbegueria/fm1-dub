/* SPDX-License-Identifier: GPL-3.0-only */
/* The instrument on the host: the pure parts (Euclid, scales, chords, voicings, grooves, names, texts) and
 * the render (sound, no NaN, bounded; the kick on the floor, the chord drifts and holds, never on a
 * diminished fifth, freeze holds, the arrangement moves with Motion, throws, the loops evolve). */
#define _POSIX_C_SOURCE 200809L                /* (fork, pipe: the LFOs' test, one engine run two ways) */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <math.h>
#include "amb.h"

static int fails;
#define CHECK(c, ...)                                                                                              \
    do {                                                                                                           \
        if (!(c)) {                                                                                                \
            printf("FAIL %s:%d: ", __FILE__, __LINE__);                                                            \
            printf(__VA_ARGS__);                                                                                   \
            printf("\n");                                                                                          \
            fails++;                                                                                               \
        }                                                                                                          \
    } while (0)

/* MIDI out: counted; the kick's note-ons (36 on channel 10) by the bar they fall in, and each bar's
 * section (as a note-on from the grid sees it; 0xFF: no note that bar) */
static uint32_t midi_count, kicks, kick_off_beat, drum_notes37, claps;
static int bass_lo = 999, bass_hi = -1, bass_pcs;  /* the bass's note-ons (channel 3): its range, its pitch classes */
#define KBARS 512
static uint8_t kick_bar[KBARS], bar_sec[KBARS], bar_kd[KBARS];   /* (bar_kd: format 31, Motion's kick drop-out on) */
void en_midi_out(uint32_t st, uint32_t d1, uint32_t d2)
{
    uint32_t b = en_bars / 16u;
    midi_count++;
    if ((st & 0xF0u) == 0x90u && d2 > 0u && b < KBARS)
        bar_sec[b] = en_section, bar_kd[b] = (uint8_t)(en_mo_kick != 0);
    if (st == 0x92u && d2 > 0u) {
        bass_lo = (int)d1 < bass_lo ? (int)d1 : bass_lo;
        bass_hi = (int)d1 > bass_hi ? (int)d1 : bass_hi;
        bass_pcs |= 1 << (d1 % 12u);
    }
    if (st == 0x99u && d1 == 37u && d2 > 0u)
        drum_notes37++;
    if (st == 0x99u && d1 == 39u && d2 > 0u)
        claps++;
    if (st == 0x99u && d1 == 36u && d2 > 0u) {
        kicks++;
        kick_off_beat += (en_bars & 3u) != 0u;
        if (b < KBARS)
            kick_bar[b]++;
    }
}

static uint32_t pat(const char *s)
{
    uint32_t m = 0;
    int i;
    for (i = 0; s[i]; i++)
        if (s[i] == 'x')
            m |= 1u << i;
    return m;
}

static int popcount(uint32_t m)
{
    int c = 0;
    while (m) {
        c += (int)(m & 1u);
        m >>= 1;
    }
    return c;
}

static int32_t buf[2 * 256];
static float peak, sum2;
static int bad;
static void run(float secs)
{
    int blocks = (int)(secs * 44100.0f / 256.0f), b, j;
    for (b = 0; b < blocks; b++) {
        en_render(buf, 256, 2048);
        for (j = 0; j < 512; j++) {
            float x = (float)buf[j] / 8388608.0f;
            if (x != x)
                bad++;
            if (fabsf(x) > peak)
                peak = fabsf(x);
            sum2 += x * x;
        }
    }
}

/* (1.4.0, the user's) the dub sirens: one traced from its key's press, a block at a time (its gate, its pitch in
 * semitones over the root as voiced, in Hz); hold < 0: a tap, else held (the hold a third of a second on) and let go
 * at hold seconds. Returns the blocks traced */
#define SIR_TN 6000
static uint8_t sir_on[SIR_TN];
static float sir_st[SIR_TN], sir_hz[SIR_TN];
static int sir_trace(int s, float hold, float secs)
{
    const float bt = 256.0f / 44100.0f;
    int b, n = (int)(secs / bt), h = 0, r = 0;
    if (n > SIR_TN)
        n = SIR_TN;
    en_siren(s, EN_SIR_TAP);
    for (b = 0; b < n; b++) {
        const float t = (float)b * bt;
        if (hold >= 0.0f && !h && t >= 0.334f)
            en_siren(s, EN_SIR_HOLD), h = 1;
        if (hold >= 0.0f && !r && t >= hold)
            en_siren(s, EN_SIR_OFF), r = 1;
        en_render(buf, 256, 2048);
        sir_on[b] = en_sir_on, sir_st[b] = en_sir_st, sir_hz[b] = en_sir_hz;
    }
    return n;
}
static float sir_gate(int n)                     /* the seconds its gate stayed open */
{
    int b, k = 0;
    for (b = 0; b < n; b++)
        k += sir_on[b] != 0;
    return (float)k * 256.0f / 44100.0f;
}

/* (1.4.0) the LFOs: a sixteenth at a time, as the LFO pages' trace has it (en_lfo_tr), n of them from now: the
 * chord's (k 0) and the noise's (1); and when the chord's drift took a new place (picks, the sixteenth's index) */
#define LFO_REC 4800
static int8_t lfo_v[2][LFO_REC];
static int lfo_pick[64], lfo_npick;
static void lfo_rec(int n)
{
    uint32_t w = en_lfo_w, p = en_lfo_picks[0];
    int i = 0;
    lfo_npick = 0;
    while (i < n) {
        en_render(buf, 256, 2048);
        if (en_lfo_w != w) {
            w = en_lfo_w;
            lfo_v[0][i] = en_lfo_tr[0][(w - 1u) % EN_LFO_N];
            lfo_v[1][i] = en_lfo_tr[1][(w - 1u) % EN_LFO_N];
            if (en_lfo_picks[0] != p && lfo_npick < 64)
                lfo_pick[lfo_npick++] = i;
            p = en_lfo_picks[0];
            i++;
        }
    }
}
/* one engine, from here two ways (a fork): as it is, and with parameter p set to v; the same sound either way */
static uint32_t out_hash(float secs)
{
    int blocks = (int)(secs * 44100.0f / 256.0f), b, j;
    uint32_t h = 2166136261u;
    for (b = 0; b < blocks; b++) {
        en_render(buf, 256, 2048);
        for (j = 0; j < 512; j++)
            h = (h ^ (uint32_t)buf[j]) * 16777619u;
    }
    return h;
}
static int same_pair(int p, int a, int b)       /* (1.4.0: a centred knob) the same with p at a as at b */
{
    int fd[2];
    pid_t pid;
    uint32_t h0, h1 = 0;
    fflush(stdout);
    if (pipe(fd))
        return -1;
    pid = fork();
    if (pid == 0) {
        en_set(p, a);
        h1 = out_hash(12.0f);
        if (write(fd[1], &h1, sizeof h1) != (ssize_t)sizeof h1)
            _exit(1);
        _exit(0);
    }
    en_set(p, b);
    h0 = out_hash(12.0f);
    if (read(fd[0], &h1, sizeof h1) != (ssize_t)sizeof h1)
        h1 = ~h0;
    waitpid(pid, 0, 0);
    close(fd[0]);
    close(fd[1]);
    return h0 == h1;
}
static int same_with(int p, int v)
{
    int fd[2];
    pid_t pid;
    uint32_t h0, h1 = 0;
    fflush(stdout);
    if (pipe(fd))
        return -1;
    pid = fork();
    if (pid == 0) {
        en_set(p, v);
        h1 = out_hash(12.0f);
        if (write(fd[1], &h1, sizeof h1) != (ssize_t)sizeof h1)
            _exit(1);
        _exit(0);
    }
    en_set(p, en_param_info(p)->lo);             /* (a command here too: the same drain) */
    h0 = out_hash(12.0f);
    if (read(fd[0], &h1, sizeof h1) != (ssize_t)sizeof h1)
        h1 = ~h0;
    waitpid(pid, 0, 0);
    close(fd[0]);
    close(fd[1]);
    return h0 == h1;
}

/* (format 31) p at v0 and at v1 from here (a fork): the same notes for secs (the stabs' pattern and the chord, every
 * 0.2 s)? */
static uint32_t chord_sig(void);
static int same_notes(int p, int v0, int v1, float secs)
{
    int fd[2], k;
    pid_t pid;
    uint32_t h0 = 0, h1 = 0;
    fflush(stdout);
    if (pipe(fd))
        return -1;
    pid = fork();
    en_set(p, pid == 0 ? v1 : v0);
    for (k = 0; k < (int)(secs * 5.0f); k++) {
        run(0.2f);
        h0 = h0 * 16777619u ^ (en_stab_pat | chord_sig() << 16);
    }
    if (pid == 0) {
        if (write(fd[1], &h0, sizeof h0) != (ssize_t)sizeof h0)
            _exit(1);
        _exit(0);
    }
    if (read(fd[0], &h1, sizeof h1) != (ssize_t)sizeof h1)
        h1 = ~h0;
    waitpid(pid, 0, 0);
    close(fd[0]);
    close(fd[1]);
    return h0 == h1;
}

/* a new engine, stopped, the count from the top (en_init leaves the transport, en_bars, as they were) */
/* (1.4.0) the kick's rumble alone: one engine, from here two ways (a fork), the rumble at 0 and at v, the same
 * otherwise; the difference's mid (m), its side's energy, and each sample's sixteenth within its beat (q: 0 on the
 * beat, where the kick lands) */
#define RB_TN (6 * 44100)
static float rb_m[RB_TN];
static uint8_t rb_q[RB_TN];
static double rb_side;
static void rumble_ab(int v)
{
    static float alt[2 * RB_TN];
    int fd[2], b, j, i = 0;
    pid_t pid;
    fflush(stdout);
    if (pipe(fd))
        return;
    pid = fork();
    if (pid == 0) {                              /* (the rumble at v: its output, through the pipe) */
        en_set(P_RUMBLE, v);
        for (b = 0; b < RB_TN / 256; b++) {
            en_render(buf, 256, 2048);
            for (j = 0; j < 256; j++, i++)
                alt[2 * i] = (float)buf[2 * j] / 8388608.0f, alt[2 * i + 1] = (float)buf[2 * j + 1] / 8388608.0f;
        }
        for (j = 0; j < (int)sizeof alt; j += (int)write(fd[1], (char *)alt + j, sizeof alt - (size_t)j))
            ;
        _exit(0);
    }
    en_set(P_RUMBLE, 0);
    rb_side = 0.0;
    for (b = 0; b < RB_TN / 256; b++) {
        en_render(buf, 256, 2048);
        for (j = 0; j < 256; j++, i++) {
            alt[2 * i] = (float)buf[2 * j] / 8388608.0f, alt[2 * i + 1] = (float)buf[2 * j + 1] / 8388608.0f;
            rb_q[i] = (uint8_t)((en_bars + 3u) % 4u);   /* (en_bars: the step coming, one past the playing one) */
        }
    }
    {
        static float other[2 * RB_TN];
        size_t got = 0;
        ssize_t r;
        while (got < sizeof other && (r = read(fd[0], (char *)other + got, sizeof other - got)) > 0)
            got += (size_t)r;
        for (i = 0; i < RB_TN; i++) {
            const float dl = other[2 * i] - alt[2 * i], dr = other[2 * i + 1] - alt[2 * i + 1];
            rb_m[i] = 0.5f * (dl + dr);
            rb_side += (double)(0.5f * (dl - dr)) * (double)(0.5f * (dl - dr));
        }
    }
    waitpid(pid, 0, 0);
    close(fd[0]);
    close(fd[1]);
}
static int chord_root_pc(void) { return en_chord_root % 12; }

/* (format 32, the variety plan's step 4) a knob's dead steps: piece pc, from the same seed, each value of p from lo to hi
 * (mac: a master action instead, EN_MAC_*) for secs, the output hashed in a child of its own; the steps v -> v + 1
 * that changed nothing at all (bit for bit), and the first of them in *first (-999: none) */
static void fresh(void);
static uint32_t dz_seed = 1;                     /* (dead_steps' seed) */
static int dead_steps(int pc, int p, int mac, int lo, int hi, float secs, int *first)
{
    int16_t pp[P_NPARAMS];
    uint16_t pl;
    uint32_t h[128];
    int v, k, n = 0;
    *first = -999;
    for (v = lo; v <= hi && v - lo < 128; v++) {
        int fd[2];
        pid_t pid;
        fflush(stdout);
        if (pipe(fd))
            return -1;
        pid = fork();
        if (pid == 0) {
            uint32_t hh = 2166136261u;
            int b, j;
            close(fd[0]);
            en_init();                           /* (an engine set up, if this is the first test) */
            fresh();
            en_piece(pc, pp, &pl);
            for (k = 0; k < P_NPARAMS; k++)
                if (k != P_TUNE && k != P_MIDI)
                    en_set(k, pp[k]);
            en_pool(pl);
            for (k = 0; k < EN_NMAC; k++)        /* (the master actions, the mutes, the moves, Lock, the effects as at
                                                  * boot: a test before may have left one) */
                en_macro(k, 0);
            en_mute(0);
            en_perf(0);
            en_lock(0);
            en_fx(0);
            en_freeze(0);
            if (mac >= 0)
                en_macro(mac, v);
            else
                en_set(p, v);
            en_seed(dz_seed);
            en_regrow();
            en_play(1);
            for (b = 0; b < (int)(secs * 44100.0f / 256.0f); b++) {
                en_render(buf, 256, 2048);
                for (j = 0; j < 512; j++)
                    hh = (hh ^ (uint32_t)(buf[j] >> 6)) * 16777619u;
            }
            if (write(fd[1], &hh, sizeof hh) != (ssize_t)sizeof hh)
                _exit(1);
            _exit(0);
        }
        close(fd[1]);                            /* (a child that dies: its end closed, the read returns) */
        if (read(fd[0], &h[v - lo], sizeof h[0]) != (ssize_t)sizeof h[0])
            h[v - lo] = (uint32_t)v | 0x80000000u;
        waitpid(pid, 0, 0);
        close(fd[0]);
        if (v > lo && h[v - lo] == h[v - lo - 1]) {
            if (*first == -999)
                *first = v - 1;
            n++;
        }
    }
    return n;
}

static void fresh(void)
{
    en_play(0);
    run(0.01f);
    en_init();
    en_bars = 0;
    bad = 0;
    memset(kick_bar, 0, sizeof kick_bar);
    memset(bar_sec, 0xFF, sizeof bar_sec);
    memset(bar_kd, 0, sizeof bar_kd);
}

static uint32_t chord_sig(void)
{
    return (uint32_t)en_chord[0] | (uint32_t)en_chord[1] << 7 | (uint32_t)en_chord[2] << 14 |
           (uint32_t)en_chord[3] << 21;
}

/* the chord playing, by its root: on the degree whose pitch class it is, its tones and name; 0 if none */
static int chord_now(uint32_t pl, int root, int *pcs, char *name)
{
    int st[12], m = en_scale(pl, st), d, n;
    for (d = 0; d < m; d++)
        if ((root + st[d]) % 12 == en_chord_root) {
            n = en_chord_tones(pl, root, d, pcs);
            en_chord_name(pcs, n, name);
            return n;
        }
    return 0;
}

int main(void)
{
    int pcs[4], n, i;
    char name[16];

    /* (format 32, the variety plan's step 4) */
    {   /* (Drone's one note: its Density the Pickups zone alone, from 30 exactly: a busy fourth or eighth bar's
         * pickup; four seeds, 90 s each; first, the engine as it boots) */
        int f, n = 0, n2 = 0;
        for (dz_seed = 1; dz_seed <= 4; dz_seed++) {
            n += dead_steps(3, P_BNOTES, -1, 29, 30, 90.0f, &f);
            n2 += dead_steps(3, P_BNOTES, -1, 30, 31, 90.0f, &f);
        }
        dz_seed = 1;
        printf("  Drone's Density, 4 seeds: 29 -> 30 alike in %d, 30 -> 31 in %d\n", n, n2);
        CHECK(n < 4 && n2 == 4, "Drone's pickups not from 30 (29 -> 30 alike %d, 30 -> 31 alike %d)", n, n2);
    }

    /* Euclid: the classic patterns */
    CHECK(en_euclid(3, 8, 0) == pat("x..x..x."), "E(3,8) %x", en_euclid(3, 8, 0));
    CHECK(en_euclid(5, 8, 0) == pat("x.xx.xx."), "E(5,8) %x", en_euclid(5, 8, 0));
    CHECK(en_euclid(4, 16, 0) == pat("x...x...x...x..."), "E(4,16)");
    CHECK(en_euclid(0, 16, 0) == 0, "E(0,16)");
    CHECK(en_euclid(7, 7, 0) == 0x7Fu, "E(7,7)");
    for (i = 0; i <= 12; i++)
        CHECK(popcount(en_euclid(i, 12, 5)) == i, "E(%d,12) has %d hits", i, popcount(en_euclid(i, 12, 5)));
    CHECK(en_euclid(1, 7, 3) == 1u << 3, "rotation");

    /* scales and chords */
    {
        int st[12];
        CHECK(en_scale(EN_MODE_POOL[MODE_DORIAN], st) == 7 && st[2] == 3 && st[6] == 10, "Dorian steps");
        CHECK(en_scale(EN_MODE_POOL[MODE_PENT_MIN], st) == 5, "pentatonic minor has 5");
        CHECK(en_scale(0, st) == 1, "the root is always in");
    }
    n = en_chord_tones(EN_MODE_POOL[MODE_IONIAN], 0, 0, pcs);
    CHECK(n == 4 && pcs[0] == 0 && pcs[1] == 4 && pcs[2] == 7 && pcs[3] == 11, "C Ionian I = Cmaj7");
    CHECK(!strcmp(en_chord_name(pcs, n, name), "CM7"), "name %s", name);
    n = en_chord_tones(EN_MODE_POOL[MODE_DORIAN], 2, 0, pcs);
    CHECK(!strcmp(en_chord_name(pcs, n, name), "Dm7"), "D Dorian i: %s", name);
    n = en_chord_tones(EN_MODE_POOL[MODE_DORIAN], 2, 3, pcs);
    CHECK(!strcmp(en_chord_name(pcs, n, name), "G7"), "D Dorian IV: %s", name);
    n = en_chord_tones(EN_MODE_POOL[MODE_IONIAN], 0, 6, pcs);
    CHECK(!strcmp(en_chord_name(pcs, n, name), "Bm7b5"), "C Ionian vii: %s", name);
    n = en_chord_tones(EN_MODE_POOL[MODE_PENT_MAJ], 0, 0, pcs);
    CHECK(n == 3, "pentatonic chords have three tones (%d)", n);
    n = en_chord_tones(1u | 1u << 4 | 1u << 7 | 1u << 9, 0, 0, pcs);   /* four notes: no doubled tones */
    CHECK(n == 3 && pcs[0] != pcs[1] && pcs[1] != pcs[2] && pcs[0] != pcs[2], "4-note pool: distinct");
    n = en_chord_tones(1u | 1u << 4 | 1u << 7, 0, 1, pcs);
    CHECK(n == 3, "3-note pool: all three (%d)", n);

    /* voicings: four rising notes from the root, within two octaves, no pitch class twice, in the pool */
    {
        int iv[4], m, d, s, st[12];
        en_voicing(EN_MODE_POOL[MODE_DORIAN], 0, 0, iv);
        CHECK(iv[0] == 0 && iv[1] == 7 && iv[2] == 10 && iv[3] == 15, "Dorian i open: %d %d %d %d", iv[0], iv[1],
              iv[2], iv[3]);
        en_voicing(EN_MODE_POOL[MODE_DORIAN], 0, 1, iv);
        CHECK(iv[0] == 0 && iv[1] == 10 && iv[2] == 14 && iv[3] == 15, "Dorian i 9th: %d %d %d %d",
              iv[0], iv[1], iv[2], iv[3]);
        for (m = 0; m < EN_NMODES; m++) {
            int ns = en_scale(EN_MODE_POOL[m], st);
            for (d = 0; d < ns; d++)
                for (s = 0; s < 2; s++) {
                    int k, j, ok = 1;
                    en_voicing(EN_MODE_POOL[m], d, s, iv);
                    ok &= iv[0] == 0 && iv[3] <= 26;
                    for (k = 1; k < 4; k++)
                        ok &= iv[k] > iv[k - 1];
                    for (k = 0; k < 4; k++) {
                        for (j = 0; j < k; j++)
                            ok &= iv[k] % 12 != iv[j] % 12;
                        ok &= EN_MODE_POOL[m] >> ((st[d] + iv[k]) % 12) & 1u;
                    }
                    CHECK(ok, "%s degree %d shape %d: %d %d %d %d", EN_MODE_NAME[m], d, s, iv[0], iv[1], iv[2], iv[3]);
                }
        }
    }

    /* grooves, drift */
    CHECK(en_groove(0) == 1u, "Drone: the downbeat");
    for (i = 0; i < 5; i++)
        CHECK(en_groove(i) & 0xFFFFu && !(en_groove(i) >> 16), "groove %d: %x", i, en_groove(i));
    CHECK(en_groove(1) == 0x4444u, "Offbeat: %x", en_groove(1));
    CHECK(en_drift_bars(0) == 2 && en_drift_bars(5) == 64 && en_drift_bars(6) == 0, "drift bars");

    /* texts */
    {
        char t[12];
        static const char *const GRV[5] = {"Drone", "Offbeat", "Dotted", "Sync", "Roller"};   /* (the Groove knob, sparse
                                                                                              * to busy; Notes thins it) */
        en_param_text(P_DRIFT, 6, t);
        CHECK(!strcmp(t, "Hold"), "drift 6: %s", t);
        en_param_text(P_DRIFT, 4, t);
        CHECK(!strcmp(t, "32 bars"), "drift 4: %s", t);
        en_param_text(P_DRIFT, 0, t);
        CHECK(!strcmp(t, "2 bars"), "drift 0: %s", t);
        en_param_text(P_ETIME, 1, t);
        CHECK(!strcmp(t, "1/8."), "etime 1: %s", t);
        en_param_text(P_ETIME, 5, t);
        CHECK(!strcmp(t, "1 bar"), "etime 5: %s", t);
        for (i = 0; i < 5; i++) {
            en_param_text(P_GROOVE, i, t);
            CHECK(!strcmp(t, GRV[i]), "groove %d: %s", i, t);
        }
        en_param_text(P_CVOICE, 0, t);
        CHECK(!strcmp(t, "Saws"), "ctone 0: %s", t);
        en_param_text(P_CVOICE, 1, t);
        CHECK(!strcmp(t, "Keys"), "ctone 1: %s", t);
        en_param_text(P_SWING, 0, t);
        CHECK(!strcmp(t, "50%"), "swing 0: %s", t);
        en_param_text(P_BASSOCT, 1, t);
        CHECK(!strcmp(t, "+1"), "bass oct: %s", t);
        en_param_text(P_MODE, MODE_DORIAN, t);
        CHECK(!strcmp(t, "Dorian"), "mode: %s", t);
        for (i = 0; i < P_NPARAMS; i++) {
            const en_param_t *pi = en_param_info(i);
            CHECK(pi->lo <= pi->def && pi->def <= pi->hi, "%s: default %d out of %d..%d", pi->name, pi->def, pi->lo,
                  pi->hi);
            en_param_text(i, pi->hi, t);
            CHECK(strlen(t) < 10, "%s: text too long", pi->name);
        }
    }

    /* the render: silent at rest, sound when playing, never NaN, bounded */
    en_init();
    CHECK(en_drum_pat[0] == 0x1111u, "four on the floor: %x", en_drum_pat[0]);
    en_set(P_CRACKLE, 0);
    run(1.0f);
    CHECK(peak == 0.0f, "silent before play (%f)", (double)peak);
    en_set(P_CRACKLE, en_param_info(P_CRACKLE)->def);
    en_play(1);
    peak = sum2 = 0.0f;
    run(20.0f);
    CHECK(!bad, "NaN in the output");
    CHECK(peak > 0.1f && peak < 0.95f, "peak %f", (double)peak);
    printf("  20 s playing: peak %.3f, rms %.4f, bars %u, kicks %u\n", (double)peak,
           (double)sqrtf(sum2 / (20.0f * 44100.0f * 2.0f)), en_bars / 16u, kicks);
    CHECK(en_bars / 16u >= 9u, "the grid ran (%u sixteenths)", en_bars);

    /* the kick: four on the floor, on the beats, while Full (Motion 0: always Full; a fill halves a phrase's
     * last bar) */
    {
        uint32_t b, full = 0, two = 0, other = 0;
        fresh();
        en_set(P_MOTION, 0);
        kicks = kick_off_beat = 0;
        en_play(1);
        run(200.0f);
        for (b = 0; b + 1 < en_bars / 16u && b < KBARS; b++) {
            if (kick_bar[b] == 4)
                full++;
            else if (kick_bar[b] == 2 && (b & 7u) == 7u)
                two++;
            else
                other++;
        }
        CHECK(!kick_off_beat, "%u kicks off the beat", kick_off_beat);
        CHECK(!other && full > 80u, "kick bars: %u of four, %u fills, %u else", full, two, other);
        printf("  200 s at Motion 0: %u bars of four kicks, %u fills\n", full, two);
    }

    /* the chord drifts: at 2 bars, it moves every 4 s or so; never onto a diminished fifth; in the pool */
    {
        static const int MODES[2] = {MODE_IONIAN, MODE_DORIAN};
        int mi;
        for (mi = 0; mi < 2; mi++) {
            uint32_t pl = EN_MODE_POOL[MODES[mi]], last;
            int changes = 0, dim = 0, out = 0, k, seen = 0;
            fresh();
            en_set(P_ROOT, 0);
            en_pool(pl);
            en_set(P_DRIFT, 0);
            en_play(1);
            run(0.1f);
            last = chord_sig();
            for (k = 0; k < 4 * 180; k++) {
                int t[4], nt;
                uint32_t s;
                run(0.25f);
                s = chord_sig();
                changes += s != last;
                last = s;
                nt = chord_now(pl, 0, t, name);
                CHECK(nt, "%s: chord root %d off the scale", EN_MODE_NAME[MODES[mi]], en_chord_root);
                if (nt) {
                    seen |= 1 << en_chord_root;
                    dim += strstr(name, "b5") || strstr(name, "dim");
                }
                for (i = 0; i < EN_NPAD; i++) {
                    out += !(pl >> (en_chord[i] % 12) & 1u);
                    dim += en_chord[i] % 12 == (en_chord_root + 6) % 12;
                }
            }
            CHECK(changes >= 30, "%s: %d chord changes in 3 min at 2 bars", EN_MODE_NAME[MODES[mi]], changes);
            CHECK(popcount((uint32_t)seen) >= 3, "%s: only %d roots", EN_MODE_NAME[MODES[mi]], popcount((uint32_t)seen));
            CHECK(!dim, "%s: %d times on a diminished fifth", EN_MODE_NAME[MODES[mi]], dim);
            CHECK(!out, "%s: %d chord notes off the pool", EN_MODE_NAME[MODES[mi]], out);
            printf("  %s, 3 min at 2 bars: %d chord changes, %d roots\n", EN_MODE_NAME[MODES[mi]], changes,
                   popcount((uint32_t)seen));
        }
    }

    /* Drift Hold: one chord */
    {
        uint32_t held;
        int k, moved = 0;
        fresh();
        en_set(P_DRIFT, 6);
        en_play(1);
        run(0.1f);
        held = chord_sig();
        for (k = 0; k < 120; k++) {
            run(1.0f);
            moved += chord_sig() != held;
        }
        CHECK(!moved && en_drift == 0.0f, "Hold: the chord moved (%d s)", moved);
        en_next();                               /* but Next still moves it, on the bar */
        run(2.5f);
        CHECK(chord_sig() != held, "Next while holding: no change");
    }

    /* freeze (dub hold): the chord holds, though Drift is at its shortest; then moves again */
    {
        uint32_t held;
        int k, moved = 0;
        en_set(P_DRIFT, 0);
        run(5.0f);
        en_freeze(1);
        run(0.1f);
        CHECK(en_frozen, "not frozen");
        held = chord_sig();
        for (k = 0; k < 60; k++) {
            run(1.0f);
            moved += chord_sig() != held;
        }
        CHECK(!moved, "the chord moved while frozen (%d s)", moved);
        en_freeze(0);
        for (k = 0, moved = 0; k < 10; k++) {   /* (it may move and come home again: any move counts) */
            run(1.0f);
            moved += chord_sig() != held;
        }
        CHECK(moved, "the chord stayed after the freeze");
    }

    /* Vary 0, Motion 0: a strict loop, every stab every bar, none thrown (format 30, the user's: Vary 0 no variation at
     * all: the looseness at its most, still none thrown) */
    {
        uint32_t first = 0, t0;
        int k, diff = 0;
        fresh();
        en_set(P_VARY, 0);
        en_set(P_LOOSE, 100);
        en_set(P_MOTION, 0);
        en_set(P_DRIFT, 6);
        en_regrow();
        en_play(1);
        run(2.5f);
        first = en_stab_pat;
        t0 = en_throws;
        for (k = 0; k < 60; k++) {
            run(2.0f);
            diff += en_stab_pat != first;
        }
        CHECK(first && !diff && en_throws == t0, "Vary 0: the stabs changed (%d of 60, %u throws)", diff, en_throws - t0);
    }

    /* Throws: 0 none (Motion 0: no fills), 100 well over 50 (the same draws) */
    {
        uint32_t n[3] = {0, 0, 0};
        int k, sd;
        static const int TV[3] = {0, 50, 100};
        for (k = 0; k < 3; k++)
            for (sd = 7; sd < 10; sd++) {        /* (format 31: three seeds, a minute each: the engine's draws as the
                                                  * tests before left them) */
                uint32_t t0;
                fresh();
                en_seed((uint32_t)sd);
                en_set(P_VARY, 100);
                en_set(P_MOTION, 0);
                en_set(P_STABS, 6);
                en_set(P_CTHROW, TV[k]);
                en_regrow();
                en_play(1);
                t0 = en_throws;
                run(60.0f);
                n[k] += en_throws - t0;
            }
        printf("  throws in three minutes at Throws 0 / 50 / 100: %u / %u / %u\n", n[0], n[1], n[2]);
        CHECK(n[0] == 0 && n[1] > 0 && n[2] * 2u > n[1] * 3u, "Throws: %u / %u / %u", n[0], n[1], n[2]);
    }

    /* the kick's shapes (1.4.0: a switch, Four, Ghost, Half, One): Four (100) four on the floor in every bar it plays;
     * Half (50), mostly half-time, with Vary's wander some of the others */
    {
        int k, b, four[2] = {0, 0}, half[2] = {0, 0}, one[2] = {0, 0}, played[2] = {0, 0};
        static const int KD[2] = {100, 50};
        for (k = 0; k < 2; k++) {
            fresh();
            en_seed(11);
            en_set(P_KDENS, KD[k]);
            en_set(P_MOTION, 0);
            en_regrow();
            en_play(1);
            run(1.0f);
            for (b = 0; b < 240; b++) {
                uint16_t p;
                run(60.0f / 122.0f * 4.0f);      /* (a bar at 122) */
                p = en_lane[1];
                if (!p || b < 64)                /* (past the piece's opening on four: eight phrases) */
                    continue;
                played[k]++;
                four[k] += p == 0x1111u;
                half[k] += p == 0x0101u;
                one[k] += p == 0x0001u;
            }
        }
        printf("  the kick, Four: four in %d of %d bars; Half: four %d, half %d, one %d of %d\n", four[0], played[0],
               four[1], half[1], one[1], played[1]);
        CHECK(played[0] > 100 && four[0] == played[0], "Four: four in %d of %d", four[0], played[0]);
        CHECK(played[1] > 100 && half[1] + one[1] > played[1] / 5 && four[1] < played[1] * 7 / 10,   /* (past the
                                                  * opening; four here counts Ghost too, Half's neighbour) */
              "Half: four %d, half %d, one %d of %d", four[1], half[1], one[1], played[1]);
    }

    /* (1.4.0, format 18) the noise's Density: 0 no hits; turned, its hits from the next bar, not the next phrase */
    {
        int n0, n1;
        fresh();
        en_seed(5);
        en_set(P_NGRAIN, 50);
        en_regrow();
        en_play(1);
        run(60.0f / 122.0f * 4.0f * 3.0f + 0.1f);   /* (into the third bar of the first phrase) */
        en_set(P_NGRAIN, 100);
        run(60.0f / 122.0f * 4.0f * 1.05f);
        n1 = __builtin_popcount(en_nz_pat);
        en_set(P_NGRAIN, 0);
        run(60.0f / 122.0f * 4.0f * 1.05f);
        n0 = __builtin_popcount(en_nz_pat);
        printf("  the noise's Density: at 100, %d hits in two bars; at 0, %d (each a bar after the turn)\n", n1, n0);
        CHECK(n1 >= 14 && n0 == 0, "noise Density: 100 gave %d, 0 gave %d", n1, n0);
    }

    /* the drums' and the bass's phrase plan: A A A B A A A B, the A bars the same, the two B bars the same */
    {
        int ph_n = 0, ok = 0, b, k;
        uint16_t lane[8][3];
        fresh();
        en_seed(5);
        en_set(P_BNOTES, 75);
        en_set(P_MOTION, 0);                    /* (no Breaks: the bass and hats in every phrase) */
        en_regrow();
        en_play(1);
        run(2.0f);
        for (b = 0; b < 64 && ph_n < 4; b++) {
            int last = en_phrase_bar;
            for (k = 0; k < 4000 && en_phrase_bar == last; k++)
                run(0.006f);                     /* (a block at a time, to the bar's start) */
            run(0.2f);
            lane[en_phrase_bar][0] = en_lane[5], lane[en_phrase_bar][1] = en_lane[2], lane[en_phrase_bar][2] = en_lane[1];
            if (en_phrase_bar == 7) {            /* (a whole phrase seen) */
                int a = 1, j, q;
                for (q = 0; q < 3; q++) {
                    for (j = 1; j < 3; j++)
                        a &= lane[j][q] == lane[0][q];
                    for (j = 4; j < 7; j++)
                        a &= lane[j][q] == lane[0][q];
                    a &= q || lane[3][q] == lane[7][q];   /* (B: the bass; the drums' bar 8 may carry the fill) */
                }
                ok += a;
                ph_n++;
            }
        }
        printf("  the phrase plan: %d of %d phrases A A A B A A A B\n", ok, ph_n);
        CHECK(ph_n >= 3 && ok >= ph_n - 1, "the phrase plan: %d of %d", ok, ph_n);
    }

    /* the bass: Octave -1 a true octave under 0 (E0..Eb1); with Drone, Fifth gives whole bars of the fifth */
    {
        int lo0, hi0;
        fresh();
        en_set(P_DRIFT, 6);
        en_set(P_GROOVE, 4);                     /* (Roller, the knob's top) */
        en_set(P_FIFTH, 0);
        en_regrow();
        en_play(1);
        bass_lo = 999, bass_hi = -1;
        run(20.0f);
        lo0 = bass_lo, hi0 = bass_hi;
        en_set(P_BASSOCT, -1);
        bass_lo = 999, bass_hi = -1;
        run(20.0f);
        CHECK(lo0 >= 28 && hi0 <= 51 && bass_lo == lo0 - 12, "Octave -1: %d..%d, at 0 %d..%d", bass_lo, bass_hi, lo0, hi0);
        en_set(P_BASSOCT, 0);
        en_set(P_GROOVE, 0);
        en_set(P_FIFTH, 100);
        run(4.0f);
        bass_pcs = 0;
        run(20.0f);
        CHECK(bass_pcs == 1 << ((chord_root_pc() + 7) % 12), "Drone at Fifth 100: only the fifth (%x)", bass_pcs);
    }

    /* a piece's parameters all at once (a piece change, the boot): none of them, nor PLAY after them, dropped */
    {
        int16_t pp[P_NPARAMS];
        uint16_t pl;
        int k;
        fresh();
        en_piece(3, pp, &pl);
        for (k = 0; k < P_NPARAMS; k++)
            en_set(k, pp[k]);
        en_pool(pl);
        en_regrow();
        en_play(1);
        run(3.0f);
        CHECK(en_playing && en_bars > 16u && peak > 0.02f, "a piece's parameters, then PLAY: not playing (%u)", en_bars);
    }

    /* the safety valve (the classics pass): under load (en_load over 75, the FM-1's meter) the engine sheds: no new
     * noise hits, the width and the rumble eased off, tails shorter; the music goes on, nothing broken, and back */
    {
        int16_t pp[P_NPARAMS];
        uint16_t pl;
        int k;
        uint32_t w0, w1, w2;
        float p_shed;
        fresh();
        en_piece(7, pp, &pl);                    /* (Haze: Dust, the cloud, the widest load on the FM-1) */
        for (k = 0; k < P_NPARAMS; k++)
            en_set(k, pp[k]);
        en_pool(pl);
        en_regrow();
        en_play(1);
        run(10.0f);
        w0 = en_nev_w;
        en_load = 95;
        peak = 0.0f, bad = 0;
        run(10.0f);
        p_shed = peak;
        w1 = en_nev_w;
        en_load = 40;
        run(10.0f);
        w2 = en_nev_w;
        printf("  the valve: noise events %u in 10 s shedding, %u after; peak %.3f shedding\n", w1 - w0, w2 - w1, (double)p_shed);
        CHECK(bad == 0 && p_shed > 0.02f && w2 - w1 > w1 - w0, "the valve: bad %d, peak %.3f, events %u then %u",
              bad, (double)p_shed, w1 - w0, w2 - w1);
    }

    /* the chords' slots, Loop: Fm7, Bb, (a gap: Bb holds), C6, played in turn a change every two bars, round again
     * (the rest gaps: the loop is four long); the slot playing edited, heard at once; to Endless: the one playing up
     * to slot 1; the labels */
    {
        static const int WANT[6] = {5, 10, 0, 5, 10, 0};
        int k, seen[16], ns = 0, ok = 1, last = -1, holds = 0;
        char nm[16];
        fresh();
        en_set(P_ROOT, 5);
        en_pool(EN_MODE_POOL[MODE_DORIAN]);
        en_set(P_DRIFT, 0);
        en_set(P_CR0, 5), en_set(P_CQ0, EN_CQ_MINOR), en_set(P_CX0, EN_CX_7);
        en_set(P_CR1, 10), en_set(P_CQ1, EN_CQ_MAJOR);
        en_set(P_CR2, 0), en_set(P_CQ2, EN_CQ_EMPTY), en_set(P_CX2, 0);
        en_set(P_CR3, 0), en_set(P_CQ3, EN_CQ_MAJOR), en_set(P_CX3, EN_CX_6);
        for (k = 4; k < EN_NPROG; k++)
            en_set(P_CR0 + k, 0), en_set(P_CQ0 + k, EN_CQ_EMPTY), en_set(P_CX0 + k, 0);
        en_set(P_PMODE, EN_PLOOP);               /* (Loop) */
        en_regrow();
        en_play(1);
        for (k = 0; k < 900 && ns < 16; k++) {
            run(0.1f);
            if (en_chord_root != last) {
                last = en_chord_root;
                seen[ns++] = last;
            }
            holds += en_prog_pos == 2 && en_chord_root == 10;
        }
        for (k = 0; k < 6; k++)
            ok &= seen[k] == WANT[k];
        CHECK(ns >= 6 && ok, "the slots' roots: %d %d %d %d %d %d (%d changes)", seen[0], seen[1], seen[2], seen[3], seen[4],
              seen[5], ns);
        CHECK(holds > 0, "the empty slot didn't hold the chord before it");
        while (en_chord_slot != 0)
            run(0.1f);
        en_set(P_CR0, 7);                        /* the slot playing, edited: G minor 7 now */
        run(0.05f);
        CHECK(en_chord_root == 7, "the slot playing, edited: root %d", en_chord_root);
        en_set(P_PMODE, 1);                      /* to Endless (Wander, by hand: turned round): slot 1 plays, the gaps */
        en_prog_rotate();                        /* stay, the walk goes on */
        run(0.05f);
        CHECK(en_prog_pos == 0 && en_prog_slot[0][1] == EN_CQ_MINOR && en_prog_slot[0][0] == 7,
              "to Endless: slot 1 %d/%d/%d at %d", en_prog_slot[0][0], en_prog_slot[0][1], en_prog_slot[0][2], en_prog_pos);
        CHECK(en_prog_slot[2][1] == 0 && en_prog_slot[2][0] == 0, "to Endless: the gap filled");
        CHECK(!strcmp(en_chord_label(5, EN_CQ_MINOR, EN_CX_7, nm), "Fm7"), "label %s", nm);
        CHECK(!strcmp(en_chord_label(0, EN_CQ_SUS4, EN_CX_7, nm), "C7sus4"), "label %s", nm);
        CHECK(!strcmp(en_chord_label(10, EN_CQ_MAJOR, EN_CX_MAJ7, nm), "BbM7"), "label %s", nm);
        CHECK(!strcmp(en_chord_label(3, EN_CQ_EMPTY, 0, nm), "-"), "label %s", nm);
        {   /* Endless from a fresh piece: eight from the walk; on a change, all up one, the walk writes slot 8; a gap
             * stays a gap; the walk's follow the key */
            int j, before[EN_NPROG][3], walk = 1, up = 1;
            fresh();
            en_set(P_ROOT, 5);
            en_pool(EN_MODE_POOL[MODE_DORIAN]);
            en_set(P_DRIFT, 0);
            en_set(P_PMODE, 0);
            for (j = 0; j < EN_NPROG; j++)
                en_set(P_CR0 + j, 0), en_set(P_CQ0 + j, EN_CQ_EMPTY), en_set(P_CX0 + j, 0);
            en_regrow();
            run(0.05f);
            for (j = 0; j < EN_NPROG; j++)
                walk &= en_prog_slot[j][1] == 0 && en_prog_slot[j][0] >= 1 && en_prog_slot[j][0] <= 7;
            CHECK(walk && en_prog_slot[0][0] == 1, "Endless: a fresh piece's eight from the walk, from home (%d)", en_prog_slot[0][0]);
            en_set(P_CR3, 0), en_set(P_CQ3, EN_CQ_EMPTY);   /* a gap at slot 4 */
            en_set(P_CR5, 0), en_set(P_CQ5, EN_CQ_MAJOR), en_set(P_CX5, EN_CX_7);   /* a C7 written at slot 6 */
            run(0.05f);
            for (j = 0; j < EN_NPROG; j++)
                for (k = 0; k < 3; k++)
                    before[j][k] = en_prog_slot[j][k];
            en_play(1);
            {
                uint32_t g = en_prog_gen;
                for (k = 0; k < 200 && en_prog_gen == g; k++)
                    run(0.05f);
            }
            for (j = 0; j < EN_NPROG - 1; j++)
                for (k = 0; k < 3; k++)
                    up &= en_prog_slot[j][k] == before[j + 1][k];
            CHECK(up, "Endless: the slots didn't move up one");
            CHECK(en_prog_slot[2][0] == 0 && en_prog_slot[2][1] == 0, "Endless: the gap didn't stay a gap");
            CHECK(en_prog_slot[4][1] == EN_CQ_MAJOR && en_prog_slot[4][2] == EN_CX_7, "Endless: the written C7 moved");
            CHECK(en_prog_slot[7][1] == 0 && en_prog_slot[7][0] >= 1, "Endless: the walk didn't write slot 8");
            CHECK(en_prog_pos == 0, "Endless: slot %d plays", en_prog_pos);
            {   /* the walk's character turned by hand: its slots walked again (not the one playing); yours and the
                 * gaps kept */
                int diff = 0, keep = 1, p0[3];
                for (j = 0; j < 3; j++)
                    p0[j] = en_prog_slot[0][j];
                for (j = 0; j < EN_NPROG; j++)
                    for (k = 0; k < 3; k++)
                        before[j][k] = en_prog_slot[j][k];
                for (k = 0; k < 4 && !diff; k++) {   /* (a few tries: the walk may write the same by chance) */
                    en_set(P_PMODE, EN_WALK_FREE);
                    en_prog_rewalk();
                    run(0.02f);
                    for (j = 1; j < EN_NPROG; j++)
                        diff += before[j][1] == 0 && before[j][0] && en_prog_slot[j][0] != before[j][0];
                }
                for (j = 0; j < EN_NPROG; j++)
                    if (before[j][1] || !before[j][0])
                        keep &= en_prog_slot[j][0] == before[j][0] && en_prog_slot[j][1] == before[j][1];
                CHECK(diff > 0, "re-walk: the walk's slots didn't change");
                CHECK(keep, "re-walk: a written slot or a gap changed");
                CHECK(en_prog_slot[0][0] == p0[0] && en_prog_slot[0][1] == p0[1], "re-walk: the slot playing changed");
            }
        }
        {   /* the walk's character: Home, Wander, Free, each more varied (fewer A-B-A bounces, home less often) */
            int st, pp[3] = {0}, hm[3] = {0};
            for (st = 0; st < 3; st++) {
                int r[64], nr = 0, j, last = -1;
                fresh();
                en_set(P_ROOT, 5);
                en_pool(EN_MODE_POOL[MODE_DORIAN]);
                en_set(P_DRIFT, 0);
                en_set(P_PMODE, st);
                for (j = 0; j < EN_NPROG; j++)
                    en_set(P_CR0 + j, 0), en_set(P_CQ0 + j, EN_CQ_EMPTY), en_set(P_CX0 + j, 0);
                en_regrow();
                en_play(1);
                for (j = 0; j < 6000 && nr < 64; j++) {
                    run(0.05f);
                    if (en_chord_root != last || j == 0)
                        last = en_chord_root, r[nr++] = last;
                }
                for (j = 2; j < nr; j++)
                    pp[st] += r[j] == r[j - 2];
                for (j = 0; j < nr; j++)
                    hm[st] += r[j] == 5;
                printf("  the walk, %s: %d changes, %d bounces (A-B-A), %d at home\n", st == 0 ? "Home" : st == 1 ? "Wander" : "Free",
                       nr, pp[st], hm[st]);
            }
            CHECK(pp[1] < pp[0] && pp[2] < pp[0], "the walk: Wander (%d) and Free (%d) bounce less than Home (%d)", pp[1], pp[2], pp[0]);
            CHECK(hm[2] < hm[0], "the walk: Free (%d) at home less than Home (%d)", hm[2], hm[0]);
        }
        for (k = 0; k < EN_NCQ * EN_NCX; k++) {   /* every voicing: four rising notes, within two octaves */
            int iv[4];
            en_chord_iv(k / EN_NCX, k % EN_NCX, iv);
            ok = iv[0] == 0 && iv[1] > iv[0] && iv[2] > iv[1] && iv[3] > iv[2] && iv[3] <= 24;
            CHECK(ok, "voicing %d/%d: %d %d %d %d", k / EN_NCX, k % EN_NCX, iv[0], iv[1], iv[2], iv[3]);
        }
    }

    /* the drums' pattern: Play on Pattern plays the written steps only (the kick on 1 and 11 here), Chance 0 a
     * lane silent, the rim heard; Auto's kick is four on the floor */
    {
        int k, bars = 0, off = 0, ok_rim;
        fresh();
        en_set(P_MOTION, 0);
        en_set(P_DPAT0, (int)(int16_t)(uint16_t)0x0401u);   /* steps 1 and 11 */
        en_set(P_DPAT3, (int)(int16_t)(uint16_t)0x1010u);   /* the rim on 5 and 13 */
        en_set(P_DMODE, 1);
        en_regrow();
        en_play(1);
        kicks = kick_off_beat = 0;
        memset(kick_bar, 0, sizeof kick_bar);
        run(20.0f);
        for (k = 1; k < 9; k++)
            bars += kick_bar[k] == 2;
        CHECK(bars >= 7, "Pattern: two kicks a bar, %d of 8 bars", bars);
        (void)off;
        CHECK(en_lane[1] == 0x0401u && en_lane[3] == 0x1010u, "the lanes: kick %04x, rim %04x", en_lane[1], en_lane[3]);
        ok_rim = drum_notes37 > 10u;
        CHECK(ok_rim, "the rim: %u hits", drum_notes37);
        en_set(P_DCH0, 0);
        kicks = 0;
        run(10.0f);
        CHECK(kicks == 0u, "Chance 0: still %u kicks", kicks);
        en_set(P_DMODE, 0);
        kicks = 0;
        memset(kick_bar, 0, sizeof kick_bar);
        run(10.0f);
        CHECK(kicks >= 16u, "Auto again: %u kicks", kicks);
        claps = 0;                               /* the clap, in Auto: now and then, never every time */
        run(120.0f);
        CHECK(claps >= 2u && claps <= 40u, "Auto's claps in 2 min: %u (of ~120 chances)", claps);
        en_set(P_DMODE, 1);
        en_set(P_DPAT4, (int)(int16_t)(uint16_t)0x1010u);
        en_set(P_DCH4, 100);
        claps = 0;
        run(10.0f);
        CHECK(claps >= 8u, "the clap at Chance 100: %u in 10 s", claps);
    }

    /* the effects grid: each effect alone, then all sixteen at once, then let go: no NaN, under full scale,
     * and back to the music after */
    {
        int e;
        for (e = 0; e < EN_NFX; e++) {
            fresh();
            en_play(1);
            run(2.0f);
            en_fx(1u << e);
            run(3.0f);
            CHECK(!bad && peak < 0.99f, "effect %s: peak %f", EN_FX_NAME[e], (double)peak);
            en_fx(0);
            run(2.0f);
            CHECK(!bad && peak > 0.02f, "after %s: silent (%f)", EN_FX_NAME[e], (double)peak);
        }
        fresh();
        en_play(1);
        run(2.0f);
        en_fx(0xFFFFu);
        run(6.0f);
        CHECK(!bad && peak < 0.995f, "all sixteen effects: peak %f", (double)peak);
        en_fx(0);
        run(4.0f);
        CHECK(!bad && peak > 0.02f && peak < 0.99f, "after all effects: %f", (double)peak);
    }

    /* every voice of every layer: it sounds, without NaN, under full scale */
    {
        static const int VP[4] = {P_CVOICE, P_PVOICE, P_BVOICE, P_KIT};
        int l, v;
        for (l = 0; l < 4; l++)
            for (v = en_param_info(VP[l])->lo; v <= en_param_info(VP[l])->hi; v++) {
                fresh();
                en_set(P_DENSITY, 80);
                en_set(VP[l], v);
                en_regrow();
                en_play(1);
                run(6.0f);
                CHECK(!bad && peak > 0.02f && peak < 0.99f, "voice %d of %s: peak %f", v, en_param_info(VP[l])->name, (double)peak);
            }
    }

    /* the arrangement: at Motion 100, it leaves Full now and then; at Motion 0, never */
    {
        int k, m, secs[EN_NSEC];
        static const int MOT[2] = {100, 0};
        for (m = 0; m < 2; m++) {
            memset(secs, 0, sizeof secs);
            fresh();
            en_set(P_MOTION, MOT[m]);
            en_play(1);
            for (k = 0; k < 600; k++) {
                run(1.0f);
                secs[en_section < EN_NSEC ? en_section : 0]++;
            }
            if (MOT[m]) {                        /* the kick: four a bar while Full (but a fill, and Motion's drop-outs:
                                                  * none then), none in a Break */
                uint32_t b, kf = 0, kb = 0, nb = 0, kd = 0;
                for (b = 0; b + 1 < en_bars / 16u && b < KBARS; b++) {
                    if (bar_sec[b] == SEC_FULL && bar_kd[b])
                        kd++, kf += kick_bar[b] != 0;
                    else if (bar_sec[b] == SEC_FULL && (b & 7u) != 7u)
                        kf += kick_bar[b] != 4;
                    if (bar_sec[b] == SEC_BREAK)
                        nb++, kb += kick_bar[b];
                }
                printf("  Motion 100: the kick out a bar or two (Motion's drop-outs) in %u Full bars\n", kd);
                CHECK(!kf && !kb && nb > 10u, "Motion 100: %u Full bars without four kicks, %u kicks in %u Break bars",
                      kf, kb, nb);
            }
            printf("  10 min at Motion %d: Full %d s, Break %d, Groove %d, Thin %d\n", MOT[m], secs[SEC_FULL],
                   secs[SEC_BREAK], secs[SEC_GROOVE], secs[SEC_THIN]);
            if (MOT[m])
                CHECK(secs[SEC_FULL] < 500 && secs[SEC_FULL] > 150 && secs[SEC_BREAK] > 0,
                      "Motion 100: Full %d s, Break %d s", secs[SEC_FULL], secs[SEC_BREAK]);
            else
                CHECK(secs[SEC_FULL] == 600, "Motion 0 left Full (%d s Full)", secs[SEC_FULL]);
        }
    }

    /* (format 31, the variety plan's step 3) Motion, the arrangement only, in three zones, every step heard: at 0
     * nothing leaves; Breathes (to 39) the parts ridden out and back and the kick out a bar or two, more as it rises,
     * the kick's off the phrase's line, no section; Sections (40-69) Thin and Groove join, no Break; Breaks (70-100)
     * Breaks join. Five minutes of Fog a run, four seeds */
    {
        static const int MZ[6] = {0, 10, 20, 39, 55, 85};
        uint32_t ev[6][EN_MO_N + 3], kline = 0;
        int z, sd, k;
        memset(ev, 0, sizeof ev);
        for (z = 0; z < 6; z++)
            for (sd = 1; sd <= 4; sd++) {
                int16_t pp[P_NPARAMS];
                uint16_t pl;
                uint32_t c0[EN_MO_N], lastb = 0;
                fresh();
                en_piece(3, pp, &pl);
                pp[P_MOTION] = (int16_t)MZ[z];
                for (k = 0; k < P_NPARAMS; k++)
                    en_set(k, pp[k]);
                en_pool(pl);
                en_seed((uint32_t)sd);
                en_regrow();
                en_play(1);
                run(0.05f);
                for (k = 0; k < EN_MO_N; k++)
                    c0[k] = en_mo_count[k];
                for (k = 0; k < 3000; k++) {     /* (five minutes, a tenth of a second at a time) */
                    const uint32_t kd = en_mo_count[EN_MO_KICK];
                    run(0.1f);
                    if (en_mo_count[EN_MO_KICK] != kd && (en_phrase_bar == 0 || en_phrase_bar == 7))
                        kline++;
                    if (en_bars / 16u != lastb && en_phrase_bar == 0) {   /* (a phrase begun: its section) */
                        lastb = en_bars / 16u;
                        if (en_section != SEC_FULL)
                            ev[z][EN_MO_N + (en_section == SEC_BREAK ? 2 : 1)]++;
                    }
                }
                for (k = 0; k < EN_MO_N; k++)
                    ev[z][k] += en_mo_count[k] - c0[k];
            }
        for (z = 0; z < 6; z++)
            printf("  Motion %2d, Fog, 4 x 5 min: %u ride-outs, %u swells, %u kick drop-outs; %u Thin or Groove phrases, %u "
                   "Breaks\n", MZ[z], ev[z][EN_MO_OUT], ev[z][EN_MO_SWELL], ev[z][EN_MO_KICK], ev[z][EN_MO_N + 1],
                   ev[z][EN_MO_N + 2]);
        CHECK(!ev[0][EN_MO_OUT] && !ev[0][EN_MO_SWELL] && !ev[0][EN_MO_KICK] && !ev[0][EN_MO_N + 1] && !ev[0][EN_MO_N + 2],
              "Motion 0: something left (%u %u %u %u %u)", ev[0][0], ev[0][1], ev[0][2], ev[0][3], ev[0][4]);
        for (z = 1; z < 4; z++)
            CHECK(ev[z][EN_MO_N + 1] == 0 && ev[z][EN_MO_N + 2] == 0 && ev[z][EN_MO_KICK] > 0,
                  "Breathes (Motion %d): a section, or no kick drop-out", MZ[z]);
        CHECK(ev[1][EN_MO_KICK] + ev[1][EN_MO_OUT] + ev[1][EN_MO_SWELL] < ev[3][EN_MO_KICK] + ev[3][EN_MO_OUT] + ev[3][EN_MO_SWELL],
              "Breathes growing: Motion 10 no quieter than 39");
        CHECK(ev[3][EN_MO_KICK] >= 4u && ev[3][EN_MO_KICK] <= 24u,
              "Motion 39: the kick out about Coldest Season's 2-3 times in 5 minutes (%u in 4 x 5)", ev[3][EN_MO_KICK]);
        CHECK(kline == 0, "the kick's drop-outs off the phrase's line (%u on its first or last bar)", kline);
        CHECK(ev[4][EN_MO_N + 1] > 0 && ev[4][EN_MO_N + 2] == 0, "Sections (Motion 55): Thin or Groove, no Break");
        CHECK(ev[5][EN_MO_N + 2] > 0, "Breaks (Motion 85): a Break");
    }
    /* (format 31) under 40 the arrangement's draws are Motion's own: Motion 10 and 30 play the same notes (the stabs,
     * the chords), only the moves and the kick's drop-outs differ */
    {
        fresh();
        en_seed(3);
        en_regrow();
        en_play(1);
        run(0.05f);
        CHECK(same_notes(P_MOTION, 10, 30, 180.0f), "Motion 10 and 30: other notes");
    }
    /* (format 31) a save before 31: its Motion and Mixer as Motion (en_motion30) */
    {
        static const int16_t IN[7][2] = {{0, 0}, {30, 45}, {40, 100}, {10, 25}, {55, 60}, {70, 20}, {100, 90}};
        static const int16_t OUT[7] = {0, 18, 39, 10, 58, 69, 98};
        int16_t pp[P_NPARAMS];
        int i, ok = 1;
        for (i = 0; i < 7; i++) {
            memset(pp, 0, sizeof pp);
            pp[P_MOTION] = IN[i][0], pp[P_MIXER] = IN[i][1];
            en_motion30(pp);
            if (pp[P_MOTION] != OUT[i])
                printf("  (Motion %d, Mixer %d: %d, not %d)\n", IN[i][0], IN[i][1], pp[P_MOTION], OUT[i]), ok = 0;
        }
        CHECK(ok, "the format 31 upgrade's Motion");
    }
    /* (format 32, the variety plan's step 4) the knobs whose steps did nothing: every step now moves something (four
     * seconds of each value from one seed, bit for bit against the next): Stabs a count; Hits, the bass's and the noise's
     * Density each its last step faded in; the hats' and the pings' Tone the room either side (Concrete's hats at 75,
     * its pings at 60; Fog's pings at 25); the noise's Tone and Decay on Tape (Fog) and Dust (Haze); the echo's Tone to
     * 5 kHz at 100; the Pad's Attack and Drone's Gate (Fog); Energy to its ends */
    {
        static const struct { int pc, p, mac, lo, hi; const char *nm; } DZ[] = {
            {0, P_STABS, -1, 1, 6, "Stabs (Basic)"},          {0, P_HITS, -1, 0, 100, "Hits (Basic)"},
            {3, P_HITS, -1, 0, 100, "Hits (Fog)"},            {0, P_BNOTES, -1, 0, 100, "the bass's Density (Basic)"},
            {4, P_BNOTES, -1, 0, 100, "the bass's Density (Concrete)"}, {0, P_NGRAIN, -1, 0, 100, "the noise's Density (Basic)"},
            {3, P_NGRAIN, -1, 0, 100, "the noise's Density (Fog)"}, {4, P_HTONE, -1, -50, 50, "the hats' Tone (Concrete)"},
            {3, P_HTONE, -1, -50, 50, "the hats' Tone (Fog)"}, {3, P_PTON, -1, -50, 50, "the pings' Tone (Fog)"},
            {4, P_PTON, -1, -50, 50, "the pings' Tone (Concrete)"}, {3, P_NCOL, -1, -50, 50, "the noise's Tone (Fog)"},
            {7, P_NCOL, -1, -50, 50, "the noise's Tone (Haze)"}, {3, P_NTUNE, -1, 0, 100, "the noise's Decay (Fog)"},
            {0, P_ETONE, -1, 0, 100, "the echo's Tone (Basic)"}, {3, P_CATT, -1, 0, 100, "the Pad's Attack (Fog)"},
            {3, P_BLEN, -1, -50, 50, "Drone's Gate (Fog)"},    {0, 0, EN_MAC_ENERGY, -50, 50, "Energy (Basic)"},
            {4, 0, EN_MAC_ENERGY, -50, 50, "Energy (Concrete)"},
        };
        unsigned z;
        for (z = 0; z < sizeof DZ / sizeof DZ[0]; z++) {
            int f, n = dead_steps(DZ[z].pc, DZ[z].p, DZ[z].mac, DZ[z].lo, DZ[z].hi, 4.0f, &f);
            for (dz_seed = 2; n && dz_seed <= 4; dz_seed++) {   /* (a seed stirs the streams as the tests before left
                                                  * them, en_seed's: four seconds of one can hide a part, a ride-out, a
                                                  * kick or a hat that sits out; a step that does nothing does nothing
                                                  * on any seed) */
                int f2, n2 = dead_steps(DZ[z].pc, DZ[z].p, DZ[z].mac, DZ[z].lo, DZ[z].hi, 4.0f, &f2);
                if (n2 < n)
                    n = n2, f = f2;
            }
            dz_seed = 1;
            if (n)
                printf("  %s: %d steps alike, the first %d -> %d\n", DZ[z].nm, n, f, f + 1);
            CHECK(n == 0, "%s: %d steps change nothing", DZ[z].nm, n);
        }
    }
    /* (format 32) a save before 32: the rescaled knobs as near as they sounded (en_zones31) */
    {
        static const struct { int p, v, ctx, cv, want; } ZC[] = {
            {P_STABS, 30, -1, 0, 3}, {P_STABS, 77, -1, 0, 5}, {P_STABS, 0, -1, 0, 1}, {P_STABS, 100, -1, 0, 6},
            {P_HITS, 10, -1, 0, 8}, {P_HITS, 45, -1, 0, 41}, {P_HITS, 38, -1, 0, 39}, {P_HITS, 0, -1, 0, 0},
            {P_HITS, 100, -1, 0, 100},
            {P_BNOTES, 90, P_GROOVE, 1, 100}, {P_BNOTES, 50, P_GROOVE, 1, 33}, {P_BNOTES, 30, P_GROOVE, 0, 29},
            {P_BNOTES, 85, P_GROOVE, 4, 80}, {P_BNOTES, 60, P_GROOVE, 0, 60},
            {P_NGRAIN, 44, -1, 0, 43}, {P_NGRAIN, 56, -1, 0, 56}, {P_NGRAIN, 0, -1, 0, 0},
            {P_NCOL, -10, P_NCHAR, 1, -13}, {P_NCOL, 5, P_NCHAR, 2, 7}, {P_NCOL, 20, P_NCHAR, 0, 20},
            {P_ETONE, 70, -1, 0, 80}, {P_ETONE, 100, -1, 0, 100}, {P_ETONE, 30, -1, 0, 30},
            {P_NTUNE, 70, P_NCHAR, 1, 81}, {P_NTUNE, 95, P_NCHAR, 1, 100}, {P_NTUNE, 30, P_NCHAR, 1, 30},
            {P_CATT, 10, P_CVOICE, 3, 57}, {P_CATT, 50, P_CVOICE, 0, 50},
            {P_BLEN, 2, P_GROOVE, 0, 27}, {P_BLEN, 50, P_GROOVE, 0, 50}, {P_BLEN, 20, P_GROOVE, 1, 20},
            {P_HTONE, 20, P_HATS, 75, 40}, {P_HTONE, -20, P_HATS, 30, -33}, {P_PTON, 30, P_PTONE, 50, 30},
        };
        int16_t pp[P_NPARAMS];
        unsigned i;
        int ok = 1;
        for (i = 0; i < sizeof ZC / sizeof ZC[0]; i++) {
            int16_t rec[P_NPARAMS];
            uint16_t pl;
            en_piece(0, rec, &pl);
            memcpy(pp, rec, sizeof pp);
            pp[P_NCHAR] = 0, pp[P_CVOICE] = 0, pp[P_GROOVE] = 1, pp[P_HATS] = 50, pp[P_PTONE] = 40;
            if (ZC[i].ctx >= 0)
                pp[ZC[i].ctx] = (int16_t)ZC[i].cv;
            pp[ZC[i].p] = (int16_t)ZC[i].v;
            en_zones31(pp, P_NPARAMS);
            if (pp[ZC[i].p] != ZC[i].want)
                printf("  (%s %d: %d, not %d)\n", en_param_info(ZC[i].p)->name, ZC[i].v, pp[ZC[i].p], ZC[i].want), ok = 0;
        }
        CHECK(ok, "the format 32 upgrade's rescaled knobs");
    }

    /* throws: with the piece's looseness up, stabs go into the echo (format 29: the looseness the piece's own, P_LOOSE,
     * Evolve's before) */
    {
        uint32_t t0, lo, hi;
        fresh();
        en_set(P_LOOSE, 0);
        en_set(P_MOTION, 0);
        en_play(1);
        t0 = en_throws;
        run(120.0f);
        lo = en_throws - t0;
        fresh();
        en_set(P_LOOSE, 100);
        en_set(P_VARY, 100);
        en_play(1);
        t0 = en_throws;
        run(120.0f);
        hi = en_throws - t0;
        printf("  throws in 2 min: %u at looseness 0, %u at 100\n", lo, hi);
        CHECK(hi >= 3u && lo == 0u, "throws: %u at looseness 100, %u at 0", hi, lo);   /* (fewer fills since the classics pass: a fill throws its last stab) */
    }
    /* (format 30, the user's) the throws as the chord's amount: none at its Hold (Vary 100), a stab thrown more often at
     * Vary 100 than at the piece's own (P_TVARY 40, Vary 40), the looseness at 100 and Motion 0 (no fills) */
    {
        uint32_t t0, s0, n[3] = {0, 0, 0}, st[3] = {0, 0, 0};
        int k, sd;
        for (k = 0; k < 3; k++)
            for (sd = 3; sd <= 5; sd++) {        /* (three seeds, and a stab's chance: Vary 100 turns the stabs more, a
                                                  * bar now and then with none, so fewer stabs to throw; one seed's 3
                                                  * minutes held as many throws either way after step 5's recipes) */
                fresh();
                en_seed((uint32_t)sd);
                en_set(P_LOOSE, 100);
                en_set(P_MOTION, 0);
                en_set(P_TVARY, 40);
                en_set(P_VARY, k ? 100 : 82);   /* (format 35: the amount 0.4, the old 40) */
                en_set(P_CVARY, k == 2 ? -50 : 0);
                en_play(1);
                t0 = en_throws, s0 = en_stabs;
                run(180.0f);
                n[k] += en_throws - t0, st[k] += en_stabs - s0;
            }
        printf("  throws in 3 x 3 min: %u of %u stabs at the piece's own amount (40), %u of %u at Vary 100, %u at the chord's"
               " Hold\n", n[0], st[0], n[1], st[1], n[2]);
        CHECK(n[0] >= 2u && (uint64_t)n[1] * st[0] > (uint64_t)n[0] * st[1] && n[2] == 0u,
              "throws by the chord's amount: %u of %u at 40, %u of %u at 100, %u at Hold", n[0], st[0], n[1], st[1], n[2]);
    }

    /* the loops evolve: with Vary at 100, the phrases change */
    {
        int8_t before[EN_NLOOP][EN_LOOP_MAX];
        int diff = 0, l;
        en_set(P_DENSITY, 80);
        run(5.0f);
        memcpy(before, (const void *)en_loop_note, sizeof before);
        run(60.0f);
        for (l = 0; l < EN_NLOOP; l++)
            for (i = 0; i < EN_LOOP_MAX; i++)
                diff += before[l][i] != en_loop_note[l][i];
        CHECK(diff > 0, "the loops evolved");
        for (l = 0; l < EN_NLOOP; l++)
            for (i = EN_LOOP_LEN[l]; i < EN_LOOP_MAX; i++)
                CHECK(en_loop_note[l][i] == -1, "loop %d slot %d past its end", l, i);
    }

    /* a new key while playing: the chord's notes in it at once */
    {
        uint32_t pent = EN_MODE_POOL[MODE_PENT_MIN];
        en_set(P_ROOT, 2);
        en_pool(pent);
        en_set(P_PMODE, 0);                      /* (a fresh piece: every slot a gap, so the walk starts from home) */
        for (i = 0; i < EN_NPROG; i++)
            en_set(P_CR0 + i, 0), en_set(P_CQ0 + i, EN_CQ_EMPTY), en_set(P_CX0 + i, 0);
        en_regrow();
        run(0.1f);
        for (i = 0; i < EN_NPAD; i++) {
            CHECK(pent >> ((en_chord[i] - 2 + 120) % 12) & 1u, "chord note %d not in the pool", en_chord[i]);
            CHECK(en_chord[i] >= 36 && en_chord[i] <= 84, "chord note %d out of range", en_chord[i]);
        }
        CHECK(en_chord_root == 2, "regrow: the home chord (root %d)", en_chord_root);
    }

    /* the dub echo at its wildest: Feedback 100, Echo 100, frozen and not; bounded, and it dies away */
    {
        float p1, rms;
        fresh();
        en_set(P_FEEDBACK, 100);
        en_set(P_ECHO, 100);
        en_set(P_VARY, 100);
        en_set(P_MOTION, 100);
        en_play(1);
        bad = 0;
        peak = sum2 = 0.0f;
        run(180.0f);
        p1 = peak;
        rms = sqrtf(sum2 / (180.0f * 44100.0f * 2.0f));
        /* (0.98, from 0.97 in 1.3.0: the master +1.5 dB and the noise bed into the echo put the soft clip a little
         * higher, 0.969 here; the echo is as bounded as it was) */
        CHECK(!bad && peak < 0.98f, "Feedback 100: peak %f", (double)peak);
        en_freeze(1);
        peak = 0.0f;
        run(30.0f);
        CHECK(!bad && peak < 0.98f, "Feedback 100 frozen: peak %f", (double)peak);
        en_freeze(0);
        en_set(P_CRACKLE, 0);
        en_play(0);
        run(60.0f);
        peak = 0.0f;
        run(1.0f);
        printf("  3 min at Feedback 100: peak %.3f, rms %.4f; a minute after stop: %.4f\n", (double)p1,
               (double)rms, (double)peak);
        CHECK(peak < 0.3f, "Feedback 100: still %f a minute after stopping", (double)peak);
    }

    /* (1.4.0, REPORT10) the LFOs' rates: each sine's cycle as long as its rate (1 to 32 bars), on the trace (the wave
     * alone: en_lfo_solo); full depth there at Drift 100, the Trims at +50 (format 30) */
    {
        int r, k;
        en_set(P_TEMPO, 140);
        for (r = 0; r < EN_LFO_DRIFT; r++) {
            const int per = 16 << r, n = 2 * per + 40;
            for (k = 0; k < 2; k++) {
                int i, ups = 0, first = -1, last = -1, ok = 1, hi = 0;
                fresh();
                en_set(P_TEMPO, 140);
                en_set(P_MOVE, 100);
                en_set(P_CLTRIM, 50);
                en_set(P_NLTRIM, 50);
                en_lfo_solo = 1;
                en_set(P_CLRATE, r);
                en_set(P_NLRATE, r);
                en_play(1);
                lfo_rec(n);
                for (i = 8; i < n; i++) {        /* (upward through the middle: a cycle apart) */
                    if (lfo_v[k][i - 1] < 0 && lfo_v[k][i] >= 0) {
                        if (last >= 0 && (i - last < per - 1 || i - last > per + 1))
                            ok = 0;
                        if (first < 0)
                            first = i;
                        last = i;
                        ups++;
                    }
                    hi = lfo_v[k][i] > hi ? lfo_v[k][i] : hi;
                }
                CHECK(ok && ups >= 2 && hi > 110, "%s LFO at %d bars: %d crossings %d..%d, its top %d",
                      k ? "noise" : "chord", 1 << r, ups, first, last, hi);
            }
        }
        en_lfo_solo = 0;
        printf("  the LFOs' rates: 1 to 32 bars, each a cycle as long\n");
    }

    /* Drift: within the depth, gliding (no step over a small bound a sixteenth), a new place every 8 to 32 bars, never
     * a cycle; the same with the same seed, another with another; the noise's its own */
    {
        static int8_t a[600], c[600];
        int i, lo = 127, hi = -127, step = 0, iv_ok = 1, iv_n = 0, iv_min = 1 << 30, iv_max = 0, per = 0, L, same, other;
        fresh();
        en_seed(11);
        en_set(P_TEMPO, 140);
        en_set(P_CLWAVE, 0);                    /* (format 30, layout B: Wave 0, the wander alone; at its full reach) */
        en_set(P_NLWAVE, 0);
        en_set(P_MOVE, 100);
        en_set(P_CLTRIM, 50);
        en_set(P_NLTRIM, 50);
        en_play(1);
        lfo_rec(LFO_REC);
        for (i = 0; i < LFO_REC; i++) {
            const int d = i ? lfo_v[0][i] - lfo_v[0][i - 1] : 0;
            lo = lfo_v[0][i] < lo ? lfo_v[0][i] : lo;
            hi = lfo_v[0][i] > hi ? lfo_v[0][i] : hi;
            step = (d < 0 ? -d : d) > step ? (d < 0 ? -d : d) : step;
        }
        for (i = 2; i < lfo_npick; i++) {        /* (the first, at the start, left out) */
            const int g = lfo_pick[i] - lfo_pick[i - 1];
            iv_ok &= g >= 128 && g <= 513;
            iv_min = g < iv_min ? g : iv_min;
            iv_max = g > iv_max ? g : iv_max;
            iv_n++;
        }
        for (L = 128; L <= 2400; L += 4) {       /* (no lag at which it comes back as it was) */
            int sum = 0;
            for (i = 0; i < 2400; i++)
                sum += abs(lfo_v[0][i] - lfo_v[0][i + L]);
            if (sum < 2400 * 4)
                per++;
        }
        for (same = 0, i = 0; i < LFO_REC; i++)
            same += lfo_v[0][i] == lfo_v[1][i];
        printf("  Drift at 50: %d..%d (of +-127, its reach), its biggest step a sixteenth %d, %d new places %d..%d sixteenths apart\n",
               lo, hi, step, iv_n + 1, iv_min, iv_max);
        CHECK(lo >= -127 && hi <= 127 && hi - lo > 100, "Drift: %d..%d at 50", lo, hi);
        CHECK(step <= 8, "Drift: a step of %d in a sixteenth", step);
        CHECK(iv_ok && iv_n >= 8 && iv_max > iv_min, "Drift: places %d..%d sixteenths apart (%d)", iv_min, iv_max, iv_n);
        CHECK(per == 0, "Drift: like itself at %d lags", per);
        CHECK(same < LFO_REC / 3, "Drift: the noise's the chord's (%d of %d alike)", same, LFO_REC);
        fresh();
        en_seed(11);
        en_set(P_TEMPO, 140);
        en_set(P_CLWAVE, 0);
        en_set(P_MOVE, 50);
        en_play(1);
        lfo_rec(600);
        memcpy(a, lfo_v[0], sizeof a);
        fresh();
        en_seed(11);
        en_set(P_TEMPO, 140);
        en_set(P_CLWAVE, 0);
        en_set(P_MOVE, 50);
        en_play(1);
        lfo_rec(600);
        memcpy(c, lfo_v[0], sizeof c);
        same = !memcmp(a, c, sizeof a);
        fresh();
        en_seed(12);
        en_set(P_TEMPO, 140);
        en_set(P_CLWAVE, 0);
        en_set(P_MOVE, 50);
        en_play(1);
        lfo_rec(600);
        other = memcmp(a, lfo_v[0], sizeof a) != 0;
        CHECK(same && other, "Drift: the same seed the same path (%d), another seed another (%d)", same, other);
    }

    /* (format 30, layout B, the user's) the LFO's Wave: its wave's share against the wander; 100 the wave alone (a bar's
     * sine, back as it was each bar), 0 the wander alone (never so), 50 between; the reach the same */
    {
        int w, k, i, off[3];
        for (k = 0; k < 3; k++) {
            w = k == 0 ? 100 : k == 1 ? 50 : 0;
            fresh();
            en_seed(11);
            en_set(P_TEMPO, 140);
            en_set(P_CLRATE, 0);                 /* (1 bar) */
            en_set(P_CLWAVE, w);
            en_set(P_MOVE, 100);
            en_set(P_CLTRIM, 50);
            en_play(1);
            lfo_rec(LFO_REC);
            for (off[k] = 0, i = 64; i < LFO_REC - 16; i++)
                off[k] += abs(lfo_v[0][i] - lfo_v[0][i + 16]);
            off[k] /= LFO_REC - 80;
        }
        printf("  Wave 100, 50, 0: away from itself a bar later by %d, %d, %d (of +-127)\n", off[0], off[1], off[2]);
        CHECK(off[0] <= 2, "Wave 100: the wave alone, a bar later %d away", off[0]);
        CHECK(off[2] > off[0] + 2 && off[1] > off[0], "Wave: 50 %d and 0 %d no closer than 100's %d", off[1], off[2], off[0]);
    }

    /* the pings' LFO gone: its old knob (P_PFDEC) changes nothing (the bass's, P_BFDEC, Env decay's slot since format
     * 36: below); (format 30) nor do the chord's and the noise's old Depth and Drift; their Trims and Form's Drift do
     * (the test hears a change) */
    {
        int b = 1, pg, ch, nz, cw, nw;
        fresh();
        en_seed(3);
        en_set(P_BASS, 100);
        en_set(P_PINGS, 100);
        en_set(P_MOVE, 50);
        en_play(1);
        run(2.0f);
        pg = same_with(P_PFDEC, 100);
        ch = same_with(P_CFDEC, 100) && same_with(P_CLWALK, 100);   /* (format 30: the old Depths and Drifts unread) */
        nz = same_with(P_NLFO, 100) && same_with(P_NLWALK, 100);
        cw = same_with(P_CLTRIM, 50);            /* (the Trims: heard) */
        nw = same_with(P_NLTRIM, 50);            /* (the noise's, a filter on the whole noise) */
        printf("  the old LFO knobs: the pings' %s; the chord's old %s, the noise's %s; the Trims %s, %s\n",
               pg ? "nothing" : "heard", ch ? "nothing" : "heard", nz ? "nothing" : "heard",
               cw ? "nothing" : "heard", nw ? "nothing" : "heard");
        CHECK(b == 1 && pg == 1 && ch == 1 && nz == 1 && cw == 0 && nw == 0 && same_with(P_MOVE, 100) == 0,
              "LFOs: the bass's %d, the pings' %d, the chord's old %d, the noise's old %d, the Trims %d %d (1: unheard)",
              b, pg, ch, nz, cw, nw);
        en_set(P_TEMPO, 122);
    }

    /* (format 36, REPORT-stabenv) Env decay: the chord's filter envelope's own time around its tie to Decay. At 0 the
     * tie as it was (a quarter of the stab's T60 and 20 ms: the picture's envelope, en_env_shape, on that time);
     * each 12.5 a doubling (+25: four times as long, -25 a quarter); heard; Decay still moves it at any setting; and
     * each piece's recipe within the knob's range */
    {
        float lv[400], ax[400], T, Ts;
        int k, hz[3], heard, d0, d1;
        const int v[3] = {0, 25, -25};
        fresh();
        en_seed(3);
        en_set(P_CVOICE, 0);
        en_set(P_DECAY, 35);
        for (k = 0; k < 3; k++) {
            int j = 0;
            en_set(P_CEDEC, v[k]);
            run(0.01f);
            en_env_shape(0, lv, ax, 400, &T);
            while (j < 399 && ax[j] > 0.5f * ax[0])
                j++;
            hz[k] = j;
        }
        Ts = T;
        {
            const float t60 = 0.06f * exp2f(35.0f * 0.0464f), ft = 0.25f * t60 + 0.02f,   /* (Saws: x1) */
                        want = ft * logf(2.0f) / 6.9f / Ts * 399.0f;   /* (the picture's half, in its steps, at 0:
                                                  * ft its -60 dB, as the engine closes it; the picture drew it 6.9
                                                  * times slower before the Chord filter page) */
            printf("  Env decay: the picture's half at 0, +25, -25: %d, %d, %d steps (the tie's %.1f)\n", hz[0], hz[1], hz[2],
                   (double)want);
            CHECK(fabsf((float)hz[0] - want) <= 1.5f, "Env decay 0: the half %d steps, the tie's %.1f", hz[0], (double)want);
        }
        CHECK(hz[1] >= 3 * hz[0] && hz[1] <= 5 * hz[0] && hz[2] * 3 <= hz[0] + 2, "Env decay +25 x4 (%d), -25 x1/4 (%d) of %d",
              hz[1], hz[2], hz[0]);
        {   /* (the Chord filter page's picture, en_chord_env: Env 0 the voice's own depth (Saws 2.5 octaves), its
             * left none, its right twice; the time the engine's, as the picture's above) */
            float o[3], f, ta, t6;
            const int ev[3] = {-50, 0, 50};
            for (k = 0; k < 3; k++) {
                en_set(P_CENV, ev[k]);
                run(0.01f);
                en_chord_env(&o[k], &f, &ta, &t6);
            }
            en_set(P_CENV, 0);
            printf("  Env -50, 0, +50: the sweep %.2f, %.2f, %.2f octaves; its time %.1f ms, the stab's %.0f ms\n", (double)o[0],
                   (double)o[1], (double)o[2], (double)(f * 1000.0f), (double)(t6 * 1000.0f));
            CHECK(o[0] < 0.001f && fabsf(o[1] - 2.5f) < 0.01f && fabsf(o[2] - 5.0f) < 0.01f && ta > 0.0f,
                  "en_chord_env: Env -50, 0, +50 gave %.2f, %.2f, %.2f octaves", (double)o[0], (double)o[1], (double)o[2]);
        }
        en_set(P_CEDEC, 25);
        en_set(P_DECAY, 60);                     /* (Decay still moves it) */
        run(0.01f);
        en_env_shape(0, lv, ax, 400, &T);
        for (d1 = 0; d1 < 399 && ax[d1] > 0.5f * ax[0]; d1++)
            ;
        d0 = (int)((float)hz[1] * Ts / T + 0.5f);   /* (35's, on 60's grid) */
        CHECK(d1 > d0, "Env decay with Decay 60: the half at %d steps, Decay 35's %d", d1, d0);
        en_set(P_DECAY, 35);
        en_set(P_CHORD, 100);
        en_play(1);
        run(2.0f);
        heard = !same_with(P_CEDEC, 0);
        printf("  Env decay -50 against 0: %s\n", heard ? "heard" : "nothing");
        CHECK(heard, "Env decay: -50 and 0 alike");
        for (k = 0; k < EN_NPIECES; k++) {
            int16_t rp[P_NPARAMS];
            uint16_t pl;
            en_piece(k, rp, &pl);
            CHECK(rp[P_CEDEC] >= -50 && rp[P_CEDEC] <= 50 && rp[P_CENV] >= -50 && rp[P_CENV] <= 50,
                  "piece %d: Env decay %d, Env amount %d", k, rp[P_CEDEC], rp[P_CENV]);
        }
        en_play(0);
    }

    /* (1.4.0, the variety study) Energy leans, it doesn't lock: at Motion 65, Energy -50 and +50 both leave Motion's
     * sections working (some Full bars at -50, some not-Full at +50; +50 no Break); and the filter's sweep follows
     * Drift, not Motion: at Motion 0 it moves (Drift 30), at Drift 0 it stays still at Motion 35 (before, Motion's:
     * +-0.35; format 30: Form's Drift, the chord LFO's Drift before) */
    {
        int e, full[2] = {0, 0}, nfull[2] = {0, 0}, brk = 0;
        for (e = 0; e < 2; e++) {
            uint32_t last = 0xFFFFFFFFu;
            fresh();
            en_seed(5);
            en_set(P_TEMPO, 140);
            en_set(P_MOTION, 65);
            en_set(P_VARY, 86);                 /* (format 35: the amount 0.5, the old 50) */
            en_macro(EN_MAC_ENERGY, e ? 50 : -50);
            en_play(1);
            while (en_bars < 16u * 241u) {
                run(0.25f);
                if (en_bars / 16u != last) {
                    last = en_bars / 16u;
                    if (en_section == SEC_FULL)
                        full[e]++;
                    else
                        nfull[e]++;
                    if (e && en_section == SEC_BREAK)
                        brk++;
                }
            }
            en_macro(EN_MAC_ENERGY, 0);
        }
        printf("  Energy -50: %d bars Full, %d not; +50: %d Full, %d not (%d Break)\n", full[0], nfull[0], full[1], nfull[1],
               brk);
        CHECK(full[0] > 20 && nfull[1] > 4 && brk == 0, "Energy pins the section: -50 Full %d, +50 not Full %d, Break %d",
              full[0], nfull[1], brk);
    }
    {
        float lo[2] = {9.0f, 9.0f}, hi[2] = {-9.0f, -9.0f};
        int k;
        for (k = 0; k < 2; k++) {
            fresh();
            en_seed(9);
            en_set(P_TEMPO, 140);
            en_set(P_MOTION, k ? 35 : 0);
            en_set(P_MOVE, k ? 0 : 30);
            en_play(1);
            while (en_bars < 16u * 121u) {
                run(0.25f);
                if (en_section == SEC_FULL) {
                    lo[k] = fminf(lo[k], en_arc);
                    hi[k] = fmaxf(hi[k], en_arc);
                }
            }
        }
        printf("  the filter's sweep in Full: Motion 0, Drift 30: %.2f .. %.2f; Motion 35, Drift 0: %.2f .. %.2f\n",
               (double)lo[0], (double)hi[0], (double)lo[1], (double)hi[1]);
        CHECK(hi[0] - lo[0] > 0.3f && hi[1] - lo[1] < 0.05f, "the sweep follows Motion, not the Drift: %.2f, %.2f",
              (double)(hi[0] - lo[0]), (double)(hi[1] - lo[1]));
    }

    /* (1.4.0, the user's) the chord's Vary: how often its stabs' pattern turns, Evolve over it. At 50 (the recipes') as
     * Evolve alone turned it: the bars whose stabs differ from the last as many as before (1.3.0 to 1.4.0's LFO pages,
     * here, the same seeds: 203, 167, 166 in 300 bars at Evolve 40; since the variety study's sweep, its depth the chord's
     * Drift and no longer Motion's, which the stabs' density follows, and its own timer drawing from the random stream:
     * 179, 186, 169, within a quarter: 50 was shown the same as Evolve alone bit for bit when Vary came; this keeps the
     * rate; format 30: the echo's second head drawing from its own stream, the music's another path, within it still); (format 29: Evolve Form's Vary, the chord's Vary a trim on it, 0 following it) at Hold the pattern still,
     * at any Vary, and at Vary 0 too (the arrangement held: Motion 0); at +50 (Vary 40: 90), turning over twice as often */
    {
        static const int OLD[3] = {179, 186, 169};
        int s, k, turns[3], still = 0, still_turns = 0;
        for (s = 1; s <= 3; s++) {
            uint32_t b, last;
            int n = 0;
            fresh();
            en_seed((uint32_t)s * 7u);
            en_set(P_TEMPO, 140);
            en_set(P_VARY, 82);             /* (format 35: the amount 0.4, the old 40) */
            en_play(1);
            b = en_bars / 16u, last = en_stab_pat;
            for (k = 0; k < 300;) {
                en_render(buf, 256, 2048);
                if (en_bars / 16u != b) {
                    b = en_bars / 16u, k++;
                    n += en_stab_pat != last;
                    last = en_stab_pat;
                }
            }
            printf("  Vary 40, its trim 0, seed %d: the stabs changed in %d bars of 300 (before Vary: %d)\n", s, n, OLD[s - 1]);
            CHECK(n * 4 >= OLD[s - 1] * 3 && n * 4 <= OLD[s - 1] * 5, "trim 0: %d bars changed, %d before", n,
                  OLD[s - 1]);
        }
        for (k = 0; k < 3; k++) {                /* the turns: the trim Hold, 0, +50 at Vary 40, 300 bars */
            uint32_t t0;
            fresh();
            en_seed(9);
            en_set(P_TEMPO, 140);
            en_set(P_VARY, 82);
            en_set(P_CVARY, k * 50 - 50);
            en_play(1);
            run(1.0f);
            t0 = en_stab_turns;
            while (en_bars < 16u * 301u)
                run(0.5f);
            turns[k] = (int)(en_stab_turns - t0);
        }
        for (s = 0; s < 3; s++) {                /* Hold at Vary 40 and 100, and Vary 0 its trim 0, Motion 0 (Full
                                                  * only): still */
            uint32_t b, last, t0;
            fresh();
            en_seed(13);
            en_set(P_TEMPO, 140);
            en_set(P_VARY, s == 2 ? 0 : s ? 100 : 82);
            en_set(P_MOTION, 0);
            en_set(P_CVARY, s == 2 ? 0 : -50);
            en_play(1);
            run(1.0f);
            t0 = en_stab_turns;
            b = en_bars / 16u, last = en_stab_pat;
            for (k = 0; k < 200;) {
                en_render(buf, 256, 2048);
                if (en_bars / 16u != b) {
                    b = en_bars / 16u, k++;
                    still += en_stab_pat != last;
                    last = en_stab_pat;
                }
            }
            still_turns += (int)(en_stab_turns - t0);
        }
        printf("  the chord's trim Hold, 0, +50: the stabs' pattern turned %d, %d, %d times in 300 bars; held (Vary 40, 100, "
               "and Vary 0) %d bars changed, %d turns\n", turns[0], turns[1], turns[2], still, still_turns);
        CHECK(turns[0] == 0 && turns[1] > 10 && 10 * turns[2] > 18 * turns[1], "the chord's trim: turns %d %d %d",
              turns[0], turns[1], turns[2]);
        CHECK(still == 0 && still_turns == 0, "held: the stabs changed in %d bars, %d turns", still, still_turns);
        en_set(P_CVARY, 0);
        en_set(P_MOTION, 50);
        en_set(P_TEMPO, 122);
    }

    /* (1.4.0, the user's: the layers' pages) each layer's Vary, as the chord's: how often its part changes, a trim on
     * Form's Vary (format 29). At Hold still, at any Vary, and at Vary 0: no loop note moved, no new plan, the shaker's steps kept, the noise's groove the
     * same (the arrangement held: Motion 0); at 100 more often than at 50 (the probabilities saturate a little: the
     * plan's and the noise's at Evolve 40 near 1 already, so 1.5x at least). (At 50 the music as before, bit for bit:
     * every piece, checked against 1.4.0's Chord pages while building it) */
    {
        static const int VP[4] = {P_PVARY, P_BVARY, P_DVARY, P_NVARY};
        static const char *const VN[4] = {"pings'", "bass's", "drums'", "noise's"};
        int l, k, e, turns[4][3], still[4];
        for (l = 0; l < 4; l++) {
            for (k = 0; k < 3; k++) {            /* the trim Hold, 0, +50 at Vary 40, 200 bars */
                uint32_t t0;
                fresh();
                en_seed(17);
                en_set(P_TEMPO, 140);
                en_set(P_VARY, 82);         /* (format 35: the amount 0.4, the old 40) */
                en_set(P_MOTION, 0);
                en_set(VP[l], k * 50 - 50);
                en_play(1);
                run(1.0f);
                t0 = en_vary_turns[l];
                while (en_bars < 16u * 201u)
                    run(0.5f);
                turns[l][k] = (int)(en_vary_turns[l] - t0);
            }
            still[l] = 0;
            for (e = 0; e < 2; e++) {            /* Hold at Vary 100, and Vary 0 its trim 0 */
                uint32_t t0;
                fresh();
                en_seed(23);
                en_set(P_TEMPO, 140);
                en_set(P_VARY, e ? 0 : 100);
                en_set(P_MOTION, 0);
                en_set(VP[l], e ? 0 : -50);
                en_play(1);
                run(1.0f);
                t0 = en_vary_turns[l];
                while (en_bars < 16u * 101u)
                    run(0.5f);
                still[l] += (int)(en_vary_turns[l] - t0);
            }
            printf("  the %s trim Hold, 0, +50: %d, %d, %d changes in 200 bars (held, Vary 100 and Vary 0: %d)\n", VN[l], turns[l][0],
                   turns[l][1], turns[l][2], still[l]);
            /* (100 at least 1.4 times 50's: the noise's few changes in 200 bars are a handful, the random stream's
             * order moving them a little: 1.47 after the variety study's arc timer) */
            /* (and 50 at least 3: the bass's plan, a phrase at a time, 3 in 200 bars here since the variety study's
             * random stream) */
            CHECK(turns[l][0] == 0 && still[l] == 0 && turns[l][1] >= 3 && 10 * turns[l][2] >= 14 * turns[l][1],
                  "the %s trim: %d %d %d, held %d", VN[l], turns[l][0], turns[l][1], turns[l][2], still[l]);
            en_set(VP[l], 0);
        }
        en_set(P_VARY, 82);
        en_set(P_MOTION, 50);
        en_set(P_TEMPO, 122);
    }

    /* (1.4.0, the user's) the chord's Cutoff a knob around the piece's own: its base (the recipe's, P_CUTOFF) and the
     * knob's offset (P_CCUT) as one: 40 and +10 sound as 50 and 0, bit for bit; and the knob heard */
    {
        uint32_t h0, h1 = 0;
        int fd[2];
        pid_t pid;
        fresh();
        en_seed(5);
        en_set(P_CUTOFF, 45);
        en_play(1);
        run(1.0f);
        fflush(stdout);
        if (pipe(fd))
            return 1;
        pid = fork();
        if (pid == 0) {                          /* (from the same state: 40 and +10 here, 50 and 0 there) */
            en_set(P_CUTOFF, 40);
            en_set(P_CCUT, 10);
            h1 = out_hash(6.0f);
            if (write(fd[1], &h1, sizeof h1) != (ssize_t)sizeof h1)
                _exit(1);
            _exit(0);
        }
        en_set(P_CUTOFF, 50);
        en_set(P_CCUT, 0);
        h0 = out_hash(6.0f);
        if (read(fd[0], &h1, sizeof h1) != (ssize_t)sizeof h1)
            h1 = ~h0;
        waitpid(pid, 0, 0);
        close(fd[0]);
        close(fd[1]);
        CHECK(h0 == h1, "Cutoff: 40 and +10 not as 50");
        fresh();
        en_seed(5);
        en_set(P_CUTOFF, 50);
        en_play(1);
        CHECK(same_with(P_CCUT, -30) == 0, "Cutoff's knob not heard");
        printf("  the chord's Cutoff knob: around the piece's own, 40 and +10 as 50 (heard)\n");
        en_set(P_CUTOFF, 45);
        en_set(P_CCUT, 0);
    }

    /* (1.4.0, the user's) the chord's LFO's Destination: Reso or Colour moved (the sound differs), the Cutoff left be; on
     * the Cutoff, the others left be */
    {
        int d, heard[EN_NCLDEST];
        float mx[EN_NCLDEST][EN_NCLDEST];
        for (d = 0; d < EN_NCLDEST; d++) {
            int k, j;
            fresh();
            en_seed(4);
            en_set(P_MOVE, 100);                 /* (format 30: at its most, the wave alone) */
            en_set(P_CLTRIM, 50);
            en_lfo_solo = 1;
            en_set(P_CLRATE, 0);                 /* (a bar's wave: all of it heard in the time) */
            en_set(P_CLDEST, d);
            en_play(1);
            for (j = 0; j < EN_NCLDEST; j++)
                mx[d][j] = 0.0f;
            for (k = 0; k < 600; k++) {          /* (about 3.5 s) */
                en_render(buf, 256, 2048);
                for (j = 0; j < EN_NCLDEST; j++)
                    mx[d][j] = fmaxf(mx[d][j], fabsf(en_lfo_at[j]));
            }
            heard[d] = d ? !same_with(P_CLDEST, d) : 1;   /* (against the Cutoff's, the same engine from here) */
        }
        printf("  the LFO's Destination: on the Cutoff %.2f oct (Reso %.2f, Colour %.2f); on Reso %.2f (Cutoff %.2f), on "
               "Colour %.2f (Cutoff %.2f); Reso %s, Colour %s\n", (double)mx[0][0], (double)mx[0][1], (double)mx[0][2],
               (double)mx[1][1], (double)mx[1][0], (double)mx[2][2], (double)mx[2][0], heard[1] ? "heard" : "unheard",
               heard[2] ? "heard" : "unheard");
        CHECK(mx[0][0] > 1.5f && mx[0][1] == 0.0f && mx[0][2] == 0.0f, "LFO on the Cutoff: %f %f %f", (double)mx[0][0],
              (double)mx[0][1], (double)mx[0][2]);
        CHECK(mx[1][1] > 0.5f && mx[1][0] == 0.0f && mx[1][2] == 0.0f, "LFO on Reso: %f %f %f", (double)mx[1][0],
              (double)mx[1][1], (double)mx[1][2]);
        CHECK(mx[2][2] > 0.5f && mx[2][0] == 0.0f && mx[2][1] == 0.0f, "LFO on Colour: %f %f %f", (double)mx[2][0],
              (double)mx[2][1], (double)mx[2][2]);
        CHECK(heard[1] && heard[2], "LFO's Destination: Reso %d, Colour %d (1: heard)", heard[1], heard[2]);
        en_set(P_CLDEST, 0);
        en_lfo_solo = 0;
    }

    /* (1.4.0, the user's) the noise's LFO's Destination: the Filter as before; Tone or Air moved (the sound differs),
     * within their knobs' ranges (Tone near its top, Air near its top: never past them), the others left be */
    {
        int d, heard[EN_NNLDEST], inr = 1;
        float mx[EN_NNLDEST][EN_NNLDEST];
        for (d = 0; d < EN_NNLDEST; d++) {
            int k, j;
            fresh();
            en_seed(4);
            en_set(P_CRACKLE, 80);
            en_set(P_MOVE, 100);                 /* (format 30: at its most: the old Depth 100 and Drift 100's) */
            en_set(P_NLTRIM, 50);
            en_set(P_NLRATE, 0);                 /* (a bar's wave: all of it heard in the time) */
            en_set(P_NCOL, 40);
            en_set(P_NROOM, 90);
            en_set(P_NLDEST, d);
            en_play(1);
            for (j = 0; j < EN_NNLDEST; j++)
                mx[d][j] = 0.0f;
            for (k = 0; k < 600; k++) {          /* (about 3.5 s) */
                en_render(buf, 256, 2048);
                for (j = 0; j < EN_NNLDEST; j++)
                    mx[d][j] = fmaxf(mx[d][j], fabsf(en_nlfo_at[j]));
                if (0.8f + en_nlfo_at[EN_NLD_TONE] > 1.0001f || 0.8f + en_nlfo_at[EN_NLD_TONE] < -1.0001f ||
                    0.9f + en_nlfo_at[EN_NLD_AIR] > 1.0001f || 0.9f + en_nlfo_at[EN_NLD_AIR] < -0.0001f)
                    inr = 0;
            }
            heard[d] = d ? !same_with(P_NLDEST, d) : 1;   /* (against the Filter's, the same engine from here) */
        }
        printf("  the noise LFO's Destination: on the Filter %.2f (Tone %.2f, Air %.2f); on Tone %.2f (Filter %.2f), on Air "
               "%.2f (Filter %.2f); Tone %s, Air %s; %s their knobs' ranges\n", (double)mx[0][0], (double)mx[0][1],
               (double)mx[0][2], (double)mx[1][1], (double)mx[1][0], (double)mx[2][2], (double)mx[2][0],
               heard[1] ? "heard" : "unheard", heard[2] ? "heard" : "unheard", inr ? "within" : "PAST");
        CHECK(mx[0][0] > 0.5f && mx[0][1] == 0.0f && mx[0][2] == 0.0f, "noise LFO on the Filter: %f %f %f",
              (double)mx[0][0], (double)mx[0][1], (double)mx[0][2]);
        CHECK(mx[1][1] > 0.2f && mx[1][0] == 0.0f && mx[1][2] == 0.0f, "noise LFO on Tone: %f %f %f", (double)mx[1][0],
              (double)mx[1][1], (double)mx[1][2]);
        CHECK(mx[2][2] > 0.1f && mx[2][0] == 0.0f && mx[2][1] == 0.0f, "noise LFO on Air: %f %f %f", (double)mx[2][0],
              (double)mx[2][1], (double)mx[2][2]);
        CHECK(heard[1] && heard[2], "noise LFO's Destination: Tone %d, Air %d (1: heard)", heard[1], heard[2]);
        CHECK(inr, "noise LFO's Destination: past its knob's range");
        en_set(P_NLDEST, 0);
    }

    /* every setting at its extremes: still bounded */
    {
        int p;
        fresh();
        en_play(1);
        for (p = 0; p < P_NPARAMS; p++)
            en_set(p, en_param_info(p)->hi);
        en_set(P_MIDI, 1);
        peak = 0.0f;
        run(30.0f);
        CHECK(!bad && peak < 1.0f, "all at max: peak %f", (double)peak);
        en_set(P_CVOICE, 0);
        run(30.0f);
        CHECK(!bad && peak < 1.0f, "all at max, saws: peak %f", (double)peak);
        printf("  a minute at max: peak %.3f\n", (double)peak);
        for (p = 0; p < P_NPARAMS; p++)
            en_set(p, en_param_info(p)->lo);
        run(30.0f);
        CHECK(!bad, "all at min: NaN");
    }

    /* (1.4.0) the Arrangement page's meters: each part's alone (the others muted) moves its own meter only; the master
     * follows the output (its peak going into the soft clip: the output's own, a little more where it squashes) */
    {
        static const uint32_t M[6] = {EN_M_CHORD, EN_M_PINGS, EN_M_BASS, EN_M_KICK, EN_M_HATS, EN_M_NOISE};
        static const char *const N[6] = {"chord", "pings", "bass", "kick", "hats", "noise"};   /* (1.4.0, the user's: the
                                                  * kick and the hats apart, as the meters' rows) */
        int k, i;
        fresh();
        en_seed(3);                              /* (its own seed: not what the tests before it left) */
        en_set(P_MOTION, 0);                     /* (format 31: no part ridden out, Motion's, while it's listened to) */
        en_play(1);
        en_set(P_CRACKLE, 60);
        en_meter_on = 1;
        run(4.0f);
        for (i = 0; i < EN_NMETER; i++)
            en_meter[i] = 0.0f;
        peak = 0.0f;
        run(4.0f);
        for (i = 0; i < EN_NMETER; i++)
            CHECK(en_meter[i] > 0.003f, "meter %d moves with all playing (%f)", i, (double)en_meter[i]);
        CHECK(en_meter[EN_MT_MASTER] >= peak * 0.99f && en_meter[EN_MT_MASTER] < peak * 1.6f,
              "the master meter %f, the output %f", (double)en_meter[EN_MT_MASTER], (double)peak);
        for (k = 0; k < 6; k++) {
            en_mute(0x3Fu & ~M[k]);
            run(1.0f);
            for (i = 0; i < EN_NMETER; i++)
                en_meter[i] = 0.0f;
            peak = 0.0f;
            run(3.0f);
            for (i = 0; i < 17 && en_meter[k] <= 0.003f; i++)   /* (a part resting, a phrase at most: until it plays; the
                                                  * seed XORs into the engine's state, so what rests depends on the tests
                                                  * before) */
                run(1.0f);
            printf("  %s alone: meters", N[k]);
            for (i = 0; i < EN_NMETER; i++)
                printf(" %.4f", (double)en_meter[i]);
            printf("\n");
            CHECK(en_meter[k] > 0.003f, "%s alone: its meter %f", N[k], (double)en_meter[k]);
            for (i = 0; i < 6; i++)
                if (i != k)
                    CHECK(en_meter[i] < 3e-4f, "%s alone: meter %d at %f", N[k], i, (double)en_meter[i]);   /* (-70 dB: a
                                                  * muted part's last ring, -77 dB, under it) */
            CHECK(en_meter[EN_MT_MASTER] >= peak * 0.99f, "%s alone: the master %f, the output %f", N[k],
                  (double)en_meter[EN_MT_MASTER], (double)peak);
        }
        en_mute(0);
        en_meter_on = 0;
        for (i = 0; i < EN_MT_MASTER; i++)
            en_meter[i] = 0.0f;
        run(1.0f);
        for (i = 0; i < EN_MT_MASTER; i++)
            CHECK(en_meter[i] == 0.0f, "meter %d measured while off", i);
    }

    /* (1.4.0, the user's: the second try) the kick's rumble: none at 0; at its knob a low bed (under ~150 Hz, nearly
     * nothing over 200), mono, ducked on the kick (the beat's first sixteenth well under its last); the master never
     * over full scale at 100 (Pressure, the loudest kick, and Concrete) */
    {
        int16_t pp[P_NPARAMS];
        uint16_t pl;
        int k, i, pc;
        for (pc = 0; pc < 2; pc++) {
            const int piece = pc ? 4 : 6;
            double e = 0.0, eh = 0.0, eq[4] = {0, 0, 0, 0};
            int nq[4] = {0, 0, 0, 0};
            float lo1 = 0.0f, lo2 = 0.0f, mx = 0.0f, m0 = 0.0f;
            fresh();
            en_piece(piece, pp, &pl);
            for (k = 0; k < P_NPARAMS; k++)
                if (k != P_TUNE && k != P_MIDI)
                    en_set(k, pp[k]);
            en_pool(pl);
            en_set(P_RUMBLE, 0);
            en_regrow();
            en_play(1);
            run(8.0f);
            CHECK(same_with(P_RUMBLE, 0) == 1 && same_with(P_RUMBLE, 35) == 0, "the rumble: none at 0, heard at 35");
            rumble_ab(35);
            for (i = 44100; i < RB_TN; i++) {    /* (the first second left out: the tail building) */
                const float x = rb_m[i];
                float y;
                lo1 += 0.0350f * (x - lo1);      /* (two one-poles at ~250 Hz: what's over them, x - their low) */
                y = x - lo1;
                lo2 += 0.0350f * (y - lo2);
                y -= lo2;
                e += (double)(x * x);
                eh += (double)(y * y);
                eq[rb_q[i]] += (double)(x * x), nq[rb_q[i]]++;
            }
            for (k = 0; k < 4; k++)
                eq[k] /= nq[k] ? nq[k] : 1;
            printf("  the rumble (%s, 35): %.1f dB, %.1f%% of it over ~250 Hz, its side %.1f dB under it; "
                   "the beat's sixteenths %.1f %.1f %.1f %.1f dB\n", en_piece_name(piece),
                   10.0 * log10(e / (RB_TN - 44100) + 1e-20), 100.0 * eh / (e + 1e-20),
                   10.0 * log10(e / (rb_side + 1e-30)), 10.0 * log10(eq[0] + 1e-20), 10.0 * log10(eq[1] + 1e-20),
                   10.0 * log10(eq[2] + 1e-20), 10.0 * log10(eq[3] + 1e-20));
            CHECK(e / (RB_TN - 44100) > 1e-6, "the rumble at 35 heard (%g)", e / (RB_TN - 44100));
            CHECK(eh < 0.05 * e, "the rumble low: %.1f%% over ~250 Hz", 100.0 * eh / (e + 1e-20));
            CHECK(rb_side < 1e-3 * e, "the rumble mono (its side %.1f dB under it)", 10.0 * log10(e / (rb_side + 1e-30)));
            CHECK(eq[0] * 2.0 < eq[3], "the rumble ducked on the kick: the beat's first sixteenth %.1f dB, its last %.1f",
                  10.0 * log10(eq[0] + 1e-20), 10.0 * log10(eq[3] + 1e-20));
            {   /* (a minute, the master's peak each block: at 100 (a fork) and at 0, the same minute) */
                int fd[2], w;
                pid_t pid;
                float *pk;
                fflush(stdout);
                if (pipe(fd))
                    fd[0] = fd[1] = -1;
                pid = fork();
                for (w = 0; w < 2; w++) {
                    if ((pid == 0) != (w == 0))
                        continue;
                    pk = w == 0 ? &mx : &m0;
                    en_set(P_RUMBLE, w == 0 ? 100 : 0);
                    en_meter[EN_MT_MASTER] = 0.0f;
                    for (k = 0; k < 60 * 172; k++) {
                        en_render(buf, 256, 2048);
                        *pk = en_meter[EN_MT_MASTER] > *pk ? en_meter[EN_MT_MASTER] : *pk;
                        en_meter[EN_MT_MASTER] = 0.0f;
                    }
                }
                if (pid == 0) {
                    if (write(fd[1], &mx, sizeof mx) != (ssize_t)sizeof mx)
                        _exit(1);
                    _exit(0);
                }
                if (read(fd[0], &mx, sizeof mx) != (ssize_t)sizeof mx)
                    mx = 9.0f;
                waitpid(pid, 0, 0);
                close(fd[0]);
                close(fd[1]);
            }
            printf("  the rumble (%s, 100): the master's peak %.2f (%.2f without it)\n", en_piece_name(piece), (double)mx,
                   (double)m0);
            CHECK(mx < 1.0f && mx < m0 * 1.25f, "the rumble at 100: the master's peak %.2f (%.2f without it)", (double)mx,
                  (double)m0);
        }
    }

    /* (1.4.0, the user's) the Arrangement page's channel strips: each part's Echo and Plate sends and its Low cut reach
     * the engine (heard against 0; the defaults are the recipes' own: the sound as before, checked by the scenarios and
     * the renders); a Filter (1.4.0, format 28: the Low cut before) heard on each part either way (+25 a high-pass,
     * -25 a low-pass, against 0, off); its high-pass takes a part's lows (the bass alone: under ~120 Hz, +36, the old
     * Low cut's 100, against 0) */
    {
        static const uint8_t SP[] = {P_CECHO, P_CPLATE, P_PECHO, P_PPLATE, P_BECHO, P_BPLATE, P_DECHO, P_DPLATE,
                                     P_NECHO, P_NPLATE, P_KECHO, P_KPLATE, P_KLVL};   /* (1.4.0: the Kick strip's too) */
        static const uint8_t LP[] = {P_CLOW, P_PLOW, P_BLOW, P_DLOW, P_NLOW, P_KLOW};
        int16_t pp[P_NPARAMS];
        uint16_t pl;
        unsigned k;
        int heard = 0, low0 = 0, lowh = 0;
        fresh();
        en_piece(0, pp, &pl);
        for (k = 0; k < P_NPARAMS; k++)
            if (k != P_TUNE && k != P_MIDI)
                en_set((int)k, pp[k]);
        en_pool(pl);
        en_set(P_PINGS, 80);
        en_set(P_CRACKLE, 80);
        en_regrow();
        en_play(1);
        run(4.0f);
        for (k = 0; k < sizeof SP; k++)
            heard += same_with(SP[k], 100) == 0;
        for (k = 0; k < sizeof LP; k++) {
            low0 += same_pair(LP[k], -25, 0) == 0;   /* (low0: the low-pass heard; lowh: the high-pass) */
            lowh += same_pair(LP[k], 25, 0) == 0;
        }
        printf("  the strips: %d of %u sends heard at 100 against 0; the Filters' low-pass heard %d of %u, their high-pass %d\n",
               heard, (unsigned)sizeof SP, low0, (unsigned)sizeof LP, lowh);
        CHECK(heard == (int)sizeof SP && low0 == (int)sizeof LP && lowh == (int)sizeof LP,
              "the strips: sends heard %d, Filters' low-pass heard %d, high-pass %d", heard, low0, lowh);
        {   /* the bass alone, its lows: the Low cut at 0 (here) and at 100 (a fork), the same minute otherwise */
            int fd[2], w, b, j;
            pid_t pid;
            double lo[2] = {0.0, 0.0};
            en_mute(0x1Fu & ~(uint32_t)EN_M_BASS);
            fflush(stdout);
            if (pipe(fd))
                fd[0] = fd[1] = -1;
            pid = fork();
            for (w = 0; w < 2; w++) {
                float l1 = 0.0f;
                if ((pid == 0) != (w == 1))
                    continue;
                en_set(P_BLOW, w ? 36 : 0);   /* (+36: ~600 Hz, the old Low cut's 100) */
                for (b = 0; b < 8 * 172; b++) {
                    en_render(buf, 256, 2048);
                    for (j = 0; j < 256; j++) {
                        l1 += 0.017f * ((float)(buf[2 * j] + buf[2 * j + 1]) * (0.5f / 8388608.0f) - l1);   /* (~120 Hz) */
                        lo[w] += (double)(l1 * l1);
                    }
                }
            }
            if (pid == 0) {
                if (write(fd[1], &lo[1], sizeof lo[1]) != (ssize_t)sizeof lo[1])
                    _exit(1);
                _exit(0);
            }
            if (read(fd[0], &lo[1], sizeof lo[1]) != (ssize_t)sizeof lo[1])
                lo[1] = lo[0];
            waitpid(pid, 0, 0);
            close(fd[0]);
            close(fd[1]);
            en_mute(0);
            printf("  the bass's Low cut at 100: its lows under ~120 Hz %.1f dB against 0\n",
                   10.0 * log10((lo[1] + 1e-20) / (lo[0] + 1e-20)));
            CHECK(lo[0] > 1e-3 && lo[1] < 0.25 * lo[0], "the bass's Low cut at 100: its lows %.1f dB (want under -6)",
                  10.0 * log10((lo[1] + 1e-20) / (lo[0] + 1e-20)));
        }
    }

    /* (1.4.0, the user's: "I can throw the hats to the echo"; the moves rethought: a throw a part, its black key held)
     * the hats, the pings, the bass, the kick thrown (EN_PF_HATTHROW, _PTHROW, _BTHROW, _KTHROW; the kick's the noise's
     * before, the user's, zzq): each into the echo, its tail ringing after it's let go (the part alone: the second after
     * it, well over the same without); the kick never with the others (the kick alone: the same, to a hair: the echo's
     * feedback pushed while held, on nothing); the kick's sends only the kick (the hats' tail rings longer with the
     * feedback, as with every throw: not checked). (zzq: each throw on its own bus, at the part's own level, the
     * feedback pushed towards THR_FB) And the
     * Kick strip's Low cut leaves the rumble be (the kick's ear before it). (The bass the least: the echo takes nothing
     * under ~350 Hz, so a Sub is thrown its upper harmonics alone: +1.5 dB at least) */
    {
        static const uint32_t TH[4] = {EN_PF_HATTHROW, EN_PF_PTHROW, EN_PF_BTHROW, EN_PF_KTHROW},
                              AL[4] = {EN_M_HATS, EN_M_PINGS, EN_M_BASS, EN_M_KICK},
                              OT[4] = {EN_M_KICK, EN_M_KICK, EN_M_KICK, EN_M_HATS};   /* (the part that mustn't go) */
        static const char *const TN[4] = {"hats", "pings", "bass", "kick"};
        int16_t pp[P_NPARAMS];
        uint16_t pl;
        unsigned k;
        int w, q;
        double e[2][2];
        for (q = 0; q < 4; q++) {
            for (w = 0; w < 2; w++) {
                int t, fd[2];
                pid_t pid;
                fresh();
                en_piece(0, pp, &pl);
                for (k = 0; k < P_NPARAMS; k++)
                    if (k != P_TUNE && k != P_MIDI)
                        en_set((int)k, pp[k]);
                en_pool(pl);
                en_set(P_HITS, 90);
                en_set(P_PINGS, 80);
                en_set(P_CRACKLE, 80);
                if (q == 2)                      /* (the bass on Growl: its upper harmonics, what the echo takes; a
                                                  * Sub's, near none, left the check to the random stream's order) */
                    en_set(P_BVOICE, 4);
                en_seed(5);
                en_regrow();
                en_mute(0x3Fu & ~(w ? OT[q] : AL[q]));
                en_play(1);
                run(4.0f);
                fflush(stdout);                  /* (from here two ways, a fork: thrown or not, the same otherwise; the
                                                  * seed XORs into the engine's state, so two runs would differ) */
                if (pipe(fd))
                    break;
                pid = fork();
                t = pid == 0;
                en_perf(t ? TH[q] : 0u);
                run(2.0f);
                en_perf(0);
                run(0.05f);
                en_mute(0x3Fu);                  /* (the tail alone: everything stopped, the echo ringing on; a moment
                                                  * first, the bass's own release gone) */
                run(0.4f);
                sum2 = 0.0f;
                run(1.5f);
                e[w][t] = (double)sum2;
                if (t) {
                    if (write(fd[1], &e[w][1], sizeof e[w][1]) != (ssize_t)sizeof e[w][1])
                        _exit(1);
                    _exit(0);
                }
                if (read(fd[0], &e[w][1], sizeof e[w][1]) != (ssize_t)sizeof e[w][1])
                    e[w][1] = -1.0;
                waitpid(pid, 0, 0);
                close(fd[0]);
                close(fd[1]);
            }
            en_mute(0);
            printf("  the %s thrown: their tail %.1f dB over not thrown; the %s's %.2f dB (none)\n", TN[q],
                   10.0 * log10((e[0][1] + 1e-20) / (e[0][0] + 1e-20)), q == 3 ? "hats" : "kick",
                   10.0 * log10((e[1][1] + 1e-20) / (e[1][0] + 1e-20)));
            /* (the bass x1.3: the echo takes little under ~350 Hz; the rest x2) */
            CHECK(e[0][1] > (q == 2 ? 1.3 : 2.0) * e[0][0] && e[0][1] > 1e-4, "the %s thrown: their tail %g against %g", TN[q], e[0][1],
                  e[0][0]);
            CHECK(q == 3 || (e[1][1] < 1.05 * e[1][0] + 1e-6 && e[1][1] > 0.95 * e[1][0] - 1e-6),
                  "the kick not thrown with the %s: %g against %g", TN[q], e[1][1], e[1][0]);   /* (the kick's own: the
                                                  * hats not sent, but their echo's feedback pushed while it's held, as
                                                  * every throw's: their tail longer, not checked) */
        }
    }
    {   /* the rumble's bed the same with the kick's Low cut on: the kick alone, under ~60 Hz, the rumble's share (on less
         * off) with the cut at 100 as without it */
        int16_t pp[P_NPARAMS];
        uint16_t pl;
        unsigned k;
        double rb[4];
        int t;
        for (t = 0; t < 4; t++) {                /* (t: bit 0 the rumble on, bit 1 the Low cut) */
            fresh();
            en_piece(6, pp, &pl);                /* (Pressure: its rumble) */
            for (k = 0; k < P_NPARAMS; k++)
                if (k != P_TUNE && k != P_MIDI)
                    en_set((int)k, pp[k]);
            en_pool(pl);
            en_set(P_RUMBLE, t & 1 ? 60 : 0);
            en_set(P_KLOW, t & 2 ? 36 : 0);   /* (+36: the old Low cut's 100) */
            en_seed(5);
            en_regrow();
            en_mute(0x3Fu & ~(uint32_t)EN_M_KICK);
            en_play(1);
            run(6.0f);
            {
                int b, j;
                float l1 = 0.0f;
                rb[t] = 0.0;
                for (b = 0; b < 4 * 172; b++) {
                    en_render(buf, 256, 2048);
                    for (j = 0; j < 256; j++) {
                        l1 += 0.0085f * ((float)(buf[2 * j] + buf[2 * j + 1]) * (0.5f / 8388608.0f) - l1);   /* (~60 Hz) */
                        rb[t] += (double)(l1 * l1);
                    }
                }
            }
        }
        en_mute(0);
        {
            const double r0 = rb[1] - rb[0], r1 = rb[3] - rb[2];
            printf("  the kick's Low cut at 100: the rumble's lows %.1f dB against the cut off (the kick's own %.1f dB)\n",
                   10.0 * log10((fabs(r1) + 1e-20) / (fabs(r0) + 1e-20)), 10.0 * log10((rb[2] + 1e-20) / (rb[0] + 1e-20)));
            CHECK(r0 > 1e-6 && r1 > 0.5 * r0 && r1 < 2.0 * r0, "the kick's Low cut moved the rumble: %g against %g", r1, r0);
        }
    }

    /* (1.4.0, the user's: song structure) Endless as ever, the song's knobs heard nowhere; a song: its intro, main
     * and outro their bars, the parts in in the Kind's order and out in the outro's, the kick One, Half, Four, then
     * the grid stopped (PLAY off) and quiet; the outro now (from the next phrase, in Endless too) and back */
    {
        static uint8_t ph_at[96];
        static uint16_t kl_at[96];
        static const char *const PN[EN_NPART] = {"chord", "pings", "bass", "kick", "hats", "noise"};
        int kind, first_in[EN_NPART], last_in[EN_NPART], i, k, ok;
        fresh();
        en_seed(5);
        en_play(1);
        run(1.0f);
        ok = same_with(P_SINTRO, 16) && same_with(P_SKIND, 1) && same_with(P_SMAIN, 64) && same_with(P_SOUTRO, 16);
        CHECK(ok, "Endless: the song's knobs changed the sound");
        printf("  Endless: the song's knobs heard nowhere\n");
        for (kind = 0; kind < 2; kind++) {
            uint32_t nb = 0;
            fresh();
            en_seed(9 + (uint32_t)kind);
            en_set(P_SMODE, 1);
            en_set(P_SINTRO, 2);
            en_set(P_SMAIN, 4);
            en_set(P_SOUTRO, 2);
            en_set(P_SKIND, kind);
            en_set(P_TEMPO, 140);
            en_play(1);
            memset(ph_at, 0xFF, sizeof ph_at);
            for (i = 0; i < EN_NPART; i++)
                first_in[i] = -1, last_in[i] = -1;
            for (i = 0; i < 30000 && nb < 90u; i++) {
                en_render(buf, 256, 2048);
                if (!en_playing)
                    break;
                if (en_bars) {
                    const uint32_t b = (en_bars - 1u) / 16u;
                    nb = b;
                    if (b < 96u) {
                        ph_at[b] = en_song_ph;
                        if ((en_bars - 1u) % 16u == 8u)
                            kl_at[b] = en_lane[1];
                    }
                    for (k = 0; k < EN_NPART; k++)
                        if (en_fader[k] > 0.01f) {
                            if (first_in[k] < 0)
                                first_in[k] = (int)b;
                            last_in[k] = (int)b;
                        }
                }
            }
            ok = 1;
            for (i = 0; i < 64; i++)
                if (ph_at[i] != (i < 16 ? EN_SG_INTRO : i < 48 ? EN_SG_MAIN : EN_SG_OUTRO) && ok)
                    printf("  (bar %d: place %d)\n", i, ph_at[i]), ok = 0;
            CHECK(ok, "the song's places by bar (kind %d)", kind);
            CHECK(!en_playing && en_song_ph == EN_SG_END && en_bars == 64u * 16u,
                  "the song over: PLAY off at bar 64 (playing %d, place %d, %u sixteenths)", en_playing, en_song_ph, en_bars);
            printf("  a song (16, 32, 16 bars, %s first): in at bars", kind ? "the pulse" : "ambient");
            for (k = 0; k < EN_NPART; k++)
                printf(" %s %d", PN[k], first_in[k]);
            printf("; out after");
            for (k = 0; k < EN_NPART; k++)
                printf(" %s %d", PN[k], last_in[k]);
            printf("\n");
            if (!kind)
                CHECK(first_in[5] == 0 && first_in[5] <= first_in[0] && first_in[0] < first_in[3] &&
                          first_in[3] <= first_in[2] && first_in[2] < first_in[1] && first_in[1] < first_in[4],   /* (the
                                                  * kick and the bass a bar off their places either way: they may
                                                  * share one) */
                      "ambient first: the order in");
            else
                CHECK(first_in[3] == 0 && first_in[2] == 0 && first_in[0] < first_in[1] && first_in[1] <= first_in[5] &&   /* (the
                                                  * entries a bar off the line either way: the pings and the noise may
                                                  * share one) */
                          first_in[5] < first_in[4],
                      "pulse first: the order in");
            CHECK(first_in[4] >= 10 && first_in[4] < 16, "the hats last, in the intro (%d)", first_in[4]);
            CHECK(last_in[4] < last_in[1] && last_in[1] <= last_in[2] && last_in[2] < last_in[3] &&
                      last_in[3] < last_in[0] && last_in[3] < last_in[5] && last_in[0] >= 60 && last_in[5] >= 60,
                  "the outro's order out: the hats first, the chord and the noise last");
            {   /* the kick in the intro: One, then Half, then Four (its A bars; the B bars, 4 and 8, sparser) */
                int prev = 0, grow = 1, last4 = 1;
                for (i = 0; i < 16; i++) {
                    const int n = popcount(kl_at[i]);
                    if ((i & 3) == 3 || (i & 7) == 7 || !n)
                        continue;
                    grow &= n >= prev;
                    prev = n;
                }
                last4 = popcount(kl_at[14]) == 4;  /* (Four by the intro's last A bar) */
                if (!grow || !last4)
                    for (i = 0; i < 16; i++)
                        printf("  (bar %d: the kick %04X)\n", i, kl_at[i]);
                CHECK(grow && last4 && popcount(kl_at[first_in[3] + 1 < 16 ? first_in[3] + 1 : 15]) <= 2,
                      "the intro's kick: One, Half, Four");
            }
            peak = 0.0f;
            run(40.0f);
            peak = 0.0f;
            run(1.0f);
            en_set(P_CRACKLE, 0);
            CHECK(!en_playing && peak < 0.05f, "after the song, the tails gone (%f)", (double)peak);
        }
        printf("  a song's places and parts, the kick One, Half, Four, the end\n");
        /* the outro now: in Endless, from the next phrase; again in it: back, playing on; and once more, to the end */
        fresh();
        en_seed(3);
        en_set(P_SOUTRO, 2);
        en_play(1);
        while (en_bars < 10u * 16u + 2u)
            run(0.05f);
        en_outro();
        run(0.05f);
        CHECK(en_song_bo == 16u && en_song_be == 32u && en_song_ph == EN_SG_MAIN, "outro now: from bar 16 (%u, %u, %d)",
              en_song_bo, en_song_be, en_song_ph);
        while (en_bars < 17u * 16u + 2u)
            run(0.05f);
        CHECK(en_song_ph == EN_SG_OUTRO, "outro now: in it at bar 17 (%d)", en_song_ph);
        while (en_bars < 22u * 16u + 2u)
            run(0.05f);
        CHECK(en_fader[4] < 0.01f, "outro now: the hats gone (%f)", (double)en_fader[4]);
        en_outro();
        run(0.05f);
        CHECK(en_song_ph == EN_SG_OFF && en_playing, "outro now, again: back to Endless (%d)", en_song_ph);
        run(12.0f);
        for (k = 0, ok = 1; k < EN_NPART; k++)
            ok &= en_fader[k] > 0.999f;
        CHECK(ok, "back: every part up again");
        en_outro();
        run(0.05f);
        while (en_playing && en_bars < 200u * 16u)
            run(0.5f);
        CHECK(!en_playing && en_song_ph == EN_SG_END, "outro now: played to its end, stopped");
        /* a song: the outro asked early, then let go: the outro as planned (or four phrases on, at least) */
        fresh();
        en_seed(4);
        en_set(P_SMODE, 1);
        en_set(P_SINTRO, 0);
        en_set(P_SMAIN, 8);
        en_set(P_SOUTRO, 2);
        en_play(1);
        run(0.05f);
        CHECK(en_song_ph == EN_SG_MAIN && en_song_bo == 64u, "no intro: the main from bar 1, the outro at 64 (%d, %u)",
              en_song_ph, en_song_bo);
        while (en_bars < 9u * 16u + 2u)
            run(0.05f);
        en_outro();
        run(0.05f);
        CHECK(en_song_bo == 16u, "a song's outro now: from bar 16 (%u)", en_song_bo);
        en_outro();
        run(0.05f);
        CHECK(en_song_bo == 64u && en_song_ph == EN_SG_MAIN, "let go: the outro as planned, bar 64 (%u)", en_song_bo);
        en_set(P_SMAIN, 4);                      /* (Main turned as it plays: the outro moves) */
        run(0.05f);
        CHECK(en_song_bo == 32u, "Main turned while it plays: the outro at 32 (%u)", en_song_bo);
        en_set(P_SMODE, 0);                      /* (to Endless: the song let go, it plays on) */
        run(0.05f);
        CHECK(en_song_ph == EN_SG_OFF && en_playing, "Endless again: no outro (%d)", en_song_ph);
        printf("  the outro now, in Endless and in a song, and back\n");
    }

    /* (1.4.0, the user's: the dub mixer; format 30: its rides Form's Drift's, its moves the Mixer's) Mixer and Drift 0:
     * nothing rides; up: each part's level and sends ride slowly, within
     * their bounds (the levels never over +3 dB), mostly off the phrase lines, a part out and back or a swell now and
     * then, the master never into the clip; Lock rests it; a song's intro and outro left to the song (no moves, the
     * rides at half depth); back to 0: every ride home, exactly */
    {
        float lo[EN_NPART], hi[EN_NPART], elo = 99.0f, ehi = -99.0f, mp = 0.0f, mp0 = 0.0f, ilv = 0.0f;
        uint32_t m0, l0, m1;
        int k, b, ok, evs = 0, was = 0, inev = 0;
        fresh();
        en_seed(9);
        en_set(P_MOTION, 0);                     /* (format 31: the moves Motion's, the Mixer's before) */
        en_meter_on = 1;
        en_play(1);
        m0 = en_mx_moves;
        for (b = 0; b < (int)(60.0f * 44100.0f / 256.0f); b++) {   /* (the master's peak without it) */
            en_render(buf, 256, 2048);
            if (en_meter[EN_MT_MASTER] > mp0)
                mp0 = en_meter[EN_MT_MASTER];
            for (k = 0; k < EN_NMETER; k++)
                en_meter[k] = 0.0f;
        }
        for (k = 0, ok = 1; k < EN_NPART; k++)
            ok &= en_mx_db[k] == 0.0f && en_mx_snd[k][0] == 0.0f && en_mx_snd[k][1] == 0.0f;
        CHECK(ok && en_mx_moves == m0, "Motion 0, Drift 0: nothing rides (%u moves)", en_mx_moves - m0);
        en_set(P_MOTION, 100);
        en_set(P_MOVE, 100);
        for (k = 0; k < EN_NPART; k++)
            lo[k] = 99.0f, hi[k] = -99.0f;
        m0 = en_mx_moves, l0 = en_mx_line;
        for (b = 0; b < (int)(480.0f * 44100.0f / 256.0f); b++) {   /* (eight minutes) */
            en_render(buf, 256, 2048);
            if (en_meter[EN_MT_MASTER] > mp)
                mp = en_meter[EN_MT_MASTER];
            for (k = 0; k < EN_NMETER; k++)
                en_meter[k] = 0.0f;
            for (k = 0; k < EN_NPART; k++) {
                if (en_mx_ev != 1 && en_mx_db[k] < lo[k])   /* (the drift: a part out and back apart) */
                    lo[k] = en_mx_db[k];
                if (en_mx_db[k] > hi[k])
                    hi[k] = en_mx_db[k];
                if (en_mx_snd[k][0] < elo)
                    elo = en_mx_snd[k][0];
                if (en_mx_snd[k][0] > ehi)
                    ehi = en_mx_snd[k][0];
            }
            if (en_mx_ev && !was)
                evs++;
            was = en_mx_ev;
        }
        for (k = 0, ok = 1; k < EN_NPART; k++)
            ok &= hi[k] <= 3.01f && hi[k] - lo[k] >= (k == 3 ? 0.3f : 1.5f) && lo[k] >= -7.01f;
        printf("  the dub mixer at 100, 8 minutes: %u moves, %.0f%% on a phrase's line; %d out-and-back or swells; the "
               "levels %+.1f..%+.1f dB (the chord), %+.1f..%+.1f (the kick); the echo sends %+.1f..%+.1f dB; the master's "
               "peak %.2f (%.2f at 0)\n", en_mx_moves - m0, 100.0 * (en_mx_line - l0) / (double)(en_mx_moves - m0 + 1u),
               evs, (double)lo[0], (double)hi[0], (double)lo[3], (double)hi[3], (double)elo, (double)ehi, (double)mp,
               (double)mp0);
        CHECK(ok, "the dub mixer's level rides within their bounds, and riding (the chord %+.1f..%+.1f, the kick %+.1f..%+.1f)",
              (double)lo[0], (double)hi[0], (double)lo[3], (double)hi[3]);
        CHECK(en_mx_moves - m0 > 200u, "the dub mixer moves (%u in 8 minutes)", en_mx_moves - m0);
        CHECK((en_mx_line - l0) * 100u < 35u * (en_mx_moves - m0) && (en_mx_line - l0) * 100u > 4u * (en_mx_moves - m0),
              "the dub mixer off the phrase lines, mostly (%u of %u on one)", en_mx_line - l0, en_mx_moves - m0);
        CHECK(evs >= 1, "the dub mixer's moves: a part out and back, or a swell (%d)", evs);
        CHECK(elo >= -8.01f && ehi <= 16.01f, "the echo sends' rides within bounds (%+.1f..%+.1f dB)", (double)elo, (double)ehi);
        CHECK(mp < 1.0f && !bad, "the dub mixer at 100: the master never into the clip (%.2f)", (double)mp);
        en_lock(1);                              /* (Lock: no new places, no new moves) */
        run(1.0f);
        m1 = en_mx_moves;
        run(90.0f);
        CHECK(en_mx_moves == m1, "Lock rests the dub mixer (%u moves)", en_mx_moves - m1);
        en_lock(0);
        en_set(P_MOTION, 0);                     /* (back to 0: home, exactly) */
        en_set(P_MOVE, 0);
        run(30.0f);
        for (k = 0, ok = 1; k < EN_NPART; k++)
            ok &= en_mx_db[k] == 0.0f && en_mx_snd[k][0] == 0.0f && en_mx_snd[k][1] == 0.0f;
        CHECK(ok && !en_mx_ev, "Motion and Drift back to 0: every ride home");
        /* a song: the intro the song's (no moves; the rides at half depth), the main the mixer's too */
        fresh();
        en_seed(5);
        en_set(P_MOTION, 100);
        en_set(P_MOVE, 100);
        en_set(P_SMODE, 1);
        en_set(P_SINTRO, 8);
        en_set(P_SMAIN, 16);
        en_set(P_SOUTRO, 2);
        en_play(1);
        run(0.05f);
        while (en_playing && en_bars < 64u * 16u) {
            run(0.1f);
            inev |= en_mx_ev != 0;
            for (k = 0; k < EN_NPART; k++)
                if (fabsf(en_mx_db[k]) > ilv)
                    ilv = fabsf(en_mx_db[k]);
        }
        CHECK(!inev && ilv <= 3.6f, "a song's intro: the dub mixer's moves wait, its rides half deep (%.1f dB)", (double)ilv);
        {
            uint32_t mm;
            float mlv = 0.0f;
            while (en_playing && en_song_ph == EN_SG_INTRO)
                run(0.1f);
            mm = en_mx_moves;
            while (en_playing && en_song_ph == EN_SG_MAIN) {
                run(0.5f);
                for (k = 0; k < EN_NPART; k++)
                    if (en_mx_ev != 1 && fabsf(en_mx_db[k]) > mlv)
                        mlv = fabsf(en_mx_db[k]);
            }
            CHECK(en_mx_moves > mm + 50u && mlv > ilv, "a song's main: the dub mixer at full depth (%.1f dB, %u moves)",
                  (double)mlv, en_mx_moves - mm);
        }
        printf("  the dub mixer: off at 0, riding within bounds, Lock resting it, a song's intro left to the song\n");
    }

    /* (1.4.0, format 28, KellyOffski's) a strip's Filter, either way: the chord alone, a fork from one moment, the filter
     * off in one and on in the other; the low-pass at -40 (~500 Hz) takes its highs (over ~2 kHz), the high-pass at +36 its lows
     * (under ~150 Hz), on the dry and on what it sent (the echo's tail, everything muted after: the filter before the
     * sends); the bass, not filtered, left as it is */
    {
        static const int16_t FV[2] = {-40, 36};
        int16_t pp[P_NPARAMS];
        uint16_t pl;
        unsigned k;
        int w;
        for (w = 0; w < 2; w++) {
            double r[2][2], bs[2];
            int t, fd[2];
            pid_t pid;
            fresh();
            en_piece(0, pp, &pl);
            for (k = 0; k < P_NPARAMS; k++)
                if (k != P_TUNE && k != P_MIDI)
                    en_set((int)k, pp[k]);
            en_set(P_MOVE, 0);                   /* (format 30: the sound still, the echo's band its middle's: the
                                                  * filter alone heard) */
            en_set(P_CECHO, 100);                /* (zzq: the chord well into the echo, so its tail is there to measure
                                                  * whatever the tests before left the random stream at) */
            en_set(P_ECHO, 80);
            en_set(P_ETONE, 100);                /* (and bright: its highs there for the low-pass to take) */
            en_pool(pl);
            en_seed(9);
            en_regrow();
            en_mute(0x3Fu & ~(uint32_t)EN_M_CHORD);
            en_play(1);
            run(4.0f);
            fflush(stdout);
            if (pipe(fd))
                break;
            pid = fork();
            t = pid == 0;
            en_set(P_CLOW, t ? FV[w] : 0);
            {
                int ph, b, j;
                for (ph = 0; ph < 2; ph++) {     /* (ph 0 the chord, 1 its echo's tail alone) */
                    float lp = 0.0f, hp = 0.0f, x0 = 0.0f;
                    double e = 0.0;
                    if (ph == 1) {
                        en_mute(0x3Fu);
                        run(0.3f);
                    }
                    for (b = 0; b < (ph ? 172 : 4 * 172); b++) {
                        en_render(buf, 256, 2048);
                        for (j = 0; j < 256; j++) {
                            const float x = (float)(buf[2 * j] + buf[2 * j + 1]) * (0.5f / 8388608.0f);
                            lp += 0.021f * (x - lp);                    /* (~150 Hz) */
                            hp = 0.75f * (hp + x - x0), x0 = x;        /* (over ~2 kHz) */
                            e += w ? (double)(lp * lp) : (double)(hp * hp);
                        }
                    }
                    r[ph][t] = e;
                }
            }
            if (t) {
                if (write(fd[1], r, sizeof r) != (ssize_t)sizeof r)
                    _exit(1);
                _exit(0);
            }
            {
                double rc[2][2];
                if (read(fd[0], rc, sizeof rc) != (ssize_t)sizeof rc)
                    rc[0][1] = rc[1][1] = -1.0;
                r[0][1] = rc[0][1], r[1][1] = rc[1][1];
            }
            waitpid(pid, 0, 0);
            close(fd[0]);
            close(fd[1]);
            bs[0] = r[0][0], bs[1] = r[1][0];
            printf("  the chord's Filter at %+d: its %s %.1f dB, its echo's %.1f dB, against off\n", FV[w], w ? "lows" : "highs",
                   10.0 * log10((r[0][1] + 1e-20) / (bs[0] + 1e-20)), 10.0 * log10((r[1][1] + 1e-20) / (bs[1] + 1e-20)));
            CHECK(r[0][1] < 0.5 * bs[0] && r[1][1] < 0.5 * bs[1] && bs[1] > 1e-8,   /* (zzq: the echo's tail there; its
                                                  * highs small, the tape at 11 kHz: 1e-6 held by the random stream's luck) */
                  "the chord's Filter at %+d: the dry %g against %g, the echo %g against %g", FV[w], r[0][1], bs[0], r[1][1], bs[1]);
        }
        en_mute(0);
    }

    /* (1.4.0, format 28) the low-pass while the engine sheds (the FM-1's load past 75%: en_load): its 6 dB stage
     * alone, then the 12 dB back, clean both ways (no NaN, no jump in the output over a soft bound) */
    {
        int b, j;
        float last = 0.0f, jump = 0.0f;
        fresh();
        en_set(P_CLOW, -40);
        en_set(P_PLOW, -40);
        en_set(P_BLOW, -40);
        en_play(1);
        run(2.0f);
        bad = 0;
        for (b = 0; b < 6 * 172; b++) {
            en_load = (uint8_t)((b / 172) & 1 ? 0 : 90);   /* (a second heavy, a second light, in turn) */
            en_render(buf, 256, 2048);
            for (j = 0; j < 256; j++) {
                const float x = (float)buf[2 * j] / 8388608.0f, d = fabsf(x - last);
                if (x != x)
                    bad++;
                if (d > jump)
                    jump = d;
                last = x;
            }
        }
        en_load = 0;
        printf("  the low-pass shedding and back: the biggest step a sample %.3f\n", (double)jump);
        CHECK(!bad && jump < 0.5f, "the low-pass shedding: %d bad, a step of %f", bad, (double)jump);
        en_set(P_CLOW, 0);
        en_set(P_PLOW, 0);
        en_set(P_BLOW, 0);
    }

    /* (1.4.0, format 28, KellyOffski's) the tempo from 40 BPM: at 40, 60 and 90 the grid runs at its rate, the sound clean
     * (no NaN, no clipping), the echo's longest times capped to its tape and shown as they are ("5.6 s" at 40 for a bar:
     * 6 s would not fit; at 90, "1 bar") */
    {
        static const int TT[3] = {40, 60, 90};
        int i;
        char tb[16];
        for (i = 0; i < 3; i++) {
            uint32_t b0;
            fresh();
            en_set(P_TEMPO, TT[i]);
            en_set(P_ETIME, 5);
            en_play(1);
            run(1.0f);
            b0 = en_bars;
            peak = 0.0f;
            bad = 0;
            run(10.0f);
            {
                const float rate = (float)(en_bars - b0) / 10.0f, want = (float)TT[i] * 4.0f / 60.0f;
                printf("  tempo %d: %.2f sixteenths a second (%.2f), peak %.2f\n", TT[i], (double)rate, (double)want,
                       (double)peak);
                CHECK(fabsf(rate - want) < 0.3f && !bad && peak < 1.0f, "tempo %d: %.2f sixteenths a second, peak %f",
                      TT[i], (double)rate, (double)peak);
            }
            en_param_text(P_ETIME, 5, tb);
            CHECK(!strcmp(tb, TT[i] == 40 ? "5.6 s" : "1 bar"), "tempo %d: a bar's echo shown \"%s\"", TT[i], tb);
        }
        en_set(P_TEMPO, 122);
        en_set(P_ETIME, 1);
    }

    /* (1.4.0, format 28, KellyOffski's) the Japanese scales: five notes each, in the table; their chords' voicings never with
     * two neighbours a semitone apart (spread to a ninth) */
    {
        static const struct { int m; uint16_t pool; } JS[5] = {
            {MODE_HIRAJOSHI, 1u | 1u << 2 | 1u << 3 | 1u << 7 | 1u << 8}, {MODE_IN, 1u | 1u << 1 | 1u << 5 | 1u << 7 | 1u << 8},
            {MODE_IWATO, 1u | 1u << 1 | 1u << 5 | 1u << 6 | 1u << 10}, {MODE_YO, 1u | 1u << 2 | 1u << 5 | 1u << 7 | 1u << 9},
            {MODE_INSEN, 1u | 1u << 1 | 1u << 5 | 1u << 7 | 1u << 10}};
        int i, d, sh, k, near = 0;
        for (i = 0; i < 5; i++) {
            CHECK(EN_MODE_POOL[JS[i].m] == JS[i].pool && strlen(EN_MODE_NAME[JS[i].m]) <= 8, "the scale %s: its notes",
                  EN_MODE_NAME[JS[i].m]);
            for (d = 0; d < 5; d++)
                for (sh = 0; sh < 2; sh++) {
                    int iv[4];
                    en_voicing(JS[i].pool, d, sh, iv);
                    for (k = 1; k < 4; k++)
                        near += iv[k] - iv[k - 1] == 1;
                }
        }
        printf("  the Japanese scales: their voicings' semitone neighbours %d\n", near);
        CHECK(near == 0, "the Japanese scales' voicings: %d semitone neighbours", near);
    }

    /* stop: the tails die away (but the vinyl turns on) */
    fresh();
    en_play(1);
    run(20.0f);
    en_play(0);
    run(30.0f);
    peak = 0.0f;
    run(1.0f);
    printf("  30 s after stop, crackle on: %.4f\n", (double)peak);
    CHECK(peak < 0.05f, "quiet after stopping, crackle on (%f)", (double)peak);
    en_set(P_CRACKLE, 0);
    run(1.0f);
    peak = 0.0f;
    run(1.0f);
    CHECK(peak < 0.01f, "quiet after stopping (%f)", (double)peak);
    CHECK(midi_count > 50u, "MIDI out (%u)", midi_count);

    /* (1.4.0, the FM-1's peaks) a piece sent while playing (every parameter, the pool, a seed, a regrow: as the UI's
     * set_piece) goes in a few blocks, not one (a block takes 28 commands' worth, a regrow ten); a knob's at once, as
     * ever; and once through, the piece plays (the regrow's patterns there) */
    {
        int16_t pp[P_NPARAMS];
        uint16_t pl;
        int k, b, left1 = -1, done_at = -1;
        fresh();
        en_seed(5);
        en_play(1);
        run(2.0f);
        en_piece(3, pp, &pl);
        for (k = 0; k < P_NPARAMS; k++)
            if (k != P_TUNE && k != P_MIDI)
                en_set(k, pp[k]);
        en_pool(pl);
        en_seed(6);
        en_regrow();
        for (b = 0; b < 40 && done_at < 0; b++) {
            en_render(buf, 256, 2048);
            if (b == 0)
                left1 = (int)en_queue_left();
            if (!en_queue_left())
                done_at = b;
        }
        printf("  a piece sent while playing: %d commands left after its first block, all through by block %d\n", left1,
               done_at);
        CHECK(left1 > P_NPARAMS - 40 && done_at > 2 && done_at < 12, "a piece spread: %d left after the first block, through "
              "by block %d", left1, done_at);
        en_set(P_ECHO, 60);                      /* (a knob's: at once) */
        en_render(buf, 256, 2048);
        CHECK(en_queue_left() == 0, "a knob's change at once (%u left)", en_queue_left());
        run(4.0f);
        CHECK(!bad, "a piece spread: NaN");
    }

    /* (last: the other tests' random draws as they were) */
    /* (1.4.0, the user's) the dub sirens, in two keys (F Dorian; D Iwato, its fifth a b5) at two tempos: their turning
     * points on the root, the fifth, the octave (the root's pitch class the key's); Wail's and Hi-lo's cycles the bar's,
     * Whoop's and the Laser's the same at either tempo; a tap's played to its end, a held one looping till let go.
     * (Format 34) their knobs at their defaults: the sirens as they were (Whoop's new: a box's, out of tune, free) */
    {
        static const int ROOT[2] = {5, 2}, MODE[2] = {MODE_DORIAN, 10}, TEMPO[2] = {122, 80};
        const float bt = 256.0f / 44100.0f;
        float free_gate[2][2];
        int kk, tt, b;
        CHECK(!strcmp(EN_MODE_NAME[10], "Iwato"), "mode 10 is %s", EN_MODE_NAME[10]);
        for (kk = 0; kk < 2; kk++)
            for (tt = 0; tt < 2; tt++) {
                const float f5 = kk ? 6.0f : 7.0f, bar = 240.0f / (float)TEMPO[tt];
                float g, mx = -99.0f, mn = 99.0f, pk[16];
                int npk = 0, last = 0;
                fresh();
                en_set(P_ROOT, ROOT[kk]);
                en_set(P_MODE, MODE[kk]);
                en_pool(EN_MODE_POOL[MODE[kk]]);
                en_set(P_TEMPO, TEMPO[tt]);
                en_play(1);
                run(1.0f);
                /* Wail, tapped: one rise and fall over a bar, lingering on the fifth, the octave at its top */
                n = sir_trace(EN_SIR_WAIL, -1.0f, bar + 1.0f);
                g = sir_gate(n);
                CHECK(fabsf(g - bar) < 0.02f, "Wail tapped: %.3f s, a bar is %.3f", (double)g, (double)bar);
                CHECK(fabsf(sir_st[(int)(bar * 0.25f / bt)] - f5) < 0.1f && fabsf(sir_st[(int)(bar * 0.75f / bt)] - f5) < 0.1f,
                      "Wail: the fifth at a quarter of its cycle (%.3f, %.3f; %.0f)", (double)sir_st[(int)(bar * 0.25f / bt)],
                      (double)sir_st[(int)(bar * 0.75f / bt)], (double)f5);
                b = (int)(bar * 0.5f / bt);
                CHECK(fabsf(sir_st[b] - 12.0f) < 0.1f, "Wail: the octave at its top (%.3f)", (double)sir_st[b]);
                {
                    const float m = 69.0f + 12.0f * log2f(sir_hz[b] / 440.0f) - 12.0f - (float)ROOT[kk];
                    CHECK(fabsf(m - 12.0f * floorf(m / 12.0f + 0.5f)) < 0.05f, "Wail: its octave the key's root (%.3f)",
                          (double)m);
                }
                for (b = 0; b < n && sir_on[b]; b++)
                    last = b;
                CHECK(fabsf(sir_st[last]) < 0.15f, "Wail: it ends on the root (%.3f)", (double)sir_st[last]);
                CHECK(!sir_on[n - 1], "Wail: a tap's let go by itself");
                /* (format 34) Whoop, tapped: a burst of whoops at a box's rate, 2.5 Hz, an octave each, free of the
                 * tempo; its own pitch, A3 and 40 cents: off the key */
                n = sir_trace(EN_SIR_WHOOP, -1.0f, 2.0f);
                free_gate[0][tt] = sir_gate(n);
                CHECK(fabsf(free_gate[0][tt] - 1.2f) < 0.02f, "Whoop tapped: %.3f s (three whoops at 2.5 Hz: 1.2)",
                      (double)free_gate[0][tt]);
                for (b = 1; b + 1 < n; b++)
                    if (sir_on[b] && sir_st[b] > 11.5f && sir_st[b] >= sir_st[b - 1] && sir_st[b] > sir_st[b + 1] && npk < 16)
                        pk[npk++] = (float)b * bt;
                CHECK(npk == 3, "Whoop tapped: %d whoops to the octave", npk);
                for (b = 1; b < npk; b++)
                    CHECK(fabsf(pk[b] - pk[b - 1] - 0.4f) < 0.02f, "Whoop: %.3f s apart (2.5 Hz: 0.4)",
                          (double)(pk[b] - pk[b - 1]));
                {
                    const float m = 69.0f + 12.0f * log2f(sir_hz[0] / 440.0f) - sir_st[0];   /* (its floor) */
                    CHECK(fabsf(m - 57.4f) < 0.02f, "Whoop: from A3 and 40 cents (%.3f)", (double)m);
                }
                /* Wail held: the octave every 4 bars */
                n = sir_trace(EN_SIR_WAIL, 9.0f * bar, 9.0f * bar + 1.0f);
                npk = 0;
                for (b = 1; b + 1 < n; b++)
                    if (sir_on[b] && sir_st[b] > 11.9f && sir_st[b] >= sir_st[b - 1] && sir_st[b] > sir_st[b + 1] && npk < 16)
                        pk[npk++] = (float)b * bt;
                CHECK(npk == 2 && fabsf(pk[1] - pk[0] - 4.0f * bar) < 0.05f, "Wail held: %d tops, %.3f s apart (4 bars "
                      "%.3f)", npk, npk > 1 ? (double)(pk[1] - pk[0]) : 0.0, (double)(4.0f * bar));
                CHECK(sir_on[(int)(8.9f * bar / bt)] && !sir_on[n - 1], "Wail held: on till let go, then gone");
                /* (format 34) Whoop held: whoops on and on at 2.5 Hz, their floor rising a fifth over 4 s and back */
                n = sir_trace(EN_SIR_WHOOP, 9.0f, 9.5f);
                mx = -99.0f;
                for (b = 0; b < n; b++)
                    if (sir_on[b])
                        mx = fmaxf(mx, sir_st[b]);
                CHECK(fabsf(mx - 19.0f) < 0.15f, "Whoop held: up to a fifth over the octave (%.3f)", (double)mx);
                npk = 0;
                for (b = 1; b + 1 < n; b++)
                    if (sir_on[b] && sir_st[b] > 11.5f && sir_st[b] >= sir_st[b - 1] && sir_st[b] > sir_st[b + 1] && npk < 32)
                        pk[npk++ & 15] = (float)b * bt;
                CHECK(npk >= 21 && npk <= 23, "Whoop held: %d whoops in 9 s (2.5 Hz)", npk);
                CHECK(sir_on[(int)(8.9f / bt)] && !sir_on[n - 1], "Whoop held: on till let go, then gone");
                /* Hi-lo, tapped: a bar, root and fifth swapped a corchea (8th) each (the tempo's, zzz); held: the pair up
                 * to the fifth and the octave */
                n = sir_trace(EN_SIR_HILO, -1.0f, bar + 0.5f);
                g = sir_gate(n);
                CHECK(fabsf(g - bar) < 0.02f, "Hi-lo tapped: %.3f s, a bar is %.3f", (double)g, (double)bar);
                for (b = 0; b < 8; b++) {
                    const float s = sir_st[(int)((bar * 0.125f * ((float)b + 0.5f)) / bt)];
                    CHECK(fabsf(s - (b & 1 ? f5 : 0.0f)) < 0.02f, "Hi-lo: swap %d on %.3f", b, (double)s);
                }
                n = sir_trace(EN_SIR_HILO, 9.0f * bar, 9.0f * bar + 1.0f);
                mx = -99.0f, mn = 99.0f;
                for (b = 0; b < n; b++)
                    if (sir_on[b])
                        mx = fmaxf(mx, sir_st[b]), mn = fminf(mn, sir_st[b]);
                CHECK(fabsf(mx - 12.0f) < 0.05f && fabsf(mn) < 0.05f, "Hi-lo held: %.3f .. %.3f", (double)mn, (double)mx);
                CHECK(!sir_on[n - 1], "Hi-lo held: gone once let go");
                /* Laser, tapped: 1.5 s from two octaves up to the root; held: 8 s falls, again */
                n = sir_trace(EN_SIR_LASER, -1.0f, 2.0f);
                free_gate[1][tt] = sir_gate(n);
                for (b = 0; b < n && sir_on[b]; b++)
                    last = b;
                CHECK(fabsf(sir_st[0] - 24.0f) < 0.7f && fabsf(sir_st[last]) < 0.1f, "Laser: %.3f down to %.3f",
                      (double)sir_st[0], (double)sir_st[last]);
                n = sir_trace(EN_SIR_LASER, 20.0f, 21.0f);
                npk = 0;
                for (b = 1; b < n; b++)
                    if (sir_st[b] - sir_st[b - 1] > 12.0f && npk < 16)
                        pk[npk++] = (float)b * bt;
                CHECK(npk == 2 && fabsf(pk[1] - pk[0] - 8.5f) < 0.03f, "Laser held: %d restarts, %.3f s apart", npk,
                      npk > 1 ? (double)(pk[1] - pk[0]) : 0.0);
                CHECK(!bad && peak < 0.99f, "the sirens: peak %f", (double)peak);
                (void)mn;
            }
        CHECK(fabsf(free_gate[1][0] - free_gate[1][1]) < 0.01f && fabsf(free_gate[1][0] - 1.5f) < 0.02f,
              "Laser: %.3f s at 122, %.3f at 80 (free of the tempo)", (double)free_gate[1][0], (double)free_gate[1][1]);
        CHECK(fabsf(free_gate[0][0] - free_gate[0][1]) < 0.01f, "Whoop: %.3f s at 122, %.3f at 80 (free of the tempo)",
              (double)free_gate[0][0], (double)free_gate[0][1]);
        /* one voice: a new one takes over; a held one let go by another's key does nothing */
        fresh();
        en_play(1);
        en_siren(EN_SIR_HILO, EN_SIR_TAP);
        en_siren(EN_SIR_HILO, EN_SIR_HOLD);
        run(1.0f);
        en_siren(EN_SIR_LASER, EN_SIR_TAP);
        run(0.1f);
        CHECK(en_sir_on == EN_SIR_LASER + 1, "Laser over Hi-lo: %u sounding", en_sir_on);
        en_siren(EN_SIR_HILO, EN_SIR_OFF);
        run(0.5f);
        CHECK(en_sir_on == EN_SIR_LASER + 1, "Hi-lo's key let go: the Laser still (%u)", en_sir_on);
        run(1.5f);
        CHECK(en_sir_on == 0, "the Laser's tap done (%u)", en_sir_on);
        en_siren(EN_SIR_WAIL, EN_SIR_TAP);
        en_siren(EN_SIR_WAIL, EN_SIR_HOLD);
        run(1.0f);
        en_panic();
        run(0.05f);
        CHECK(en_sir_on == 0, "a panic: the siren gone (%u)", en_sir_on);
    }
    /* (format 34, the user's) the sirens' knobs: each moves what it should, tapped and held; the tuned ones' Pitch and
     * Depth on the key's notes, Whoop's free; Wail's and Hi-lo's Rate a note value (the tempo's), Whoop's and the
     * Laser's Hz (not); the Echo the send, past 50 the feedback pushed; kept from one tap to the next */
    {
        const float bt = 256.0f / 44100.0f;
        float pk[32], g, mx, mn, e[3][2];
        int b, npk, tt, k, s, v;
        fresh();
        for (s = 0; s < EN_NSIR; s++)                /* (the defaults, as the engine starts) */
            for (k = 0; k < EN_NSK; k++) {
                int lo, hi;
                en_sir_range(s, k, &lo, &hi);
                CHECK(en_sir_def(s, k) >= lo && en_sir_def(s, k) <= hi, "siren %d knob %d: its default %d in %d..%d", s,
                      k, en_sir_def(s, k), lo, hi);
            }
        CHECK(en_sir_def(EN_SIR_WAIL, EN_SK_RATE) == 11 && en_sir_def(EN_SIR_HILO, EN_SK_RATE) == 3 &&
              en_sir_def(EN_SIR_HILO, EN_SK_DEPTH) == 7 && en_sir_def(EN_SIR_LASER, EN_SK_DEPTH) == 24 &&
              en_sir_def(EN_SIR_LASER, EN_SK_RATE) == 15 && en_sir_def(EN_SIR_WHOOP, EN_SK_RATE) == 11,
              "the sirens' defaults: 4 bars, a quarter, the fifth, two octaves, 6 Hz, 2.5 Hz");
        en_set(P_ROOT, 5);
        en_set(P_MODE, MODE_DORIAN);
        en_pool(EN_MODE_POOL[MODE_DORIAN]);           /* (F Dorian: 0 2 3 5 7 9 10) */
        en_set(P_TEMPO, 122);
        en_play(1);
        run(0.5f);
        {   /* the words and the steps (the UI's): on the key's notes, a click a note */
            char t[16];
            CHECK(en_sir_eff(EN_SIR_WAIL, EN_SK_PITCH, 1) == 0 && en_sir_eff(EN_SIR_WAIL, EN_SK_PITCH, 4) == 3 &&
                  en_sir_step(EN_SIR_WAIL, EN_SK_PITCH, 0, 1) == 2 && en_sir_step(EN_SIR_WAIL, EN_SK_PITCH, 2, 1) == 3 &&
                  en_sir_step(EN_SIR_WAIL, EN_SK_PITCH, 0, -1) == -2 && en_sir_step(EN_SIR_HILO, EN_SK_DEPTH, 7, 1) == 9,
                  "the tuned ones' Pitch and Depth: on F Dorian's notes");
            CHECK(en_sir_eff(EN_SIR_WHOOP, EN_SK_PITCH, -4) == -4 && en_sir_step(EN_SIR_WHOOP, EN_SK_PITCH, -4, 1) == -3,
                  "Whoop's Pitch: free, in tenths");
            en_sir_text(EN_SIR_WAIL, EN_SK_RATE, 11, t);
            CHECK(!strcmp(t, "4 bars"), "Wail's Rate 11: %s", t);
            en_sir_text(EN_SIR_HILO, EN_SK_RATE, 2, t);
            CHECK(!strcmp(t, "1/8."), "Hi-lo's Rate 2: %s", t);
            en_sir_text(EN_SIR_WHOOP, EN_SK_RATE, 11, t);
            CHECK(!strcmp(t, "2.5 Hz"), "Whoop's Rate 11: %s", t);
            en_sir_text(EN_SIR_WHOOP, EN_SK_PITCH, -4, t);
            CHECK(!strcmp(t, "-0.4"), "Whoop's Pitch -4: %s", t);
            en_sir_text(EN_SIR_WAIL, EN_SK_PITCH, 3, t);
            CHECK(!strcmp(t, "+3"), "Wail's Pitch 3: %s", t);
            en_sir_text(EN_SIR_LASER, EN_SK_DEPTH, 24, t);
            CHECK(!strcmp(t, "24 st"), "the Laser's Depth 24: %s", t);
        }
        /* Pitch: Wail up 3 (the minor third, F Dorian's): from it, its octave over it; Whoop down 0.4: on A3 */
        en_sir_set(EN_SIR_WAIL, EN_SK_PITCH, 3);
        n = sir_trace(EN_SIR_WAIL, -1.0f, 2.5f);
        mn = 99.0f, mx = -99.0f;
        for (b = 0; b < n; b++)
            if (sir_on[b])
                mn = fminf(mn, sir_st[b]), mx = fmaxf(mx, sir_st[b]);
        CHECK(fabsf(mn - 3.0f) < 0.15f && fabsf(mx - 15.0f) < 0.1f, "Wail at Pitch +3: %.3f .. %.3f", (double)mn, (double)mx);
        n = sir_trace(EN_SIR_WAIL, -1.0f, 2.5f);         /* (kept: the next tap from it too) */
        CHECK(fabsf(sir_st[2] - 3.0f) < 0.2f, "Wail's Pitch kept for the next tap (%.3f)", (double)sir_st[2]);
        en_sir_set(EN_SIR_WAIL, EN_SK_PITCH, 0);
        en_sir_set(EN_SIR_WHOOP, EN_SK_PITCH, -4);
        n = sir_trace(EN_SIR_WHOOP, -1.0f, 1.5f);
        {
            const float m = 69.0f + 12.0f * log2f(sir_hz[0] / 440.0f) - sir_st[0] - 0.4f;   /* (its floor, the Pitch
                                                  * in sir_st) */
            CHECK(fabsf(m - 57.0f) < 0.02f, "Whoop at Pitch -0.4: on A3 (%.3f)", (double)m);
        }
        en_sir_set(EN_SIR_WHOOP, EN_SK_PITCH, 0);
        /* Depth: Wail's top a fifth (7), the Laser from an octave, Hi-lo's other note the fourth (5), Whoop 5 st */
        en_sir_set(EN_SIR_WAIL, EN_SK_DEPTH, 7);
        n = sir_trace(EN_SIR_WAIL, -1.0f, 2.5f);
        mx = -99.0f;
        for (b = 0; b < n; b++)
            mx = fmaxf(mx, sir_on[b] ? sir_st[b] : -99.0f);
        CHECK(fabsf(mx - 7.0f) < 0.1f, "Wail at Depth 7: its top %.3f", (double)mx);
        CHECK(fabsf(sir_st[(int)(1.967f * 0.25f / bt)] - 3.0f) < 0.1f, "Wail at Depth 7: its middle a note of the key, "
              "the minor third (%.3f)", (double)sir_st[(int)(1.967f * 0.25f / bt)]);
        en_sir_set(EN_SIR_WAIL, EN_SK_DEPTH, 12);
        en_sir_set(EN_SIR_LASER, EN_SK_DEPTH, 12);
        n = sir_trace(EN_SIR_LASER, -1.0f, 2.0f);
        CHECK(fabsf(sir_st[0] - 12.0f) < 0.7f, "the Laser at Depth 12: from %.3f", (double)sir_st[0]);
        en_sir_set(EN_SIR_LASER, EN_SK_DEPTH, 24);
        en_sir_set(EN_SIR_HILO, EN_SK_DEPTH, 5);
        n = sir_trace(EN_SIR_HILO, -1.0f, 2.5f);
        g = sir_st[(int)(1.967f * 0.125f * 1.5f / bt)];
        CHECK(fabsf(g - 5.0f) < 0.05f, "Hi-lo at Depth 5: its other note %.3f", (double)g);
        en_sir_set(EN_SIR_HILO, EN_SK_DEPTH, 7);
        en_sir_set(EN_SIR_WHOOP, EN_SK_DEPTH, 5);
        n = sir_trace(EN_SIR_WHOOP, -1.0f, 1.5f);
        mx = -99.0f;
        for (b = 0; b < n; b++)
            mx = fmaxf(mx, sir_on[b] ? sir_st[b] : -99.0f);
        CHECK(fabsf(mx - 5.0f) < 0.15f, "Whoop at Depth 5: up %.3f", (double)mx);
        en_sir_set(EN_SIR_WHOOP, EN_SK_DEPTH, 12);
        /* Rate: Wail held 2 bars a cycle, Hi-lo held a corchea (8th) a swap, at 122 and 80 (the tempo's); Whoop held
         * 4 Hz at both (not) */
        for (tt = 0; tt < 2; tt++) {
            const float bar = tt ? 3.0f : 240.0f / 122.0f;
            en_set(P_TEMPO, tt ? 80 : 122);
            run(0.3f);
            en_sir_set(EN_SIR_WAIL, EN_SK_RATE, 9);
            n = sir_trace(EN_SIR_WAIL, 5.0f * bar, 5.0f * bar + 1.0f);
            npk = 0;
            for (b = 1; b + 1 < n; b++)
                if (sir_on[b] && sir_st[b] > 11.9f && sir_st[b] >= sir_st[b - 1] && sir_st[b] > sir_st[b + 1] && npk < 32)
                    pk[npk++] = (float)b * bt;
            CHECK(npk >= 2 && fabsf(pk[npk - 1] - pk[npk - 2] - 2.0f * bar) < 0.05f, "Wail at Rate 2 bars (%d BPM): %d tops, "
                  "%.3f s apart (%.3f)", tt ? 80 : 122, npk, npk > 1 ? (double)(pk[npk - 1] - pk[npk - 2]) : 0.0,
                  (double)(2.0f * bar));
            en_sir_set(EN_SIR_WAIL, EN_SK_RATE, 11);
            en_sir_set(EN_SIR_HILO, EN_SK_RATE, 1);
            n = sir_trace(EN_SIR_HILO, 3.0f, 3.2f);
            npk = 0;
            for (b = (int)(1.0f / bt); b < (int)(2.9f / bt); b++)
                if ((sir_st[b] - sir_st[b - 1] > 2.0f || sir_st[b - 1] - sir_st[b] > 2.0f) && npk < 32 &&
                    (!npk || (float)b * bt - pk[npk - 1] > 0.03f))
                    pk[npk++] = (float)b * bt;
            CHECK(npk >= 4 && fabsf((pk[npk - 1] - pk[0]) / (float)(npk - 1) - bar * 0.125f) < 0.01f,
                  "Hi-lo at Rate 1/8 (%d BPM): %d swaps, %.4f s each (%.4f)", tt ? 80 : 122, npk,
                  npk > 1 ? (double)((pk[npk - 1] - pk[0]) / (float)(npk - 1)) : 0.0, (double)(bar * 0.125f));
            en_sir_set(EN_SIR_HILO, EN_SK_RATE, 3);
            en_sir_set(EN_SIR_WHOOP, EN_SK_RATE, 13);
            n = sir_trace(EN_SIR_WHOOP, 5.0f, 5.2f);
            npk = 0;
            for (b = 1; b + 1 < n; b++)
                if (sir_on[b] && sir_st[b] > 11.0f && sir_st[b] >= sir_st[b - 1] && sir_st[b] > sir_st[b + 1] && npk < 32)
                    pk[npk++] = (float)b * bt;
            CHECK(npk >= 10 && fabsf((pk[npk - 1] - pk[0]) / (float)(npk - 1) - 0.25f) < 0.01f,
                  "Whoop at 4 Hz (%d BPM): %d whoops, %.4f s apart", tt ? 80 : 122, npk,
                  npk > 1 ? (double)((pk[npk - 1] - pk[0]) / (float)(npk - 1)) : 0.0);
            en_sir_set(EN_SIR_WHOOP, EN_SK_RATE, 11);
        }
        /* the Laser's Rate: its warble, 12 Hz (free); its fall's time kept (1.5 s) */
        en_sir_set(EN_SIR_LASER, EN_SK_RATE, 18);
        n = sir_trace(EN_SIR_LASER, -1.0f, 2.0f);
        g = sir_gate(n);
        npk = 0;
        for (b = 2; b < (int)(0.8f / bt); b++) {      /* (the warble's tops over the fall: its turns) */
            const float d0 = sir_st[b - 1] - sir_st[b - 2], d1 = sir_st[b] - sir_st[b - 1];
            npk += d0 > 0.0f && d1 <= 0.0f;
        }
        CHECK(fabsf(g - 1.5f) < 0.02f && npk >= 8, "the Laser at 12 Hz: %.3f s, %d warbles in 0.8 s", (double)g, npk);
        en_sir_set(EN_SIR_LASER, EN_SK_RATE, 15);
        /* Echo: the parts muted, the tail after a Laser's tap: none at 0; at 100 more than its send's (x2, +6 dB): the
         * feedback pushed */
        en_mute(0x3Fu);
        run(3.0f);
        for (v = 0; v < 3; v++) {
            en_sir_set(EN_SIR_LASER, EN_SK_ECHO, v * 50);
            run(4.0f);
            sir_trace(EN_SIR_LASER, 4.0f, 4.1f);   /* (held 4 s, let go) */
            sum2 = 0.0f;
            run(1.0f);
            e[v][0] = sum2;
            sum2 = 0.0f;
            run(2.0f);
            e[v][1] = sum2;
            en_panic();
            run(6.0f);
        }
        printf("  (the sirens' Echo: the tail's energy at 0, 50, 100: %g, %g, %g; then %g, %g, %g)\n", (double)e[0][0],
               (double)e[1][0], (double)e[2][0], (double)e[0][1], (double)e[1][1], (double)e[2][1]);
        CHECK(e[0][0] < 0.03f * e[1][0], "Echo 0: no tail (%g against %g)", (double)e[0][0], (double)e[1][0]);
        CHECK(e[2][0] > 6.0f * e[1][0] && e[2][1] > 12.0f * e[1][1], "Echo 100: the tail %.1f, then %.1f times Echo 50's (its "
              "send's alone 4)", (double)(e[2][0] / e[1][0]), (double)(e[2][1] / e[1][1]));
        en_sir_set(EN_SIR_LASER, EN_SK_ECHO, 50);
        en_mute(0);
        /* Whoop out of tune in every piece: its own pitch at least a third of a semitone from each of the key's notes */
        for (k = 0; k < 8; k++) {
            int16_t pp[P_NPARAMS];
            uint16_t pl;
            float m, d = 99.0f;
            int q;
            en_piece(k, pp, &pl);
            fresh();
            for (q = 0; q < P_NPARAMS; q++)
                en_set(q, pp[q]);
            en_pool(pl);
            en_play(1);
            run(0.5f);
            sir_trace(EN_SIR_WHOOP, -1.0f, 0.1f);
            m = 69.0f + 12.0f * log2f(sir_hz[0] / 440.0f) - sir_st[0];   /* (its floor) */
            for (q = 0; q < 12; q++)
                if (pl >> q & 1u) {
                    const float r = m - (float)(pp[P_ROOT] + q);
                    d = fminf(d, fabsf(r - 12.0f * floorf(r / 12.0f + 0.5f)));
                }
            CHECK(d > 0.33f, "piece %d: Whoop %.3f semitones from its key's nearest note", k, (double)d);
            en_panic();
            run(0.1f);
        }
    }

    printf(fails ? "amb: %d FAILED\n" : "amb: ok\n", fails);
    return fails != 0;
}
