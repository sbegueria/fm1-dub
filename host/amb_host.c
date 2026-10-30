/* SPDX-License-Identifier: GPL-3.0-only */
/* AMB-1 on the host: the whole app (UI, project, engine) against a simulated FM-1 driven by a script.
 * Audio goes to a WAV, the screen to PNGs, the lights to text, MIDI out to a log. The device-only
 * parts (HAL, USB, OTA, flash driver) are replaced by plat.h implemented here. (From X0X's x0x_host.)
 *
 *   amb_host SCRIPT [OUTDIR]
 *
 * Script, one command per line (# comments):
 *   wait MS                      run the device for MS milliseconds
 *   press BTN | release BTN | tap BTN     (BTN: FX SEL ENV LFO EDIT GLO HOME SAVE ARP SEQ PLAY REC OCT- OCT+)
 *   key K down|up | tapkey K     (K: 0..26 = F3..G5, or w0..w15 white keys, b0..b10 black keys)
 *   turn ENC N [MS]              (ENC: SELECT ALGO PRESET K1 K2 K3 K4; N detents, MS apart, default 80)
 *   spin ENC N                   N detents at once, exactly N (no acceleration)
 *   master N                     MASTER pot 0..4096
 *   midi B0 B1 B2                incoming USB MIDI message (hex bytes)
 *   wav FILE | wavstop           start / stop recording the output
 *   peakreset                    restart the output peak (expect peak_db_min / peak_db_max)
 *   shot FILE.png                save the screen
 *   storescreen N                the project saved with the piece's view N, as a 1.4.0 build with six views would
 *   leds                         print the lit buttons and keys
 *   expect WHAT VALUE            check state: exit 1 on mismatch; VALUE "<N" / ">N" is a bound
 *   reboot                       re-run boot from the simulated flash (persistence test)
 *   splash N                     draw the launch screen's frame N (0..UI_SPLASH_N-1) on the screen
 *   at S [LABEL]                 run until S seconds after the film started (or the boot, without a film)
 *   bar N [LABEL]                run until bar N's downbeat (N x 16 sixteenths played): a move on the beat.
 *                                Both print the film's time there, "@ mm:ss.s  bar N  LABEL" (the edit's cue)
 *   store8                       save the project as DUB-1 1.0.0 did (format 8): reboot then reads it
 *   store10                      save it as 1.2.0 did (format 10, without the noise's five)
 *   storefile PATH               a saved project from a file (tests/host/proj-*.bin)
 *   recipe N                     piece N back to its recipe, unplayed (a piece never changed, as on a device)
 *   store30s                     save it as step 2's test builds did (zzo, zzp: format 30, the LFOs' Shape, Sine
 *                                or Wander, in the Wave's slots, no layout B mark)
 *   store34                      save it as the variety plan's step 5 did (format 34: Vary on its old scale, no
 *                                sweep's pace)
 *   store32                      save it as the variety plan's step 4 did (format 32: today's layout without the
 *                                sirens' knobs, proj.sir: its bytes not written)
 *   store31                      save it as the variety plan's step 3 did (format 31: Stabs 0..100, Hits, the
 *                                Densities, the Tones, the noise's Decay, the echo's Tone and Attack as they were)
 *   store30 [MIXER]              save it as the variety plan's step 2 did (format 30: Motion and the Mixer in their
 *                                old meaning; MIXER the playing piece's Mixer)
 *   store29                      save it as the variety plan's step 1 did (format 29: no Drift, no LFO Trims, no
 *                                throws' amount: the LFOs' Depth and Drift and the Mixer as they are)
 *   store28                      save it as 1.4.0's Filter did (format 28: no looseness, the Varys 0..100 on Evolve)
 *   store27                      save it as 1.4.0's dub mixer did (format 27, the strips' Low cut, 0..100)
 *   store26                      save it as 1.4.0's song structure did (format 26, without the dub mixer)
 *   store25                      save it as 1.4.0's noise LFO Destination did (format 25, without song structure)
 *   store24                      save it as 1.4.0's six strips did (format 24, without the noise LFO's Destination)
 *   store23                      save it as 1.4.0's channel strips did (format 23, the drums one strip: no Kick
 *                                strip's level, sends, Low cut)
 *   store22                      save it as 1.4.0's layers' pages did (format 22, without the channel strips' sends
 *                                and Low cuts)
 *   store21                      save it as 1.4.0's Chord pages did (format 21, without the layers' Vary, the
 *                                chord's Cutoff the knob itself: its base and offset as one)
 *   store20                      save it as 1.4.0's LFO pages did (format 20, without the chord's Vary and its LFO's
 *                                Destination)
 *   store18                      save it as 1.4.0's test builds did (format 18, without the LFOs' rates)
 *   store16                      save it as 1.3.0's noise LFO did (format 16, without the kick's rumble)
 *   store15                      save it as 1.3.0's Noise space did (format 15, without the noise's LFO)
 *   store14                      save it as 1.3.0's echo and density did (format 14)
 *   store13                      save it as 1.3.0's pages did (format 13, without the chord's Ghost .. the shimmer's Tune)
 *   store12 [G]                  save it as 1.3.0's ladder did (format 12, without Throws, Ghost, Notes, Hats level;
 *                                each groove as its ladder step: the knob's 0..4 as 0 2 4 6 8; G: the playing
 *                                piece's ladder step instead, 0..8)
 *   pingmark                     remember the ping loops (expect pingsame 1: they are as they were)
 *   splashrun                    the launch screen played out as the device does at power-on (55 ms a frame,
 *                                the music running, the UI not), then the UI started again
 *   film FILE | filmstop         record the device, 30 frames a second of simulated time, to FILE: each frame
 *                                a header (FILM_HDR words: ms, buttons held, keys held, buttons lit, keys lit,
 *                                master, the detents each encoder turned since the frame before) and the
 *                                screen (240 x 240 RGB565, big-endian). tools/film.py makes it a video.
 *   stems DIR [NAME,NAME..|all]  (before film) while the film records, each of the screen's pictures alone, its own
 *                                video, frame for frame with it: DIR/NAME.mp4 (host/stems.c: the names, the framing)
 *   listen                       a screen reader starts listening (HELLO, again every 2 s): each SAY is printed
 *                                ("said: ..."; "said!:" cuts off what was being said)
 *   where                        the reader asks for everything on the screen
 *   said TEXT                    some SAY since the last "said" (or "listen") has TEXT in it: else a failure
 *   said_none TEXT               no SAY since the last check has TEXT in it: else a failure
 *   speakdump                    every page, knob value, help card, chord and message as the reader gets them
 * AMB_SAID_LOG=FILE: a reader listens from boot, quietly, and every SAY is appended to FILE (the audit).
 * AMB_STEMS="DIR [NAMES]": stems from the start, DIR as given (the film's scripts untouched: tools/make_set.sh STEMS=1).
 */
#define EN_HOST 1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <time.h>
#define __attribute__(x)

/* ------------------------------------------------------------ screen --- */
static uint16_t fb[240 * 240];          /* RGB565, byte-swapped as the panel takes it (gfx.c) */
static void lcd_sync(void) {}
static void lcd_fill(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint16_t c)
{
    uint32_t i, j;
    uint16_t sc = (uint16_t)(((c >> 8) & 0xFFu) | ((c & 0xFFu) << 8));
    for (j = y; j < y + h && j < 240u; j++)
        for (i = x; i < x + w && i < 240u; i++)
            fb[j * 240u + i] = sc;
}
static uint32_t blits;
static void lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *px)
{
    uint32_t i, j;
    blits++;
    for (j = 0; j < h; j++)
        for (i = 0; i < w; i++)
            if (x + i < 240u && y + j < 240u)
                fb[(y + j) * 240u + x + i] = px[j * w + i];
}
#include "../firmware/src/gfx.c"


/* ---------------------------------------------------------- platform --- */
#include "../firmware/src/app/plat.h"

static uint32_t now_ms, held_btn, held_keys, master = 2048;
static int32_t enc_acc[NE];
static uint32_t lit_btn, lit_keys;

uint32_t plat_ms(void) { return now_ms; }
uint32_t plat_buttons(void) { return held_btn; }
uint32_t plat_keys(void) { return held_keys; }
int32_t plat_enc(int role)
{
    int32_t v = enc_acc[role];
    enc_acc[role] = 0;
    return v;
}
uint32_t plat_master(void) { return master; }
void plat_leds(uint32_t b, uint32_t k)
{
    lit_btn = b;
    lit_keys = k;
}
static int glow_on = 1;                 /* the device's button glow (plat_glow); the lights printed are the bright ones */
void plat_glow(int on) { glow_on = on; }
void plat_play_red(int red) { (void)red; }      /* the device's PLAY colour: green, red while recording */

#define MQ 256
static uint32_t min_q[MQ], mi_w, mi_r;
static FILE *midi_log;
static uint32_t midi_out_count, clock_out_count;
int plat_midi_in(uint32_t *pkt)
{
    if (mi_r == mi_w)
        return 0;
    *pkt = min_q[mi_r++ % MQ];
    return 1;
}
void plat_midi_out(uint32_t pkt)
{
    midi_out_count++;
    if (((pkt >> 8) & 0xFF) == 0xF8) {
        clock_out_count++;
        return;
    }
    if (midi_log)
        fprintf(midi_log, "%u ms: %02X %02X %02X\n", now_ms, (pkt >> 8) & 0xFF, (pkt >> 16) & 0xFF, (pkt >> 24) & 0xFF);
}
/* the engine's MIDI out, as the device's glue sends it (USB-MIDI packet, cable 0) */
void en_midi_out(uint32_t st, uint32_t d1, uint32_t d2) { plat_midi_out(st >> 4 | st << 8 | d1 << 16 | d2 << 24); }

/* flash: one buffer per object, kept across a simulated reboot */
static uint8_t store[OBJ_NOBJ][PLAT_STORE_MAX];
static int store_len[OBJ_NOBJ] = {-1, -1, -1};
static uint32_t store_writes;
int plat_store_load(uint32_t obj, void *dst, uint32_t max)
{
    int n;
    if (obj >= OBJ_NOBJ || (n = store_len[obj]) < 0)
        return -1;
    if ((uint32_t)n > max)
        n = (int)max;
    memcpy(dst, store[obj], (size_t)n);
    return n;
}
int plat_store_save(uint32_t obj, const void *src, uint32_t len)
{
    if (obj >= OBJ_NOBJ || len > PLAT_STORE_MAX)
        return -1;
    memcpy(store[obj], src, len);
    store_len[obj] = (int)len;
    store_writes++;
    return 0;
}

static uint32_t cpu_pct;
uint32_t plat_cpu_pct(void) { return cpu_pct; }
uint32_t plat_xruns(void) { return 0; }
uint32_t plat_clock_mhz(void) { return 0; }
void plat_clock_regs(uint32_t r[3]) { r[0] = r[1] = r[2] = 0; }
void plat_prof(uint32_t peak[PLAT_NPROF], uint32_t mean[PLAT_NPROF])
{
    int i;
    for (i = 0; i < PLAT_NPROF; i++)
        peak[i] = mean[i] = 0;
}
uint32_t plat_cpu_peak(void) { return cpu_pct; }
int32_t plat_battery(void) { return getenv("AMB_BATT") ? atoi(getenv("AMB_BATT")) : -1; }   /* (to see it drawn) */
int plat_usb_power(void) { return getenv("AMB_USB") != 0; }

