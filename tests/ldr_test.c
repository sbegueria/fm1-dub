/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Host test of the Felucca update loader (firmware/loader/ldr_core.c): a fake
 * host serves a package over the SysEx protocol, the "device" flash starts as
 * another package's flash.bin (as if that firmware were installed).
 *   ldr_test OLD.fwsc NEW.fwsc
 * Then the loader's power-loss safety (build/design/REPORT-ldrfix.txt): F1, the
 * app's first sector erased first and written last; F2, the RAM record kept
 * armed while the loader works, with a start counter. Installs are cut at many
 * points (the host gone, or the loader stopped dead in the middle of a flash
 * operation), followed by a reset or a power cycle, a model of the SPL (spl()
 * below: which of its rules are seen on hardware and which are assumed), the
 * loader again and the host's resume. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>

#define FELUCCA_ID "ota-FM-1_900"
#define FELUCCA_OTA_DRYRUN 0

static uint8_t nor[0x100000], *logical;
static size_t logical_len;
static uint8_t rx[1024];
static uint32_t rx_len, rx_full, now_ms, requests, erases, bad_range, record_cleared, f0_asked;
static uint32_t cut_after, first_addr;          /* the host goes away after that many requests (0: never) */
static uint8_t ram_rec[112];                    /* the RAM update record (0x01C7FD88) */
static volatile uint32_t ram_cnt[2];            /* the loader's start counter (0x01C7FDF8) */
static uint32_t seq, crash_at, flash_ops;       /* crash_at: the loader stops dead at that flash operation */
static int crash_half = -1;                      /* (1 halfway, 0 before it; -1: odd crash_at halfway) */
static jmp_buf crash_jmp;
static const uint8_t *prog_src;
static char crash_op;
static uint32_t crash_off;
static uint32_t log_n, f0_seq, req_log_n, req_log[4096];
static struct { char op; uint32_t off, seq; } op_log[16384];   /* erases ('E') and programs ('P'), in order */

static uint32_t pack7(const uint8_t *in, uint32_t n, uint8_t *out)
{
    uint32_t acc = 0, nb = 0, o = 0;
    while (n--) {
        acc |= (uint32_t)*in++ << nb;
        nb += 8;
        while (nb >= 7) { out[o++] = acc & 0x7F; acc >>= 7; nb -= 7; }
    }
    if (nb) out[o++] = acc & 0x7F;
    return o;
}
static uint32_t unpack7h(const uint8_t *in, uint32_t n, uint8_t *out)
{
    uint32_t acc = 0, nb = 0, o = 0;
    while (n--) {
        acc |= (uint32_t)*in++ << nb;
        nb += 7;
        if (nb >= 8) { out[o++] = (uint8_t)acc; acc >>= 8; nb -= 8; }
    }
    return o;
}

