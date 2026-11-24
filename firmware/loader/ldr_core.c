/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Felucca update loader, the portable part (step 2 of the M-UPGRADE update
 * protocol).
 *
 * The host serves the package ("logical image") with cmd 0x30 reads (ota.c).
 * The loader:
 *   1. reads the UFW header and finds flash.bin;
 *   2. checks that the package's app area decrypts with THIS chip's key
 *      (taken from isd_config.ini in the device's own flash head), so a
 *      package for another key is refused before anything is erased;
 *   3. writes ONLY the app area [0x4000, 0x93000), sector by sector, skipping
 *      sectors that are already equal, and verifies each one; the flash head
 *      (SPL, isd_config) is never written. The app's first sector (0x4000,
 *      what the SPL mounts) is erased before any other sector changes and
 *      written last (F1);
 *   4. asks 0xF0000000 ("success"), then invalidates the update record (RAM
 *      and flash) so the SPL boots the new app, and resets.
 * A session cut during 3 (the host silent for 3 x 5 s): it returns, the
 * loader waits for a new handshake + upgrade command, and the new session
 * reads the whole package again and only erases/writes what still differs
 * (tests/ldr_test.c; the installers' resume). A reset or power loss during 3
 * (build/design/REPORT-ldrfix.txt): the app area has no valid start, so it
 * doesn't mount and the SPL runs the loader again from the flash record
 * (F1); a crash or watchdog reset of the loader comes back through the RAM
 * record, which the loader keeps armed while it works (F2, ldr_arm). Before
 * F1 a half-written app mounted, ran, and DUB-1's boot guard ended in the
 * ROM's UBOOT (build/design/REPORT-installer.md).
 *
 * Hooks from the platform (loader.c, tests/ldr_test.c):
 *   ldr_fread(off, p, n)  ldr_erase(off)  ldr_prog(off, p, n)   flash, 0 = ok
 *   ldr_record_clear()    forget the RAM update record
 *   ldr_record_set(r)     arm the RAM update record (112 bytes)
 *   ldr_count()           2 words of RAM that survive a warm reset (the start counter)
 *   ldr_progress(done, total)
 * plus the ota.c hooks (frames, time, idle). */
#define LDR_APP_LO 0x4000u
#define LDR_APP_HI 0x93000u
#define LDR_REC_LO 0x93000u                     /* update records live above the app ... */
#define LDR_REC_HI 0xFC000u                     /* ... and below Felucca's globals */

static int ldr_fread(uint32_t off, void *p, uint32_t n);
static int ldr_erase(uint32_t off);
static int ldr_prog(uint32_t off, const void *p, uint32_t n);
static void ldr_record_clear(void);
static void ldr_record_set(const uint8_t *r);
static volatile uint32_t *ldr_count(void);
static void ldr_progress(uint32_t done, uint32_t total);

/* SFC cipher: per 32-byte block, key ^ (block offset >> 2) */
static void ldr_sfc(uint8_t *p, uint32_t n, uint32_t base_off, uint32_t key)
{
    uint32_t i, j;
    for (i = 0; i < n; i += 32u) {
        uint32_t k = key ^ ((base_off + i) >> 2);
        for (j = 0; j < 32u && i + j < n; j++) {
            p[i + j] ^= (uint8_t)k;
            k = ((k << 1) ^ (k & 0x8000u ? 0x1021u : 0u)) & 0xFFFFu;
        }
    }
}

/* chip key from the device's flash head: JLFS top entries (ENC 0xFFFF) -> isd_config.ini blob */
static int ldr_chip_key(uint32_t *key)
{
    uint8_t e[32], blob[34];
    uint32_t off, i, sum, k;
    for (off = 32; off < 0x400u; off += 32u) {
        if (ldr_fread(off, e, 32))
            return -1;
        ota_jl_enc(e, 32);
        if (ota_crc16(e + 2, 30, 0) != ota_rd16(e))
            return -2;
        if (!ota_memeq(e + 16, (const uint8_t *)"isd_config.ini", 15)) {
            if (ota_rd16(e + 14))
                return -3;                               /* last entry, not found */
            continue;
        }
        if (ldr_fread(ota_rd32(e + 4), blob, 34) || ota_crc16(blob, 32, 0) != ota_rd16(blob + 32))
            return -4;
        for (i = 0, sum = 0; i < 16u; i++)
            sum += blob[i];
        sum &= 0xFFu;
        sum = sum >= 0xE0u ? 0xAAu : sum <= 0x10u ? 0x55u : sum;
        for (i = 0, k = 0; i < 16u; i++)
            if ((uint32_t)(blob[16 + i] ^ blob[15 - i]) < sum)
                k |= 1u << i;
        *key = k;
        return 0;
    }
    return -3;
}

static int ldr_rec_ok(const uint8_t *r)          /* a valid update record (ota.c's layout) */
{
    return ota_rd16(r + 6) == 0x5441u && ota_rd16(r) && ota_rd16(r) == ota_crc16(r + 2, 78, 0);
}

/* F2. Every start (ldr_cstart), counted first: the RAM record armed again from the flash copy, so a crash
 * or a watchdog reset while writing warm-resets back into the loader. From the 4th start without a power
 * cycle or a finished update in between: cleared instead (the loader before F2), so a loader that keeps
 * crashing doesn't come back through the RAM record for ever. */
#define LDR_ARM_MAGIC 0x31524C46u                   /* "FLR1" */
#define LDR_ARM_TRIES 3u
static uint32_t ldr_arm(void)
{
    volatile uint32_t *c = ldr_count();
    uint8_t r[112];
    uint32_t n = c[0] == LDR_ARM_MAGIC && c[1] < 0x100u ? c[1] + (c[1] < 0xFFu) : 1u;
    c[1] = n;                                       /* (RAM after a power cycle: no magic, 1) */
    c[0] = LDR_ARM_MAGIC;
    if (n <= LDR_ARM_TRIES && !ldr_fread(OTA_RES, r, sizeof r) && ldr_rec_ok(r))
        ldr_record_set(r);
    else
        ldr_record_clear();
    return n;
}
static void ldr_disarm(void)                        /* finished: the SPL boots the app */
{
    volatile uint32_t *c = ldr_count();
    ldr_record_clear();
    c[0] = 0;
    c[1] = 0;
}

static int ldr_blank(const uint8_t *p)
{
    uint32_t i;
    for (i = 0; i < 0x1000u; i++)
        if (p[i] != 0xFFu)
            return 0;
    return 1;
}
/* F1: the app's first sector erased (unless blank already) before any other sector changes */
static int ldr_hold(uint8_t *cur)
{
    if (ldr_fread(LDR_APP_LO, cur, 0x1000u))
        return -8;
    if (!ldr_blank(cur) && (ldr_erase(LDR_APP_LO) || ldr_fread(LDR_APP_LO, cur, 0x1000u)))
        return -9;
    return ldr_blank(cur) ? 0 : -9;
}
/* one sector: erase, program (the last page first: 0x4000's first page, the app's start, is the very last
 * write of an update), read back; twice at most */
static int ldr_put(uint32_t s, const uint8_t *src, uint8_t *cur)
{
    uint32_t tries, k;
    for (tries = 0; tries < 2u; tries++) {
        if (ldr_erase(s))
            return -9;
        for (k = 0x1000u; k;) {
            k -= 256u;
            if (ldr_prog(s + k, src + k, 256))
                return -10;
        }
        if (ldr_fread(s, cur, 0x1000u))
            return -8;
        if (ota_memeq(src, cur, 0x1000u))
            return 0;
    }
    return -11;                                     /* verify failed twice */
}

static int ldr_session(void)
{
    static uint8_t hdr[0x400], sec[0x1000], cur[0x1000], first[0x1000];
    uint32_t i, k, fl_off, fl_size, ota_off, ota_len, key, s, held = 0, done = 0;
    int rc;
    /* 1. UFW header + entry list (ota.c) */
    if ((rc = ota_ufw(hdr, &fl_off, &fl_size, &ota_off, &ota_len)) != 0)
        return rc;
    if (!fl_off || fl_size < LDR_APP_HI)
        return -3;
    /* 2. the package's app area must decrypt with this chip's key */
    if ((rc = ldr_chip_key(&key)) != 0)
        return -40 + rc;
    if (ota_read(fl_off + LDR_APP_LO, sec, 32))
        return -5;
    ldr_sfc(sec, 32, 0, key);
    if (ota_crc16(sec + 2, 30, 0) != ota_rd16(sec))
        return -6;                                       /* package for another chip key */
    /* 3. app area, sector by sector, in the package's order (the host's reads unchanged); the first
     * sector kept in `first`, erased as soon as anything differs (held), written after all the rest */
    for (s = LDR_APP_LO; s < LDR_APP_HI; s += 0x1000u) {
        uint8_t *b = s == LDR_APP_LO ? first : sec;
        for (k = 0; k < 0x1000u; k += 512u)
            if (ota_read(fl_off + s + k, b + k, 512))
                return -7;
        if (ldr_fread(s, cur, 0x1000u))
            return -8;
        if (!ota_memeq(b, cur, 0x1000u)) {
            if (!held && (rc = ldr_hold(cur)) != 0)
                return rc;
            held = 1;
            if (s != LDR_APP_LO && (rc = ldr_put(s, sec, cur)) != 0)
                return rc;
        }
        done += 0x1000u;
        ldr_progress(done, LDR_APP_HI - LDR_APP_LO);
    }
    if (held && (rc = ldr_put(LDR_APP_LO, first, cur)) != 0)    /* last: from here the app mounts */
        return rc;
    /* 4. finish: the host confirms, the record goes, the SPL boots the app */
    for (i = 0; i < 4u; i++)
        if (!ota_read(0xF0000000u, sec, 8))
            break;
    ldr_disarm();
    for (s = LDR_REC_HI; s > LDR_REC_LO; s -= 0x1000u) {   /* flash records: 4K boundary - 256 */
        uint8_t r[80];
        if (ldr_fread(s - 0x100u, r, 80))
            break;
        if (ldr_rec_ok(r))
            ldr_erase(s - 0x1000u);
    }
    return 0;
}