/* -------------------------------------------------------------- app --- */
#include "../firmware/src/app/app.h"
#define ANN_NAME "DUB-1 " EN_VERSION
#include "ann_words.h"                                  /* the words file's link (tools/gen_words.py) */
/* (an older format's save: the chord's Cutoff the knob itself, before 22: its base and the knob's offset as one) */
static int16_t cut_abs(const int16_t *par)
{
    const int c = par[P_CUTOFF] + par[P_CCUT];
    return (int16_t)(c < 0 ? 0 : c > 100 ? 100 : c);
}
#include "../firmware/src/announce.c"
/* what the reader is told: every SAY to stdout with "said:" (listen), and kept for the "said" check */
static char said_last[ANN_MAX + 1], said_log[16384];   /* said_log: every SAY since the last "said" check */
static uint32_t said_len;
static int said_print = 1, sim_listen;
static FILE *said_log_f;                                /* AMB_SAID_LOG=FILE: every SAY, for fm1-reader's audit */
#ifdef EN_WEB
static void web_said(const uint8_t *p, uint32_t n);
#endif
static int ann_wire_send(const uint8_t *p, uint32_t n)
{
#ifdef EN_WEB
    web_said(p, n);
#endif
    if (n >= 7u && p[4] == ANN_SAY) {
        uint32_t i, k = 0;
        for (i = 6; i + 1u < n && k < ANN_MAX; i++)
            said_last[k++] = (char)p[i];
        said_last[k] = 0;
        if (said_len + k + 2u < sizeof said_log) {
            memcpy(said_log + said_len, said_last, k);
            said_len += k;
            said_log[said_len++] = '\n';
            said_log[said_len] = 0;
        }
#ifndef EN_WEB
        if (said_print)
            printf("said%s: %s\n", (p[5] & ANN_NOW) ? "!" : "", said_last);
        if (said_log_f)
            fprintf(said_log_f, "%s\t%s\n", (p[5] & ANN_KEEP) ? "WARN" : "MSG", said_last);
#endif
    }
    return 0;
}
#include "../firmware/src/app/project.c"
#include "../firmware/src/app/ui.c"

/* ---------------------------------------------------------- outputs --- */
static void png_write(const char *path)
{
    /* RGB PNG with stored (uncompressed) deflate blocks: no zlib needed */
    static uint8_t raw[240 * (1 + 240 * 3)];
    FILE *f = fopen(path, "wb");
    uint32_t crc_t[256], i, j, k, n = 0, a = 1, b = 0, pos, len = sizeof raw;
    uint8_t hdr[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    if (!f) {
        fprintf(stderr, "cannot write %s\n", path);
        return;
    }
    for (i = 0; i < 256; i++) {
        uint32_t c = i;
        for (k = 0; k < 8; k++)
            c = c & 1 ? 0xEDB88320u ^ (c >> 1) : c >> 1;
        crc_t[i] = c;
    }
    for (j = 0; j < 240; j++) {
        raw[n++] = 0;
        for (i = 0; i < 240; i++) {
            uint16_t p = fb[j * 240 + i];
            p = (uint16_t)((p >> 8) | (p << 8));
            raw[n++] = (uint8_t)(((p >> 11) & 31) * 255 / 31);
            raw[n++] = (uint8_t)(((p >> 5) & 63) * 255 / 63);
            raw[n++] = (uint8_t)((p & 31) * 255 / 31);
        }
    }
    for (i = 0; i < len; i++) {
        a = (a + raw[i]) % 65521;
        b = (b + a) % 65521;
    }
#define PUT32(v) do { uint8_t q_[4] = {(uint8_t)((v) >> 24), (uint8_t)((v) >> 16), (uint8_t)((v) >> 8), (uint8_t)(v)}; fwrite(q_, 1, 4, f); } while (0)
    fwrite(hdr, 1, 8, f);
    {
        uint8_t ih[17] = {'I', 'H', 'D', 'R', 0, 0, 0, 240, 0, 0, 0, 240, 8, 2, 0, 0, 0};
        uint32_t c = 0xFFFFFFFFu;
        PUT32(13u);
        fwrite(ih, 1, 17, f);
        for (i = 0; i < 17; i++)
            c = crc_t[(c ^ ih[i]) & 255] ^ (c >> 8);
        PUT32(~c);
    }
    {
        uint32_t nblk = (len + 65534) / 65535, zlen = 2 + len + nblk * 5 + 4, c = 0xFFFFFFFFu;
        uint8_t *z = malloc(zlen + 4), *q = z;
        memcpy(q, "IDAT", 4);
        q += 4;
        *q++ = 0x78;
        *q++ = 0x01;
        for (pos = 0; pos < len;) {
            uint32_t m = len - pos > 65535 ? 65535 : len - pos;
            *q++ = pos + m >= len ? 1 : 0;
            *q++ = (uint8_t)m;
            *q++ = (uint8_t)(m >> 8);
            *q++ = (uint8_t)~m;
            *q++ = (uint8_t)(~m >> 8);
            memcpy(q, raw + pos, m);
            q += m;
            pos += m;
        }
        *q++ = (uint8_t)(b >> 8);
        *q++ = (uint8_t)b;
        *q++ = (uint8_t)(a >> 8);
        *q++ = (uint8_t)a;
        PUT32(zlen);
        fwrite(z, 1, zlen + 4, f);
        for (i = 0; i < zlen + 4; i++)
            c = crc_t[(c ^ z[i]) & 255] ^ (c >> 8);
        PUT32(~c);
        free(z);
    }
    {
        uint8_t ie[4] = {'I', 'E', 'N', 'D'};
        uint32_t c = 0xFFFFFFFFu;
        PUT32(0u);
        fwrite(ie, 1, 4, f);
        for (i = 0; i < 4; i++)
            c = crc_t[(c ^ ie[i]) & 255] ^ (c >> 8);
        PUT32(~c);
    }
    fclose(f);
}

static FILE *wav;
static uint32_t wav_frames;
static FILE *tlog;                                       /* (1.4.0: Transition) tlog FILE: the song's state every 50 ms */
/* (1.4.0: Transition) trwatch: a Set's hand-over watched, from the outro on: the longest stretch under -60 dBFS (250 ms
 * windows, in ms); the largest sample-to-sample step (the 2nd difference) against the music's RMS around it (x100;
 * 10 ms either side: a click stands out of its music, a strike's edge doesn't), from the cue to 2 s past the downbeat, a strike's own first 40 ms left out (a drum, a stab, a noise hit); the next piece's downbeats seen without a stop; the sixteenths from
 * the downbeat to its chord's fader up (0.01), its kick's, its bass's; over its first 8 bars, a fader's largest fall
 * from the highest it reached (x1000: a dip) and its largest rise in a ms (x1000: a slam), the kick's left out (whole
 * on the downbeat, a Mix's) */
static int trw_on, trw_quiet_ms, trw_run_ms, trw_restarts, trw_click, trw_ch = -1, trw_kick = -1, trw_bass = -1,
           trw_dip, trw_jump;
static float trw_fmax[EN_NPART], trw_f1[EN_NPART];
static double trw_acc, trw_sq, trw_x1[2], trw_x2[2];
#define TRW_N 1024                                /* (a ring of the samples: the step judged 10 ms back, the level
                                                  * around it, 10 ms either side) */
static float trw_ring[TRW_N][2];
static uint32_t trw_w;
static uint32_t trw_n, trw_bars_was, trw_t_restart, trw_t_cue, trw_onset_t, trw_was[5];
/* (1.4.2: the Mix's hole, build/design/REPORT-mixgap.txt) the hand-over's loudness as a loudness meter has it (BS.1770:
 * K-weighted, 100 ms steps; the momentary 400 ms, the short-term 3 s), against the music's: the 16 s before the outro
 * (or what there was since trwatch). trhole: the longest stretch of the momentary more than 12 LU under it, from the
 * outro to the next piece's bar 8, ms; trswell: the short-term's highest over it there, LU x10; trlow: the
 * short-term's lowest under it, LU x10 (both from 3 s into the outro) */