static int ota_wire_send(const uint8_t *p, uint32_t n)
{
    uint8_t u[64], m[600];
    uint32_t addr, len, i, s = 0;
    unpack7h(p + 1, n - 2, u);
    if (u[2] == 0x11) return 0;
    addr = u[7] | u[8] << 8 | u[9] << 16 | (uint32_t)u[10] << 24;
    len = u[11] | u[12] << 8 | u[13] << 16;
    if (!requests++)
        first_addr = addr;
    seq++;
    if (req_log_n < 4096u)
        req_log[req_log_n++] = addr;
    if (addr == 0xF0000000u && !f0_seq)
        f0_seq = seq;
    if (cut_after && requests > cut_after)
        return 0;                                   /* asked, never answered */
    memcpy(m, "\x00\x59\x30", 3);
    m[3] = (uint8_t)(len + 8); m[4] = (uint8_t)((len + 8) >> 8); m[5] = 0; m[6] = 0;
    memcpy(m + 7, &addr, 4);
    m[11] = (uint8_t)len; m[12] = (uint8_t)(len >> 8); m[13] = 0;
    if (addr >= 0xE0000000u) {
        f0_asked += addr == 0xF0000000u;
        memset(m + 14, 0, len);
        memcpy(m + 14, "success", 8);
    } else {
        if (addr + len > logical_len) { printf("read past the package %#x\n", addr); exit(1); }
        memcpy(m + 14, logical + addr, len);
    }
    for (i = 6; i < 14 + len; i++) s += m[i];
    m[14 + len] = (uint8_t)~s;
    rx_len = pack7(m, 15 + len, rx);
    rx_full = 1;
    return 0;
}
static int ota_frame_get(const uint8_t **p, uint32_t *n) { if (!rx_full) return 0; *p = rx; *n = rx_len; return 1; }
static void ota_frame_done(void) { rx_full = 0; }
static uint32_t ota_now_ms(void) { return now_ms; }
static void ota_idle(void) { now_ms++; }
static int ota_erase(uint32_t off) { (void)off; return -1; }
static int ota_prog(uint32_t off, const void *p, uint32_t n) { (void)off; (void)p; (void)n; return -1; }
static int ota_fread(uint32_t off, void *p, uint32_t n) { memcpy(p, nor + off, n); return 0; }
static void ota_show(uint32_t step, int32_t code) { (void)step; (void)code; }
static void ota_commit(const uint8_t *parm) { (void)parm; }
#include "../firmware/src/ota.c"

static int ldr_fread(uint32_t off, void *p, uint32_t n) { memcpy(p, nor + off, n); return 0; }
static void flash_op(char op, uint32_t off)     /* the log; and the loader stopped dead here (a reset or power
                                                 * loss): an odd crash_at halfway through the operation */
{
    if (crash_at && ++flash_ops == crash_at) {
        if (crash_half < 0 ? (int)(crash_at & 1u) : crash_half) {
            if (op == 'E') memset(nor + off, 0xFF, 0x800);
            else memcpy(nor + off, prog_src, 128);  /* half a page (the sector was erased: = its bits) */
        }
        crash_op = op;
        crash_off = off;
        longjmp(crash_jmp, 1);
    }
    seq++;
    if (log_n < 16384u) { op_log[log_n].op = op; op_log[log_n].off = off; op_log[log_n].seq = seq; log_n++; }
}
static int ldr_erase(uint32_t off)
{
    if (off < 0x4000u || off >= 0xFC000u || (off & 0xFFFu)) bad_range++;
    if (off < 0x4000u) { printf("ERASE IN THE HEAD %#x\n", off); exit(1); }
    flash_op('E', off);
    erases++;
    memset(nor + off, 0xFF, 0x1000);
    return 0;
}
static int ldr_prog(uint32_t off, const void *p, uint32_t n)
{
    const uint8_t *s = p;
    uint32_t i;
    if (off < 0x4000u || off + n > 0x93000u || ((off & 0xFFu) + n) > 256u) bad_range++;
    prog_src = s;
    flash_op('P', off);
    for (i = 0; i < n; i++) nor[off + i] &= s[i];
    return 0;
}
static void ldr_record_clear(void) { record_cleared++; memset(ram_rec, 0, sizeof ram_rec); }
static void ldr_record_set(const uint8_t *r) { memcpy(ram_rec, r, sizeof ram_rec); }
static volatile uint32_t *ldr_count(void) { return ram_cnt; }
static void ldr_progress(uint32_t done, uint32_t total) { (void)done; (void)total; }
#include "../firmware/loader/ldr_core.c"

static uint8_t *load_logical(const char *path, size_t *len)
{
    FILE *f = fopen(path, "rb");
    uint8_t *raw = malloc(0x200000), *lg;
    size_t n, i;
    if (!f) { perror(path); exit(2); }
    n = fread(raw, 1, 0x200000, f);
    fclose(f);
    lg = malloc(n);
    for (i = 0; i < 20; i++) memcpy(lg + i * 47, raw + i * 48, 47);
    memcpy(lg + 20 * 47, raw + 20 * 48, n - 20 * 48);
    *len = n - 20;
    free(raw);
    return lg;
}

