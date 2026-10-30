/* SPDX-License-Identifier: GPL-3.0-only */
/* The engine alone, to a WAV: for listening to changes quickly (no UI, no screen).
 *
 *   en_render OUT.wav SECS [piece=N] [NAME=V ...] [freeze=T0-T1] [seed=S] [outro=T] [log]
 *
 * piece=N takes piece N's recipe (0..7) first; mode=N a scale (MODE_*), its notes too; NAME=V sets a parameter by its name as amb.c's table
 * has it, spaces as underscores (Echo_time=2, Cutoff=60); freeze=T0-T1 holds from T0 to T1 seconds;
 * NAME=V@T sets it at T seconds instead; fx=N@T0-T1 holds the effects grid's effect N (0..15) from T0 to T1; outro=T asks
 * for the outro at T seconds (HOME held + PLAY; a song: Song_mode=1 Intro=2 Main=4 Outro=2, in phrases); siren=S@T taps
 * PERFORM's dub siren S (0 Wail, 1 Whoop, 2 Hi-lo, 3 Laser) at T, siren=S@T0-T1 holds it (its key down at T0, the hold a
 * third of a second on, let go at T1); sirknob=S:K:V sets siren S's knob K (0 Pitch, 1 Rate, 2 Depth, 3 Echo: amb.h
 * EN_SK_*) to V, sirknob=S:K:V0:V1@T0-T1 sweeps it from V0 to V1 over T0..T1 (a step a block); sirlog=FILE writes the siren's pitch each block (s, on, semitones, Hz); fdlog=FILE writes, each bar,
 * its start (s), the bar, the song's place (0 off, 1 intro, 2 main, 3 outro, 4 end) and the six parts' faders as heard
 * (en_fader: the chord, the pings, the bass, the kick, the hats, the noise); log prints each
 * bar's section and stabs, the song's place, the kick's and the hats' lanes, and the chord as it moves.
 *   cc -O2 -ffp-contract=off -DEN_HOST -Ifirmware/src/dsp -o build/host/en_render host/en_render.c \
 *      firmware/src/dsp/amb.c -lm */
#define EN_HOST 1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "amb.h"

void en_midi_out(uint32_t st, uint32_t d1, uint32_t d2) { (void)st, (void)d1, (void)d2; }

static void wr32(FILE *f, uint32_t v) { fputc((int)(v & 255), f), fputc((int)(v >> 8 & 255), f), fputc((int)(v >> 16 & 255), f), fputc((int)(v >> 24), f); }
static void wr16(FILE *f, uint32_t v) { fputc((int)(v & 255), f), fputc((int)(v >> 8 & 255), f); }

static int find_param(const char *name, int len)
{
    int i, k;
    for (i = 0; i < P_NPARAMS; i++) {
        const char *n = en_param_info(i)->name;
        for (k = 0; k < len && n[k]; k++)
            if (n[k] != name[k] && !(n[k] == ' ' && name[k] == '_'))
                break;
        if (k == len && !n[k])
            return i;
    }
    return -1;
}