#define TRK_N 192
static double trk_b[2][3], trk_a[2][3], trk_z[2][2][2], trk_acc, trk_blk[TRK_N], trk_ref;
static uint32_t trk_n, trk_w, trk_out_w;
static int trk_on, trk_run, trw_hole, trw_swell, trw_low;
static void trk_init(void)
{
    const double fs = 44100.0, pi = 3.14159265358979;
    double K = tan(pi * 1681.974450955533 / fs), Q = 0.7071752369554196, Vh = pow(10.0, 3.999843853973347 / 20.0),
           Vb = pow(Vh, 0.4996667741545416), a0 = 1.0 + K / Q + K * K;
    trk_b[0][0] = (Vh + Vb * K / Q + K * K) / a0, trk_b[0][1] = 2.0 * (K * K - Vh) / a0;
    trk_b[0][2] = (Vh - Vb * K / Q + K * K) / a0;
    trk_a[0][1] = 2.0 * (K * K - 1.0) / a0, trk_a[0][2] = (1.0 - K / Q + K * K) / a0;
    K = tan(pi * 38.13547087602444 / fs), Q = 0.5003270373238773, a0 = 1.0 + K / Q + K * K;
    trk_b[1][0] = 1.0, trk_b[1][1] = -2.0, trk_b[1][2] = 1.0;
    trk_a[1][1] = 2.0 * (K * K - 1.0) / a0, trk_a[1][2] = (1.0 - K / Q + K * K) / a0;
    memset(trk_z, 0, sizeof trk_z);
    trk_acc = 0.0, trk_n = trk_w = 0, trk_on = 0, trk_run = 0, trk_ref = 0.0;
    trw_hole = 0, trw_swell = -999, trw_low = 999;
}
static double trk_mean(uint32_t k)               /* (the last k steps' power) */
{
    double m = 0.0;
    uint32_t i;
    for (i = 1; i <= k; i++)
        m += trk_blk[(trk_w + TRK_N - i) % TRK_N];
    return m / (double)k;
}
static void trk_sample(float l, float r)
{
    const float x[2] = {l, r};
    int c, st;
    for (c = 0; c < 2; c++) {                     /* (two biquads, transposed direct form II) */
        double v = x[c];
        for (st = 0; st < 2; st++) {
            const double y = trk_b[st][0] * v + trk_z[c][st][0];
            trk_z[c][st][0] = trk_b[st][1] * v - trk_a[st][1] * y + trk_z[c][st][1];
            trk_z[c][st][1] = trk_b[st][2] * v - trk_a[st][2] * y;
            v = y;
        }
        trk_acc += v * v;
    }
    if (++trk_n < 4410u)
        return;
    trk_blk[trk_w % TRK_N] = trk_acc / 4410.0;
    trk_w++;
    trk_acc = 0.0, trk_n = 0;
    if (!trk_on && en_song_ph >= 3 && !trw_t_restart) {   /* (the outro: the music's level from before it) */
        const uint32_t k = trk_w - 1u < 160u ? trk_w - 1u : 160u;
        uint32_t i;
        trk_ref = 0.0;
        for (i = 2; i <= k + 1u; i++)
            trk_ref += trk_blk[(trk_w + TRK_N - i) % TRK_N];
        trk_ref = 10.0 * log10(trk_ref / (double)(k ? k : 1u) + 1e-20);
        trk_on = 1, trk_out_w = trk_w;
    }
    if (trk_on == 1 && trw_t_restart && en_bars >= 128u)
        trk_on = 2;                               /* (the next piece's bar 8: done) */
    if (trk_on == 1 && trk_w >= 4u) {
        const double m = 10.0 * log10(trk_mean(4u) + 1e-20) - trk_ref;
        trk_run = m < -12.0 ? trk_run + 100 : 0;
        if (trk_run > trw_hole)
            trw_hole = trk_run;
        if (trk_w >= trk_out_w + 30u) {
            const int sh = (int)floor(10.0 * (10.0 * log10(trk_mean(30u) + 1e-20) - trk_ref) + 0.5);
            trw_swell = sh > trw_swell ? sh : trw_swell;
            trw_low = sh < trw_low ? sh : trw_low;
        }
    }
}
static void trw_sample(float l, float r)
{
    const uint32_t w = trw_w % TRW_N, o = (trw_w + TRW_N - 882u) % TRW_N, m = (trw_w + TRW_N - 441u) % TRW_N;
    int c;
    trw_acc += 0.5 * ((double)l * l + (double)r * r);
    trw_sq += 0.5 * ((double)l * l + (double)r * r) - 0.5 * ((double)trw_ring[o][0] * trw_ring[o][0] +
                                                         (double)trw_ring[o][1] * trw_ring[o][1]);
    trw_ring[w][0] = l, trw_ring[w][1] = r;
    trw_w++;
    trk_sample(l, r);
    for (c = 0; c < 2; c++) {                     /* (the sample 10 ms back, against the level either side of it) */
        const double xm = trw_ring[m][c], d2 = fabs(xm - 2.0 * trw_x1[c] + trw_x2[c]),
                     rms = sqrt((trw_sq > 0.0 ? trw_sq : 0.0) / 882.0);
        const int in = trw_w > 882u && trw_t_cue && now_ms >= trw_t_cue + 10u &&
                       (!trw_t_restart || now_ms < trw_t_restart + 2010u) && now_ms > trw_onset_t + 50u;   /* (not a
                                                  * strike's own edge: a drum, a stab, a noise hit) */
        if (in && (int)(100.0 * d2 / (rms + 1e-4)) > trw_click)
            trw_click = (int)(100.0 * d2 / (rms + 1e-4));
        trw_x2[c] = trw_x1[c], trw_x1[c] = xm;
    }
    if (++trw_n >= 11025u) {                      /* (250 ms) */
        const double db = 10.0 * log10(trw_acc / (double)trw_n + 1e-20);
        trw_run_ms = db < -60.0 && (en_song_ph >= 3 || trw_t_restart) ? trw_run_ms + 250 : 0;   /* (from the outro) */
        if (trw_run_ms > trw_quiet_ms)
            trw_quiet_ms = trw_run_ms;
        trw_acc = 0.0, trw_n = 0;
    }
}
static void trw_ms(void)                          /* (each ms: the song's place) */
{
    if (!trw_on)
        return;
    {   /* (a strike heard: its part's fader up; a silent one, a stab under a faded fader, not left out) */
        const uint32_t now[5] = {en_hits[0], en_hits[1], en_hits[2], en_hits[3], en_hits[4]};   /* (the layers' strikes:
                                                  * the chord, the pings, the bass, the drums, the noise) */
        const int heard[5] = {en_fader[0] > 0.001f, en_fader[1] > 0.001f, en_fader[2] > 0.001f,
                              en_fader[3] > 0.001f || en_fader[4] > 0.001f, en_fader[5] > 0.001f};
        int k;
        for (k = 0; k < 5; k++) {
            if (now[k] != trw_was[k] && heard[k])
                trw_onset_t = now_ms;
            trw_was[k] = now[k];
        }
    }
    if (!trw_t_cue && en_tr_cue)
        trw_t_cue = now_ms;
    if (en_playing && en_bars < trw_bars_was && trw_t_cue) {
        trw_restarts++;
        trw_t_restart = now_ms;
        trw_ch = trw_kick = trw_bass = -1;
        trw_dip = trw_jump = 0;
        memset(trw_fmax, 0, sizeof trw_fmax);
    }
    trw_bars_was = en_bars;
    {   /* (the new piece's first 8 bars: each fader up without a fall, and not at once) */
        int f;
        for (f = 0; f < EN_NPART; f++) {
            if (trw_t_restart && en_bars < 128u && f != 3) {
                const int dip = (int)(1000.0f * (trw_fmax[f] - en_fader[f])),
                          jump = (int)(1000.0f * (en_fader[f] - trw_f1[f]));
                trw_fmax[f] = en_fader[f] > trw_fmax[f] ? en_fader[f] : trw_fmax[f];
                trw_dip = dip > trw_dip ? dip : trw_dip;
                trw_jump = jump > trw_jump ? jump : trw_jump;
            }
            trw_f1[f] = en_fader[f];
        }
    }
    if (trw_t_restart) {
        if (trw_ch < 0 && en_fader[0] > 0.01f)
            trw_ch = (int)en_bars;
        if (trw_kick < 0 && en_fader[3] > 0.01f)
            trw_kick = (int)en_bars;
        if (trw_bass < 0 && en_fader[2] > 0.01f)
            trw_bass = (int)en_bars;
    }
}
static float peak_out;
static void wav_open(const char *path)
{
    static const uint8_t h[44] = {0};
    wav = fopen(path, "wb");
    if (wav)
        fwrite(h, 1, 44, wav);
    wav_frames = 0;
}
static void wav_close(void)
{
    uint32_t bytes = wav_frames * 4, v;
    if (!wav)
        return;
    fseek(wav, 0, SEEK_SET);
    fwrite("RIFF", 1, 4, wav);
    v = 36 + bytes;
    fwrite(&v, 4, 1, wav);
    fwrite("WAVEfmt ", 1, 8, wav);
    v = 16;
    fwrite(&v, 4, 1, wav);
    {
        uint16_t fmt[2] = {1, 2};
        uint32_t r[2] = {44100, 44100 * 4};
        uint16_t al[2] = {4, 16};
        fwrite(fmt, 2, 2, wav);
        fwrite(r, 4, 2, wav);
        fwrite(al, 2, 2, wav);
    }
    fwrite("data", 1, 4, wav);
    fwrite(&bytes, 4, 1, wav);
    fclose(wav);
    wav = 0;
}

/* the ping loops, as a hash (pingmark, expect pingsame) */
static uint32_t ping_mark;
static uint32_t ping_hash(void)
{
    uint32_t h = 2166136261u;
    int l, i;
    for (l = 0; l < EN_NLOOP; l++)
        for (i = 0; i < EN_LOOP_MAX; i++)
            h = (h ^ (uint32_t)(uint8_t)en_loop_note[l][i]) * 16777619u;
    return h;
}

/* ------------------------------------------------------------ film --- */
#define FILM_HDR (6 + NE)
static FILE *film;
static double film_due;
static uint32_t film_t0;
static int32_t film_enc[NE];
static int ui_off;                       /* (splashrun: the launch screen owns the screen) */
/* the stems (host/stems.c: the UI a second time, compiled apart, so its pictures' state is its own): armed by
 * `stems`, recording with the film, fed the audio, ticked after each of the screen's frames */
#ifndef EN_WEB
int stems_arm(const char *dir, const char *names);
void stems_open(void);
void stems_close(void);
void stems_frame(void);
void stems_audio(const int32_t *blk, uint32_t n);
void stems_tick(void);
void stems_main(void *u, size_t nu, void *m, size_t nm, void *e, size_t ne, uint8_t *ew);
void stems_main(void *u, size_t nu, void *m, size_t nm, void *e, size_t ne, uint8_t *ew)   /* (what the screen took) */
{
    if (nu != sizeof ui || nm != sizeof mt || ne != sizeof epk) {
        fprintf(stderr, "stems: the UI's state not the screen's\n");
        exit(2);
    }
    memcpy(u, &ui, nu);
    memcpy(m, &mt, nm);
    memcpy(e, epk, ne);
    *ew = epk_w;
}
#else
#define stems_frame()
#define stems_audio(blk, n)
#define stems_tick()
#endif
static void film_frame(void)
{
    uint32_t h[FILM_HDR], i;
    h[0] = now_ms, h[1] = held_btn, h[2] = held_keys, h[3] = lit_btn, h[4] = lit_keys, h[5] = master;
    for (i = 0; i < NE; i++) {
        h[6 + i] = (uint32_t)film_enc[i];
        film_enc[i] = 0;
    }
    fwrite(h, 4, FILM_HDR, film);
    fwrite(fb, 2, 240 * 240, film);
    stems_frame();
}

/* --------------------------------------------------------- the device --- */
static double audio_due_ms, render_ns_total, render_budget_ns_total;
static uint32_t ui_due;
static double now_ns(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec * 1e9 + (double)t.tv_nsec;
}

static void boot(void)
{
    en_init();
    project_load();
    project_apply();
    lcd_fill(0, 0, 240, 240, 0);
    ui_init();
}

static void run_ms(uint32_t ms)
{
    static int32_t blk[512];
    uint32_t i, k;
    for (i = 0; i < ms; i++) {
        now_ms++;
        while (audio_due_ms <= (double)now_ms) {      /* 256-frame blocks, as the device renders them */
            double t0 = now_ns(), dt;
            en_render(blk, 256, master);
#ifdef EN_WEB
            web_audio(blk, 256);
#endif
            stems_audio(blk, 256);
            dt = now_ns() - t0;
            render_ns_total += dt;
            render_budget_ns_total += 256.0 * 1e9 / 44100.0;
            cpu_pct = (uint32_t)(100.0 * dt / (256.0 * 1e9 / 44100.0));
            for (k = 0; k < 256; k++) {
                float l = (float)blk[2 * k] / 8388608.0f, r = (float)blk[2 * k + 1] / 8388608.0f;
                int16_t s[2];
                if (fabsf(l) > peak_out)
                    peak_out = fabsf(l);
                if (fabsf(r) > peak_out)
                    peak_out = fabsf(r);
                s[0] = (int16_t)(l * 32767.0f);
                s[1] = (int16_t)(r * 32767.0f);
                if (trw_on)
                    trw_sample(l, r);
                if (wav) {
                    fwrite(s, 2, 2, wav);
                    wav_frames++;
                }
            }
            audio_due_ms += 256.0 * 1000.0 / 44100.0;
        }
        if (sim_listen && now_ms % 2000u == 0) {        /* the reader's HELLO, as it sends it */
            static const uint8_t HELLO[] = {ANN_H0, ANN_H1, ANN_H2, ANN_HELLO, ANN_VER};
            ann_frame(HELLO, sizeof HELLO, now_ms);
        }
        trw_ms();
        if (tlog && now_ms % 50u == 0) {                /* ms, wav s, tempo heard, the song's place, bars, the echo, faders,
                                                         * the piece, the Transition's cue */
            int f;
            fprintf(tlog, "%u %.3f %.3f %d %.3f %.1f", now_ms, wav ? (double)wav_frames / 44100.0 : -1.0, (double)en_bpm,
                    (int)en_song_ph, (double)en_bars / 16.0, (double)en_echo_ms);
            for (f = 0; f < EN_NPART; f++)
                fprintf(tlog, " %.4f", (double)en_fader[f]);
            fprintf(tlog, " %d %d %d %.3f %.1f\n", proj.piece, (int)en_tr_cue, (int)en_playing, (double)en_echo_g, 20.0 * log10((double)en_echo_pk + 1e-9));
        }
        if (ui_off) {
        } else if (now_ms >= ui_due) {
            ui_due = now_ms + 16;
            ui_frame();
            stems_tick();
        } else {
            ui_input_only();
        }
        if (film && (double)now_ms >= film_due) {
            film_frame();
            film_due += 1000.0 / 30.0;
        }
    }
}

/* ---------------------------------------------------------- script --- */
static const char *const BTN_N[NB] = {"FX", "SEL", "ENV", "LFO", "EDIT", "GLO", "HOME", "SAVE", "ARP", "SEQ",
                                      "PLAY", "REC", "OCT-", "OCT+"};
static const char *const ENC_N[NE] = {"SELECT", "ALGO", "PRESET", "K1", "K2", "K3", "K4"};

static int find_name(const char *const *t, int n, const char *s)
{
    int i;
    for (i = 0; i < n; i++)
        if (!strcmp(t[i], s))
            return i;
    return -1;
}

static const uint8_t WHITE_K[16] = {0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26};
static const uint8_t BLACK_K[11] = {1, 3, 5, 8, 10, 13, 15, 17, 20, 22, 25};
static int key_of(const char *s)
{
    if (s[0] == 'w')
        return WHITE_K[atoi(s + 1) & 15];
    if (s[0] == 'b')
        return BLACK_K[atoi(s + 1) % 11];
    return atoi(s);
}

