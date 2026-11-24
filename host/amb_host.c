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
 *   lvwatch                      each bar's loudness from here, K-weighted (expect lvin, lvin4, lvu10, lvout, lvoutend:
 *                                the song's intro and outro against its main, LU x10; lvu10 the intro's bars 10 LU
 *                                under it)
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
 *   store35                      save it as 1.4.2 did (format 35: today's layout, Env decay's slot the bass's old
 *                                LFO's, unread: the playing piece a stray 37 in it, to show it read as 0; every
 *                                older store the recipes' envelope as it was, Env amount and that slot 0)
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
 *   store19, store36             (1.5.0) save it as format 19 / 36 (1.4.2 .. 1.4.x: OBJ_PROJ, 1.4's last layout)
 *                                (1.5.0: after every store the save is read by project_load and, wrapped as it was, as a
 *                                container: the same project, or a failure)
 *   refmt F                      (1.5.0) the save in OBJ_PROJ said to be format F (11, 17, 33: 12's, 18's, 34's layouts)
 *   fuzz N [SEED]                (1.5.0) today's project as a container, mutated N times: none read unmended, all valid
 *   fuzzboot N [SEED]            (1.5.0) mutated WORKs written as the newest version, a boot each: taken whole or not at all
 *   bootfail                     (1.5.0) the next reboot as after a failed boot (the version before)
 *   powercut N                   (1.5.0) the power lost at the Nth flash erase or program from now (till the reboot)
 *   bootcut N                    (first sets) the next reboot's Nth flash erase or program fails, and every one after
 *                                it till the reboot after (the flash failing in the boot, which goes on from RAM)
 *   setsdelete [SLOT]            (first sets) every stored set (or SLOT's) deleted, as DELETE's yes deletes one
 *   setsbeta                     (first sets) WORK saved as a 1.5.0 beta saved it, without D1C_F_SETS: the next boot
 *                                offers the sets the FM-1 comes with again (into the empty slots)
 *   setbytes SLOT FILE           (first sets) set SLOT's bytes, as stored, FILE's (a failure if not)
 *                                (expect setmask: the slots with a set stored, a mask; firstsets: WORK's D1C_F_SETS)
 *   name I TEXT | expectname I TEXT   (1.5.0) piece I's name as a set gives it (I -1: the set's; "-" none) | check it
 *   workcheck                    (1.5.0) after a boot: WORK's newest version is the project, and it reads back as itself
 *   dub1out FILE | dub1in FILE   (1.5.0) the project as a .dub1 file (WORK's bytes) | one taken as WORK is at a boot
 *   dub1no FILE                  (1.5.0, step 4) a .dub1 file read as WORK: refused (a failure if taken)
 *   dub1set FILE N [noglob]      (step 4) the first N pieces as a SET file (its GLOB too, unless noglob)
 *   slotrecipe I R               (step 4) slot I takes recipe R's piece, as a set may put it there
 *   setfile FILE SLOTS NAME      (step 6) the pieces SLOTS (0..7, in the set's order: "3,4,5,6,7") as a SET file named
 *                                NAME, as the FM-1 will hold it: its slots 1..n, the scenario's Song mode, Transition,
 *                                sirens' knobs and Tune (its GLOB); read back at once onto the project, the same (a
 *                                failure if not). The pieces as a scenario leaves them become a downloadable set
 *                                (tools/make_sets.sh, SETFILES.md piece 6)
 *   setin FILE                   (step 6) a SET file into the slots as the FM-1 loads one (APPLY's yes; the emulator's
 *                                Hear a set): its first piece chosen, WORK written; a failure if refused
 *   piece I                      (step 6) piece I chosen as PRESET chooses it (the one playing kept), at once: no
 *                                detents' time, so two runs choosing different numbers keep the same clock
 *   include FILE [STOP]          (step 6) FILE's lines here, up to its first STOP command (not run) or its end: a
 *                                scenario run as it is (tests/film's, read only), then more (`include 21-set.amb splash`:
 *                                its setup, not its film)
 *   setsend SLOT FILE [WHY]      (step 4) the set protocol (setwire.c), as tools/dub1_set.py speaks it: FILE sent to
 *                                set SLOT (0..7: BEGIN, DATA in 512-byte chunks, COMMIT), the question then up; WHY (hex):
 *                                a NAK with it expected instead, somewhere on the way (200: kept without a question, a
 *                                restore's)
 *   worksend 0 FILE [WHY]        (Decisions 4) a restore's pieces: FILE (a WORK) sent as BEGIN kind 1
 *   sxreq CMD BODY ANSWER [WHY]  (step 4) one request (hex: its command, its body or -, the answer's command, a BUSY's or
 *                                NAK's why): a failure if answered otherwise
 *   setread KIND INDEX FILE      (step 4) WORK (1 0) or a set (2 SLOT) read over the wire, 256 bytes a time, to FILE
 *   sethas SLOT NAME|-           (step 4) LIST: set SLOT stored with that name ("-": not stored)
 *   setstate STATE               (step 4) the last question: idle asking yes no timeout failed playing
 *   irqreset                     (step 5) the flash's IRQ-off windows counted from zero (expect flashxruns, irqlong,
 *                                irqmax: the device's measured times, while the music plays)
 *   usbpower 0|1                 (step 5) on USB power or not (AMB_USB's otherwise): the header's battery shown
 *   battery N                    (Decisions 4) the battery's reading, 521 (empty) to 601 (full), -1 none (AMB_BATT's
 *                                otherwise): under 537 (20%) it is low, its warning never hidden (expect hdrcut)
 *   (1.5.0) the Set page's list, read from the screen: expect setlit (the row lit: the piece + 1), setnext (the rows
 *                                with the next mark, a mask), setdim (the rows dimmed, a mask), setcut (the names wider
 *                                than their column, a mask: 0)
 *   namecheck                    (step 4) names.h's NAME_PX the header's room (worked out again from its fonts; and
 *                                NAME_PX_BATT, with the battery shown), its widths no narrower than the font's, every
 *                                recipe's name within them
 *   cue SLOT PART BAR STEP OP ARG VAL LEN   (step 7) a cue for piece SLOT (0..7; PART i m o b: intro, main, outro,
 *                                the bar from PLAY; OP mute solo move fx siren sirk macro knob outro next lock hold: app.h's
 *                                CUE_*), gathered; cueclear SLOT none; cuesave: into the pieces, WORK saved (they then
 *                                live in its frame, as a set's on the FM-1); cuelist SLOT: a piece's cues printed;
 *                                cueson 0|1: the Set page's Cues, at once; filmclock: the film's clock from here
 *                                (expect cuen, cuelive, cuedone, cuemoves, cuefx, cuesoff, solo, macN, eparN, slotcuesI)
 *   cuetables FILE               (step 7b) the Sets page's cue editor's tables, JSON (tools/gen_cue_tables.py)
 *   cuecheck IN OUT              (step 7b) each cue of IN ("pos op arg val len" a line) checked as the FM-1 checks one
 *                                (cue_ok): 1 or 0 a line of OUT
 *   cuerec FILE | cuerec off     (step 7) every move as the engine gets it, a line each (crec_poll: tools/cues_film.py
 *                                compiles a film script's moves into cues from it, and compares a set's with the film's)
 *   sxpipe                       (step 4) the rest from stdin, a line at a time, and a frame ("sx F0 .. F7") into the set
 *                                protocol as the USB would bring it, its answer printed ("sxout F0 .. F7"); the time runs
 *                                as on the device meanwhile (tests/host/dub1_set_test.py drives tools/dub1_set.py so)
 *   (1.5.0: the flash a simulated NOR with the firmware's storage.c on it; expect works / migrated / objseq: WORK's good
 *   versions, META's migrated seq, OBJ_PROJ's seq)
 *   pingmark                     remember the ping loops (expect pingsame 1: they are as they were)
 *   projmark                     (the Set page's Store) remember the pieces as they are now (the piece playing kept)
 *   projsame N                   pieces 1..N, the set's Mode, Transition, sirens, Tune and the name as at projmark
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
 *   cardis TEXT|-                (Decisions 7) the card in the middle shows TEXT as its head now ("-": none)
 *   msgis TEXT|-                 (Decisions 7) the header's message is TEXT now ("-": none)
 *   outis TEXT|-                 (the user's, 1.5.0) the set question's outcome screen shows TEXT, big, now ("-": none;
 *                                expect outcome: 1 while it's up)
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

/* flash: (1.5.0) a NOR flash of 1 MiB, the firmware's own storage.c on it (the A/B objects, the frame store): an
 * erase to 0xFF, a program only clears bits, a page never wraps; kept across a simulated reboot. powercut N: the power
 * lost at the Nth erase or program from now, the flash deaf till the reboot */
static uint8_t host_nor[0x100000];
static uint8_t host_nor_up, host_dead, host_failed;   /* (host_failed: bootfail, for the next reboot) */
static long host_cut = -1;
static long host_bootcut;                       /* (bootcut N: the next reboot's host_cut) */
static uint32_t store_writes;                   /* saves: an object's, a WORK version committed */
static void host_nor_init(void)
{
    if (!host_nor_up) {
        memset(host_nor, 0xFF, sizeof host_nor);
        host_nor_up = 1;
    }
}
static int host_op(void)                        /* an erase or a program may go ahead: 0 */
{
    host_nor_init();
    if (host_dead)
        return -9;
    if (host_cut > 0 && --host_cut == 0) {
        host_dead = 1;
        return -9;
    }
    return 0;
}
/* (step 5) the device's IRQ-off windows while the music plays, as the store test measured them on the user's FM-1
 * (STORAGE-1.5.0.md, "Measured"): a read of 256 bytes ~0.30 ms (16 KB in 19.3 ms; main.c's st_read reads in such
 * windows), a page 1.7 ms, a 4 KB erase 17.1 ms. Pages missed no audio block there, erases ~3 each. Counted: a window
 * longer than a page's (irqlong) and one of a whole 5.8-ms block or more (flashxruns: an xrun for certain) */
extern volatile uint8_t en_playing, en_frozen, en_ext;
static uint32_t host_irq_long, host_flash_xruns, host_irq_max;   /* (us) */
static void host_irqoff(uint32_t us)
{
    if (!en_playing)
        return;
    host_irq_max = us > host_irq_max ? us : host_irq_max;
    host_irq_long += us > 1700u;
    host_flash_xruns += us >= 5800u;
}
static int st_read(uint32_t off, void *dst, uint32_t n)
{
    uint32_t k;
    host_nor_init();
    if (off > sizeof host_nor || n > sizeof host_nor - off)
        return -1;
    memcpy(dst, host_nor + off, n);
    for (; n; n -= k) {                          /* (main.c's 256-byte windows) */
        k = n > 256u ? 256u : n;
        host_irqoff(20u + k * 19300u / 16384u);
    }
    return 0;
}
static int st_erase(uint32_t off)
{
    if ((off & 0xFFFu) || off >= sizeof host_nor || host_op())
        return -9;
    memset(host_nor + off, 0xFF, 4096);
    host_irqoff(17100u);
    return 0;
}
static int st_prog(uint32_t off, const void *src, uint32_t n)
{
    const uint8_t *b = src;
    uint32_t i;
    host_irqoff(1700u);
    if ((off & 0xFFu) + n > 256u || off >= sizeof host_nor || host_op())
        return -9;
    for (i = 0; i < n; i++)
        host_nor[off + i] &= b[i];
    return 0;
}
#include "../firmware/src/storage.c"
int plat_store_load(uint32_t obj, void *dst, uint32_t max) { return obj < OBJ_NOBJ ? st_load(obj, dst, max) : -1; }
int plat_store_save(uint32_t obj, const void *src, uint32_t len)
{
    int rc;
    if (obj >= OBJ_NOBJ || len > PLAT_STORE_MAX)
        return -1;
    if ((rc = st_save(obj, src, len)) == 0)
        store_writes++;
    return rc;
}
uint32_t plat_store_seq(uint32_t obj) { return st_seq(obj); }
void plat_fs_scan(void) { fs_scan(); }
int plat_fs_find(uint32_t kind, uint32_t index, uint32_t nth, uint32_t *len) { return fs_find(kind, index, nth, len); }
int plat_fs_has(uint32_t kind, uint32_t index) { return fs_has(kind, index); }
int plat_fs_read(int frame, uint32_t off, void *dst, uint32_t n) { return fs_read(frame, off, dst, n); }
int plat_fs_begin(uint32_t kind, uint32_t index, uint32_t len, int may_erase) { return fs_begin(kind, index, len, may_erase); }
int plat_fs_put(const void *src, uint32_t n) { return fs_put(src, n); }
int plat_fs_commit(void)
{
    const int rc = fs_commit();
    if (rc == 0)
        store_writes++;
    return rc;
}
int plat_fs_finish(void) { return fs_finish(); }
int plat_fs_seal(void)
{
    const int rc = fs_seal();
    if (rc == 0)
        store_writes++;
    return rc;
}
int plat_fs_open(void) { return fs_open_frame(); }
void plat_fs_abort(void) { fs_abort(); }
void plat_fs_keep(int f) { fs_keep(f); }
int plat_fs_prepare(void) { return fs_prepare(); }
int plat_fs_delete(uint32_t kind, uint32_t index) { return fs_delete(kind, index); }
int plat_boot_failed(void) { return host_failed; }

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
static int host_batt = -2;                      /* (Decisions 4) "battery N": the divider's reading (-1 none; -2: AMB_BATT's) */
int32_t plat_battery(void) { return host_batt > -2 ? host_batt : getenv("AMB_BATT") ? atoi(getenv("AMB_BATT")) : -1; }
static int host_usb = -1;                       /* (step 5) "usbpower 0|1" (-1: AMB_USB's) */
int plat_usb_power(void) { return host_usb >= 0 ? host_usb : getenv("AMB_USB") != 0; }

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
/* (1.5.0, step 4) the set protocol (setwire.c): its answers kept here (the last one; sxpipe prints each) */
static uint8_t sx_ans[1024];
static uint32_t sx_ans_n, sx_ans_count;
static int sx_pipe_on;
static int sxw_wire(const uint8_t *p, uint32_t n)
{
    uint32_t i;
    if (n > sizeof sx_ans)
        return -1;
    memcpy(sx_ans, p, n);
    sx_ans_n = n, sx_ans_count++;
#ifndef EN_WEB
    if (sx_pipe_on) {
        printf("sxout");
        for (i = 0; i < n; i++)
            printf(" %02X", p[i]);
        printf("\n");
        fflush(stdout);
    }
#else
    (void)i;
#endif
    return 0;
}
#include "../firmware/src/app/setwire.c"
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
/* (1.5.0, step 7) cuerec FILE: every move as the engine gets it, a line each, from here (tools/cues_film.py compiles a
 * film script's into cues from them, and checks a set's cues against the film's): the ms, the semicorcheas (16ths)
 * played (en_bars) and how far into the next one (0..999), the piece (a Set's ending one till its next downbeat), the
 * song's part and the bars its main, outro and end start on, playing, the hand-over cued; then what: MUTE part 0/1,
 * SOLO part 0/1, MOVE bit 0/1 (the keys' and the cues'), FX bit 0/1, SKEY siren 1 down / 2 held / 0 let go (the keys'),
 * SIRON siren+1 (the siren sounding, 0 none), MAC i v, SIRK i v (proj.sir: the knobs'), PAR p v (the engine's value:
 * a knob's turn or a cue's ride; the chord slots and the globals left out), LOCK v, HOLD v (dub hold), OUTRO 1 (asked
 * by hand), PIECE i, PLAY 0/1 */
static FILE *crec;
static struct {
    uint32_t mutes, solo, moves, fx, bars, bars_ms, piece_ms;
    int skey, shold, siron, lock, frozen, ask, piece, playing;
    int mac[EN_NMAC];
    int8_t sir[EN_NSIR * EN_NSK];
    int16_t par[P_NPARAMS];
} crs;
static void crec_line(const char *what, int a, int v)
{
    const uint32_t ms = now_ms - crs.bars_ms;
    const float step = 15000.0f / (en_bpm > 1.0f ? en_bpm : 120.0f);
    int fr = (int)(1000.0f * (float)ms / step);
    fprintf(crec, "%u %u %d %d %d %u %u %u %d %d %s %d %d\n", now_ms, en_bars, fr > 999 ? 999 : fr,
            en_tr_cue && ui.cue_from >= 0 ? ui.cue_from : proj.piece, (int)en_song_ph, en_song_b0, en_song_bo, en_song_be,
            (int)en_playing, (int)en_tr_cue, what, a, v);
}
static int crec_par_skip(int p) { return (p >= P_CR0 && p <= P_CX7) || p == P_TUNE || p == P_MIDI || p == P_SMODE; }
static void crec_snap(void)                      /* the state as it is: no lines */
{
    int i;
    crs.mutes = ui.mutes, crs.solo = ui.solo, crs.moves = ui.moves | cue.moves, crs.fx = ui.fx | ui.pfx | cue.fx;
    crs.skey = ui.skey, crs.shold = ui.skey_hold, crs.siron = en_sir_on, crs.lock = ui.lock, crs.frozen = en_frozen;
    crs.ask = en_song_ask, crs.piece = proj.piece, crs.playing = en_playing;
    for (i = 0; i < EN_NMAC; i++)
        crs.mac[i] = ui.mac[i];
    for (i = 0; i < EN_NSIR * EN_NSK; i++)
        crs.sir[i] = proj.sir[i / EN_NSK][i % EN_NSK];
    for (i = 0; i < P_NPARAMS; i++)
        crs.par[i] = (int16_t)en_par_now(i);
}
static void crec_bits(const char *what, uint32_t was, uint32_t now, int n)
{
    int i;
    for (i = 0; i < n; i++)
        if ((was ^ now) >> i & 1u)
            crec_line(what, i, (int)(now >> i & 1u));
}
static void crec_poll(void)                      /* (each ms of the run) */
{
    uint32_t m;
    int i;
    if (en_bars != crs.bars)
        crs.bars = en_bars, crs.bars_ms = now_ms;
    if (!crec)
        return;
    if (proj.piece != crs.piece) {
        crec_line("PIECE", proj.piece, 0);
        crs.piece = proj.piece, crs.piece_ms = now_ms;
    }
    if (en_playing != crs.playing)
        crec_line("PLAY", 0, en_playing), crs.playing = en_playing;
    crec_bits("MUTE", crs.mutes, ui.mutes, EN_NPART), crs.mutes = ui.mutes;
    crec_bits("SOLO", crs.solo, ui.solo, EN_NPART), crs.solo = ui.solo;
    m = ui.moves | cue.moves;
    crec_bits("MOVE", crs.moves, m, CUE_NMOVE), crs.moves = m;
    m = ui.fx | ui.pfx | cue.fx;
    crec_bits("FX", crs.fx, m, EN_NFX), crs.fx = m;
    if (ui.skey != crs.skey || ui.skey_hold != crs.shold) {
        if (ui.skey && !crs.skey)
            crec_line("SKEY", ui.skey - 1, 1);
        if (ui.skey && ui.skey_hold && !crs.shold)
            crec_line("SKEY", ui.skey - 1, 2);
        if (!ui.skey && crs.skey)
            crec_line("SKEY", crs.skey - 1, 0);
        crs.skey = ui.skey, crs.shold = ui.skey_hold;
    }
    if (en_sir_on != crs.siron)
        crec_line("SIRON", en_sir_on, 0), crs.siron = en_sir_on;
    for (i = 0; i < EN_NMAC; i++)
        if (ui.mac[i] != crs.mac[i])
            crec_line("MAC", i, ui.mac[i]), crs.mac[i] = ui.mac[i];
    for (i = 0; i < EN_NSIR * EN_NSK; i++)
        if (proj.sir[i / EN_NSK][i % EN_NSK] != crs.sir[i])
            crec_line("SIRK", i, proj.sir[i / EN_NSK][i % EN_NSK]), crs.sir[i] = proj.sir[i / EN_NSK][i % EN_NSK];
    if (ui.lock != crs.lock)
        crec_line("LOCK", 0, ui.lock), crs.lock = ui.lock;
    if (en_frozen != crs.frozen)
        crec_line("HOLD", 0, en_frozen), crs.frozen = en_frozen;
    if (en_song_ask != crs.ask) {
        if (en_song_ask)
            crec_line("OUTRO", 0, 1);
        crs.ask = en_song_ask;
    }
    if (now_ms - crs.piece_ms < 200u || en_queue_left()) {   /* (a piece coming in: its values, not a move) */
        for (i = 0; i < P_NPARAMS; i++)
            crs.par[i] = (int16_t)en_par_now(i);
    } else {
        for (i = 0; i < P_NPARAMS; i++)
            if (!crec_par_skip(i) && en_par_now(i) != crs.par[i])
                crec_line("PAR", i, en_par_now(i)), crs.par[i] = (int16_t)en_par_now(i);
    }
}
/* (step 7) a scenario's own cues: `cue` lines gathered a piece at a time, `cuesave` puts them into the project as a set
 * would (and saves WORK: they then live in its frame, as on the FM-1) */
static d1c_cue_t hc_q[NPIECES][CUE_MAX];
static uint8_t hc_n[NPIECES], hc_on[NPIECES];
static const char *const CUE_OP_NAME[CUE_NOPS] = {"mute", "solo", "move", "fx", "siren", "sirk", "macro", "knob",
                                                   "outro", "next", "lock", "hold"};
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
static uint32_t trw_n, trw_bars_was, trw_t_restart, trw_t_cue, trw_onset_t, trw_was[5], trw_nev, trw_click_ms;
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
/* (1.4.3: the songs' intros and outros, build/design/REPORT-introoutro.txt) lvwatch: each bar's loudness from here,
 * K-weighted (trk's two filters), its mean power, by the song's bar (en_bars / 16; a Set's next piece from its own bar
 * 0, the bars before it dropped). Against the main's median bar (its bars so far): lvin, the intro's level (its mean
 * power, LU x10); lvin4, its first 4 bars'; lvu10, its bars more than 10 LU under; lvout, the outro's; lvoutend, its
 * last 8 bars' (lv_get) */
#define LV_N 512
static double lv_z[2][2][2], lv_p[LV_N];
static uint32_t lv_c[LV_N], lv_bars_was, lv_b0, lv_bo, lv_be;
static int lv_on;
static void lv_sample(float l, float r)
{
    const float x[2] = {l, r};
    const uint32_t b = en_bars / 16u;
    int c, st;
    double e = 0.0;
    if (en_bars < lv_bars_was) {                 /* (a Set's next piece: its own bars) */
        memset(lv_p, 0, sizeof lv_p);
        memset(lv_c, 0, sizeof lv_c);
    }
    lv_bars_was = en_bars;
    lv_b0 = en_song_b0;
    if (en_song_be)
        lv_bo = en_song_bo, lv_be = en_song_be;
    for (c = 0; c < 2; c++) {
        double v = x[c];
        for (st = 0; st < 2; st++) {
            const double y = trk_b[st][0] * v + lv_z[c][st][0];
            lv_z[c][st][0] = trk_b[st][1] * v - trk_a[st][1] * y + lv_z[c][st][1];
            lv_z[c][st][1] = trk_b[st][2] * v - trk_a[st][2] * y;
            v = y;
        }
        e += v * v;
    }
    if (b < LV_N)
        lv_p[b] += e, lv_c[b]++;
}
static int lv_cmp(const void *a, const void *b)
{
    const double x = *(const double *)a, y = *(const double *)b;
    return x < y ? -1 : x > y;
}
static int lv_get(const char *what)              /* (-999: nothing to measure yet) */
{
    static double m[LV_N];
    const uint32_t me = lv_be ? lv_bo : LV_N;
    uint32_t i, n = 0, a, z, k = 0;
    double ref, sum = 0.0;
    for (i = lv_b0; i < me && i < LV_N; i++)     /* (the main's bars, whole) */
        if (lv_c[i] > 44100u && i + 1u < LV_N && lv_c[i + 1u])
            m[n++] = lv_p[i] / (double)lv_c[i];
    if (!n)
        return -999;
    qsort(m, n, sizeof m[0], lv_cmp);
    ref = m[n / 2];
    if (!strcmp(what, "lvu10")) {
        for (i = 0; i < lv_b0; i++)
            k += lv_c[i] && lv_p[i] / (double)lv_c[i] < ref * 0.1;
        return (int)k;
    }
    a = !strcmp(what, "lvin") || !strcmp(what, "lvin4") ? 0u : !strcmp(what, "lvout") ? lv_bo : lv_be - 8u;
    z = !strcmp(what, "lvin") ? lv_b0 : !strcmp(what, "lvin4") ? 4u : lv_be;
    for (i = a, n = 0; i < z && i < LV_N; i++)
        if (lv_c[i])
            sum += lv_p[i], k += lv_c[i];
    return k ? (int)floor(100.0 * log10(sum / (double)k / ref) + 0.5) : -999;
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
            trw_click = (int)(100.0 * d2 / (rms + 1e-4)), trw_click_ms = now_ms;
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
        if (en_nev_w != trw_nev && heard[4])     /* (1.4.3: the noise's dry clicks too, the vinyl's: struck, not a
                                                  * hand-over's step; under a hand-over's quiet start they read high) */
            trw_onset_t = now_ms;
        trw_nev = en_nev_w;
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
static project_t proj_mark;                      /* (projmark, projsame: the Set page's Store) */
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
                if (lv_on)
                    lv_sample(l, r);
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
        crec_poll();                             /* (step 7: cuerec's) */
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

/* (step 7b) "cuetables FILE": what the Sets page's cue editor needs to know of this firmware, as JSON (web/dub1cuetables.js,
 * tools/gen_cue_tables.py): the format, each parameter's name, range and what the screen shows for its values, where it
 * is (its page and knob label: the manual's), whether a cue may ride it (cue_ok: not Tune, MIDI out or Song mode); the
 * sirens' knobs, the master actions', the grid's effects, PERFORM's cards (what a cue shows), the recipes' songs */
static void js_str(FILE *f, const char *s)
{
    fputc('"', f);
    for (; *s; s++) {
        if (*s == '"' || *s == '\\')
            fputc('\\', f);
        fputc(*s, f);
    }
    fputc('"', f);
}
static void js_texts(FILE *f, int lo, int hi, void (*txt)(int, int, int, char *), int a, int b)   /* the screen's text
                                                  * for each value lo..hi, or null when it's just the number */
{
    char t[40], n[16];
    int v, plain = 1;
    for (v = lo; v <= hi && plain; v++) {
        txt(a, b, v, t);
        snprintf(n, sizeof n, "%d", v);
        plain = !strcmp(t, n);
    }
    if (plain || hi - lo > 400) {
        fprintf(f, "null");
        return;
    }
    fputc('[', f);
    for (v = lo; v <= hi; v++) {
        txt(a, b, v, t);
        if (v > lo)
            fputc(',', f);
        js_str(f, t);
    }
    fputc(']', f);
}
static void tx_par(int i, int unused, int v, char *t) { (void)unused; memset(t, 0, 12); en_param_text(i, v, t); }
static void tx_sir(int s, int k, int v, char *t) { memset(t, 0, 12); en_sir_text(s, k, v, t); }
static void tx_mac(int a, int unused, int v, char *t)
{
    const int8_t was = ui.mac[a];
    (void)unused;
    ui.mac[a] = (int8_t)v;
    memset(t, 0, 12);
    knob_text(K_FILTER + a, t);
    ui.mac[a] = was;
}
static void cue_tables(FILE *f)
{
    int i, v, k, s, first;
    slot_t sl;
    fprintf(f, "{\"format\":%d,\"npar\":%d,\"maxcue\":%d,\"nmove\":%d,\"nfx\":%d,\"nsir\":%d,\"nsk\":%d,\"nmac\":%d,"
               "\"npart\":%d,\"ops\":[", PROJ_FORMAT, P_NPARAMS, CUE_MAX, CUE_NMOVE, EN_NFX, EN_NSIR, EN_NSK, EN_NMAC,
            EN_NPART);
    for (i = 0; i < CUE_NOPS; i++) {
        fprintf(f, i ? "," : "");
        js_str(f, CUE_OP_NAME[i]);
    }
    fprintf(f, "],\n\"params\":[\n");
    for (i = 0; i < P_NPARAMS; i++) {
        const en_param_t *p = en_param_info(i);
        fprintf(f, "%s{\"i\":%d,\"name\":", i ? ",\n" : "", i);
        js_str(f, p->name);
        fprintf(f, ",\"lo\":%d,\"hi\":%d,\"def\":%d,\"cue\":%d,\"at\":[", p->lo, p->hi, p->def, !global_par(i));
        first = 1;
        for (v = 0; v < NVIEWS; v++) {           /* (its pages: each knob it is on; the Mix page gone, a strip's below) */
            if (v == V_MIX || v == V_STRIP)
                continue;
            for (k = 0; k < 4; k++)
                if (VIEW_KNOB[v][k] == i) {
                    fprintf(f, first ? "[" : ",[");
                    js_str(f, view_name(v));
                    fputc(',', f);
                    js_str(f, KNOB_LABEL[v][k]);
                    fputc(',', f);
                    js_str(f, KNOB_HELP[v][k] ? KNOB_HELP[v][k] : "");
                    fputc(']', f);
                    first = 0;
                }
        }
        for (s = 0; s < NSTRIP; s++)
            for (k = 0; k < 4; k++)
                if (STRIP_PAR[s][k] == i) {
                    static const char *const SN[NSTRIP] = {"Chord strip", "Pings strip", "Bass strip", "Kick strip",
                                                           "Hats strip", "Noise strip"};
                    fprintf(f, first ? "[" : ",[");
                    js_str(f, SN[s]);
                    fputc(',', f);
                    js_str(f, KNOB_LABEL[V_STRIP][k]);
                    fputc(',', f);
                    js_str(f, KNOB_HELP[V_STRIP][k] ? KNOB_HELP[V_STRIP][k] : "");
                    fputc(']', f);
                    first = 0;
                }
        fprintf(f, "],\"text\":");
        js_texts(f, p->lo, p->hi, tx_par, i, 0);
        fputc('}', f);
    }
    fprintf(f, "],\n\"sirens\":[");
    for (s = 0; s < EN_NSIR; s++) {
        fprintf(f, "%s{\"name\":", s ? ",\n" : "");
        js_str(f, EN_SIR_NAME[s]);
        fprintf(f, ",\"card\":");
        js_str(f, PERF_SNAME[s]);
        fprintf(f, ",\"tap\":");
        js_str(f, PERF_STAP[s]);
        fprintf(f, ",\"held\":");
        js_str(f, PERF_SHOLD[s]);
        fprintf(f, ",\"knobs\":[");
        for (k = 0; k < EN_NSK; k++) {
            int lo, hi;
            en_sir_range(s, k, &lo, &hi);
            fprintf(f, "%s{\"name\":", k ? "," : "");
            js_str(f, KNOB_LABEL[V_SIREN][k]);
            fprintf(f, ",\"lo\":%d,\"hi\":%d,\"def\":%d,\"text\":", lo, hi, en_sir_def(s, k));
            js_texts(f, lo, hi, tx_sir, s, k);
            fputc('}', f);
        }
        fprintf(f, "]}");
    }
    fprintf(f, "],\n\"macros\":[");
    for (i = 0; i < EN_NMAC; i++) {
        fprintf(f, "%s{\"name\":", i ? ",\n" : "");
        js_str(f, knob_name(K_FILTER + i));
        fprintf(f, ",\"lo\":-50,\"hi\":50,\"text\":");
        js_texts(f, -50, 50, tx_mac, i, 0);
        fputc('}', f);
    }
    fprintf(f, "],\n\"fx\":[");
    for (i = 0; i < EN_NFX; i++) {
        fprintf(f, i ? "," : "");
        js_str(f, EN_FX_NAME[i]);
    }
    fprintf(f, "],\"perffx\":[");                /* (PERFORM's four: their cards) */
    for (i = 0; i < PERF_NF; i++)
        fprintf(f, "%s%d", i ? "," : "", PERF_FX[i]);
    fprintf(f, "],\n\"moves\":[");               /* (each EN_PF_* bit: its card, its line) */
    for (i = 0; i < CUE_NMOVE; i++) {
        const char *n = "", *w = "";
        for (k = 0; k < PERF_NB; k++)
            if (PERF_BTHR[k] == 1u << i)
                n = PERF_THRNAME[k], w = PERF_THRWHAT[k];
        for (k = 0; k < PERF_NM; k++)
            if (PERF_MBIT[k] == 1u << i)
                n = PERF_NAME[PERF_NP + k], w = PERF_WHAT[k];
        fprintf(f, "%s[", i ? "," : "");
        js_str(f, n);
        fputc(',', f);
        js_str(f, w);
        fputc(']', f);
    }
    fprintf(f, "],\n\"parts\":[");
    for (i = 0; i < EN_NPART; i++) {
        fprintf(f, i ? "," : "");
        js_str(f, PERF_NAME[i]);
    }
    fprintf(f, "],\n\"recipes\":[");             /* (today's: a RECIPE or DELTA piece's song lengths start from these) */
    for (i = 0; i < NPIECES; i++) {
        recipe(i, &sl);
        fprintf(f, "%s{\"name\":", i ? ",\n" : "");
        js_str(f, en_piece_name(i));
        fprintf(f, ",\"song\":[%d,%d,%d]}", sl.par[P_SINTRO], sl.par[P_SMAIN], sl.par[P_SOUTRO]);
    }
    fprintf(f, "],\n\"smodes\":[");
    for (i = 0; i <= en_param_info(P_SMODE)->hi; i++) {
        char t[16] = {0};
        en_param_text(P_SMODE, i, t);
        fprintf(f, i ? "," : "");
        js_str(f, t);
    }
    fprintf(f, "],\"song\":[%d,%d,%d,%d,%d]}\n", P_SMODE, P_SINTRO, P_SMAIN, P_SOUTRO, P_TEMPO);
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
    else if (!strcmp(what, "trclickat"))         /* (1.4.3) where it was: ms after the cue (trclickdb: the downbeat's) */
        got = (int)(trw_click_ms - trw_t_cue);
    else if (!strcmp(what, "trclickdb"))
        got = (int)(trw_t_restart - trw_t_cue);
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
    else if (!strncmp(what, "lv", 2) && (!strcmp(what + 2, "in") || !strcmp(what + 2, "in4") ||
                                         !strcmp(what + 2, "u10") || !strcmp(what + 2, "out") ||
                                         !strcmp(what + 2, "outend")))
        got = lv_get(what);                       /* (1.4.3: lvwatch's, LU x10 against the main; lvu10 bars) */
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
    else if (!strcmp(what, "loadset"))            /* (the Set page's Load) the set chosen last (1..8; 0 none) */
        got = ui.ld;
    else if (!strcmp(what, "storeset"))           /* (the Set page's Store) the set it would store into (1..8): the one
                                                  * being chosen, else the one it offers */
        got = knob_get(K_STORE) + 1;
    else if (!strcmp(what, "dirty"))
        got = ui.dirty;
    else if (!strcmp(what, "flashxruns"))        /* (step 5) the flash's IRQ-off windows while playing: a block missed */
        got = (int)host_flash_xruns;
    else if (!strcmp(what, "irqlong"))           /* (step 5) ... longer than a page's (1.7 ms) */
        got = (int)host_irq_long;
    else if (!strcmp(what, "irqmax"))            /* (step 5) ... the longest, in 0.1 ms */
        got = (int)(host_irq_max / 100u);
    else if (!strcmp(what, "hdrbatt"))           /* (step 5) the header's battery drawn: its nub, (235, 13), not the
                                                  * background (as the corner, (239, 1)) */
        got = fb[13 * 240 + 235] != fb[1 * 240 + 239];
    else if (!strcmp(what, "hdrcut"))            /* (Decisions 4) the header's name cut ("…"): beside a low battery */
        got = hdr_cut;
    else if (!strcmp(what, "setlit") || !strcmp(what, "setnext") || !strcmp(what, "setdim") ||
             !strcmp(what, "setcut")) {          /* (1.5.0) the Set page's list, from the screen: the row lit (its
                                                  * panel; the piece + 1, 0 none), the rows with the next mark (a mask,
                                                  * piece i bit i), the rows dimmed (their names in K_LINE: a mask), the
                                                  * names drawn past their column (a mask: 0, all fit) */
        int i;
        got = 0;
        for (i = 0; i < NPIECES; i++) {
            const int32_t x = (i / 4) * 120, y = MAIN_Y - MAIN_SHIFT + SL_Y0 + (i % 4) * SL_ROW;
            if (what[3] == 'l') {
                if (!got && fb[(y + 2) * 240 + x + SL_PAD + 8] == swap16(K_PANEL))
                    got = i + 1;
            } else if (what[3] == 'n') {
                if (fb[(y + SL_H / 2) * 240 + x + SL_MARK] == swap16(K_ACC))
                    got |= 1 << i;
            } else if (what[3] == 'd') {
                int32_t px, py, n = 0;
                for (py = y; py < y + SL_H; py++)
                    for (px = x + SL_NAME; px < x + 120; px++)
                        n += fb[py * 240 + px] == swap16(K_LINE);
                if (n)
                    got |= 1 << i;
            } else if (text_w(&FONT_B, piece_name(i)) > 120 - SL_NAME - SL_PAD) {
                got |= 1 << i;
            }
        }
    }
    else if (!strcmp(what, "store_writes"))
        got = (int)store_writes;
    else if (!strcmp(what, "migrated"))          /* (1.5.0) WORK's META: OBJ_PROJ's seq when 1.4's save was taken (0 none) */
        got = (int)work_mig;
    else if (!strcmp(what, "works")) {           /* (1.5.0) WORK's good versions in the frame store */
        uint32_t n;
        got = 0;
        while (plat_fs_find(FS_WORK, 0, (uint32_t)got, &n) >= 0)
            got++;
    } else if (!strcmp(what, "setmask")) {       /* (first sets) the slots with a set stored (by its header), a mask */
        uint32_t i;
        for (got = 0, i = 0; i < PLAT_FS_SETS; i++)
            got |= plat_fs_has(FS_SET, i) << i;
    } else if (!strcmp(what, "firstsets"))       /* (first sets) WORK's D1C_F_SETS: the sets the FM-1 comes with offered */
        got = work_sets;
    else if (!strcmp(what, "objseq"))            /* (1.5.0) OBJ_PROJ's seq (1.4's save; 0 none) */
        got = (int)plat_store_seq(OBJ_PROJ);
    else if (!strcmp(what, "setlen"))            /* (step 4) the set's pieces (0: all eight) */
        got = proj.setlen;
    else if (!strcmp(what, "cuen"))              /* (step 7) the cues in the player's list (the playing piece's) */
        got = cue.n;
    else if (!strcmp(what, "cuelive"))           /* (step 7) the player armed (playing, cues on) */
        got = cue.live;
    else if (!strcmp(what, "cuedone"))           /* (step 7) its cues fired or dropped so far, this run */
        got = __builtin_popcountll(cue.done);
    else if (!strcmp(what, "cuemoves"))          /* (step 7) the moves the cues hold (EN_PF_*) */
        got = (int)cue.moves;
    else if (!strcmp(what, "cuefx"))             /* (step 7) the effects the cues hold (EN_FX_*) */
        got = (int)cue.fx;
    else if (!strcmp(what, "cuesoff"))           /* (step 7) the player's Cues off */
        got = proj.cues_off;
    else if (!strcmp(what, "solo"))              /* (step 7) the layers soloed (EN_M_*) */
        got = (int)ui.solo;
    else if (!strncmp(what, "slotcues", 8))      /* (step 7) slotcuesI: piece I's cues, where they are (read and checked) */
        got = (int)cues_read(&proj.cues[atoi(what + 8) & 7], 0);
    else if (!strncmp(what, "mac", 3))           /* (step 7) macN: master action N (0 the DJ filter), as the knob has it */
        got = ui.mac[atoi(what + 3) % EN_NMAC];
    else if (!strncmp(what, "epar", 4))          /* (step 7) eparN: parameter N as the engine has it (a cue's ride) */
        got = en_par_now(atoi(what + 4));
    else if (!strncmp(what, "recipe", 6))        /* (step 4) recipeI: the recipe slot I came from */
        got = proj.slot[atoi(what + 6) & 7].recipe;
    else if (!strcmp(what, "asking"))            /* (step 4) the set protocol's question up: its op (0 none) */
        got = sxw_asking();
    else if (!strcmp(what, "again"))             /* (beta2) the question up is LOAD IT NOW?: 1 */
        got = sxw_asking() && sxw_again;
    else if (!strcmp(what, "live"))              /* (beta2) the set loaded as setwire.c knows it (1..8; 0 not known) */
        got = sxw_live + 1;
    else if (!strcmp(what, "outcome"))           /* (the user's) the question's outcome on the screen: 1 */
        got = out_on(plat_ms());
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
static void env_before36(void)                   /* (format 36, REPORT-stabenv) an older format's store: Env amount
                                                  * and the bass's old LFO's slot, Env decay's now, as the recipes had
                                                  * them then (0) where a piece has today's */
{
    int16_t rec[P_NPARAMS];
    uint16_t pool;
    int i;
    for (i = 0; i < NPIECES; i++) {
        int16_t *p = proj.slot[i].par;
        en_piece(i, rec, &pool);
        if (p[P_CENV] == rec[P_CENV])
            p[P_CENV] = 0;
        if (p[P_CEDEC] == rec[P_CEDEC])
            p[P_CEDEC] = 0;
    }
    proj.par[P_CENV] = proj.slot[proj.piece].par[P_CENV];
    proj.par[P_CEDEC] = proj.slot[proj.piece].par[P_CEDEC];
}

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
/* (1.5.0) the project in 1.4's last layout (formats 35 and 36, project36_t: OBJ_PROJ's), for the stores of 35 and 36 */
static void as36(project36_t *o, uint32_t format)
{
    int i;
    memset(o, 0, sizeof *o);
    o->magic = proj.magic, o->format = format;
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
    o->rsv[0] = PROJ_WAVE_MARK;
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

/* (1.5.0, step 4) the set protocol from the computer's side: a request framed as tools/dub1_set.py frames it, into
 * setwire.c as usb.c hands it a frame (the bytes between F0 and F7), its answer checked (one frame, the same seq, its
 * checksum, no bigger than usb.c's send ring) and unpacked into sxh_body. 1: answered */
static uint8_t sxh_body[1024];
static uint32_t sxh_n, sxh_cmd, sxh_seq;
static int sxh_ask(uint32_t cmd, const uint8_t *body, uint32_t n)
{
    static uint8_t f[1200];
    uint32_t w = 0, s = 0, i;
    const uint32_t c0 = sx_ans_count;
    sxh_seq = (sxh_seq + 1u) & 0x7Fu;
    f[w++] = 0x7D, f[w++] = 0x44, f[w++] = 0x31, f[w++] = (uint8_t)cmd, f[w++] = (uint8_t)sxh_seq;
    w += sxw_pack7(body, n, f + w);
    for (i = 3; i < w; i++)
        s += f[i];
    f[w++] = (uint8_t)(s & 0x7Fu);
    if (w > 640u) {
        printf("  FAIL a request of %u bytes: over usb.c's frame (640)\n", w);
        return 0;
    }
    if (!sxw_frame(f, w, now_ms) || sx_ans_count != c0 + 1u)
        return 0;
    for (s = 0, i = 4; i + 2u < sx_ans_n; i++)
        s += sx_ans[i];
    if (sx_ans_n < 8u || sx_ans_n > 192u * 3u || sx_ans[0] != 0xF0 || sx_ans[1] != 0x7D || sx_ans[2] != 0x44 ||
        sx_ans[3] != 0x31 || sx_ans[5] != sxh_seq || sx_ans[sx_ans_n - 1u] != 0xF7 || (s & 0x7Fu) != sx_ans[sx_ans_n - 2u]) {
        printf("  FAIL a bad answer (%u bytes)\n", sx_ans_n);
        return 0;
    }
    sxh_cmd = sx_ans[4];
    sxh_n = sxw_unpack7(sx_ans + 6, sx_ans_n - 8u, sxh_body, sizeof sxh_body);
    return 1;
}
static uint32_t sxh_hex(const char *s, uint8_t *b, uint32_t max)   /* "0a02ff" -> bytes ("-": none) */
{
    uint32_t n = 0;
    unsigned v;
    if (!strcmp(s, "-"))
        return 0;
    while (n < max && s[0] && s[1] && sscanf(s, "%2x", &v) == 1)
        b[n++] = (uint8_t)v, s += 2;
    return n;
}
/* setsend: FILE to set slot (worksend: kind 1, a restore's pieces); 0 sent and asked (the COMMIT's answer: the question
 * up), 0x200 kept without asking (a restore's: 0x205 failed), else the NAK's why, 0x100 + a
 * BUSY's (or -1) */
static int sxh_send(uint32_t kind, uint32_t slot, const uint8_t *b, uint32_t n)
{
    uint8_t q[600];
    const uint32_t crc = ~d1c_crc_up(0xFFFFFFFFu, b, n);
    uint32_t off;
    q[0] = (uint8_t)kind, q[1] = (uint8_t)slot, q[2] = (uint8_t)n, q[3] = (uint8_t)(n >> 8);
    q[4] = (uint8_t)crc, q[5] = (uint8_t)(crc >> 8), q[6] = (uint8_t)(crc >> 16), q[7] = (uint8_t)(crc >> 24);
    if (!sxh_ask(SXW_BEGIN, q, 8))
        return -1;
    if (sxh_cmd != SXW_BEGIN)
        return sxh_cmd == SXW_NAK ? sxh_body[1] : sxh_cmd == SXW_BUSY ? 0x100 + sxh_body[1] : -1;
    for (off = 0; off < n; off += 512u) {
        const uint32_t k = n - off > 512u ? 512u : n - off;
        q[0] = (uint8_t)off, q[1] = (uint8_t)(off >> 8);
        memcpy(q + 2, b + off, k);
        if (!sxh_ask(SXW_DATA, q, 2u + k))
            return -1;
        if (sxh_cmd != SXW_DATA)
            return sxh_cmd == SXW_NAK ? sxh_body[1] : -1;
        run_ms(3);                               /* (the round trip: the device runs on meanwhile) */
    }
    if (!sxh_ask(SXW_COMMIT, 0, 0))
        return -1;
    if (sxh_cmd != SXW_COMMIT)
        return sxh_cmd == SXW_NAK ? sxh_body[1] : -1;
    if (sxh_body[1] == SXW_S_YES)
        return 0x200;                            /* (a restore's: kept, not asked) */
    if (sxh_body[1] == SXW_S_FAILED)
        return 0x205;
    return sxh_body[1] == SXW_S_ASKING ? 0 : -1;
}
static uint32_t file_in(const char *path, uint8_t *b, uint32_t max)   /* a file's bytes: its length (0: none) */
{
    FILE *f = fopen(path, "rb");
    uint32_t n;
    if (!f)
        return 0;
    n = (uint32_t)fread(b, 1, max, f);
    fclose(f);
    return n;
}
#ifndef EN_WEB
#include <sys/select.h>
#include <unistd.h>
/* sxpipe: the script's rest from stdin, with SysEx frames in and out, the device's time running as the real one */
static int sx_pipe_line(char *line, size_t cap)  /* a line from stdin (1), the device run while waiting; 0 at the end */
{
    static double t_last;
    size_t n = 0;
    if (t_last == 0.0)
        t_last = now_ns();
    for (;;) {
        fd_set rd;
        struct timeval tv = {0, 2000};
        const double t = now_ns();
        const uint32_t ms = (uint32_t)((t - t_last) / 1e6);
        if (ms) {
            run_ms(ms > 50u ? 50u : ms);
            t_last += (double)ms * 1e6;
        }
        FD_ZERO(&rd);
        FD_SET(0, &rd);
        if (select(1, &rd, 0, 0, &tv) > 0) {
            char c;
            const ssize_t r = read(0, &c, 1);
            if (r <= 0)
                return 0;
            if (c == '\n') {
                line[n] = 0;
                return 1;
            }
            if (n + 1u < cap)
                line[n++] = c;
        }
    }
}
#endif

int main(int argc, char **argv)
{
    FILE *sc;
    char line[4096], out[400];                   /* (sxpipe's frames: up to ~1,900 characters) */
    const char *dir = argc > 2 ? argv[2] : ".";
    int lineno = 0, fails = 0;
    static FILE *inc_f[4];                       /* (step 6) include: the files below, their lines, their STOPs */
    static int inc_n, inc_line[4];
    static char inc_stop[4][32];
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
    for (;;) {
        char cmd[32] = {0}, a[256] = {0}, b[64] = {0}, c[64] = {0};
        if (!sx_pipe_on) {
            if (!fgets(line, sizeof line, sc)) {
                if (inc_n) {                     /* (an included file's end: back to the one that included it) */
                    fclose(sc);
                    sc = inc_f[--inc_n], lineno = inc_line[inc_n];
                    continue;
                }
                break;
            }
        } else {
            if (!sx_pipe_line(line, sizeof line))
                break;
            printf("> %s\n", line);             /* (each line acknowledged: the driver waits for it) */
            fflush(stdout);
            if (!strncmp(line, "sx ", 3)) {      /* sx F0 .. F7: a frame in, as usb.c hands it over */
                static uint8_t fr[1024];
                uint32_t k = 0;
                unsigned v;
                const char *s = line + 3;
                int used;
                while (k < sizeof fr && sscanf(s, "%2x%n", &v, &used) == 1)
                    fr[k++] = (uint8_t)v, s += used;
                if (k >= 2u && fr[0] == 0xF0 && fr[k - 1u] == 0xF7 && k - 2u <= 640u)
                    sxw_frame(fr + 1, k - 2u, now_ms);
                printf("sxdone\n");
                fflush(stdout);
                continue;
            }
        }
        static project_t st_keep;
        int st_old, st_pre5, st_pre36;
        lineno++;
        if (line[0] == '#' || sscanf(line, "%31s %255s %63s %63s", cmd, a, b, c) < 1)
            continue;
        if (inc_n && inc_stop[inc_n - 1][0] && !strcmp(cmd, inc_stop[inc_n - 1])) {   /* (step 6) include's STOP: */
            fclose(sc);                          /* that line not run, the rest of the file left */
            sc = inc_f[--inc_n], lineno = inc_line[inc_n];
            continue;
        }
        st_old = !strncmp(cmd, "store", 5) && strcmp(cmd, "storefile") && strcmp(cmd, "storescreen") &&
                 strcmp(cmd, "store32") && strcmp(cmd, "store34") && strcmp(cmd, "store35") &&
                 strcmp(cmd, "store36");   /* (32, 34, 35:
                                                  * today's meanings, but Vary's and the sweep's (32, 34) or the
                                                  * envelope (35); no Transition in 32) */
        st_pre5 = st_old || !strcmp(cmd, "store32") || !strcmp(cmd, "store34");
        st_pre36 = st_pre5 || !strcmp(cmd, "store35");
        if (st_pre36) {                          /* (format 36) every older format's store: the envelope as before */
            keep();
            st_keep = proj;
            env_before36();
        }
        if (st_pre5)                             /* (step 5) every older format's store: the recipes before it */
            models_before5();
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
            if (crec)                            /* (step 7: where the WAV starts, for cues_film.py compare) */
                fprintf(crec, "# wav %s %u\n", a, now_ms);
        } else if (!strcmp(cmd, "wavstop"))
            wav_close();
        else if (!strcmp(cmd, "trwatch")) {      /* (1.4.0) a Set's hand-over watched from here (trquiet, trclick ..) */
            trw_on = 1, trw_quiet_ms = trw_run_ms = trw_restarts = trw_click = 0, trw_ch = trw_kick = trw_bass = -1;
            trw_dip = trw_jump = 0;
            trk_init();
            trw_acc = trw_sq = 0.0, trw_n = 0, trw_t_restart = trw_t_cue = 0, trw_bars_was = en_bars, trw_w = 0;
            memset(trw_ring, 0, sizeof trw_ring);
        } else if (!strcmp(cmd, "lvwatch")) {     /* (1.4.3) the song's bars' loudness from here (lvin, lvout ..) */
            if (!trw_on)
                trk_init();                      /* (the K-weighting's coefficients) */
            memset(lv_z, 0, sizeof lv_z), memset(lv_p, 0, sizeof lv_p), memset(lv_c, 0, sizeof lv_c);
            lv_bars_was = en_bars, lv_b0 = lv_bo = lv_be = 0, lv_on = 1;
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
        } else if (!strcmp(cmd, "filmclock")) {  /* (step 7) the film's clock from here (`at`, `bar`'s times), no film
                                                  * written (tools/cues_film.py's run of a film script) */
            film_t0 = now_ms;
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
            static project36_t o;
            keep();
            proj.screen = (uint8_t)atoi(a);
            proj.slot[proj.piece].screen = proj.screen;
            as36(&o, 36u);                       /* (1.5.0: as 1.4.x saved it, format 36) */
            plat_store_save(OBJ_PROJ, &o, sizeof o);
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
        } else if (!strcmp(cmd, "store35")) {    /* (format 36) the project as 1.4.2 saved it (35: today's layout,
                                                  * Env decay's slot unread) */
            static project36_t o;
            as36(&o, 35u);
            o.par[P_CEDEC] = o.slot[o.piece].par[P_CEDEC] = 37;   /* (the slot unread then: what it held, stray) */
            plat_store_save(OBJ_PROJ, &o, sizeof o);
        } else if (!strcmp(cmd, "store36")) {    /* (1.5.0, format 37) the project as 1.4.2 .. 1.4.x saved it (36: OBJ_PROJ,
                                                  * 1.4's last layout, today's values) */
            static project36_t o;
            keep();
            as36(&o, 36u);
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
        } else if (!strcmp(cmd, "store19")) {    /* (1.5.0) the project as 1.4.0's test builds saved it (format 19, the
                                                  * LFOs' rates, no waves; for the container's checks: the
                                                  * older formats' meanings folded back as the other stores, the chord's
                                                  * Cutoff not) */
            static project19_t o;
            int i;
            keep();
            memset(&o, 0, sizeof o);
            o.magic = PROJ_MAGIC, o.format = 19u;
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
        } else if (!strcmp(cmd, "refmt")) {     /* (1.5.0) refmt F: the save in OBJ_PROJ said to be format F (11 as 12,
                                                  * 17 as 18, 33 as 34 laid out: the formats no store writes), checked
                                                  * through the container as a store is */
            static uint8_t fb[PLAT_STORE_MAX];
            const int got = plat_store_load(OBJ_PROJ, fb, sizeof fb);
            char why[96];
            uint32_t f = (uint32_t)atoi(a);
            if (got < 8) {
                printf("line %d: refmt: nothing saved\n", lineno);
                return 2;
            }
            memcpy(fb + 4, &f, 4);
            plat_store_save(OBJ_PROJ, fb, (uint32_t)got);
            if (project_wrapcheck(&f, why, sizeof why)) {
                printf("FAIL line %d: refmt (format %u): project_load and the container differ: %s\n", lineno, f, why);
                fails++;
            } else {
                printf("  ok refmt: format %u the same through the container\n", f);
            }
        } else if (!strcmp(cmd, "bootfail")) {  /* (1.5.0) the next reboot as after a failed boot (the guard's count 1) */
            host_failed = 1;
        } else if (!strcmp(cmd, "powercut")) {  /* (1.5.0) powercut N: the power lost at the Nth erase or program */
            host_cut = atoi(a) > 0 ? atoi(a) : 1;
        } else if (!strcmp(cmd, "bootcut")) {   /* (first sets) bootcut N: the next reboot's Nth erase or program fails,
                                                  * and every one after it till the reboot after (the flash failing in
                                                  * the boot: it goes on, from RAM) */
            host_bootcut = atoi(a) > 0 ? atoi(a) : 1;
        } else if (!strcmp(cmd, "setsdelete")) { /* (first sets) setsdelete [SLOT]: every stored set (or SLOT's) deleted,
                                                  * as DELETE's yes deletes one: the store as a unit's whose sets were
                                                  * deleted (the scenarios made for an empty one) */
            uint32_t i;
            for (i = 0; i < PLAT_FS_SETS; i++)
                if ((!a[0] || (uint32_t)atoi(a) == i) && plat_fs_has(FS_SET, i) && plat_fs_delete(FS_SET, i)) {
                    printf("FAIL line %d: set %u not deleted\n", lineno, i);
                    fails++;
                }
            sxw_live = -1;
        } else if (!strcmp(cmd, "setsbeta")) {  /* (first sets) WORK saved as a 1.5.0 beta saved it: without D1C_F_SETS
                                                  * (the next boot offers the sets the FM-1 comes with again) */
            work_sets = 0;
            if (work_write(&proj, 1)) {
                printf("FAIL line %d: WORK not saved\n", lineno);
                fails++;
            }
        } else if (!strcmp(cmd, "setbytes")) {  /* (first sets) setbytes SLOT FILE: set SLOT's bytes, as stored, FILE's */
            static uint8_t want[PLAT_FS_MAX + 1], got[PLAT_FS_MAX];
            FILE *f = fopen(b, "rb");
            uint32_t len = 0;
            size_t n = f ? fread(want, 1, sizeof want, f) : 0;
            const int fr = plat_fs_find(FS_SET, (uint32_t)atoi(a), 0, &len);
            if (f)
                fclose(f);
            if (!f || fr < 0 || n != len || plat_fs_read(fr, 0, got, len) || memcmp(got, want, len)) {
                printf("FAIL line %d: set %s not %s's bytes (%s)\n", lineno, a, b,
                       !f ? "no file" : fr < 0 ? "none stored" : n != len ? "the length" : "the bytes");
                fails++;
            } else {
                printf("  ok set %s: %s's %u bytes\n", a, b, len);
            }
        } else if (!strcmp(cmd, "name")) {      /* (1.5.0) name I TEXT: piece I's name as a set would give it (I -1: the
                                                  * set's; TEXT "-": none, the recipe's), unsaved */
            const int i = atoi(a);
            char f[D1C_NAME], *t = strstr(line, a) + strlen(a), *e;
            while (*t == ' ')
                t++;
            e = t + strlen(t);
            while (e > t && (e[-1] == '\n' || e[-1] == '\r' || e[-1] == ' '))
                *--e = 0;
            d1c_name_set(f, strcmp(t, "-") ? t : "");
            if (i < -1 || i >= NPIECES || strlen(t) > PROJ_NAME || (t[0] && strcmp(t, "-") && !name_ok(f, D1C_NAME))) {
                printf("line %d: no such name: %s\n", lineno, t);
                return 2;
            }
            name_take(i < 0 ? proj.name : proj.slot[i].name, f);
            ui.dirty = 1;
        } else if (!strcmp(cmd, "expectname")) {   /* (1.5.0) expectname I TEXT: piece_name(I) is TEXT */
            const char *t = strstr(line, a) + strlen(a);
            char want[64];
            while (*t == ' ')
                t++;
            snprintf(want, sizeof want, "%s", t);
            want[strcspn(want, "\r\n")] = 0;
            if (strcmp(piece_name(atoi(a)), want)) {
                printf("FAIL line %d: piece %s's name \"%s\", not \"%s\"\n", lineno, a, piece_name(atoi(a)), want);
                fails++;
            } else {
                printf("  ok name %s \"%s\"\n", a, want);
            }
        } else if (!strcmp(cmd, "slotrecipe")) {   /* (step 4) slotrecipe I R: slot I recipe R's piece (a set's move) */
            const int i = atoi(a), r = atoi(b);
            if (i < 0 || i >= NPIECES || r < 0 || r >= NPIECES) {
                printf("line %d: slotrecipe I R: 0..7\n", lineno);
                return 2;
            }
            if (i == proj.piece)
                keep();
            recipe(r, &proj.slot[i]);
            if (i == proj.piece) {
                take(&proj.slot[i]);
                project_apply();
                en_regrow();
            }
            ui.dirty = 1;
        } else if (!strcmp(cmd, "dub1set")) {    /* (step 4) dub1set FILE N [noglob]: the first N pieces as a SET file */
            static uint8_t fb[PLAT_FS_MAX];
            static const upg_t U = {PROJ_FORMAT, P_NPARAMS, 0};
            const uint32_t n = (uint32_t)atoi(b);
            const int glob = strcmp(c, "noglob") != 0;
            d1c_w_t w;
            pmem_t m = {fb, 0, sizeof fb};
            FILE *f;
            uint32_t total;
            if (n < 1u || n > NPIECES) {
                printf("line %d: dub1set FILE N: 1..8 pieces\n", lineno);
                return 2;
            }
            keep();
            d1c_w_init(&w, 0, 0);
            proj_emit(&w, &proj, &U, 0, FS_SET, n, glob);
            total = w.len;
            d1c_w_init(&w, pmem_put, &m);
            proj_emit(&w, &proj, &U, total, FS_SET, n, glob);
            if (w.err || m.n != total || !(f = fopen(a, "wb")) || fwrite(fb, 1, total, f) != total) {
                printf("line %d: dub1set %s failed\n", lineno, a);
                return 2;
            }
            fclose(f);
        } else if (!strcmp(cmd, "setfile")) {    /* (step 6) setfile FILE SLOTS NAME: those pieces as a SET file */
            static uint8_t fb[PLAT_FS_MAX];
            static const upg_t U = {PROJ_FORMAT, P_NPARAMS, 0};
            static project_t t, r;
            int sl[NPIECES], n = 0, i, bad = 0, untouched = 0;
            char f[D1C_NAME], *nm = strstr(line + strlen(cmd), a) + strlen(a), *e;
            const char *q = b;
            uint32_t total, px = 0;
            d1c_w_t w;
            d1c_r_t rd;
            d1c_meta_t m;
            pmem_t pm = {fb, 0, sizeof fb};
            FILE *fo;
            while (*q && n < NPIECES) {          /* (SLOTS: "3,4,5,6,7", each once) */
                sl[n] = (int)strtol(q, (char **)&q, 10);
                for (i = 0; i < n; i++)
                    bad |= sl[i] == sl[n];
                bad |= sl[n] < 0 || sl[n] >= NPIECES;
                n++;
                if (*q == ',')
                    q++;
                else if (*q)
                    bad = 1;
            }
            nm = strstr(nm, b) + strlen(b);
            while (*nm == ' ')
                nm++;
            e = nm + strlen(nm);
            while (e > nm && (e[-1] == '\n' || e[-1] == '\r' || e[-1] == ' '))
                *--e = 0;
            d1c_name_set(f, nm);
            if (bad || !n || *q || !*nm || strlen(nm) > PROJ_NAME || !name_fits(f, D1C_NAME, &px)) {
                printf("line %d: setfile FILE SLOTS NAME: slots 0..7 (\"3,4,5\"), a name that fits (%s)\n", lineno, nm);
                return 2;
            }
            keep();                              /* (the piece playing, into its slot, as a save does) */
            t = proj;
            for (i = 0; i < n; i++)
                t.slot[i] = proj.slot[sl[i]], t.cues[i] = proj.cues[sl[i]];   /* ((step 7) and its cues) */
            t.piece = 0, t.setlen = (uint8_t)(n < NPIECES ? n : 0);
            name_take(t.name, f);
            d1c_w_init(&w, 0, 0);
            proj_emit(&w, &t, &U, 0, FS_SET, (uint32_t)n, 1);
            total = w.len;
            d1c_w_init(&w, pmem_put, &pm);
            proj_emit(&w, &t, &U, total, FS_SET, (uint32_t)n, 1);
            if (w.err || pm.n != total || !(fo = fopen(a, "wb")) || fwrite(fb, 1, total, fo) != total) {
                printf("line %d: setfile %s failed\n", lineno, a);
                return 2;
            }
            fclose(fo);
            /* read back as the FM-1 takes a set, onto the project as it is: slots 1..n the pieces, and the set's own */
            if (d1c_open(&rd, pmem_get, fb, total) || proj_decode(&rd, &r, &m, &proj)) {
                printf("FAIL line %d: setfile %s: not read back\n", lineno, a);
                fails++;
            } else {
                for (i = 0; i < n; i++) {        /* (each value but the globals, a slot's copy of them unread; a piece
                                                  * left as its recipe is taken as today's recipe, its loops grown afresh,
                                                  * as the FM-1 takes it: untouched_now) */
                    const slot_t *x = &r.slot[i], *y = &t.slot[i];
                    const int fresh = y->pings_ok && !x->pings_ok && untouched_now(y, y->recipe);
                    int k;
                    for (k = 0; k < P_NPARAMS; k++)
                        if (!global_par(k) && x->par[k] != y->par[k])
                            bad |= 1 << i;
                    if (x->pool != y->pool || (!fresh && (x->pings_ok != y->pings_ok ||
                        (y->pings_ok && memcmp(x->pings, y->pings, sizeof x->pings)))) || x->screen != y->screen ||
                        x->recipe != y->recipe || strcmp(x->name, y->name))
                        bad |= 1 << i;
                    untouched += fresh;
                }
                if (bad || r.trans != t.trans || memcmp(r.sir, t.sir, sizeof r.sir) || r.par[P_TUNE] != t.par[P_TUNE] ||
                    r.par[P_SMODE] != t.par[P_SMODE] || r.setlen != t.setlen || strcmp(m.name, t.name)) {
                    printf("FAIL line %d: setfile %s: read back otherwise (pieces %02x)\n", lineno, a, (unsigned)bad);
                    fails++;
                } else {
                    printf("  ok setfile %s: \"%s\" (%u px), %d pieces (", a, t.name, (unsigned)px, n);
                    for (i = 0; i < n; i++)
                        printf("%s%s", i ? ", " : "", t.slot[i].name[0] ? t.slot[i].name : en_piece_name(t.slot[i].recipe));
                    printf("), Song mode %d, Transition %d, Tune %d; %u bytes, read back the same", t.par[P_SMODE],
                           t.trans, t.par[P_TUNE], (unsigned)total);
                    if (untouched)
                        printf(" (%d as its recipe: taken as today's, its loops grown afresh, as the FM-1 takes it)",
                               untouched);
                    printf("\n");
                }
            }
        } else if (!strcmp(cmd, "setin")) {      /* (step 6) setin FILE: loaded as the FM-1 loads a set */
            static uint8_t fb[PLAT_FS_MAX + 4];
            const uint32_t n = file_in(a, fb, sizeof fb);
            int rc;
            if (!n) {
                printf("line %d: no file %s\n", lineno, a);
                return 2;
            }
            if ((rc = project_set_mem(fb, n)) < 0) {
                printf("FAIL line %d: setin %s refused (%d)\n", lineno, a, rc);
                fails++;
            } else {                             /* (as web_set_load: the engine on its first piece) */
                project_apply();
                en_regrow();
                ui.prog_from = en_queue_w();
                ui.cue_from = -1;
                ui.sig[0] = ui.sig[1] = ui.sig[2] = 0;
                ui.dirty = rc != 0;
                printf("  ok setin %s: \"%s\", %d pieces\n", a, proj.name, proj.setlen ? proj.setlen : NPIECES);
            }
        } else if (!strcmp(cmd, "cuerec")) {     /* (step 7) cuerec FILE | off: the moves, a line each (crec_poll) */
            if (crec)
                fclose(crec);
            crec = 0;
            if (strcmp(a, "off")) {
                if (!(crec = fopen(a, "w"))) {
                    printf("line %d: cuerec %s: can't write\n", lineno, a);
                    return 2;
                }
                crec_snap();
                fprintf(crec, "# cuerec: ms bars frac piece part b0 bo be playing trcue WHAT arg val\n");
            }
        } else if (!strcmp(cmd, "cue")) {        /* (step 7) cue SLOT PART BAR STEP OP ARG VAL LEN: a cue for piece SLOT
                                                  * (PART i m o b: intro, main, outro, the bar from PLAY; OP a name:
                                                  * mute solo move fx siren sirk macro knob outro next lock hold) */
            char pt[8], op[16];
            int sl, bar, st, ag, vl, ln, o, k;
            static const char PARTS[] = "imob";
            if (sscanf(line, "%*s %d %7s %d %d %15s %d %d %d", &sl, pt, &bar, &st, op, &ag, &vl, &ln) != 8 ||
                sl < 0 || sl >= NPIECES || !strchr(PARTS, pt[0]) || bar < 0 || bar > 1023 || st < 0 || st > 15) {
                printf("line %d: cue SLOT PART BAR STEP OP ARG VAL LEN\n", lineno);
                return 2;
            }
            for (o = 0; o < CUE_NOPS && strcmp(op, CUE_OP_NAME[o]); o++)
                ;
            if (o == CUE_NOPS || hc_n[sl] >= CUE_MAX) {
                printf("line %d: cue: no op %s (or 64 already)\n", lineno, op);
                return 2;
            }
            k = hc_n[sl]++;
            hc_q[sl][k].pos = D1C_POS(strchr(PARTS, pt[0]) - PARTS, bar, st);
            hc_q[sl][k].op = (uint8_t)o, hc_q[sl][k].arg = (uint8_t)ag, hc_q[sl][k].val = (int16_t)vl;
            hc_q[sl][k].len = (uint16_t)ln;
            hc_on[sl] = 1;
        } else if (!strcmp(cmd, "cueson")) {     /* (step 7) cueson 0|1: the Set page's Cues, at once */
            proj.cues_off = (uint8_t)!atoi(a);
        } else if (!strcmp(cmd, "cueclear")) {   /* (step 7) cueclear SLOT: piece SLOT's cues none (at cuesave) */
            hc_n[atoi(a) & 7] = 0, hc_on[atoi(a) & 7] = 1;
        } else if (!strcmp(cmd, "cuesave")) {    /* (step 7) the cues given into the pieces (sorted, checked), WORK saved */
            uint32_t at = 0, i, j, k;
            keep();
            cue_gen = (uint8_t)((cue_gen + 1u) & 0x7Fu);
            for (i = 0; i < NPIECES; i++) {
                if (!hc_on[i])
                    continue;
                for (j = 1; j < hc_n[i]; j++)    /* (by place, the order given kept) */
                    for (k = j; k > 0 && hc_q[i][k].pos < hc_q[i][k - 1].pos; k--) {
                        d1c_cue_t t = hc_q[i][k];
                        hc_q[i][k] = hc_q[i][k - 1], hc_q[i][k - 1] = t;
                    }
                for (j = 0; j < hc_n[i]; j++) {
                    if (!cue_ok(&hc_q[i][j])) {
                        printf("line %d: cuesave: piece %u's cue %u not one the FM-1 takes\n", lineno, i, j);
                        return 2;
                    }
                    d1c_cue_t *q = &hc_q[i][j];
                    uint8_t *b = cue_mem + at + D1C_CUE * j;
                    b[0] = (uint8_t)q->pos, b[1] = (uint8_t)(q->pos >> 8), b[2] = q->op, b[3] = q->arg;
                    b[4] = (uint8_t)q->val, b[5] = (uint8_t)((uint16_t)q->val >> 8), b[6] = (uint8_t)q->len;
                    b[7] = (uint8_t)(q->len >> 8);
                }
                proj.cues[i].src = hc_n[i] ? (uint8_t)(CUE_SRC_MEM | cue_gen) : 0u;
                proj.cues[i].n = hc_n[i], proj.cues[i].off = (uint16_t)at;
                at += D1C_CUE * hc_n[i];
                hc_n[i] = 0, hc_on[i] = 0;
            }
            for (i = 0; i < NPIECES; i++)        /* (lists from an older memory copy: gone with it) */
                if ((proj.cues[i].src & CUE_SRC_MEM) && (proj.cues[i].src & 0x7Fu) != cue_gen)
                    memset(&proj.cues[i], 0, sizeof proj.cues[i]);
            if (work_write(&proj, 1)) {
                printf("FAIL line %d: cuesave: WORK not saved\n", lineno);
                fails++;
            } else {
                plat_fs_prepare();
                printf("  ok cuesave:");
                for (i = 0; i < NPIECES; i++)
                    if (proj.cues[i].n)
                        printf(" piece %u %u cues", i + 1, proj.cues[i].n);
                printf("\n");
            }
        } else if (!strcmp(cmd, "cuetables")) {  /* (step 7b) cuetables FILE: the cue editor's tables, JSON */
            FILE *f = fopen(a, "w");
            if (!f) {
                printf("line %d: cuetables FILE\n", lineno);
                return 2;
            }
            cue_tables(f);
            fclose(f);
        } else if (!strcmp(cmd, "cuecheck")) {   /* (step 7b) cuecheck IN OUT: each line of IN a cue ("pos op arg val
                                                  * len"), a line of OUT its cue_ok (1 the FM-1 takes it, 0 not): the
                                                  * cue editor's check against the firmware's (tests/host/cue_editor_test.mjs) */
            static char pi[512], po[512];
            FILE *fi = sscanf(line, "%*s %511s %511s", pi, po) == 2 ? fopen(pi, "r") : 0, *fo = fi ? fopen(po, "w") : 0;
            unsigned pos, op, arg, len;
            int val;
            if (!fi || !fo) {
                printf("line %d: cuecheck IN OUT\n", lineno);
                return 2;
            }
            while (fscanf(fi, "%u %u %u %d %u", &pos, &op, &arg, &val, &len) == 5) {
                d1c_cue_t q;
                q.pos = (uint16_t)pos, q.op = (uint8_t)op, q.arg = (uint8_t)arg, q.val = (int16_t)val;
                q.len = (uint16_t)len;
                fprintf(fo, "%d\n", cue_ok(&q));
            }
            fclose(fi);
            fclose(fo);
        } else if (!strcmp(cmd, "cuelist")) {    /* (step 7) cuelist SLOT: piece SLOT's cues printed */
            static d1c_cue_t q[CUE_MAX];
            const uint32_t n = cues_read(&proj.cues[atoi(a) & 7], q);
            uint32_t i;
            printf("  cues of piece %d: %u\n", (atoi(a) & 7) + 1, n);
            for (i = 0; i < n; i++)
                printf("    %c %u.%u %s %u %d %u\n", "imob"[q[i].pos >> 14], q[i].pos >> 4 & 1023u, q[i].pos & 15u,
                       CUE_OP_NAME[q[i].op], q[i].arg, q[i].val, q[i].len);
        } else if (!strcmp(cmd, "piece")) {      /* (step 6) piece I: chosen as PRESET chooses it, at once */
            if (atoi(a) < 0 || atoi(a) >= NPIECES) {
                printf("line %d: piece I: 0..7\n", lineno);
                return 2;
            }
            set_piece(atoi(a));
        } else if (!strcmp(cmd, "include")) {    /* (step 6) include FILE [STOP] */
            FILE *f;
            if (inc_n >= 4 || !(f = fopen(a, "r"))) {
                printf("line %d: include %s: %s\n", lineno, a, inc_n >= 4 ? "four deep already" : "no such file");
                return 2;
            }
            inc_f[inc_n] = sc, inc_line[inc_n] = lineno;
            snprintf(inc_stop[inc_n], sizeof inc_stop[0], "%s", b);
            inc_n++;
            sc = f, lineno = 0;
        } else if (!strcmp(cmd, "dub1no")) {     /* (step 4) dub1no FILE: refused as WORK */
            static uint8_t fb[PLAT_FS_MAX + 4];
            static project_t t;
            const uint32_t n = file_in(a, fb, sizeof fb);
            int rc;
            if (!n) {
                printf("line %d: no file %s\n", lineno, a);
                return 2;
            }
            if ((rc = proj_from_mem(fb, n, &t)) == 0) {
                printf("FAIL line %d: dub1no %s: taken\n", lineno, a);
                fails++;
            } else {
                printf("  ok %s refused (%d)\n", a, rc);
            }
        } else if (!strcmp(cmd, "setsend") || !strcmp(cmd, "worksend")) {   /* (step 4) setsend SLOT FILE [WHY];
                                                  * (Decisions 4) worksend 0 FILE [WHY]: a restore's pieces (WHY 200:
                                                  * kept without asking, 205 failed) */
            static uint8_t fb[PLAT_FS_MAX + 64];
            const uint32_t n = file_in(b, fb, sizeof fb);
            const int want = c[0] ? (int)strtol(c, 0, 16) : 0;
            const int rc = n ? sxh_send(cmd[0] == 'w' ? FS_WORK : FS_SET, (uint32_t)atoi(a), fb, n) : -2;
            if (rc == -2) {
                printf("line %d: no file %s\n", lineno, b);
                return 2;
            }
            if (rc != want) {
                printf("FAIL line %d: %s %s %s: %s %x (wanted %s %x)\n", lineno, cmd, a, b, rc ? "NAK" : "asked", rc,
                       want ? "NAK" : "asked", want);
                fails++;
            } else {
                printf("  ok %s %s %s: %s\n", cmd, a, b, rc == 0x200 ? "kept" : rc ? "refused" : "asked");
            }
        } else if (!strcmp(cmd, "sxreq")) {      /* (step 4) sxreq CMD BODY ANSWER [WHY] */
            uint8_t q[600];
            const uint32_t n = sxh_hex(b, q, sizeof q), want = (uint32_t)strtol(c, 0, 16);
            char d[16] = {0};
            int why;
            sscanf(line, "%*s %*s %*s %*s %15s", d);
            why = d[0] ? (int)strtol(d, 0, 16) : -1;
            if (!sxh_ask((uint32_t)strtol(a, 0, 16), q, n)) {
                printf("FAIL line %d: sxreq %s: no answer\n", lineno, a);
                fails++;
            } else if (sxh_cmd != want || (why >= 0 && (sxh_n < 2u || sxh_body[1] != why))) {
                printf("FAIL line %d: sxreq %s: answered %02X (why %d), not %02X (why %d)\n", lineno, a, sxh_cmd,
                       sxh_n >= 2u ? sxh_body[1] : -1, want, why);
                fails++;
            } else {
                printf("  ok sxreq %s: %02X\n", a, sxh_cmd);
            }
        } else if (!strcmp(cmd, "setread")) {    /* (step 4) setread KIND INDEX FILE */
            static uint8_t fb[PLAT_FS_MAX];
            uint8_t q[6];
            uint32_t off = 0, total = 1;
            d1c_r_t r;
            FILE *f;
            while (off < total) {
                q[0] = (uint8_t)atoi(a), q[1] = (uint8_t)atoi(b), q[2] = (uint8_t)off, q[3] = (uint8_t)(off >> 8);
                q[4] = 0, q[5] = 1;
                if (!sxh_ask(SXW_READ, q, 6) || sxh_cmd != SXW_READ || sxh_n < 4u || sxh_body[0] != (uint8_t)off ||
                    sxh_body[1] != (uint8_t)(off >> 8))
                    break;
                total = sxh_body[2] | (uint32_t)sxh_body[3] << 8;
                if (total > sizeof fb || sxh_n - 4u > total - off)
                    break;
                memcpy(fb + off, sxh_body + 4, sxh_n - 4u);
                off += sxh_n - 4u;
                run_ms(3);
            }
            if (off != total || d1c_open(&r, pmem_get, fb, total) || !(f = fopen(c, "wb")) ||
                fwrite(fb, 1, total, f) != total) {
                printf("FAIL line %d: setread %s %s: %u of %u bytes, or not a container\n", lineno, a, b, off, total);
                fails++;
            } else {
                fclose(f);
                printf("  ok setread %s %s: %u bytes\n", a, b, total);
            }
        } else if (!strcmp(cmd, "sethas")) {     /* (step 4) sethas SLOT NAME|- (LIST's) */
            const char *t = strstr(strstr(line, a) + strlen(a), b);   /* (after SLOT: "sethas 4 4 to 5 a.m.") */
            char want[64];
            uint32_t i, found = 0;
            snprintf(want, sizeof want, "%s", t ? t : "");
            want[strcspn(want, "\r\n")] = 0;
            if (!sxh_ask(SXW_LIST, 0, 0) || sxh_cmd != SXW_LIST) {
                printf("FAIL line %d: no LIST\n", lineno);
                fails++;
            } else {
                for (i = 0; i < sxh_body[0]; i++) {
                    const uint8_t *e = sxh_body + 1 + 22u * i;
                    char nm[17];
                    memcpy(nm, e + 6, 16);
                    nm[16] = 0;
                    printf("  list: kind %u index %u %u bytes %u pieces flags %u \"%s\"\n", e[0], e[1],
                           e[2] | (uint32_t)e[3] << 8, e[4], e[5], nm);
                    if (e[0] == FS_SET && e[1] == atoi(a))
                        found = 1 + !strcmp(nm, want);
                }
                if (strcmp(want, "-") ? found != 2u : found != 0u) {
                    printf("FAIL line %d: set %s %s\n", lineno, a, found ? "named otherwise" : strcmp(want, "-") ? "not stored" : "stored");
                    fails++;
                } else {
                    printf("  ok set %s %s\n", a, want);
                }
            }
        } else if (!strcmp(cmd, "irqreset")) {   /* (step 5) the IRQ-off windows' counts from zero */
            host_irq_long = host_flash_xruns = host_irq_max = 0;
        } else if (!strcmp(cmd, "usbpower")) {   /* (step 5) on USB power (1: the header's battery shown) or not */
            host_usb = atoi(a) != 0;
        } else if (!strcmp(cmd, "battery")) {    /* (Decisions 4) battery N: its reading (-1 none) */
            host_batt = atoi(a);
        } else if (!strcmp(cmd, "namecheck")) {  /* (step 4) names.h against the header (draw_header, hdr_phrase) */
            static const char *const SONG_W[2] = {"Intro", "Outro"};
            char t[16];
            int32_t word = 0, w, px, bad = 0;
            int i;
            for (i = 0; i < EN_NSEC + 2; i++) {  /* (the phrase's widest word, as it shows it: capitals) */
                caps(t, i < EN_NSEC ? EN_SEC_NAME[i] : SONG_W[i - EN_NSEC], sizeof t);
                w = text_w(&FONT_XS, t);
                word = w > word ? w : word;
            }
            px = (232 - 15 - 6) - (4 + 12 + 10) - word - 6 - (8 * 5 + 7 * 2);   /* (the right edge, no battery: a wide
                                                  * name hides it; the disk, the phrase's end; the tag's 4 + 12 and its
                                                  * gap; the word and its gap; the smaller cells) */
            if (px != NAME_PX) {
                printf("FAIL line %d: the header's room for a name %d px, names.h's NAME_PX %d\n", lineno, px, NAME_PX);
                bad++;
            }
            px -= 232 - 213;                     /* (with the battery: its x, 213) */
            if (px != NAME_PX_BATT) {
                printf("FAIL line %d: the header's room beside the battery %d px, names.h's NAME_PX_BATT %d\n", lineno,
                       px, NAME_PX_BATT);
                bad++;
            }
            for (i = 32; i < 127; i++) {         /* (the widths: the font's, the digits no narrower) */
                const char s1[2] = {(char)i, 0};
                if (name_char((char)i) && (NAME_W[i - 32] < text_w(&FONT_B, s1) ||
                                           (NAME_W[i - 32] != text_w(&FONT_B, s1) && !(i >= '0' && i <= '9')))) {
                    printf("FAIL line %d: names.h's width of '%c' %u, the header font's %d\n", lineno, i,
                           NAME_W[i - 32], text_w(&FONT_B, s1));
                    bad++;
                }
            }
            for (i = 0; i < NPIECES; i++) {      /* (the recipes' own names fit) */
                char f[D1C_NAME];
                uint32_t pw;
                d1c_name_set(f, en_piece_name(i));
                if (!name_fits(f, D1C_NAME, &pw) || (int32_t)pw != text_w(&FONT_B, en_piece_name(i))) {
                    printf("FAIL line %d: the recipe's name \"%s\" (%d px) not a name\n", lineno, en_piece_name(i),
                           text_w(&FONT_B, en_piece_name(i)));
                    bad++;
                }
            }
            fails += bad;
            if (!bad)
                printf("  ok names: up to %d px (the header's room), the font's widths, the recipes' names in\n", px);
        } else if (!strcmp(cmd, "setstate")) {   /* (step 4) setstate STATE */
            static const char *const S[7] = {"idle", "asking", "yes", "no", "timeout", "failed", "playing"};
            const int want = find_name(S, 7, a);
            if (want < 0) {
                printf("line %d: no state %s\n", lineno, a);
                return 2;
            }
            if (sxw.state != want) {
                printf("FAIL line %d: the question %s, not %s\n", lineno, S[sxw.state < 7 ? sxw.state : 0], a);
                fails++;
            } else {
                printf("  ok question %s\n", a);
            }
        } else if (!strcmp(cmd, "sxpipe")) {     /* (step 4) the rest from stdin (tests/host/dub1_set_test.py) */
#ifndef EN_WEB
            sx_pipe_on = 1;
            printf("sxpipe\n");
            fflush(stdout);
#endif
        } else if (!strcmp(cmd, "workcheck")) {  /* (1.5.0) WORK's newest version, read, is the project as it is (after a
                                                  * boot or a save); and the project written and read again, the same */
            char why[96];
            if (project_workcheck(why, sizeof why)) {
                printf("FAIL line %d: workcheck: %s\n", lineno, why);
                fails++;
            } else {
                printf("  ok workcheck\n");
            }
        } else if (!strcmp(cmd, "dub1out") || !strcmp(cmd, "dub1in")) {   /* (1.5.0) dub1out FILE: the project as a
                                                  * .dub1 file (WORK's bytes); dub1in FILE: one taken as WORK is at a boot */
            static uint8_t fb[PLAT_FS_MAX + 4];
            static const upg_t U = {PROJ_FORMAT, P_NPARAMS, 0};
            FILE *f;
            uint32_t n;
            if (cmd[4] == 'o') {
                n = proj_to_mem(&proj, &U, fb, sizeof fb);
                if (!n || !(f = fopen(a, "wb")) || fwrite(fb, 1, n, f) != n) {
                    printf("line %d: dub1out %s failed\n", lineno, a);
                    return 2;
                }
                fclose(f);
            } else {
                static project_t t;
                int rc;
                if (!(f = fopen(a, "rb"))) {
                    printf("line %d: no file %s\n", lineno, a);
                    return 2;
                }
                n = (uint32_t)fread(fb, 1, sizeof fb, f);
                fclose(f);
                if ((rc = proj_from_mem(fb, n, &t)) != 0) {
                    printf("FAIL line %d: dub1in %s refused (%d)\n", lineno, a, rc);
                    fails++;
                } else {
                    proj = t;
                    project_apply();
                    en_regrow();
                }
            }
        } else if (!strcmp(cmd, "fuzzboot")) {   /* (1.5.0) fuzzboot N [SEED]: N mutated WORKs written to the flash as the
                                                  * newest version, a boot each: the mutated one taken only when it reads
                                                  * (mended CRC, values in range), else the version before, exactly */
            char why[96];
            uint32_t took;
            if (project_fuzzboot((uint32_t)atoi(a), b[0] ? (uint32_t)atoi(b) : 1u, &took, why, sizeof why)) {
                printf("FAIL line %d: fuzzboot: %s\n", lineno, why);
                fails++;
            } else {
                printf("  ok fuzzboot %s: %u taken (they read), the rest the version before\n", a, took);
            }
            project_apply();
            en_regrow();
        } else if (!strcmp(cmd, "fuzz")) {      /* (1.5.0) fuzz N [SEED]: today's project as a container, mutated N times:
                                                  * no fault, none read unmended, every one read valid, the project kept */
            char why[96];
            uint32_t nread;
            if (project_fuzz((uint32_t)atoi(a), b[0] ? (uint32_t)atoi(b) : 1u, &nread, why, sizeof why)) {
                printf("FAIL line %d: fuzz: %s\n", lineno, why);
                fails++;
            } else {
                printf("  ok fuzz %s: %u read (CRC mended, values in range), the rest refused\n", a, nread);
            }
        } else if (!strcmp(cmd, "projmark")) {   /* (the Set page's Store) the pieces now, kept (projsame) */
            keep();
            proj_mark = proj;
        } else if (!strcmp(cmd, "projsame")) {   /* projsame N: pieces 1..N (each value but the globals, the scale, the
                                                  * loops, the view, the recipe, the name), the set's Mode, Transition,
                                                  * sirens, Tune and name as at projmark. (A piece left as its recipe
                                                  * comes back with its loops grown afresh, as the FM-1 takes a set:
                                                  * setfile's untouched_now) */
            const int n = atoi(a);
            int i, k, bad = 0, fresh = 0;
            keep();
            for (i = 0; i < n && i < NPIECES; i++) {
                const slot_t *x = &proj.slot[i], *y = &proj_mark.slot[i];
                const int fr = y->pings_ok && !x->pings_ok && untouched_now(y, y->recipe);
                for (k = 0; k < P_NPARAMS; k++)
                    if (!global_par(k) && x->par[k] != y->par[k])
                        bad |= 1 << i;
                if (x->pool != y->pool || (!fr && (x->pings_ok != y->pings_ok ||
                    (y->pings_ok && memcmp(x->pings, y->pings, sizeof x->pings)))) || x->screen != y->screen ||
                    x->recipe != y->recipe || strcmp(x->name, y->name))
                    bad |= 1 << i;
                fresh += fr;
            }
            if (n < 1 || n > NPIECES || bad || proj.trans != proj_mark.trans || memcmp(proj.sir, proj_mark.sir, sizeof proj.sir) ||
                proj.par[P_TUNE] != proj_mark.par[P_TUNE] || proj.par[P_SMODE] != proj_mark.par[P_SMODE] ||
                strcmp(proj.name, proj_mark.name)) {
                printf("FAIL line %d: projsame %s: otherwise (pieces %02x; \"%s\", was \"%s\")\n", lineno, a,
                       (unsigned)bad, proj.name, proj_mark.name);
                fails++;
            } else {
                printf("  ok projsame %s: the same (%d as its recipe, its loops grown afresh)\n", a, fresh);
            }
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
            host_dead = 0, host_cut = host_bootcut ? host_bootcut : -1;   /* (the power back; bootcut's: its boot) */
            host_bootcut = 0;
            sxw.rs_note = sxw.rs_end = 0;        /* (a restore's news not yet shown: gone with the power, as on the
                                                  * FM-1; the UI's, its outcome on the screen, ui_init's) */
            sxw_live = -1;                       /* (beta2: the set loaded, RAM's: not known after a boot) */
            boot();
            host_failed = 0;
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
        } else if (!strcmp(cmd, "cardis") || !strcmp(cmd, "msgis") || !strcmp(cmd, "outis")) {
                                                 /* (Decisions 7) cardis TEXT|-: the card in the middle now (its head;
                                                  * "-" none); msgis TEXT|-: the header's message; outis TEXT|-: the
                                                  * question's outcome (its word) */
            const int card = cmd[0] == 'c', out = cmd[0] == 'o';
            char *t = line + strlen(cmd), *e;
            const char *got;
            while (*t == ' ')
                t++;
            e = t + strlen(t);
            while (e > t && (e[-1] == '\n' || e[-1] == '\r' || e[-1] == ' '))
                *--e = 0;
            got = out ? (out_on(plat_ms()) ? ui.out_word : "-") :
                  card ? (card_on(plat_ms()) ? ui.card_head : "-") : (plat_ms() < ui.msg_until ? ui.msg[0] : "-");
            if (strcmp(got, t)) {
                printf("FAIL line %d: %s \"%s\": \"%s\" shown\n", lineno, cmd, t, got);
                fails++;
            } else {
                printf("  ok %s \"%s\"\n", cmd, t);
            }
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
        if (st_pre36)
            proj = st_keep;
        if (!strncmp(cmd, "store", 5)) {        /* (1.5.0, step 2) every store: the save read by project_load and as a
                                                  * container (its own format, wrapped as it was), the same */
            char why[96];
            uint32_t f;
            if (project_wrapcheck(&f, why, sizeof why)) {
                printf("FAIL line %d: %s (format %u): project_load and the container differ: %s\n", lineno, cmd, f, why);
                fails++;
            } else {
                printf("  ok %s: format %u the same through the container\n", cmd, f);
            }
        }
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