int main(int argc, char **argv)
{
    static int32_t buf[2 * 256];
    int16_t par[P_NPARAMS];
    uint16_t pool;
    FILE *f;
    double secs;
    uint32_t frames, done = 0, last16 = 0;
    int i, logit = 0, piece = 0, sec_was = -1;
    double fz0 = -1, fz1 = -1, fx0[16], fx1[16], outro_t = -1;
    int nfx = 0, fxn[16];
    double sr0[16], sr1[16];
    int nsr = 0, srn[16], srs[16];
    FILE *slog = 0, *flog = 0;
    int nsk = 0, skn[16][2], skv[16][2], skl[16];
    double skt[16][2];
    uint32_t fxwas = 0;
    unsigned seed = 0;
    struct { double t; int p, v; } ev[64];
    int nev = 0, e;
    if (argc < 3) {
        fprintf(stderr, "usage: en_render OUT.wav SECS [piece=N] [mode=N] [NAME=V ...] [freeze=T0-T1] [seed=S] [log]\n");
        return 2;
    }
    secs = atof(argv[2]);
    en_init();
    for (i = 3; i < argc; i++)
        if (!strncmp(argv[i], "piece=", 6))
            piece = atoi(argv[i] + 6);
    en_piece(piece, par, &pool);
    for (i = 0; i < P_NPARAMS; i++)
        if (i != P_TUNE && i != P_MIDI)
            en_set(i, par[i]);
    en_pool(pool);
    for (i = 3; i < argc; i++) {
        const char *a = argv[i], *eq = strchr(a, '=');
        if (!strcmp(a, "log")) {
            logit = 1;
        } else if (!strncmp(a, "freeze=", 7)) {
            sscanf(a + 7, "%lf-%lf", &fz0, &fz1);
        } else if (!strncmp(a, "fx=", 3) && nfx < 16) {
            sscanf(a + 3, "%d@%lf-%lf", &fxn[nfx], &fx0[nfx], &fx1[nfx]);
            nfx++;
        } else if (!strncmp(a, "siren=", 6) && nsr < 16) {
            sr1[nsr] = -1;
            sscanf(a + 6, "%d@%lf-%lf", &srn[nsr], &sr0[nsr], &sr1[nsr]);
            srs[nsr++] = 0;
        } else if (!strncmp(a, "sirknob=", 8) && nsk < 16) {
            skt[nsk][0] = skt[nsk][1] = -1, skl[nsk] = -999;
            if (sscanf(a + 8, "%d:%d:%d:%d@%lf-%lf", &skn[nsk][0], &skn[nsk][1], &skv[nsk][0], &skv[nsk][1], &skt[nsk][0],
                       &skt[nsk][1]) < 6) {
                sscanf(a + 8, "%d:%d:%d", &skn[nsk][0], &skn[nsk][1], &skv[nsk][0]);
                en_sir_set(skn[nsk][0], skn[nsk][1], skv[nsk][0]);
            } else {
                nsk++;
            }
        } else if (!strncmp(a, "fdlog=", 6)) {
            flog = fopen(a + 6, "w");
        } else if (!strncmp(a, "sirlog=", 7)) {
            slog = fopen(a + 7, "w");
        } else if (!strncmp(a, "outro=", 6)) {
            outro_t = atof(a + 6);
        } else if (!strncmp(a, "seed=", 5)) {
            seed = (unsigned)atoi(a + 5);
        } else if (!strncmp(a, "mode=", 5)) {    /* (1.4.0) a scale, its notes too (Mode= sets its name alone) */
            const int m = atoi(a + 5);
            en_set(P_MODE, m);
            en_pool(EN_MODE_POOL[(unsigned)m < EN_NMODES ? m : 0]);
        } else if (eq && strncmp(a, "piece=", 6)) {
            int p = find_param(a, (int)(eq - a));
            const char *at = strchr(eq, '@');
            if (p < 0) {
                fprintf(stderr, "no parameter %.*s\n", (int)(eq - a), a);
                return 2;
            }
            if (at && nev < 64) {
                ev[nev].t = atof(at + 1), ev[nev].p = p, ev[nev].v = atoi(eq + 1);
                nev++;
            } else {
                en_set(p, atoi(eq + 1));
            }
        }
    }
    if (seed)
        en_seed(seed);
    en_regrow();
    en_play(1);
    frames = (uint32_t)(secs * 44100.0);
    f = fopen(argv[1], "wb");
    if (!f)
        return 1;
    fwrite("RIFF", 1, 4, f), wr32(f, 36 + frames * 4), fwrite("WAVEfmt ", 1, 8, f), wr32(f, 16), wr16(f, 1), wr16(f, 2);
    wr32(f, 44100), wr32(f, 44100 * 4), wr16(f, 4), wr16(f, 16), fwrite("data", 1, 4, f), wr32(f, frames * 4);
    while (done < frames) {
        uint32_t n = frames - done > 256 ? 256 : frames - done, j;
        double t = done / 44100.0;
        if (fz0 >= 0) {
            static int fzs;
            int want = t >= fz0 && t < fz1;
            if (want != fzs)
                en_freeze(fzs = want);
        }
        {
            uint32_t m = 0;
            for (e = 0; e < nfx; e++)
                if (t >= fx0[e] && t < fx1[e])
                    m |= 1u << fxn[e];
            if (m != fxwas)
                en_fx(fxwas = m);
        }
        for (e = 0; e < nev; e++)
            if (ev[e].t >= 0 && t >= ev[e].t) {
                en_set(ev[e].p, ev[e].v);
                ev[e].t = -1;
            }
        for (e = 0; e < nsk; e++) {               /* (a knob swept: a step a block, as a hand) */
            if (t >= skt[e][0]) {
                const double x = t >= skt[e][1] ? 1.0 : (t - skt[e][0]) / (skt[e][1] - skt[e][0]);
                const int v = (int)(skv[e][0] + (skv[e][1] - skv[e][0]) * x + (skv[e][1] > skv[e][0] ? 0.5 : -0.5));
                if (v != skl[e])
                    en_sir_set(skn[e][0], skn[e][1], skl[e] = v);
            }
        }
        for (e = 0; e < nsr; e++) {               /* (the key down: a tap; held a third of a second: the hold; up) */
            if (srs[e] == 0 && t >= sr0[e])
                en_siren(srn[e], EN_SIR_TAP), srs[e] = 1;
            if (srs[e] == 1 && sr1[e] > sr0[e] && t >= sr0[e] + 1.0 / 3.0)
                en_siren(srn[e], EN_SIR_HOLD), srs[e] = 2;
            if (srs[e] == 2 && t >= sr1[e])
                en_siren(srn[e], EN_SIR_OFF), srs[e] = 3;
        }
        if (outro_t >= 0 && t >= outro_t) {
            en_outro();
            outro_t = -1;
        }
        en_render(buf, n, 2048);
        for (j = 0; j < 2 * n; j++)
            wr16(f, (uint32_t)(uint16_t)(int16_t)(buf[j] >> 8));
        done += n;
        if (slog)
            fprintf(slog, "%.4f %u %.3f %.2f\n", done / 44100.0, en_sir_on, (double)en_sir_st, (double)en_sir_hz);
        if (flog && en_bars / 16u != last16 / 16u)
            fprintf(flog, "%.4f %u %u %.4f %.4f %.4f %.4f %.4f %.4f\n", t, en_bars / 16u, en_song_ph, (double)en_fader[0],
                    (double)en_fader[1], (double)en_fader[2], (double)en_fader[3], (double)en_fader[4], (double)en_fader[5]);
        if (logit && en_bars / 16u != last16 / 16u) {
            char nm[16];
            int pcs[4], k;
            for (k = 0; k < 4; k++)
                pcs[k] = en_chord[k] % 12;
            pcs[0] = en_chord_root;
            static const char *const SG[5] = {"", "INTRO", "MAIN", "OUTRO", "END"};
            printf("%6.1fs bar %3u %-6s%s %-5s kick ", t, en_bars / 16u, EN_SEC_NAME[en_section],
                   en_section != sec_was ? "*" : " ", SG[en_song_ph % 5u]);
            for (k = 0; k < 16; k++)
                putchar(en_lane[1] >> k & 1u ? 'x' : '.');
            printf(" hats ");
            for (k = 0; k < 16; k++)
                putchar(en_lane[2] >> k & 1u ? 'x' : '.');
            printf(" stabs ");
            for (k = 0; k < 16; k++)
                putchar(en_stab_pat >> k & 1u ? 'x' : '.');
            printf("  chord %s (%d %d %d %d) arc %+.2f throws %u%s\n", en_chord_name(pcs, en_chord_n, nm), en_chord[0],
                   en_chord[1], en_chord[2], en_chord[3], (double)en_arc, en_throws, en_frozen ? " HOLD" : "");
            sec_was = en_section;
        }
        last16 = en_bars;
    }
    fclose(f);
    if (slog)
        fclose(slog);
    if (flog)
        fclose(flog);
    return 0;
}