static int expect(const char *what, const char *val)
{
    int got;
    if (!strcmp(what, "playing"))
        got = en_playing;
    else if (!strcmp(what, "frozen"))
        got = en_frozen;
    else if (!strcmp(what, "view"))
        got = ui.view;
    else if (!strcmp(what, "piece"))
        got = proj.piece;
    else if (!strcmp(what, "pool"))               /* the scale, relative to the root (hex ok) */
        got = proj.pool;
    else if (!strcmp(what, "format"))
        got = (int)proj.format;
    else if (!strcmp(what, "pingsame"))           /* 1: the ping loops as at the last pingmark */
        got = ping_hash() == ping_mark;
    else if (!strcmp(what, "screen"))
        got = proj.screen;
    else if (!strcmp(what, "values"))            /* (1.4.0) the knobs' numbers: 0 Peek, 1 Always (Setup's Values) */
        got = proj.values;
    else if (!strcmp(what, "trans"))             /* (1.4.0) a Set's Transition: 0 LP, 1 Segue, 2 Mix */
        got = proj.trans;
    else if (!strcmp(what, "trcue"))             /* the Transition's cue up (the next piece set under the last bar) */
        got = en_tr_cue;
    else if (!strcmp(what, "trquiet"))           /* (trwatch) the longest stretch under -60 dBFS since the outro, ms */
        got = trw_quiet_ms;
    else if (!strcmp(what, "trclick"))           /* the largest step against the level around it, x100 (cue .. +2 s) */
        got = trw_click;
    else if (!strcmp(what, "trrestarts"))        /* the next piece's downbeats without a stop */
        got = trw_restarts;
    else if (!strcmp(what, "trchord"))           /* the next piece's chord in, sixteenths after its downbeat (-1: not yet) */
        got = trw_ch;
    else if (!strcmp(what, "trkick"))
        got = trw_kick;
    else if (!strcmp(what, "trbass"))
        got = trw_bass;
    else if (!strcmp(what, "trhole"))            /* (1.4.2) the longest stretch 12 LU under the music, ms (trk_sample) */
        got = trw_hole;
    else if (!strcmp(what, "trswell"))           /* the short-term's highest over the music, LU x10 */
        got = trw_swell;
    else if (!strcmp(what, "trlow"))             /* and its lowest, LU x10 */
        got = trw_low;
    else if (!strcmp(what, "trdip"))             /* the next piece's first 8 bars: a fader's largest fall (x1000) */
        got = trw_dip;
    else if (!strcmp(what, "trjump"))            /* and its largest rise in a ms (x1000; the kick's left out) */
        got = trw_jump;
    else if (!strcmp(what, "qdrops"))            /* (1.4.1) the engine's commands dropped, its queue full */
        got = (int)en_queue_drops();
    else if (!strcmp(what, "endiff")) {          /* (1.4.1) the parameters the engine has otherwise than the project
                                                  * (the chord slots left out: the walk and Endless move them) */
        int p;
        got = 0;
        for (p = 0; p < P_NPARAMS; p++)
            if (!(p >= P_CR0 && p <= P_CX7) && en_par_now(p) != proj.par[p])
                got++;
    }
    else if (!strcmp(what, "bpm"))               /* the tempo heard now, rounded (a Mix's glide) */
        got = (int)(en_bpm + 0.5f);
    else if (!strcmp(what, "peek"))              /* (1.4.0) 1 while a page's knobs show their numbers (a page opened) */
        got = proj.values || plat_ms() < ui.peek_until;
    else if (!strncmp(what, "kbnote", 6)) {       /* kbnoteN: the Chords page keyboard's note N (0..3; 0: the root), MIDI
                                                  * (-1: none) */
        int nt[4], held, n = slot_notes(ui.slot, nt, &held), k = atoi(what + 6) & 3;
        got = k < n ? nt[k] : -1;
    } else if (!strcmp(what, "kbheld"))           /* 1: the keyboard shows the chord a gap holds */
    {
        int nt[4], held;
        (void)slot_notes(ui.slot, nt, &held);
        got = held;
    }
    else if (!strncmp(what, "litkey", 6))         /* litkeyN: key N (0..26) lit */
        got = (int)(lit_keys >> (atoi(what + 6) % 27) & 1u);
    else if (!strcmp(what, "sirenon"))            /* (1.4.0) the dub siren sounding (its gate open): EN_SIR_* + 1, 0 none */
        got = (int)en_sir_on;
    else if (!strncmp(what, "sirk", 4))           /* (format 34) sirkN: siren N / 4's knob N % 4 as saved (proj.sir) */
        got = proj.sir[atoi(what + 4) / EN_NSK % EN_NSIR][atoi(what + 4) % EN_NSK];
    else if (!strcmp(what, "sirpage"))            /* (format 34) the siren the siren page shows (EN_SIR_*) */
        got = sir_page;
    else if (!strcmp(what, "sirhz"))              /* (format 34) the siren's pitch now, Hz (rounded) */
        got = (int)(en_sir_hz + 0.5f);
    else if (!strcmp(what, "mutes"))              /* the layers muted (EN_M_*: 1 chord .. 16 hats, 32 noise) */
        got = (int)ui.mutes;
    else if (!strcmp(what, "scopepk")) {          /* the scope's buffer, its peak (the page's layer alone, or the whole) */
        int i;
        got = 0;
        for (i = 0; i < EN_SCOPE_N; i++)
            got = abs(en_scope[i]) > got ? abs(en_scope[i]) : got;
    }
    else if (!strcmp(what, "helpfit")) {          /* the knobs' help lines too wide (under the big ring, two lines; on the rule) (0: all fit) */
        int v, k;
        char hp[48];
        got = 0;
        for (v = 0; v < NVIEWS; v++)
            for (k = 0; k < 4; k++) {
                char a2[48], b2[48];
                caps(hp, KNOB_HELP[v][k], sizeof hp);
                if (v == V_SIREN) {                  /* (format 34: each siren its own help) */
                    int r;
                    for (r = 0; r < EN_NSIR; r++)
                        if (!help_lines(SIR_HELP[r][k], a2, b2, (int)sizeof a2))
                            printf("  too wide: %s\n", SIR_HELP[r][k]), got++;
                }
                if (v == V_CHORDS || v == V_PAT) {   /* (no big ring there: the help on the rule, in capitals) */
                    if (text_w(&FONT_XS, hp) > 222 || strlen(KNOB_HELP[v][k]) >= sizeof hp)
                        printf("  too wide: %s\n", hp), got++;
                } else if (!help_lines(KNOB_HELP[v][k], a2, b2, (int)sizeof a2)) {   /* (under the big ring: two lines) */
                    printf("  too wide: %s\n", KNOB_HELP[v][k]), got++;
                }
            }
        for (k = 0; k <= EN_NTRANS; k++) {          /* (1.4.0: Transition's, a line a choice, and while not Set: one
                                                   * line of 228 px in the small font) */
            const char *h = k < EN_NTRANS ? TRANS_HELP[k] : TRANS_OFF;
            if (text_w(&FONT_XS, h) > 228)
                printf("  too wide: %s (%d px)\n", h, (int)text_w(&FONT_XS, h)), got++;
        }
        for (v = 0; v < NKNOB_ZONES; v++)           /* (format 31: the zones' sentences, under the big ring) */
            for (k = 0; k < KNOB_ZONES[v].n; k++) {
                char a2[48], b2[48];
                const zone_t *z = &KNOB_ZONES[v].z[k];
                if (!help_lines(z->help, a2, b2, (int)sizeof a2) || (z->help0 && !help_lines(z->help0, a2, b2, (int)sizeof a2)))
                    printf("  too wide: %s\n", z->help), got++;
            }
    }
    else if (!strcmp(what, "zonexs")) {           /* (format 32) the zones' sentences wider than 228 px in FONT_XS, on one
                                                   * line (0: all fit); the names wider than the big ring's side, 70 px */
        int v, k;
        got = 0;
        for (v = 0; v < NKNOB_ZONES; v++)
            for (k = 0; k < KNOB_ZONES[v].n; k++) {
                const zone_t *z = &KNOB_ZONES[v].z[k];
                char zt[16];
                caps(zt, z->name, sizeof zt);
                if (text_w(&FONT_XS, z->help) > 228 || (z->help0 && text_w(&FONT_XS, z->help0) > 228) ||
                    text_w(&FONT_XS, zt) > 70)
                    printf("  too wide: %s (%d px), %s\n", z->help, (int)text_w(&FONT_XS, z->help), zt), got++;
            }
    }
    else if (!strcmp(what, "nev"))                /* the noise's events that show, so far */
        got = (int)en_nev_w;
    else if (!strcmp(what, "blits")) {           /* the screen's draws since the last ask */
        static uint32_t was;
        got = (int)(blits - was);
        was = blits;
    }
    else if (!strcmp(what, "section"))            /* the phrase's section (0 Full, 1 Break, 2 Groove, 3 Thin) */
        got = en_section;
    else if (!strcmp(what, "song"))               /* (1.4.0) the song's place (EN_SG_*: 0 Endless, 1 intro, 2 main, 3 outro,
                                                   * 4 its end) */
        got = (int)en_song_ph;
    else if (!strcmp(what, "songbo"))             /* the bar its outro starts on (0: none coming) */
        got = (int)en_song_bo;
    else if (!strcmp(what, "card"))               /* a nudge's card in the middle: 1 a nudge's, 2 a shake's (0: none) */
        got = card_on(plat_ms()) ? 1 + ui.card_shake : 0;
    else if (!strcmp(what, "lock"))               /* PERFORM's Lock (SEL held) */
        got = ui.lock;
    else if (!strcmp(what, "viewlayer"))          /* what the scope and spectrum listen to (0 the whole, 1.. a layer) */
        got = en_view_layer;
    else if (!strcmp(what, "octring"))            /* the level ring showing: its layer + 1 (0: none) */
        got = oct_ring_on(plat_ms());
    else if (!strcmp(what, "filter"))             /* (1.4.0) the DJ filter (SELECT, PERFORM's KNOB 1): -50 Shut .. +50 Open */
        got = ui.mac[0];
    else if (!strcmp(what, "fltring"))            /* the big ring showing the filter (1) or not */
        got = big_on(plat_ms()) && ui.big_k == K_FILTER;
    else if (!strcmp(what, "bigring"))            /* the big ring showing (a knob's: 1) or not (0) */
        got = big_on(plat_ms());
    else if (!strcmp(what, "dirty"))
        got = ui.dirty;
    else if (!strcmp(what, "store_writes"))
        got = (int)store_writes;
    else if (!strcmp(what, "clocks_out"))
        got = (int)clock_out_count;
    else if (!strcmp(what, "sixteenths"))
        got = (int)en_bars;
    else if (!strcmp(what, "ext"))
        got = en_ext;
    else if (!strcmp(what, "midi_out"))
        got = (int)midi_out_count;
    else if (!strcmp(what, "pad_level"))
        got = (int)(en_pad_level * 100.0f);
    else if (!strcmp(what, "bass_level"))
        got = (int)(en_bass_level * 100.0f);
    else if (!strcmp(what, "chordroot"))
        got = en_chord_root;
    else if (!strcmp(what, "progpos"))            /* the slot the progression is on (Endless: 0; -1: none) */
        got = en_prog_pos;
    else if (!strncmp(what, "chord", 5))          /* chordN: pad voice N's note */
        got = en_chord[atoi(what + 5) & 3];
    else if (!strcmp(what, "loopnotes")) {        /* the notes in all the loops */
        int l, i;
        got = 0;
        for (l = 0; l < EN_NLOOP; l++)
            for (i = 0; i < EN_LOOP_MAX; i++)
                got += en_loop_note[l][i] >= 0;
    } else if (!strncmp(what, "par", 3)) {        /* parN: parameter N (amb.h P_*) */
        int k = atoi(what + 3);
        if (k < 0 || k >= P_NPARAMS) {
            printf("  FAIL expect %s: no such parameter (0..%d)\n", what, P_NPARAMS - 1);
            return 1;
        }
        got = proj.par[k];
    }
    else if (!strcmp(what, "peak_db_max") || !strcmp(what, "peak_db_min")) {
        double db = 20.0 * log10(peak_out > 1e-9f ? peak_out : 1e-9f);
        int bad = what[8] == 'm' && what[9] == 'a' ? db > atof(val) : db < atof(val);
        printf("%s peak %.1f dBFS (%s %s)\n", bad ? "FAIL" : "  ok", db, what + 8, val);
        return bad;
    } else {
        printf("FAIL unknown expect %s\n", what);
        return 1;
    }
    if (val[0] == '!' && val[1] == '=') {        /* (!=N: anything but N) */
        int no = (int)strtol(val + 2, 0, 0);
        if (got == no) {
            printf("FAIL expect %s %s, got %d\n", what, val, got);
            return 1;
        }
        printf("  ok %s == %d (%s)\n", what, got, val);
        return 0;
    }
    if (val[0] == '<' || val[0] == '>') {
        int lim = (int)strtol(val + 1, 0, 0);
        if (val[0] == '<' ? got >= lim : got <= lim) {
            printf("FAIL expect %s %s, got %d\n", what, val, got);
            return 1;
        }
        printf("  ok %s == %d (%s)\n", what, got, val);
        return 0;
    }
    if (got != (int)strtol(val, 0, 0)) {
        printf("FAIL expect %s == %s, got %d\n", what, val, got);
        return 1;
    }
    printf("  ok %s == %d\n", what, got);
    return 0;
}