static uint32_t flash_off(const uint8_t *lg)        /* flash.bin offset in the logical image */
{
    uint8_t h[0x400];
    uint32_t i, n;
    memcpy(h, lg, sizeof h);
    ota_jl_enc(h, 0x40);
    n = ota_rd16(h + 8);
    for (i = 0; i < n; i++) {
        uint8_t *e = h + 0x40 + i * 0x50u;
        ota_jl_enc(e, 0x50);
        if (ota_rd16(e) == 0) return ota_rd32(e + 8);
    }
    return 0;
}

static int check(const char *what, int ok) { printf("%-62s %s\n", what, ok ? "ok" : "FAIL"); return ok ? 0 : 1; }

static uint8_t *old, *newfl;                        /* the two flash.bin images */

/* the device with OLD installed, as the app leaves it when it starts the update (ota.c ota_session): the
 * staged loader at 0xE0000 (a stand-in), the record in flash (0xE4F00) and in RAM, the start counter zero
 * (fm1_mailbox_clear) */
static void device_fresh(const uint8_t *app)
{
    uint8_t r[112] = {0};
    memset(nor, 0xFF, sizeof nor);
    memcpy(nor, old, 0x4000);
    memcpy(nor + 0x4000, app + 0x4000, 0x93000 - 0x4000);
    memset(nor + 0xE0000, 0x5A, 0x4000);
    r[2] = 0x0D; r[3] = 0x5A; r[4] = 0x01; r[5] = 0x5A; r[6] = 0x41; r[7] = 0x54;
    ota_wr32(r + 72, 0xE0000);
    ota_wr16(r, ota_crc16(r + 2, 78, 0));
    memcpy(nor + 0xE4F00, r, sizeof r);
    memcpy(ram_rec, r, sizeof r);
    ram_cnt[0] = ram_cnt[1] = 0;
    record_cleared = erases = requests = f0_asked = f0_seq = log_n = req_log_n = 0;
    cut_after = crash_at = flash_ops = 0;
}

/* The SPL (JieLi's uboot.boot), as build/design/REPORT-installer.md section 4 describes it.
 *   [HW]      seen on hardware;  [ASSUMED] not yet seen, what the device test is for.
 *   RAM (the record, the counter) survives a warm reset [HW: DUB-1's boot guard in .noinit counts crashes
 *     and hangs]; a power cycle leaves garbage [ASSUMED; modelled as random bytes].
 *   1. a valid RAM record runs the staged loader: after a core reset [HW: every update starts so]; after a
 *      watchdog reset or a crash of the loader too [ASSUMED].
 *   2. else the app runs if it mounts [HW: a half-written app ran on 2026-10-31]; the mount check modelled
 *      as the loader's own check of the package (the first 32 bytes at 0x4000 decrypt with this chip's key
 *      to a JLFS entry whose CRC holds) [ASSUMED].
 *   3. else a valid flash record (0xE4F00) runs the staged loader [ASSUMED: m-bootloader's SPEC, the wiki].
 *   4. else the ROM's download mode [ASSUMED]. */
enum { BOOT_LDR_RAM, BOOT_APP, BOOT_LDR_FLASH, BOOT_ROM };
static const char *const BOOT_NAME[] = {"the loader (RAM record)", "the app", "the loader (flash record)", "ROM"};
static uint32_t chip_key;
static int app_mounts(void)
{
    uint8_t e[32];
    memcpy(e, nor + 0x4000, 32);
    ldr_sfc(e, 32, 0, chip_key);
    return ota_crc16(e + 2, 30, 0) == ota_rd16(e);
}
static int spl(int power_cycle)
{
    if (power_cycle) {
        uint32_t i;
        for (i = 0; i < sizeof ram_rec; i++) ram_rec[i] = (uint8_t)rand();
        ram_cnt[0] = (uint32_t)rand() * 65599u; ram_cnt[1] = (uint32_t)rand();
    }
    if (ldr_rec_ok(ram_rec)) return BOOT_LDR_RAM;
    if (app_mounts()) return BOOT_APP;
    if (ldr_rec_ok(nor + 0xE4F00) && nor[0xE0000] != 0xFF) return BOOT_LDR_FLASH;
    return BOOT_ROM;
}

