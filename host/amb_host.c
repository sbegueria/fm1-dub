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
 *   leds                         print the lit buttons and keys
 *   expect WHAT VALUE            check state: exit 1 on mismatch; VALUE "<N" / ">N" is a bound
 *   reboot                       re-run boot from the simulated flash (persistence test)
 *   splash N                     draw the launch screen's frame N (0..UI_SPLASH_N-1) on the screen
 *   listen                       a screen reader starts listening (HELLO, again every 2 s): each SAY is printed
 *                                ("said: ..."; "said!:" cuts off what was being said)
 *   where                        the reader asks for everything on the screen
 *   said TEXT                    some SAY since the last "said" (or "listen") has TEXT in it: else a failure
 *   said_none TEXT               no SAY since the last check has TEXT in it: else a failure
 *   speakdump                    every page, knob value, help card, chord and message as the reader gets them
 * AMB_SAID_LOG=FILE: a reader listens from boot, quietly, and every SAY is appended to FILE (the audit).
 *
 * The SMK-37 build (host/build_host.sh smk37: build/host/amb_host_smk37), its controls too:
 *   kbkey K                      tap keybed key K (0..36 = C3..C6); key / tapkey K are the middle 27 as above
 *   sbtn NAME                    tap KNOB_BANK FADER_BANK LEFT RIGHT
 *   pad P [down|up]              tap (or hold, let go) pad P (0..7 the top row, 8..15 the bottom row): a white key
 *                                in the layout the UI asked for (plat_smk_pads), the picker's pad in SMK_PADS_OWN
 *   fader I V                    fader I (0..3) to V (0..127); they start at 64
 *   wheel pitch|mod V            pitch -127..127, mod 0..127
 *   pads                         print the pads' colours
 * expect (SMK-37): fbank, picker, fcard, smkleds (bits SB_*), padlayout (SMK_PADS_*), padrN / padgN / padbN (pad N's
 * red / green / blue), warp (the pitch wheel's bend sent to the engine)
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
#if BOARD_SMK37
static uint32_t pad_keys;               /* the white keys the pads hold (each the key it pressed) */
uint32_t plat_keys(void) { return held_keys | pad_keys; }
#else
uint32_t plat_keys(void) { return held_keys; }
#endif
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
#if BOARD_SMK37
/* the SMK-37's own controls (plat.h SMK section), driven by the script */
static uint32_t smk_btn_held, smk_lit, pad_hits;
static uint64_t kb_held, kb_hits;
static int fader_v[4] = {64, 64, 64, 64}, pad_layout, pad_key_of[SMK_NPADS];
static int32_t wheel_p, wheel_m;
static int wheel_new;
static uint8_t pad_rgb[SMK_NPADS][3];
void plat_smk_pads(int layout) { pad_layout = layout; }
uint32_t plat_smk_buttons(void) { return smk_btn_held; }
void plat_smk_leds(uint32_t lit) { smk_lit = lit; }
uint64_t plat_keybed(void) { return kb_held; }
uint64_t plat_keybed_hits(void)
{
    uint64_t h = kb_hits;
    kb_hits = 0;
    return h;
}
uint32_t plat_pad_hits(void)
{
    uint32_t h = pad_hits;
    pad_hits = 0;
    return h;
}
int plat_fader(int i) { return (uint32_t)i < 4u ? fader_v[i] : -1; }
int plat_wheels(int32_t *pitch, int32_t *modw)
{
    if (!wheel_new)
        return 0;
    wheel_new = 0;
    *pitch = wheel_p;
    *modw = wheel_m;
    return 1;
}
void plat_pad_rgb(int p, uint8_t r, uint8_t g, uint8_t b)
{
    if ((uint32_t)p < SMK_NPADS) {
        pad_rgb[p][0] = r;
        pad_rgb[p][1] = g;
        pad_rgb[p][2] = b;
    }
}
#endif
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
static int store_len[OBJ_NOBJ] = {-1, -1};
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
uint32_t plat_cpu_peak(void) { return cpu_pct; }
int32_t plat_battery(void) { return getenv("AMB_BATT") ? atoi(getenv("AMB_BATT")) : -1; }   /* (to see it drawn) */
int plat_usb_power(void) { return getenv("AMB_USB") != 0; }

/* -------------------------------------------------------------- app --- */
#include "../firmware/src/app/app.h"
#define ANN_NAME "DUB-1 " EN_VERSION
#include "ann_words.h"                                  /* the words file's link (tools/gen_words.py) */
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
        if (now_ms >= ui_due) {
            ui_due = now_ms + 16;
            ui_frame();
        } else {
            ui_input_only();
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
#if BOARD_SMK37
    else if (!strcmp(what, "fbank"))              /* the fader bank, 0 A .. 2 C */
        got = smk.fbank;
    else if (!strcmp(what, "picker"))             /* the picker is up */
        got = smk.pick;
    else if (!strcmp(what, "fcard"))              /* the fader card is up */
        got = smk_fcard();
    else if (!strcmp(what, "smkleds"))            /* KNOB BANK, FADER BANK, <, > lit: bits */
        got = (int)smk_lit;
    else if (!strcmp(what, "padlayout"))          /* SMK_PADS_PIANO, _GRID, _OWN */
        got = pad_layout;
    else if (!strcmp(what, "warp"))               /* the pitch wheel, as sent */
        got = smk.pitch;
    else if (!strncmp(what, "pad", 3) && (what[3] == 'r' || what[3] == 'g' || what[3] == 'b'))   /* padgN: pad N's green */
        got = pad_rgb[atoi(what + 4) & 15][what[3] == 'r' ? 0 : what[3] == 'g' ? 1 : 2];
#endif
    else if (!strcmp(what, "frozen"))
        got = en_frozen;
    else if (!strcmp(what, "view"))
        got = ui.view;
    else if (!strcmp(what, "piece"))
        got = proj.piece;
    else if (!strcmp(what, "pool"))               /* the scale, relative to the root (hex ok) */
        got = proj.pool;
    else if (!strcmp(what, "screen"))
        got = proj.screen;
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
    boot();
    if (sim_listen) {
        static const uint8_t HELLO[] = {ANN_H0, ANN_H1, ANN_H2, ANN_HELLO, ANN_VER};
        ann_frame(HELLO, sizeof HELLO, now_ms);
    }
    while (fgets(line, sizeof line, sc)) {
        char cmd[32] = {0}, a[256] = {0}, b[64] = {0}, c[64] = {0};
        lineno++;
        if (line[0] == '#' || sscanf(line, "%31s %255s %63s %63s", cmd, a, b, c) < 1)
            continue;
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
#if BOARD_SMK37
                kb_held |= 1ull << (k + 5);       /* the keybed's F3 is key 5 */
                kb_hits |= 1ull << (k + 5);
#endif
                run_ms(60);
                held_keys &= ~(1u << k);
#if BOARD_SMK37
                kb_held &= ~(1ull << (k + 5));
#endif
                run_ms(30);
            } else {
                if (!strcmp(b, "down"))
                    held_keys |= 1u << k;
                else
                    held_keys &= ~(1u << k);
#if BOARD_SMK37
                if (!strcmp(b, "down")) {
                    kb_held |= 1ull << (k + 5);
                    kb_hits |= 1ull << (k + 5);
                } else {
                    kb_held &= ~(1ull << (k + 5));
                }
#endif
                run_ms(2);
            }
#if BOARD_SMK37
        } else if (!strcmp(cmd, "kbkey")) {
            int k = atoi(a) % SMK_NKEYBED;
            kb_held |= 1ull << k;
            kb_hits |= 1ull << k;
            if (k >= 5 && k < 32)
                held_keys |= 1u << (k - 5);
            run_ms(60);
            kb_held &= ~(1ull << k);
            if (k >= 5 && k < 32)
                held_keys &= ~(1u << (k - 5));
            run_ms(30);
        } else if (!strcmp(cmd, "sbtn")) {
            static const char *const SB_N[NSB] = {"KNOB_BANK", "FADER_BANK", "LEFT", "RIGHT"};
            int i = find_name(SB_N, NSB, a);
            if (i < 0) {
                printf("line %d: no SMK button %s\n", lineno, a);
                return 2;
            }
            smk_btn_held |= 1u << i;
            run_ms(40);
            smk_btn_held &= ~(1u << i);
            run_ms(40);
        } else if (!strcmp(cmd, "pad")) {          /* as the HAL: the key a pad presses, kept until it is let go */
            int pd = atoi(a) & 15, down = !b[0] || !strcmp(b, "down"), up = !b[0] || !strcmp(b, "up");
            if (down) {
                if (pad_layout == SMK_PADS_OWN) {
                    pad_hits |= 1u << pd;
                    pad_key_of[pd] = 0;
                } else {
                    pad_key_of[pd] = 1 + WHITE_K[pad_layout == SMK_PADS_GRID ? pd : pd < 8 ? pd + 8 : pd - 8];
                    pad_keys |= 1u << (pad_key_of[pd] - 1);
                }
                run_ms(up ? 60 : 2);
            }
            if (up) {
                if (pad_key_of[pd])
                    pad_keys &= ~(1u << (pad_key_of[pd] - 1));
                pad_key_of[pd] = 0;
                run_ms(30);
            }
        } else if (!strcmp(cmd, "fader")) {
            fader_v[atoi(a) & 3] = atoi(b);
            run_ms(20);
        } else if (!strcmp(cmd, "wheel")) {
            if (a[0] == 'p')
                wheel_p = atoi(b);
            else
                wheel_m = atoi(b);
            wheel_new = 1;
            run_ms(20);
        } else if (!strcmp(cmd, "pads")) {
            int i;
            for (i = 0; i < SMK_NPADS; i++)
                printf("%s%3d,%3d,%3d%s", i % 8 ? " " : "  pads: ", pad_rgb[i][0], pad_rgb[i][1], pad_rgb[i][2],
                       i % 8 == 7 ? "\n" : "");
#endif
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
                run_ms(gap);
            }
        } else if (!strcmp(cmd, "spin")) {
            int e = find_name(ENC_N, NE, a);
            if (e < 0) {
                printf("line %d: no encoder %s\n", lineno, a);
                return 2;
            }
            enc_acc[e] += atoi(b);
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
        else if (!strcmp(cmd, "splash"))           /* splash N: the launch screen's frame N (shot it after) */
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
    }
    wav_close();
    if (midi_log)
        fclose(midi_log);
    printf("host: %u ms simulated, %u blits, render %.1f%% of real time (host CPU)%s\n", now_ms, blits,
           render_budget_ns_total > 0 ? 100.0 * render_ns_total / render_budget_ns_total : 0.0,
           fails ? ", EXPECTATIONS FAILED" : "");
    return fails ? 1 : 0;
}
#endif /* EN_WEB */