#ifndef EN_WEB
/* (1.4.0, format 28) the strips' Filter folded back to the Low cut an older format saved (0..100; a low-pass: none) */
static void vy_fold(int16_t *par, int n)        /* (before 29) the layers' Vary trims as the old Varys (0..100, a factor
                                                  * on Evolve: 1 at 50, 3 at 100; Hold 0), project.c's trim28 undone */
{
    static const uint8_t VP[5] = {P_CVARY, P_PVARY, P_BVARY, P_DVARY, P_NVARY};
    int k;
    for (k = 0; k < 5; k++)
        if (VP[k] < n) {
            const int e = par[P_VARY], t = par[VP[k]], a = e + t < 0 ? 0 : e + t > 100 ? 100 : e + t;
            float f;
            int o;
            if (t <= -50) {
                par[VP[k]] = 0;
                continue;
            }
            f = e > 0 ? (float)a / (float)e : 1.0f;
            o = f <= 1.0f ? (int)(50.0f * f + 0.5f) : (int)(50.0f + 25.0f * (f - 1.0f) + 0.5f);
            par[VP[k]] = (int16_t)(o < 1 && a > 0 ? 1 : o > 100 ? 100 : o);
        }
}
static void lc_fold(int16_t *par, int n)        /* (before 28: the strips' Low cut; and before 29 the Varys, vy_fold) */
{
    static const uint8_t LC[6] = {P_CLOW, P_PLOW, P_BLOW, P_DLOW, P_NLOW, P_KLOW};
    int k;
    vy_fold(par, n);
    for (k = 0; k < 6; k++)
        if (LC[k] < n) {
            const int v = par[LC[k]], o = v <= 0 ? 0 : 1 + (int)((float)(v - 1) * (99.0f * 6.966f / (49.0f * 4.907f)) + 0.5f);
            par[LC[k]] = (int16_t)(o > 100 ? 100 : o);
        }
}
/* (the variety plan's step 5) the recipes' Vary, its trims, Motion, Drift and the LFOs' Trims before it (step 4's,
 * as 1.4.0's test builds had them): every older format's store writes them where a piece has today's (a piece never
 * changed then known by its old recipe on the upgrade, as on a device); (format 35) a Vary and its trims changed, on
 * the old scale (the knob the amount itself), as near as they sound now (en_vary34 the way back) */
static void models_before5(void)
{
    static const uint8_t MP[10] = {P_VARY, P_CVARY, P_PVARY, P_BVARY, P_DVARY, P_NVARY, P_MOTION, P_MOVE, P_CLTRIM,
                                   P_NLTRIM};
    static const int16_t OLD[NPIECES][10] = {
        {45, 0, 0, 0, 0, 0, 12, 41, -2, 5},           {25, 0, -17, -12, -15, -15, 8, 27, -25, 50},
        {35, 0, -14, 0, -21, 0, 10, 35, 11, 21},      {40, -16, -16, 0, 0, 0, 42, 72, -12, -15},
        {50, 0, 0, 0, 0, 0, 20, 80, 50, -6},          {40, 0, 0, -16, -16, 0, 38, 50, 12, 15},
        {30, -15, -15, -15, -15, 0, 8, 43, 3, -3},    {20, -12, -12, -12, 0, -12, 10, 41, -25, -13}};
    int16_t rec[P_NPARAMS];
    uint16_t pool;
    int i, k;
    for (i = 0; i < NPIECES; i++) {
        int16_t *p = proj.slot[i].par;
        int v, uv, vo;
        en_piece(i, rec, &pool);
        v = p[P_VARY], uv = v == rec[P_VARY];
        vo = uv ? OLD[i][0] : (int)(100.0f * en_vary_amount(v) + 0.5f);
        for (k = 1; k < 6; k++) {
            const int t = p[MP[k]];
            if (uv && t == rec[MP[k]]) {
                p[MP[k]] = OLD[i][k];
            } else if (t > -50) {
                const int a = (int)(100.0f * en_vary_amount(v + t) + 0.5f), o = a - vo;
                p[MP[k]] = (int16_t)(!a && vo > 49 ? -50 : o < -49 ? -49 : o > 50 ? 50 : o);
            }
        }
        p[P_VARY] = (int16_t)vo;
        for (k = 6; k < 10; k++)
            if (p[MP[k]] == rec[MP[k]])
                p[MP[k]] = OLD[i][k];
    }
    for (k = 0; k < 10; k++)
        proj.par[MP[k]] = proj.slot[proj.piece].par[MP[k]];
}
/* (format 35) the project in 34's layout (a parameter less: no sweep's pace), for the stores of 30 to 34 */
static void as34(project34_t *o)
{
    int i;
    memset(o, 0, sizeof *o);
    o->magic = proj.magic, o->format = proj.format;
    memcpy(o->par, proj.par, sizeof o->par);
    o->pool = proj.pool, o->leds = proj.leds, o->piece = proj.piece, o->screen = proj.screen, o->values = proj.values;
    for (i = 0; i < NPIECES; i++) {
        memcpy(o->slot[i].par, proj.slot[i].par, sizeof o->slot[i].par);
        o->slot[i].pool = proj.slot[i].pool;
        memcpy(o->slot[i].pings, proj.slot[i].pings, sizeof o->slot[i].pings);
        o->slot[i].pings_ok = proj.slot[i].pings_ok;
        o->slot[i].screen = proj.slot[i].screen;
    }
    memcpy(o->rsv, proj.rsv, sizeof o->rsv);
    o->trans = proj.trans;
    memcpy(o->sir, proj.sir, sizeof o->sir);
}
/* (format 31, the variety plan's step 3) the recipes' Motion before 31 (its old meaning: Full only to 40, the Mixer
 * the moves): an older format's store writes it where a piece has step 3's (models_before5's, first), as that build's
 * recipe had it (a piece never changed then known by its old recipe on the upgrade, as on a device) */
static void motion_before31(void)
{
    static const int16_t MOT30[NPIECES] = {30, 30, 25, 72, 35, 45, 25, 35}, MOT31[NPIECES] = {12, 8, 10, 42, 20, 38, 8, 10};
    int i;
    for (i = 0; i < NPIECES; i++)
        if (proj.slot[i].par[P_MOTION] == MOT31[i])
            proj.slot[i].par[P_MOTION] = MOT30[i];
    proj.par[P_MOTION] = proj.slot[proj.piece].par[P_MOTION];
}

/* (format 32, the variety plan's step 4) the knobs rescaled in 32 as they were before it: an older format's store
 * writes them in their old meaning, a piece at today's recipe as that build's recipe had it (known by it on the
 * upgrade, as on a device), another the value that sounded as it does now (en_zones31 the way back) */