/* one start of the loader as loader.c runs it (ldr_cstart: ldr_arm; then a session when the host asks);
 * 1 if it stopped dead (crash_at) */
static int loader_run(void)
{
    rx_full = 0;
    if (setjmp(crash_jmp)) return 1;
    ldr_arm();
    return ldr_session();
}
static int is_app(const uint8_t *app) { return !memcmp(nor + 0x4000, app + 0x4000, 0x93000 - 0x4000); }
static int finished_ok(void)                        /* the new app, nothing armed, back to the app */
{
    return is_app(newfl) && !ldr_rec_ok(ram_rec) && ram_cnt[0] == 0 && !ldr_rec_ok(nor + 0xE4F00) &&
           spl(0) == BOOT_APP;
}

/* one install cut at `at` (host: after that many requests; dead: at that flash operation), then a reset
 * (power 0) or a power cycle (1), the SPL, the loader again and the host's resume. 0 = as expected */
static struct { uint32_t cases, mixed, ldr_ram, ldr_flash, app_old, done_first, window; } cs;
static int cut_case(int dead, uint32_t at, int power, int verbose)
{
    int rc, b;
    device_fresh(old);
    if (spl(0) != BOOT_LDR_RAM) return 1;
    if (dead) crash_at = at; else cut_after = at;
    rc = loader_run();
    cut_after = crash_at = 0;
    cs.cases++;
    if (rc == 0) {                                  /* the cut came after the end */
        cs.done_first++;
        if (!finished_ok() && verbose) printf("  %s cut at %u: finished, not clean\n", dead ? "dead" : "host", at);
        return !finished_ok();
    }
    if (!is_app(old) && !is_app(newfl)) {
        cs.mixed++;
        if (app_mounts()) {
            /* the one moment left: dead halfway through the program of 0x4000's first page, the last write of
             * all (about 1 ms); its first half programmed, everything else new. A reset still comes back through
             * the RAM record (F2); after a power cycle the SPL boots that app. */
            if (dead && crash_op == 'P' && crash_off == 0x4000u && !memcmp(nor + 0x4100, newfl + 0x4100, 0x93000 - 0x4100)) {
                if (power) { cs.window++; return 0; }
            } else {
                if (verbose) printf("  %s cut at %u: a half-written app mounts\n", dead ? "dead" : "host", at);
                return 1;
            }
        }
    }
    b = spl(power);
    if (b == BOOT_APP) {                            /* the old app whole (nothing written yet: a power cycle), or the new
                                                     * one whole (dead after the last write; ota_boot_cleanup unstages) */
        if (is_app(newfl)) { cs.done_first++; return 0; }
        cs.app_old++;
        if (!(is_app(old) && power) && verbose) printf("  %s cut at %u: the old app boots after a reset\n", dead ? "dead" : "host", at);
        return !(is_app(old) && power);
    }
    if (b == BOOT_LDR_RAM) cs.ldr_ram++;
    else if (b == BOOT_LDR_FLASH) cs.ldr_flash++;
    else { if (verbose) printf("  cut at %u: %s\n", at, BOOT_NAME[b]); return 1; }
    if (!power && b != BOOT_LDR_RAM) {             /* F2: a reset comes back through the RAM record */
        if (verbose) printf("  %s cut at %u: a reset comes back by the flash record\n", dead ? "dead" : "host", at);
        return 1;
    }
    erases = requests = 0;
    rc = loader_run();
    if (rc != 0 || !finished_ok()) {
        if (verbose) printf("  %s cut at %u, %s: resumed rc %d\n", dead ? "dead" : "host", at, power ? "power" : "reset", rc);
        return 1;
    }
    return 0;
}

int main(int argc, char **argv)
{
    size_t olen;
    uint8_t *oldlg, head[0x4000];
    uint32_t ofo, nfo, full_reqs, full_erases, full_ops, last_app_op, i;
    int bad = 0, rc;
    if (argc < 3) { printf("usage: ldr_test OLD.fwsc NEW.fwsc\n"); return 2; }
    srand(1);
    oldlg = load_logical(argv[1], &olen);
    logical = load_logical(argv[2], &logical_len);
    ofo = flash_off(oldlg);
    nfo = flash_off(logical);
    old = oldlg + ofo;
    newfl = logical + nfo;
    /* device: OLD installed, plus a valid update record at 0xE4F00 */
    device_fresh(old);
    memcpy(head, nor, sizeof head);
    bad += check("the chip key from the device's head", ldr_chip_key(&chip_key) == 0);
    bad += check("SPL model: the old app mounts, an erased first sector doesn't", app_mounts() &&
                 (memset(nor + 0x4000, 0xFF, 32), !app_mounts()));
    device_fresh(old);
    bad += check("SPL model: the app's commit starts the loader", spl(0) == BOOT_LDR_RAM);
    rc = loader_run();
    printf("  rc %d, %u requests, %u sector erases, %u flash operations\n", rc, requests, erases, log_n);
    full_reqs = requests;
    full_erases = erases;
    full_ops = log_n;
    for (last_app_op = log_n; last_app_op && op_log[last_app_op - 1].off >= 0x93000u; last_app_op--)
        ;                                        /* (the last write in the app area: 0x4000's first page) */
    bad += check("install completes", rc == 0);
    bad += check("app area == the new package's flash.bin", is_app(newfl));
    bad += check("flash head [0, 0x4000) untouched", !memcmp(nor, head, sizeof head));
    bad += check("finish asked (0xF0000000)", f0_asked >= 1);
    bad += check("update record cleared (RAM + flash)", record_cleared && !ldr_rec_ok(ram_rec) && nor[0xE4F06] == 0xFF);
    bad += check("no write outside the allowed windows", bad_range == 0);
    /* F1: the order */
    {
        uint32_t k, last_other = 0, first_p0 = 0, last_app = 0, n_first_e = 0;
        for (k = 0; k < log_n; k++) {
            uint32_t o = op_log[k].off;
            if (o >= 0x93000u) continue;
            last_app = k;
            if (o < 0x5000u) { n_first_e += op_log[k].op == 'E'; if (op_log[k].op == 'P' && !first_p0) first_p0 = op_log[k].seq; }
            else last_other = op_log[k].seq;
        }
        bad += check("F1: the first operation is 0x4000's erase", log_n && op_log[0].op == 'E' && op_log[0].off == 0x4000u);
        bad += check("F1: 0x4000 programmed only after every other sector", first_p0 > last_other);
        bad += check("... its first page the very last write", op_log[last_app].op == 'P' && op_log[last_app].off == 0x4000u);
        bad += check("F1: 0x4000 erased twice (first, and before its write)", n_first_e == 2);
        bad += check("F1: the finish (0xF0000000) asked after 0x4000 written", f0_seq > op_log[last_app].seq);
    }
    /* the host's side is unchanged: the same reads, in the same order, as the loader before F1 */
    {
        uint32_t k = 0, s2, j, same = req_log_n == full_reqs;
        same &= req_log[k++] == 0 && req_log[k++] == 512 && req_log[k++] == nfo + 0x4000;
        for (s2 = 0x4000; s2 < 0x93000 && same; s2 += 0x1000)
            for (j = 0; j < 0x1000; j += 512)
                same &= req_log[k++] == nfo + s2 + j;
        same &= req_log[k] == 0xF0000000u;
        bad += check("the host's reads: the same as before F1 (header, key, sectors, finish)", same);
    }
    bad += check("finished: the SPL boots the new app (nothing armed)", finished_ok());
    /* same package again: nothing to erase, 0x4000 untouched */
    device_fresh(newfl);
    rc = loader_run();
    {
        uint32_t k, app_ops = 0;
        for (k = 0; k < log_n; k++)
            app_ops += op_log[k].off < 0x93000u;
        bad += check("re-run with the same package writes nothing (0x4000 neither)", rc == 0 && app_ops == 0);
    }
    bad += check("... finished, nothing armed", finished_ok());
    /* 0x4000 already the new one but another sector differs: 0x4000 erased before that sector, written last */
    {
        uint32_t k, e4 = 0, e50 = 0;
        device_fresh(newfl);
        memcpy(nor + 0x50000, old + 0x50000, 0x1000);
        nor[0x50000] ^= 0x01;
        rc = loader_run();
        for (k = 0; k < log_n; k++) {
            if (op_log[k].op == 'E' && op_log[k].off == 0x4000u && !e4) e4 = op_log[k].seq;
            if (op_log[k].op == 'E' && op_log[k].off == 0x50000u && !e50) e50 = op_log[k].seq;
        }
        for (k = log_n; k-- && op_log[k].off >= 0x93000u;)
            ;
        bad += check("0x4000 equal, 0x50000 not: 0x4000 erased first, written last", rc == 0 && e4 && e4 < e50 &&
                     op_log[k].op == 'P' && op_log[k].off == 0x4000u && finished_ok());
        device_fresh(newfl);
        memcpy(nor + 0x50000, old + 0x50000, 0x1000);
        nor[0x50000] ^= 0x01;
        crash_at = 2;                                /* dead in 0x50000's erase */
        rc = loader_run();
        bad += check("... dead before 0x50000 written: 0x4000 erased, no mount", rc == 1 && !app_mounts());
        crash_at = 0;
    }
    /* a package for another chip key is refused before any erase */
    {
        uint8_t save = logical[nfo + 0x4000 + 5];
        logical[nfo + 0x4000 + 5] ^= 0x5A;           /* app area head no longer decrypts */
        device_fresh(old);
        rc = loader_run();
        bad += check("foreign key / damaged app head refused, nothing erased", rc == -6 && erases == 0);
        logical[nfo + 0x4000 + 5] = save;
    }
    /* a write cut halfway (the host gone: each read asked 3 times, 5 s apart, then the session ends), then a new
     * session (the installer's resume): it starts again from the package's header and asks every read again, but
     * erases and writes only what the first one hadn't; the update record stays until the very end */
    {
        uint32_t e1, sectors_new = 0, s;
        device_fresh(old);
        cut_after = full_reqs / 2;
        rc = loader_run();
        e1 = erases;
        for (s = 0x4000; s < 0x93000; s += 0x1000)
            sectors_new += !memcmp(nor + s, newfl + s, 0x1000);
        printf("  cut after %u requests: rc %d, %u sector erases, %u of 143 sectors new\n", cut_after, rc, e1, sectors_new);
        bad += check("cut halfway: the session ends with a read error", rc == -7);
        bad += check("... the update record stays (RAM and flash)", ldr_rec_ok(ram_rec) && ldr_rec_ok(nor + 0xE4F00) && !f0_asked);
        bad += check("... the app area half old, half new", sectors_new > 0 && sectors_new < 143);
        bad += check("... F1: 0x4000 erased: the app doesn't mount", ldr_blank(nor + 0x4000) && !app_mounts());
        bad += check("... a power cycle: the SPL runs the loader (flash record)", spl(1) == BOOT_LDR_FLASH);
        cut_after = 0;
        erases = requests = 0;
        rc = loader_run();
        printf("  resumed: rc %d, %u requests (first at %#x), %u sector erases (%u + %u; %u in one go)\n", rc, requests,
               first_addr, erases, e1, erases, full_erases);
        bad += check("resumed: the install completes", rc == 0 && is_app(newfl));
        bad += check("... from the package's start again, every read again", first_addr == 0 && requests == full_reqs);
        bad += check("... only the sectors not yet written erased", e1 + erases == full_erases);
        bad += check("... and then the record cleared, the new app boots", finished_ok());
    }
    /* cuts everywhere: the host gone after every 3rd request; the loader dead at every flash operation (odd
     * ones halfway through it); each followed by a reset and by a power cycle */
    {
        uint32_t at, fails = 0, step_r = 3, step_o = 1;
        int power;
        memset(&cs, 0, sizeof cs);
        for (power = 0; power < 2; power++) {
            for (at = 1; at <= full_reqs + 4; at += step_r)
                fails += cut_case(0, at, power, fails < 5);
            for (at = 1; at <= full_ops + 2; at += step_o)
                fails += cut_case(1, at, power, fails < 5);
            crash_half = 1;                          /* (the last operation halfway too, whatever its parity: the
                                                      * window is there, once) */
            fails += cut_case(1, last_app_op, power, fails < 5);
            crash_half = -1;
        }
        printf("  %u cut installs: %u half-written, back into the loader %u (RAM record) + %u (flash record),\n"
               "  %u old app whole (cut before anything changed), %u finished before the cut,\n"
               "  %u dead halfway through the very last page (0x4000's first: mounts), %u failed\n",
               cs.cases, cs.mixed, cs.ldr_ram, cs.ldr_flash, cs.app_old, cs.done_first, cs.window, fails);
        bad += check("cut anywhere: a half-written app never mounts", fails == 0 && cs.mixed > 0);
        bad += check("... reset: back into the loader by the RAM record (F2)", fails == 0 && cs.ldr_ram > 0);
        bad += check("... power cycle: back into the loader by the flash record (F1)", fails == 0 && cs.ldr_flash > 0);
        bad += check("... resumed: byte-identical new app, nothing armed", fails == 0);
        bad += check("... unprotected: only halfway through the last page", cs.window > 0);
    }
    /* F2's counter: a loader that dies at every start comes back through the RAM record 3 times, then the
     * 4th start clears it (the loader before F2): with the old app whole, the app runs */
    {
        uint32_t starts = 0;
        int b;
        device_fresh(old);
        while ((b = spl(0)) == BOOT_LDR_RAM && starts < 50) {
            ldr_arm();                               /* ... and dies */
            starts++;
        }
        printf("  a loader dying at every start: %u starts, then %s\n", starts, BOOT_NAME[b]);
        bad += check("F2: a crash loop stops after 3 re-armed starts (4 in all)", starts == 4 && b == BOOT_APP);
        /* the same with 0x4000 erased (a write cut): the RAM record stops too; the SPL then still runs the loader
         * from the flash record [ASSUMED rule 3] (the loader answers; a power cycle counts from 1 again) */
        device_fresh(old);
        memset(nor + 0x4000, 0xFF, 0x1000);
        starts = 0;
        while ((b = spl(0)) == BOOT_LDR_RAM && starts < 50) { ldr_arm(); starts++; }
        bad += check("... with 0x4000 erased: 4 starts by RAM, then the flash record", starts == 4 && b == BOOT_LDR_FLASH);
        ldr_arm();
        bad += check("... which no longer arms the RAM record", !ldr_rec_ok(ram_rec) && ram_cnt[1] == 5);
        rc = loader_run();
        bad += check("... and still installs when the host resumes", rc == 0 && finished_ok());
        /* a reset that is no crash loop: two crashes, then a resume that finishes */
        device_fresh(old);
        crash_at = 40;
        rc = loader_run();
        crash_at = 0;
        b = spl(0);
        crash_at = 400;
        if (b == BOOT_LDR_RAM) rc = loader_run();
        crash_at = 0;
        b = spl(0);
        if (b == BOOT_LDR_RAM) rc = loader_run();
        bad += check("two crashes mid-write, then the resume finishes", b == BOOT_LDR_RAM && rc == 0 && finished_ok());
        bad += check("... the counter cleared at the end", ram_cnt[0] == 0 && ram_cnt[1] == 0);
    }
    (void)i;
    printf("%s\n", bad ? "LOADER TEST FAILED" : "loader test passed");
    return bad != 0;
}