static int zb_clamp(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
static void zones_back(int16_t *p, const int16_t *rec, int i)
{
    static const int16_t STB[NPIECES] = {30, 5, 25, 5, 30, 20, 25, 5}, HIT[NPIECES] = {10, 40, 45, 42, 55, 35, 38, 35},
                         BNT[NPIECES] = {90, 100, 90, 100, 85, 90, 90, 100}, NGR[NPIECES] = {56, 44, 63, 44, 56, 38, 19, 69},
                         NCL[NPIECES] = {0, -5, 5, -10, 0, 0, -10, 5};
    static const uint16_t GR[5] = {0x0001u, 0x4444u, 0x0909u, 0x4949u, 0x0481u};
    static const uint8_t GP[5] = {0, 1, 2, 4, 3};
    static const float TONE[5] = {0.0f, -0.2f, 0.3f, -0.3f, 0.15f}, DEC[5] = {0.0f, 0.6f, 0.0f, 0.5f, 0.2f},
                       SPAN[7] = {9.0f, 9.0f, 8.966f, 1.585f, 9.0f, 9.0f, 9.0f};
    const int n = __builtin_popcount(GR[GP[(unsigned)p[P_GROOVE] < 5u ? p[P_GROOVE] : 0]]);
    const unsigned c = (unsigned)p[P_NCHAR] < 5u ? (unsigned)p[P_NCHAR] : 0u;
    int k;
    p[P_STABS] = p[P_STABS] == rec[P_STABS] ? STB[i] : (int16_t)((zb_clamp(p[P_STABS], 1, 6) - 1) * 20);
    if (p[P_HITS] == rec[P_HITS])
        p[P_HITS] = HIT[i];
    if (p[P_BNOTES] == rec[P_BNOTES]) {
        p[P_BNOTES] = BNT[i];
    } else if (n > 1) {
        const float t = 1.0f + (float)(n - 1) * (float)p[P_BNOTES] * 0.01f;
        k = (int)t + (t > (float)(int)t + 0.004f ? 1 : 0);
        p[P_BNOTES] = (int16_t)zb_clamp((k * 100 + n / 2) / n, 0, 100);
    }
    if (p[P_NGRAIN] == rec[P_NGRAIN])
        p[P_NGRAIN] = NGR[i];
    {   /* (the Tones: base + t, from the room either side) */
        for (k = 0; k < 2; k++) {
            const int tp = k ? P_PTON : P_HTONE, b = zb_clamp(p[k ? P_PTONE : P_HATS], 0, 100);
            p[tp] = (int16_t)zb_clamp((int)(en_tone_at(b, p[tp]) + 0.5f) - b, -50, 50);
        }
    }
    if (p[P_ETONE] > 50)
        p[P_ETONE] = (int16_t)(50 + (int)((float)(p[P_ETONE] - 50) * (0.026439f / 0.04f) + 0.5f));
    if (p[P_NCOL] == rec[P_NCOL]) {
        p[P_NCOL] = NCL[i];
    } else {
        const float o = TONE[c], ta = (float)p[P_NCOL] * 0.02f, nc = o + ta * (ta > 0.0f ? 1.0f - o : 1.0f + o);
        p[P_NCOL] = (int16_t)zb_clamp((int)((nc - o) * 50.0f + (nc >= o ? 0.501f : -0.501f)), -50, 50);
    }
    if (p[P_NTUNE] > 50 && DEC[c] > 0.0f) {
        const float o = 0.3f * DEC[c], x = (float)p[P_NTUNE] * 0.01f, d = o + 0.5f + (x - 0.5f) * (1.0f - 2.0f * o);
        p[P_NTUNE] = (int16_t)zb_clamp((int)((d - o) * 100.0f + 0.5f), 0, 100);
    }
    p[P_CATT] = (int16_t)zb_clamp((int)((float)p[P_CATT] * SPAN[(unsigned)p[P_CVOICE] < 7u ? p[P_CVOICE] : 0] / 9.0f + 0.5f), 0, 100);
    if (n == 1 && p[P_BLEN] > 0)
        p[P_BLEN] = (int16_t)(int)((float)p[P_BLEN] * 0.075f + 0.5f);
}
static void zones_before32(void)
{
    int16_t rec[P_NPARAMS];
    uint16_t pool;
    int i;
    for (i = 0; i < NPIECES; i++) {
        en_piece(i, rec, &pool);
        zones_back(proj.slot[i].par, rec, i);
    }
    for (i = 0; i < P_NPARAMS; i++)
        proj.par[i] = proj.slot[proj.piece].par[i];
}

int main(int argc, char **argv)
{
    FILE *sc;
    char line[512], out[400];
    const char *dir = argc > 2 ? argv[2] : ".";
    int lineno = 0, fails = 0;
    if (argc < 2) {
        fprintf(stderr, "usage: amb_host SCRIPT [OUTDIR]\n");
        return 2;
    }
    sc = fopen(argv[1], "r");
    if (!sc) {
        perror(argv[1]);
        return 2;
    }
    snprintf(out, sizeof out, "%s/midi_out.txt", dir);
    midi_log = fopen(out, "w");
    if (getenv("AMB_SAID_LOG")) {                        /* a reader listens from the start, quietly: the log only */
        said_log_f = fopen(getenv("AMB_SAID_LOG"), "a");
        sim_listen = 1;
        said_print = 0;
    }
    if (getenv("AMB_STEMS")) {                           /* stems with the script's film: "DIR [NAMES]" */
        char sd[400] = {0}, sn[256] = {0};
        if (sscanf(getenv("AMB_STEMS"), "%399s %255s", sd, sn) < 1 || stems_arm(sd, sn) < 0)
            return 2;
    }
    boot();
    if (sim_listen) {
        static const uint8_t HELLO[] = {ANN_H0, ANN_H1, ANN_H2, ANN_HELLO, ANN_VER};
        ann_frame(HELLO, sizeof HELLO, now_ms);
    }
    while (fgets(line, sizeof line, sc)) {
        char cmd[32] = {0}, a[256] = {0}, b[64] = {0}, c[64] = {0};
        static project_t st_keep;
        int st_old, st_pre5;
        lineno++;
        if (line[0] == '#' || sscanf(line, "%31s %255s %63s %63s", cmd, a, b, c) < 1)
            continue;
        st_old = !strncmp(cmd, "store", 5) && strcmp(cmd, "storefile") && strcmp(cmd, "storescreen") &&
                 strcmp(cmd, "store32") && strcmp(cmd, "store34");   /* (32, 34: today's meanings, but Vary's
                                                  * and the sweep's; no Transition in 32) */
        st_pre5 = st_old || !strcmp(cmd, "store32") || !strcmp(cmd, "store34");
        if (st_pre5) {                           /* (step 5) every older format's store: the recipes before it */
            keep();
            st_keep = proj;
            models_before5();
        }
        if (st_old) {                            /* (format 31) an older format's store: the pieces' Motion as their
                                                  * recipes had it then, where it's today's */
            zones_before32();                    /* (format 32: the rescaled knobs, in their old meaning) */
            if (strcmp(cmd, "store31"))
                motion_before31();
        }
        if (!strcmp(cmd, "wait"))
            run_ms((uint32_t)atoi(a));
        else if (!strcmp(cmd, "press") || !strcmp(cmd, "release") || !strcmp(cmd, "tap")) {
            int i = find_name(BTN_N, NB, a);
            if (i < 0) {
                printf("line %d: no button %s\n", lineno, a);
                return 2;
            }
            if (cmd[0] != 'r')
                held_btn |= 1u << i;
            if (cmd[0] == 't')
                run_ms(40);
            if (cmd[0] != 'p')
                held_btn &= ~(1u << i);
            run_ms(cmd[0] == 't' ? 40 : 2);
        } else if (!strcmp(cmd, "key") || !strcmp(cmd, "tapkey")) {
            int k = key_of(a);
            if (cmd[0] == 't') {
                held_keys |= 1u << k;
                run_ms(60);
                held_keys &= ~(1u << k);
                run_ms(30);
            } else {
                if (!strcmp(b, "down"))
                    held_keys |= 1u << k;
                else
                    held_keys &= ~(1u << k);
                run_ms(2);
            }
        } else if (!strcmp(cmd, "strum")) {          /* strum A B [MS]: a finger across the white keys */
            int x = atoi(a), y = atoi(b), gap = c[0] ? atoi(c) : 25, s = y >= x ? 1 : -1;
            for (;; x += s) {
                held_keys |= 1u << WHITE_K[x & 15];
                run_ms((uint32_t)gap);
                held_keys &= ~(1u << WHITE_K[x & 15]);
                if (x == y)
                    break;
            }
            run_ms(2);
        } else if (!strcmp(cmd, "turn")) {
            int e = find_name(ENC_N, NE, a), n = atoi(b), s = n > 0 ? 1 : -1;
            uint32_t gap = c[0] ? (uint32_t)atoi(c) : 80u;
            if (e < 0) {
                printf("line %d: no encoder %s\n", lineno, a);
                return 2;
            }
            for (; n; n -= s) {
                enc_acc[e] += s;
                film_enc[e] += s;
                run_ms(gap);
            }
        } else if (!strcmp(cmd, "spin")) {
            int e = find_name(ENC_N, NE, a);
            if (e < 0) {
                printf("line %d: no encoder %s\n", lineno, a);
                return 2;
            }
            enc_acc[e] += atoi(b);
            film_enc[e] += atoi(b);
            accel_off = 1;
            run_ms(1);
            accel_off = 0;
        } else if (!strcmp(cmd, "master"))
            master = (uint32_t)atoi(a);
        else if (!strcmp(cmd, "midi")) {
            uint32_t s = (uint32_t)strtoul(a, 0, 16), d1 = (uint32_t)strtoul(b, 0, 16), d2 = (uint32_t)strtoul(c, 0, 16);
            uint32_t cin = s >= 0xF0 ? 0x0F : s >> 4;
            min_q[mi_w++ % MQ] = cin | s << 8 | d1 << 16 | d2 << 24;
        } else if (!strcmp(cmd, "wav")) {
            snprintf(out, sizeof out, "%s/%s", dir, a);
            wav_open(out);
        } else if (!strcmp(cmd, "wavstop"))
            wav_close();
        else if (!strcmp(cmd, "trwatch")) {      /* (1.4.0) a Set's hand-over watched from here (trquiet, trclick ..) */
            trw_on = 1, trw_quiet_ms = trw_run_ms = trw_restarts = trw_click = 0, trw_ch = trw_kick = trw_bass = -1;
            trw_dip = trw_jump = 0;
            trk_init();
            trw_acc = trw_sq = 0.0, trw_n = 0, trw_t_restart = trw_t_cue = 0, trw_bars_was = en_bars, trw_w = 0;
            memset(trw_ring, 0, sizeof trw_ring);
        } else if (!strcmp(cmd, "tlog")) {         /* tlog FILE: the song's state every 50 ms (tempo, place, faders) */
            snprintf(out, sizeof out, "%s/%s", dir, a);
            tlog = fopen(out, "w");
        } else if (!strcmp(cmd, "tlogstop")) {
            if (tlog)
                fclose(tlog);
            tlog = 0;
        }
        else if (!strcmp(cmd, "peakreset"))
            peak_out = 0.0f;
        else if (!strcmp(cmd, "shot")) {
            snprintf(out, sizeof out, "%s/%s", dir, a);
            png_write(out);
        } else if (!strcmp(cmd, "leds")) {
            int i;
            printf("  leds: buttons");
            for (i = 0; i < NB; i++)
                if (lit_btn >> i & 1u)
                    printf(" %s", BTN_N[i]);
            printf(" | white ");
            for (i = 0; i < 16; i++)
                printf("%c", (lit_keys >> WHITE_K[i] & 1u) ? '#' : '.');
            printf(" black ");
            for (i = 0; i < 11; i++)
                printf("%c", (lit_keys >> BLACK_K[i] & 1u) ? '#' : '.');
            printf("\n");
        } else if (!strcmp(cmd, "expect"))
            fails += expect(a, b);
        else if (!strcmp(cmd, "film")) {
            snprintf(out, sizeof out, "%s/%s", dir, a);
            film = fopen(out, "wb");
            film_due = (double)now_ms;
            film_t0 = now_ms;
            stems_open();
        } else if (!strcmp(cmd, "filmstop")) {
            if (film)
                fclose(film);
            film = 0;
            stems_close();
        } else if (!strcmp(cmd, "stems")) {     /* stems DIR [NAMES]: the pictures alone, with the next film */
            char nm[256] = {0};
            sscanf(line, "%*s %*s %255s", nm);   /* (the names: longer than b) */
            snprintf(out, sizeof out, "%s/%s", dir, a);
            if (stems_arm(out, nm) < 0)
                return 2;
        } else if (!strcmp(cmd, "at") || !strcmp(cmd, "bar")) {
            const char *lab = strstr(line, a) + strlen(a);
            uint32_t t;
            while (*lab == ' ' || *lab == '\t')
                lab++;
            if (cmd[0] == 'a') {
                uint32_t due = film_t0 + (uint32_t)(atof(a) * 1000.0 + 0.5);
                if (now_ms > due)
                    printf("line %d: at %s is already past (%.1f s)\n", lineno, a, (now_ms - film_t0) / 1000.0);
                while (now_ms < due)
                    run_ms(1);
            } else {
                uint32_t want = 16u * (uint32_t)atoi(a), guard = 0;
                while (en_bars < want && guard++ < 600000u)
                    run_ms(1);
            }
            t = now_ms - film_t0;
            t = (t + 50u) / 100u;                /* (in tenths, rounded first: 59.96 s is 01:00.0, not 00:60.0) */
            printf("@ %02u:%02u.%u  bar %u  %s", t / 600u, t % 600u / 10u, t % 10u, en_bars / 16u, lab[0] ? lab : "\n");
        } else if (!strcmp(cmd, "store8")) {     /* the project as DUB-1 1.0.0 saved it (format 8), into the flash */
            static project8_t o;
            int i;
            keep();
            memset(&o, 0, sizeof o);
            o.magic = PROJ_MAGIC, o.format = 8u;
            memcpy(o.par, proj.par, sizeof o.par);
            o.par[P_CUTOFF] = cut_abs(proj.par);
            o.pool = proj.pool, o.leds = proj.leds, o.piece = proj.piece, o.screen = proj.screen;
            for (i = 0; i < NPIECES; i++) {
                memcpy(o.slot[i].par, proj.slot[i].par, sizeof o.slot[i].par);
                o.slot[i].par[P_CUTOFF] = cut_abs(proj.slot[i].par);
                o.slot[i].pool = proj.slot[i].pool;
            }
            plat_store_save(OBJ_PROJ, &o, sizeof o);
        } else if (!strcmp(cmd, "store10")) {    /* the project as DUB-1 1.2.0 / 1.3.0's chords saved it (format 10) */
            static project10_t o;
            int i;
            keep();
            memset(&o, 0, sizeof o);
            o.magic = PROJ_MAGIC, o.format = 10u;
            memcpy(o.par, proj.par, sizeof o.par);
            o.par[P_CUTOFF] = cut_abs(proj.par);
            o.pool = proj.pool, o.leds = proj.leds, o.piece = proj.piece, o.screen = proj.screen;
            for (i = 0; i < NPIECES; i++) {
                memcpy(o.slot[i].par, proj.slot[i].par, sizeof o.slot[i].par);
                o.slot[i].par[P_CUTOFF] = cut_abs(proj.slot[i].par);
                o.slot[i].pool = proj.slot[i].pool;
                memcpy(o.slot[i].pings, proj.slot[i].pings, sizeof o.slot[i].pings);
                o.slot[i].pings_ok = proj.slot[i].pings_ok;
                o.slot[i].screen = proj.slot[i].screen;
            }
            plat_store_save(OBJ_PROJ, &o, sizeof o);
        } else if (!strcmp(cmd, "store12")) {    /* the project as 1.3.0's ladder saved it (format 12) */
            static project12_t o;
            static const int16_t LAD[5] = {0, 2, 4, 6, 8};
            int i;
            keep();
            memset(&o, 0, sizeof o);
            o.magic = PROJ_MAGIC, o.format = 12u;
            memcpy(o.par, proj.par, sizeof o.par);
            o.par[P_CUTOFF] = cut_abs(proj.par);
            o.par[P_GROOVE] = a[0] ? (int16_t)atoi(a) : LAD[proj.par[P_GROOVE] % 5];
            o.pool = proj.pool, o.leds = proj.leds, o.piece = proj.piece, o.screen = proj.screen;
            for (i = 0; i < NPIECES; i++) {
                memcpy(o.slot[i].par, proj.slot[i].par, sizeof o.slot[i].par);
                o.slot[i].par[P_CUTOFF] = cut_abs(proj.slot[i].par);
                o.slot[i].par[P_GROOVE] = a[0] && i == proj.piece ? (int16_t)atoi(a) : LAD[proj.slot[i].par[P_GROOVE] % 5];
                o.slot[i].pool = proj.slot[i].pool;
                memcpy(o.slot[i].pings, proj.slot[i].pings, sizeof o.slot[i].pings);
                o.slot[i].pings_ok = proj.slot[i].pings_ok;
                o.slot[i].screen = proj.slot[i].screen;
            }
            plat_store_save(OBJ_PROJ, &o, sizeof o);
        } else if (!strcmp(cmd, "recipe")) {     /* (step 5b) recipe N: piece N back to its recipe, unplayed, as the
                                                  * device's never-changed piece (the older stores can't write every
                                                  * old build's recipe: a piece they leave changed, made new again) */
            const int i = atoi(a);
            keep();
            if (i >= 0 && i < NPIECES) {
                recipe(i, &proj.slot[i]);
                if (i == proj.piece)
                    take(&proj.slot[i]);
            }
        } else if (!strcmp(cmd, "storefile")) {  /* a saved project from a file, as it would be in the flash */
            static uint8_t fb[PLAT_STORE_MAX];
            FILE *f = fopen(a, "rb");
            size_t got = f ? fread(fb, 1, sizeof fb, f) : 0;
            if (f)
                fclose(f);
            if (!got) {
                printf("line %d: no file %s\n", lineno, a);
                return 2;
            }
            plat_store_save(OBJ_PROJ, fb, (uint32_t)got);
        } else if (!strcmp(cmd, "storescreen")) {   /* storescreen N: saved as it is, the piece's view N (0..5: a
                                                  * 1.4.0 build's echo trail 3, fifths 4, tape 5) */
            keep();
            proj.screen = (uint8_t)atoi(a);
            proj.slot[proj.piece].screen = proj.screen;
            plat_store_save(OBJ_PROJ, &proj, sizeof proj);
        } else if (!strcmp(cmd, "store27") || !strcmp(cmd, "store28")) {   /* the project as 1.4.0's dub mixer
                                                  * saved it (27: the strips' Low cut) or KellyOffski's Filter (28): no
                                                  * looseness, the Varys 0..100 on Evolve */
            static project28_t o;
            const int f27 = !strcmp(cmd, "store27");
            int i;
            keep();
            memset(&o, 0, sizeof o);
            o.magic = PROJ_MAGIC, o.format = f27 ? 27u : 28u;
            memcpy(o.par, proj.par, sizeof o.par);
            if (f27)
                lc_fold(o.par, NPAR28);
            else
                vy_fold(o.par, NPAR28);
            o.pool = proj.pool, o.leds = proj.leds, o.piece = proj.piece, o.screen = proj.screen;
            for (i = 0; i < NPIECES; i++) {
                memcpy(o.slot[i].par, proj.slot[i].par, sizeof o.slot[i].par);
                if (f27)
                    lc_fold(o.slot[i].par, NPAR28);
                else
                    vy_fold(o.slot[i].par, NPAR28);
                o.slot[i].pool = proj.slot[i].pool;
                memcpy(o.slot[i].pings, proj.slot[i].pings, sizeof o.slot[i].pings);
                o.slot[i].pings_ok = proj.slot[i].pings_ok;
                o.slot[i].screen = proj.slot[i].screen;
            }
            plat_store_save(OBJ_PROJ, &o, sizeof o);
        } else if (!strcmp(cmd, "store30s")) {   /* the project as step 2's test builds saved it (zzo, zzp: format 30,
                                                  * the LFOs' Shape in the Wave's slots, Wander 4 or Sine 0, no mark) */
            static project34_t o;
            int i;
            keep();
            as34(&o);
            o.format = 30u;                      /* (format 31: the same layout) */
            o.rsv[0] = 0;
            o.par[P_CLWAVE] = (int16_t)(o.par[P_CLWAVE] ? 0 : 4), o.par[P_NLWAVE] = (int16_t)(o.par[P_NLWAVE] ? 0 : 4);
            for (i = 0; i < NPIECES; i++) {
                o.slot[i].par[P_CLWAVE] = (int16_t)(o.slot[i].par[P_CLWAVE] ? 0 : 4);
                o.slot[i].par[P_NLWAVE] = (int16_t)(o.slot[i].par[P_NLWAVE] ? 0 : 4);
            }
            plat_store_save(OBJ_PROJ, &o, sizeof o);
        } else if (!strcmp(cmd, "store34")) {    /* (format 35) the project as step 5 saved it (34: 34's layout,
                                                  * Vary on its old scale, no sweep's pace) */
            static project34_t o;
            keep();
            as34(&o);
            o.format = 34u;
            o.rsv[0] = PROJ_WAVE_MARK;
            plat_store_save(OBJ_PROJ, &o, sizeof o);
        } else if (!strcmp(cmd, "store32")) {    /* (format 34) the project as step 4 saved it (32: today's layout
                                                  * without the sirens' knobs at its end, its last byte room: the
                                                  * Transition's place set to Mix, to show it read as LP) */
            static project34_t o;
            keep();
            as34(&o);
            o.format = 32u;
            o.rsv[0] = PROJ_WAVE_MARK;
            o.trans = EN_TR_MIX;
            plat_store_save(OBJ_PROJ, &o, sizeof o - sizeof o.sir);
        } else if (!strcmp(cmd, "store31")) {    /* the project as the variety plan's step 3 saved it (31: today's
                                                  * layout, the knobs with dead steps in their old meaning) */
            static project34_t o;
            keep();
            as34(&o);
            o.format = 31u;
            o.rsv[0] = PROJ_WAVE_MARK;
            plat_store_save(OBJ_PROJ, &o, sizeof o);
        } else if (!strcmp(cmd, "store30")) {    /* store30 [MIXER]: the project as the variety plan's step 2 saved it
                                                  * (30: today's layout, Motion's old meaning, the Mixer read; MIXER the
                                                  * playing piece's Mixer, else as it is) */
            static project34_t o;
            keep();
            as34(&o);
            o.format = 30u;
            o.rsv[0] = PROJ_WAVE_MARK;
            if (a[0]) {
                o.par[P_MIXER] = (int16_t)atoi(a);
                o.slot[o.piece].par[P_MIXER] = o.par[P_MIXER];
            }
            plat_store_save(OBJ_PROJ, &o, sizeof o);
        } else if (!strcmp(cmd, "store29")) {    /* the project as the variety plan's step 1 saved it (29) */
            static project29_t o;
            int i;
            keep();
            memset(&o, 0, sizeof o);
            o.magic = PROJ_MAGIC, o.format = 29u;
            memcpy(o.par, proj.par, sizeof o.par);
            o.pool = proj.pool, o.leds = proj.leds, o.piece = proj.piece, o.screen = proj.screen;
            for (i = 0; i < NPIECES; i++) {
                memcpy(o.slot[i].par, proj.slot[i].par, sizeof o.slot[i].par);
                o.slot[i].pool = proj.slot[i].pool;
                memcpy(o.slot[i].pings, proj.slot[i].pings, sizeof o.slot[i].pings);
                o.slot[i].pings_ok = proj.slot[i].pings_ok;
                o.slot[i].screen = proj.slot[i].screen;
            }
            plat_store_save(OBJ_PROJ, &o, sizeof o);
        } else if (!strcmp(cmd, "store26")) {    /* the project as 1.4.0's song structure saved it (26) */
            static project26_t o;
            int i;
            keep();
            memset(&o, 0, sizeof o);
            o.magic = PROJ_MAGIC, o.format = 26u;
            memcpy(o.par, proj.par, sizeof o.par);
            lc_fold(o.par, NPAR26);
            o.pool = proj.pool, o.leds = proj.leds, o.piece = proj.piece, o.screen = proj.screen;
            for (i = 0; i < NPIECES; i++) {
                memcpy(o.slot[i].par, proj.slot[i].par, sizeof o.slot[i].par);
                lc_fold(o.slot[i].par, NPAR26);
                o.slot[i].pool = proj.slot[i].pool;
                memcpy(o.slot[i].pings, proj.slot[i].pings, sizeof o.slot[i].pings);
                o.slot[i].pings_ok = proj.slot[i].pings_ok;
                o.slot[i].screen = proj.slot[i].screen;
            }
            plat_store_save(OBJ_PROJ, &o, sizeof o);
        } else if (!strcmp(cmd, "store25")) {    /* the project as 1.4.0's noise LFO Destination saved it (25) */
            static project25_t o;
            int i;
            keep();
            memset(&o, 0, sizeof o);
            o.magic = PROJ_MAGIC, o.format = 25u;
            memcpy(o.par, proj.par, sizeof o.par);
            lc_fold(o.par, NPAR25);
            o.pool = proj.pool, o.leds = proj.leds, o.piece = proj.piece, o.screen = proj.screen;
            for (i = 0; i < NPIECES; i++) {
                memcpy(o.slot[i].par, proj.slot[i].par, sizeof o.slot[i].par);
                lc_fold(o.slot[i].par, NPAR25);
                o.slot[i].pool = proj.slot[i].pool;
                memcpy(o.slot[i].pings, proj.slot[i].pings, sizeof o.slot[i].pings);
                o.slot[i].pings_ok = proj.slot[i].pings_ok;
                o.slot[i].screen = proj.slot[i].screen;
            }
            plat_store_save(OBJ_PROJ, &o, sizeof o);
        } else if (!strcmp(cmd, "store24")) {    /* the project as 1.4.0's six strips saved it (format 24) */
            static project24_t o;
            int i;
            keep();
            memset(&o, 0, sizeof o);
            o.magic = PROJ_MAGIC, o.format = 24u;
            memcpy(o.par, proj.par, sizeof o.par);
            lc_fold(o.par, NPAR24);
            o.pool = proj.pool, o.leds = proj.leds, o.piece = proj.piece, o.screen = proj.screen;
            for (i = 0; i < NPIECES; i++) {
                memcpy(o.slot[i].par, proj.slot[i].par, sizeof o.slot[i].par);
                lc_fold(o.slot[i].par, NPAR24);
                o.slot[i].pool = proj.slot[i].pool;
                memcpy(o.slot[i].pings, proj.slot[i].pings, sizeof o.slot[i].pings);
                o.slot[i].pings_ok = proj.slot[i].pings_ok;
                o.slot[i].screen = proj.slot[i].screen;
            }
            plat_store_save(OBJ_PROJ, &o, sizeof o);
        } else if (!strcmp(cmd, "store23")) {    /* the project as 1.4.0's channel strips saved it (format 23) */
            static project23_t o;
            int i;
            keep();
            memset(&o, 0, sizeof o);
            o.magic = PROJ_MAGIC, o.format = 23u;
            memcpy(o.par, proj.par, sizeof o.par);
            lc_fold(o.par, NPAR23);
            o.pool = proj.pool, o.leds = proj.leds, o.piece = proj.piece, o.screen = proj.screen;
            for (i = 0; i < NPIECES; i++) {
                memcpy(o.slot[i].par, proj.slot[i].par, sizeof o.slot[i].par);
                lc_fold(o.slot[i].par, NPAR23);
                o.slot[i].pool = proj.slot[i].pool;
                memcpy(o.slot[i].pings, proj.slot[i].pings, sizeof o.slot[i].pings);
                o.slot[i].pings_ok = proj.slot[i].pings_ok;
                o.slot[i].screen = proj.slot[i].screen;
            }
            plat_store_save(OBJ_PROJ, &o, sizeof o);
        } else if (!strcmp(cmd, "store22")) {    /* the project as 1.4.0's layers' pages saved it (format 22) */
            static project22_t o;
            int i;
            keep();
            memset(&o, 0, sizeof o);
            o.magic = PROJ_MAGIC, o.format = 22u;
            memcpy(o.par, proj.par, sizeof o.par);
            vy_fold(o.par, NPAR22);
            o.pool = proj.pool, o.leds = proj.leds, o.piece = proj.piece, o.screen = proj.screen;
            for (i = 0; i < NPIECES; i++) {
                memcpy(o.slot[i].par, proj.slot[i].par, sizeof o.slot[i].par);
                vy_fold(o.slot[i].par, NPAR22);
                o.slot[i].pool = proj.slot[i].pool;
                memcpy(o.slot[i].pings, proj.slot[i].pings, sizeof o.slot[i].pings);
                o.slot[i].pings_ok = proj.slot[i].pings_ok;
                o.slot[i].screen = proj.slot[i].screen;
            }
            plat_store_save(OBJ_PROJ, &o, sizeof o);
        } else if (!strcmp(cmd, "store21")) {    /* the project as 1.4.0's Chord pages saved it (format 21) */
            static project21_t o;
            int i;
            keep();
            memset(&o, 0, sizeof o);
            o.magic = PROJ_MAGIC, o.format = 21u;
            memcpy(o.par, proj.par, sizeof o.par);
            vy_fold(o.par, NPAR21);
            o.par[P_CUTOFF] = cut_abs(proj.par);
            o.pool = proj.pool, o.leds = proj.leds, o.piece = proj.piece, o.screen = proj.screen;
            for (i = 0; i < NPIECES; i++) {
                memcpy(o.slot[i].par, proj.slot[i].par, sizeof o.slot[i].par);
                o.slot[i].par[P_CUTOFF] = cut_abs(proj.slot[i].par);
                vy_fold(o.slot[i].par, NPAR21);
                o.slot[i].pool = proj.slot[i].pool;
                memcpy(o.slot[i].pings, proj.slot[i].pings, sizeof o.slot[i].pings);
                o.slot[i].pings_ok = proj.slot[i].pings_ok;
                o.slot[i].screen = proj.slot[i].screen;
            }
            plat_store_save(OBJ_PROJ, &o, sizeof o);
        } else if (!strcmp(cmd, "store20")) {    /* the project as 1.4.0's LFO pages saved it (format 20) */
            static project20_t o;
            int i;
            keep();
            memset(&o, 0, sizeof o);
            o.magic = PROJ_MAGIC, o.format = 20u;
            memcpy(o.par, proj.par, sizeof o.par);
            o.par[P_CUTOFF] = cut_abs(proj.par);
            o.pool = proj.pool, o.leds = proj.leds, o.piece = proj.piece, o.screen = proj.screen;
            for (i = 0; i < NPIECES; i++) {
                memcpy(o.slot[i].par, proj.slot[i].par, sizeof o.slot[i].par);
                o.slot[i].par[P_CUTOFF] = cut_abs(proj.slot[i].par);
                o.slot[i].pool = proj.slot[i].pool;
                memcpy(o.slot[i].pings, proj.slot[i].pings, sizeof o.slot[i].pings);
                o.slot[i].pings_ok = proj.slot[i].pings_ok;
                o.slot[i].screen = proj.slot[i].screen;
            }
            plat_store_save(OBJ_PROJ, &o, sizeof o);
        } else if (!strcmp(cmd, "store18")) {    /* the project as 1.4.0's test builds saved it (format 18) */
            static project18_t o;
            int i;
            keep();
            memset(&o, 0, sizeof o);
            o.magic = PROJ_MAGIC, o.format = 18u;
            memcpy(o.par, proj.par, sizeof o.par);
            o.par[P_CUTOFF] = cut_abs(proj.par);
            o.pool = proj.pool, o.leds = proj.leds, o.piece = proj.piece, o.screen = proj.screen;
            for (i = 0; i < NPIECES; i++) {
                memcpy(o.slot[i].par, proj.slot[i].par, sizeof o.slot[i].par);
                o.slot[i].par[P_CUTOFF] = cut_abs(proj.slot[i].par);
                o.slot[i].pool = proj.slot[i].pool;
                memcpy(o.slot[i].pings, proj.slot[i].pings, sizeof o.slot[i].pings);
                o.slot[i].pings_ok = proj.slot[i].pings_ok;
                o.slot[i].screen = proj.slot[i].screen;
            }
            plat_store_save(OBJ_PROJ, &o, sizeof o);
        } else if (!strcmp(cmd, "store16")) {    /* the project as 1.3.0's noise LFO saved it (format 16) */
            static project16_t o;
            int i;
            keep();
            memset(&o, 0, sizeof o);
            o.magic = PROJ_MAGIC, o.format = 16u;
            memcpy(o.par, proj.par, sizeof o.par);
            o.par[P_CUTOFF] = cut_abs(proj.par);
            o.pool = proj.pool, o.leds = proj.leds, o.piece = proj.piece, o.screen = proj.screen;
            for (i = 0; i < NPIECES; i++) {
                memcpy(o.slot[i].par, proj.slot[i].par, sizeof o.slot[i].par);
                o.slot[i].par[P_CUTOFF] = cut_abs(proj.slot[i].par);
                o.slot[i].pool = proj.slot[i].pool;
                memcpy(o.slot[i].pings, proj.slot[i].pings, sizeof o.slot[i].pings);
                o.slot[i].pings_ok = proj.slot[i].pings_ok;
                o.slot[i].screen = proj.slot[i].screen;
            }
            plat_store_save(OBJ_PROJ, &o, sizeof o);
        } else if (!strcmp(cmd, "store15")) {    /* the project as 1.3.0's Noise space saved it (format 15) */
            static project15_t o;
            int i;
            keep();
            memset(&o, 0, sizeof o);
            o.magic = PROJ_MAGIC, o.format = 15u;
            memcpy(o.par, proj.par, sizeof o.par);
            o.par[P_CUTOFF] = cut_abs(proj.par);
            o.pool = proj.pool, o.leds = proj.leds, o.piece = proj.piece, o.screen = proj.screen;
            for (i = 0; i < NPIECES; i++) {
                memcpy(o.slot[i].par, proj.slot[i].par, sizeof o.slot[i].par);
                o.slot[i].par[P_CUTOFF] = cut_abs(proj.slot[i].par);
                o.slot[i].pool = proj.slot[i].pool;
                memcpy(o.slot[i].pings, proj.slot[i].pings, sizeof o.slot[i].pings);
                o.slot[i].pings_ok = proj.slot[i].pings_ok;
                o.slot[i].screen = proj.slot[i].screen;
            }
            plat_store_save(OBJ_PROJ, &o, sizeof o);
        } else if (!strcmp(cmd, "store14")) {    /* the project as 1.3.0's echo and density saved it (format 14) */
            static project14_t o;
            int i;
            keep();
            memset(&o, 0, sizeof o);
            o.magic = PROJ_MAGIC, o.format = 14u;
            memcpy(o.par, proj.par, sizeof o.par);
            o.par[P_CUTOFF] = cut_abs(proj.par);
            o.pool = proj.pool, o.leds = proj.leds, o.piece = proj.piece, o.screen = proj.screen;
            for (i = 0; i < NPIECES; i++) {
                memcpy(o.slot[i].par, proj.slot[i].par, sizeof o.slot[i].par);
                o.slot[i].par[P_CUTOFF] = cut_abs(proj.slot[i].par);
                o.slot[i].pool = proj.slot[i].pool;
                memcpy(o.slot[i].pings, proj.slot[i].pings, sizeof o.slot[i].pings);
                o.slot[i].pings_ok = proj.slot[i].pings_ok;
                o.slot[i].screen = proj.slot[i].screen;
            }
            plat_store_save(OBJ_PROJ, &o, sizeof o);
        } else if (!strcmp(cmd, "store13")) {    /* the project as 1.3.0's pages saved it (format 13) */
            static project13_t o;
            int i;
            keep();
            memset(&o, 0, sizeof o);
            o.magic = PROJ_MAGIC, o.format = 13u;
            memcpy(o.par, proj.par, sizeof o.par);
            o.par[P_CUTOFF] = cut_abs(proj.par);
            o.pool = proj.pool, o.leds = proj.leds, o.piece = proj.piece, o.screen = proj.screen;
            for (i = 0; i < NPIECES; i++) {
                memcpy(o.slot[i].par, proj.slot[i].par, sizeof o.slot[i].par);
                o.slot[i].par[P_CUTOFF] = cut_abs(proj.slot[i].par);
                o.slot[i].pool = proj.slot[i].pool;
                memcpy(o.slot[i].pings, proj.slot[i].pings, sizeof o.slot[i].pings);
                o.slot[i].pings_ok = proj.slot[i].pings_ok;
                o.slot[i].screen = proj.slot[i].screen;
            }
            plat_store_save(OBJ_PROJ, &o, sizeof o);
        } else if (!strcmp(cmd, "pingmark")) {
            ping_mark = ping_hash();
        } else if (!strcmp(cmd, "splashrun")) {
            int f;
            ui_off = 1;
            for (f = 1; f < UI_SPLASH_N; f++) {
                ui_splash(f);
                run_ms(55);
            }
            ui_off = 0;
            ui_init();
        } else if (!strcmp(cmd, "splash"))           /* splash N: the launch screen's frame N (shot it after) */
            ui_splash(atoi(a));
        else if (!strcmp(cmd, "reboot")) {
            memset(&proj, 0, sizeof proj);
            boot();
        } else if (!strcmp(cmd, "listen")) {
            static const uint8_t HELLO[] = {ANN_H0, ANN_H1, ANN_H2, ANN_HELLO, ANN_VER};
            sim_listen = 1;
            said_len = 0;
            said_log[0] = 0;
            ann_frame(HELLO, sizeof HELLO, now_ms);
            run_ms(40);
        } else if (!strcmp(cmd, "speakdump")) {        /* every screen's text for a screen reader, to stdout */
            speak_dump();
        } else if (!strcmp(cmd, "where")) {
            static const uint8_t WHERE[] = {ANN_H0, ANN_H1, ANN_H2, ANN_WHERE};
            ann_frame(WHERE, sizeof WHERE, now_ms);
            run_ms(40);
        } else if (!strcmp(cmd, "said") || !strcmp(cmd, "said_none")) {
            int none = cmd[4] != 0;
            char *t = line + strlen(cmd), *e;
            while (*t == ' ')
                t++;
            e = t + strlen(t);
            while (e > t && (e[-1] == '\n' || e[-1] == '\r' || e[-1] == ' '))
                *--e = 0;
            if ((strstr(said_log, t) != 0) == none) {
                printf("FAIL line %d: %s \"%s\": %s (since the last check: %s)\n", lineno, cmd, t,
                       none ? "said" : "not said", said_len ? said_log : "nothing\n");
                fails++;
            } else {
                printf("  ok %s \"%s\"\n", cmd, t);
            }
            said_len = 0;
            said_log[0] = 0;
        } else if (!strcmp(cmd, "echo"))
            printf("%s", line + 5);
        else {
            printf("line %d: unknown command %s\n", lineno, cmd);
            return 2;
        }
        if (st_pre5)
            proj = st_keep;
    }
    wav_close();
    stems_close();                               /* (a film not stopped: its stems ended) */
    if (midi_log)
        fclose(midi_log);
    printf("host: %u ms simulated, %u blits, render %.1f%% of real time (host CPU)%s\n", now_ms, blits,
           render_budget_ns_total > 0 ? 100.0 * render_ns_total / render_budget_ns_total : 0.0,
           fails ? ", EXPECTATIONS FAILED" : "");
    return fails ? 1 : 0;
}
#endif /* EN_WEB */
