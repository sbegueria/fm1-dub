/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Host test of firmware/src/storage.c against a simulated NOR flash:
 * erase -> 0xFF, program can only clear bits, page writes must not wrap.
 * (1.5.0) the frame store too: a power cut after every erase and every program of a save (the op cut torn: an erase
 * half done, a page's bits half cleared; nothing after it), then a reboot: the version before, or the new one only
 * once its header was written; CRC rot; the rotation and its wear; a save while playing (no erase); thousands of
 * random operations against a model. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static uint8_t nor[0x100000];
static int fail_after = -1;            /* torn-write injection: stop after N programs */
static long ops, cut_at = -1;          /* (1.5.0) erases + programs so far; the one a power cut lands on (-1 none) */
static int dead;                       /* the power is off: every write fails, nothing changes (until "reboot") */
static uint32_t erases[0x100];         /* erases per sector */
static uint32_t rng = 12345u;
static uint32_t rnd(void)
{
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return rng;
}

static int st_read(uint32_t off, void *dst, uint32_t n) { memcpy(dst, nor + off, n); return 0; }
static int cut_now(void)               /* this op is the one the power cut lands on: 1 (torn), and dead after it */
{
    if (dead)
        return -1;
    ops++;
    if (cut_at >= 0 && ops == cut_at) {
        dead = 1;
        return 1;
    }
    return 0;
}
static int st_erase(uint32_t off)
{
    int c = cut_now();
    uint32_t i;
    if (off & 0xFFFu) {
        printf("erase not on a sector at %#x\n", off);
        exit(1);
    }
    if (c < 0)
        return -9;
    erases[off >> 12]++;
    if (c) {                           /* (an erase cut short: some bits back to 1, not all) */
        for (i = 0; i < 4096u; i++)
            nor[off + i] |= (uint8_t)rnd();
        return -9;
    }
    memset(nor + off, 0xFF, 4096);
    return 0;
}
static int st_prog(uint32_t off, const void *src, uint32_t n)
{
    const uint8_t *s = src;
    uint32_t i;
    int c;
    if (fail_after == 0)
        return -9;
    if (fail_after > 0)
        fail_after--;
    if ((off & 0xFFu) + n > 256u) {
        printf("page wrap at %#x\n", off);
        exit(1);
    }
    if ((c = cut_now()) < 0)
        return -9;
    for (i = 0; i < n; i++)
        nor[off + i] &= c ? (uint8_t)(s[i] | rnd()) : s[i];   /* (cut: some of the bits cleared, not all) */
    return c ? -9 : 0;
}
#include "../firmware/src/app/plat.h"     /* AMB-1: the object map */
#include "../firmware/src/storage.c"
#define OBJ_PROJECT0 OBJ_PROJ                  /* AMB-1 has two objects: the project and the panel */
#define OBJ_SETTINGS OBJ_PANEL

static int check(const char *what, int ok)
{
    printf("%-58s %s\n", what, ok ? "ok" : "FAIL");
    return ok ? 0 : 1;
}

/* ---- the frame store's checks: a version's bytes from (seed, length) ---- */
static uint8_t vbyte(uint32_t seed, uint32_t i) { return (uint8_t)((seed * 2654435761u + i * 40503u) >> 13 ^ i); }
static int save_v(uint32_t kind, uint32_t index, uint32_t seed, uint32_t len, int may_erase)
{
    uint8_t b[300];
    uint32_t at = 0;
    int rc = fs_begin(kind, index, len, may_erase);
    if (rc)
        return rc;
    while (at < len) {                         /* (in uneven pieces, as an encoder writes) */
        uint32_t k = 1u + rnd() % 300u, i;
        if (k > len - at)
            k = len - at;
        for (i = 0; i < k; i++)
            b[i] = vbyte(seed, at + i);
        if ((rc = fs_put(b, k)) != 0) {
            fs_abort();
            return rc;
        }
        at += k;
    }
    return fs_commit();
}
static int holds(int f, uint32_t seed, uint32_t len)   /* frame f's payload is that version: 1 */
{
    uint8_t b[256];
    uint32_t at, i;
    for (at = 0; at < len; at += 256u) {
        const uint32_t k = len - at > 256u ? 256u : len - at;
        if (fs_read(f, at, b, k))
            return 0;
        for (i = 0; i < k; i++)
            if (b[i] != vbyte(seed, at + i))
                return 0;
    }
    return 1;
}
static int current_is(uint32_t kind, uint32_t index, uint32_t seed, uint32_t len)
{
    uint32_t n;
    const int f = fs_find(kind, index, 0, &n);
    return f >= 0 && n == len && holds(f, seed, len);
}
static void reboot(void)
{
    dead = 0;
    cut_at = -1;
    fs.scanned = 0;
    fs_scan();
}

/* a model of what each object holds: its versions (seed, length), the newest last */
#define MOBJ 9                                   /* WORK and eight sets */
static struct {
    uint32_t n, seed[64], len[64];
} model[MOBJ];
static uint32_t mkind(int o) { return o == 0 ? FS_WORK : FS_SET; }
static uint32_t mindex(int o) { return o == 0 ? 0u : (uint32_t)(o - 1); }
static void mpush(int o, uint32_t seed, uint32_t len)
{
    if (model[o].n == 64) {
        memmove(model[o].seed, model[o].seed + 1, 63 * 4);
        memmove(model[o].len, model[o].len + 1, 63 * 4);
        model[o].n--;
    }
    model[o].seed[model[o].n] = seed, model[o].len[model[o].n] = len, model[o].n++;
}
static int mcheck(void)                          /* every object as the model says: 1 */
{
    int o;
    uint32_t n;
    for (o = 0; o < MOBJ; o++) {
        if (model[o].n == 0) {
            if (fs_find(mkind(o), mindex(o), 0, &n) >= 0)
                return 0;
        } else if (!current_is(mkind(o), mindex(o), model[o].seed[model[o].n - 1], model[o].len[model[o].n - 1])) {
            return 0;
        }
    }
    return 1;
}

static int frame_store_tests(void)
{
    int bad = 0, f, i;
    uint32_t n, k, seed = 100;
    memset(nor, 0xFF, sizeof nor);
    memset(erases, 0, sizeof erases);
    reboot();
    bad += check("fs: an empty store has no WORK", fs_find(FS_WORK, 0, 0, &n) < 0);
    bad += check("fs: the frames fill 0x97000..0xDAFFF, clear of the panel", fs_base(FS_N - 1) + FS_SIZE == 0xDB000u &&
                 FS_BASE == 0x97000u && 0xDB000u <= st_sector(OBJ_PANEL, 0));
    bad += check("fs: save WORK v1 (5,000 B)", save_v(FS_WORK, 0, 1, 5000, 1) == 0);
    bad += check("fs: ... loads it", current_is(FS_WORK, 0, 1, 5000));
    bad += check("fs: ... erased only the 2 sectors it needs", erases[0x97] + erases[0x98] + erases[0x99] + erases[0x9A] <= 2);
    bad += check("fs: save WORK v2 (16,128 B, a whole frame)", save_v(FS_WORK, 0, 2, FS_MAX, 1) == 0);
    bad += check("fs: ... the newer loads", current_is(FS_WORK, 0, 2, FS_MAX));
    f = fs_find(FS_WORK, 0, 1, &n);
    bad += check("fs: ... the one before stays readable (nth 1)", f >= 0 && n == 5000 && holds(f, 1, 5000));
    bad += check("fs: too long refused", fs_begin(FS_WORK, 0, FS_MAX + 1, 1) != 0);
    bad += check("fs: a set beside it", save_v(FS_SET, 3, 3, 777, 1) == 0 && current_is(FS_SET, 3, 3, 777) &&
                 current_is(FS_WORK, 0, 2, FS_MAX));
    reboot();
    bad += check("fs: after a reboot, both as they were", current_is(FS_SET, 3, 3, 777) && current_is(FS_WORK, 0, 2, FS_MAX));

    /* a power cut on every erase and every program of a save, at two moments: frames still erased (no erase), and
     * once every frame holds a version (erases needed) */
    {
        static uint8_t snap[FS_N * FS_SIZE];
        int round, allok = 1, cuts = 0;
        for (round = 0; round < 2; round++) {
            long total;
            if (round == 1)
                for (i = 0; i < 40; i++)
                    save_v(FS_WORK, 0, 1000u + (uint32_t)i, 3000u + (uint32_t)i * 300u, 1);
            reboot();
            fs_find(FS_WORK, 0, 0, &n);
            memcpy(snap, nor + FS_BASE, sizeof snap);
            k = n;                               /* (the current version's length; its seed below) */
            seed = round ? 1039u : 2u;
            ops = 0;
            save_v(FS_WORK, 0, 77, 9000, 1);     /* (the save as it goes uncut: its erases and programs counted) */
            total = ops;
            for (i = 1; i <= total; i++) {
                memcpy(nor + FS_BASE, snap, sizeof snap);
                reboot();
                ops = 0;
                cut_at = i;
                save_v(FS_WORK, 0, 77, 9000, 1);
                reboot();
                cuts++;
                if (!(current_is(FS_WORK, 0, seed, k) || (i == total && current_is(FS_WORK, 0, 77, 9000))))
                    allok = 0, printf("  cut at op %d of %ld: neither the old version nor (header written) the new\n", i, total);
                if (!current_is(FS_SET, 3, 3, 777))
                    allok = 0, printf("  cut at op %d: the set lost\n", i);
                if (save_v(FS_WORK, 0, 88, 4000, 1) != 0 || !current_is(FS_WORK, 0, 88, 4000))
                    allok = 0, printf("  cut at op %d: the next save failed\n", i);
            }
            memcpy(nor + FS_BASE, snap, sizeof snap);
            reboot();
            printf("  (round %d: a save of %ld erases and programs, cut at each)\n", round, total);
        }
        bad += check("fs: a power cut at every erase and program: old version, or new once committed", allok && cuts > 40);
    }

    /* CRC rot */
    reboot();
    bad += check("fs: save v10, v11", save_v(FS_WORK, 0, 10, 6000, 1) == 0 && save_v(FS_WORK, 0, 11, 7000, 1) == 0);
    f = fs_find(FS_WORK, 0, 0, &n);
    nor[fs_base((uint32_t)f) + ST_PAYLOAD_OFF + 4321] ^= 0x08;
    bad += check("fs: rot in the newest payload: the one before loads", current_is(FS_WORK, 0, 10, 6000));
    reboot();
    bad += check("fs: ... after a reboot too", current_is(FS_WORK, 0, 10, 6000));
    bad += check("fs: ... a save over it", save_v(FS_WORK, 0, 12, 100, 1) == 0 && current_is(FS_WORK, 0, 12, 100));
    f = fs_find(FS_WORK, 0, 0, &n);
    nor[fs_base((uint32_t)f) + 12] ^= 0x40;     /* (the header's len) */
    reboot();
    bad += check("fs: a broken header: the version before", current_is(FS_WORK, 0, 10, 6000));
    nor[fs_base((uint32_t)f) + 12] ^= 0x40;
    reboot();
    bad += check("fs: ... mended, the newest again", current_is(FS_WORK, 0, 12, 100));

    /* the rotation, and its wear */
    memset(nor, 0xFF, sizeof nor);
    memset(erases, 0, sizeof erases);
    reboot();
    {
        int ok = 1;
        uint32_t lo = 0xFFFFFFFFu, hi = 0, s0;
        for (i = 0; i < 8; i++)
            ok &= save_v(FS_SET, (uint32_t)i, 500u + (uint32_t)i, 8000, 1) == 0;
        for (i = 0; i < 1700; i++) {
            ok &= save_v(FS_WORK, 0, 3000u + (uint32_t)i, 4000u + (uint32_t)(i % 7) * 1000u, 1) == 0;
            if (i % 97 == 0)
                reboot();
        }
        for (i = 0; i < 8; i++)
            ok &= current_is(FS_SET, (uint32_t)i, 500u + (uint32_t)i, 8000);
        ok &= current_is(FS_WORK, 0, 3000u + 1699u, 4000u + (1699u % 7u) * 1000u);
        bad += check("fs: 1,700 WORK saves beside 8 sets: all as saved", ok);
        for (s0 = 0; s0 < FS_N; s0++) {           /* (each frame's first sector: one erase a version) */
            uint32_t e = erases[(fs_base(s0) >> 12)];
            lo = e < lo ? e : lo, hi = e > hi ? e : hi;
        }
        printf("  (first-sector erases per frame: %u..%u over %u saves)\n", lo, hi, 1700u + 8u);
        bad += check("fs: the wear spread over the 9 frames the sets leave", hi <= 1700u / 8u + 2u);
        f = fs_find(FS_WORK, 0, 1, &n);
        bad += check("fs: the version before the last still there", f >= 0 && holds(f, 3000u + 1698u, n));
    }

    /* a save while playing: no erase; after fs_prepare (stopped), two saves with no erase, then BUSY */
    memset(erases, 0, sizeof erases);
    reboot();
    {
        uint32_t e0, s;
        int rc;
        rc = save_v(FS_WORK, 0, 9, 5000, 0);
        bad += check("fs: playing, nothing erased ahead: BUSY, nothing written", rc == FS_BUSY && current_is(FS_WORK, 0, 3000u + 1699u, 4000u + (1699u % 7u) * 1000u));
        bad += check("fs: prepare (stopped) erases two frames ahead", fs_prepare() > 0 && fs_prepare() == 0);
        for (e0 = 0, s = 0; s < 0x100; s++)
            e0 += erases[s];
        rc = save_v(FS_WORK, 0, 21, 16000, 0) | save_v(FS_WORK, 0, 22, 300, 0);
        for (s = 0; s < 0x100; s++)
            e0 -= erases[s];
        bad += check("fs: ... then two saves while playing, no erase", rc == 0 && e0 == 0 && current_is(FS_WORK, 0, 22, 300));
        bad += check("fs: ... a third: BUSY", save_v(FS_WORK, 0, 23, 300, 0) == FS_BUSY && current_is(FS_WORK, 0, 22, 300));
        reboot();
        bad += check("fs: ... after a reboot, prepare again, a save while playing", fs_prepare() >= 0 &&
                     save_v(FS_WORK, 0, 24, 300, 0) == 0 && current_is(FS_WORK, 0, 24, 300));
    }

    /* delete */
    bad += check("fs: delete set 5", fs_delete(FS_SET, 5) == 0 && fs_find(FS_SET, 5, 0, &n) < 0);
    reboot();
    bad += check("fs: ... gone after a reboot, the others kept", fs_find(FS_SET, 5, 0, &n) < 0 &&
                 current_is(FS_SET, 4, 504, 8000) && current_is(FS_WORK, 0, 24, 300));
    bad += check("fs: an abort leaves the frame free and the object as it was",
                 fs_begin(FS_WORK, 0, 1000, 1) == 0 && fs_put("abc", 3) == 0 && (fs_abort(), current_is(FS_WORK, 0, 24, 300)));
    bad += check("fs: more bytes than begun: refused", fs_begin(FS_WORK, 0, 2, 1) == 0 && fs_put("abc", 3) != 0 &&
                 fs_commit() != 0 && current_is(FS_WORK, 0, 24, 300));
    bad += check("fs: fewer bytes than begun: not committed", fs_begin(FS_WORK, 0, 9, 1) == 0 && fs_put("abc", 3) == 0 &&
                 fs_commit() != 0 && current_is(FS_WORK, 0, 24, 300));
    {   /* (1.5.0, step 4) a set sent in: finished (read back), looked at while headerless, then sealed or dropped */
        uint8_t b[300];
        uint32_t i;
        int f, ok;
        for (i = 0; i < 300u; i++)
            b[i] = vbyte(77, i);
        ok = fs_begin(FS_SET, 12, 300, 1) == 0 && fs_put(b, 300) == 0 && (f = fs_finish()) >= 0 && fs_open_frame() == f &&
             holds(f, 77, 300) && fs_find(FS_SET, 12, 0, &n) < 0 && fs_put(b, 1) != 0 && fs_prepare() == 0 &&
             fs_open_frame() == f;
        bad += check("fs: finish: read back, open, headerless, prepare waits", ok);
        reboot();
        bad += check("fs: ... a reboot before the seal: not there", fs_find(FS_SET, 12, 0, &n) < 0);
        ok = fs_begin(FS_SET, 12, 300, 1) == 0 && fs_put(b, 300) == 0 && fs_finish() >= 0 && fs_seal() == 0 &&
             fs_open_frame() < 0 && current_is(FS_SET, 12, 77, 300);
        reboot();
        bad += check("fs: ... sealed: the newest, after a reboot too", ok && current_is(FS_SET, 12, 77, 300));
        bad += check("fs: ... a seal without a finish refused", fs_begin(FS_SET, 12, 3, 1) == 0 && fs_put("abc", 3) == 0 &&
                     fs_seal() != 0 && (fs_abort(), current_is(FS_SET, 12, 77, 300)));
        bad += check("fs: ... a finish short of its bytes refused, the frame dropped", fs_begin(FS_SET, 12, 9, 1) == 0 &&
                     fs_put("abc", 3) == 0 && fs_finish() < 0 && fs_open_frame() < 0 && current_is(FS_SET, 12, 77, 300));
        bad += check("fs: ... delete it", fs_delete(FS_SET, 12) == 0 && fs_find(FS_SET, 12, 0, &n) < 0);
    }

    /* thousands of random operations against the model: saves (stopped or playing), cuts at a random op of a save,
     * prepares, deletes, aborts, reboots */
    memset(nor, 0xFF, sizeof nor);
    memset(model, 0, sizeof model);
    reboot();
    {
        int ok = 1, step, nsaves = 0, ncuts = 0, nbusy = 0;
        for (step = 0; step < 6000 && ok; step++) {
            const uint32_t r = rnd() % 100u;
            const int o = (int)(rnd() % MOBJ);
            if (r < 55) {                        /* a save */
                const uint32_t len = 1u + (rnd() % 4 == 0 ? rnd() % FS_MAX : rnd() % 6000u), s = rnd();
                const int may = rnd() % 4 != 0;
                const int rc = save_v(mkind(o), mindex(o), s, len, may);
                if (rc == 0)
                    mpush(o, s, len), nsaves++;
                else if (rc == FS_BUSY && !may)
                    nbusy++;
                else
                    ok = 0, printf("  step %d: a save failed (%d)\n", step, rc);
            } else if (r < 70) {                 /* a save cut by a power loss at a random op, then a reboot */
                const uint32_t len = 1u + rnd() % 9000u, s = rnd();
                long total;
                static uint8_t snap[FS_N * FS_SIZE];
                memcpy(snap, nor + FS_BASE, sizeof snap);
                ops = 0;
                {                                /* (its op count, uncut, on a copy of the state) */
                    const uint32_t rs = rng;
                    save_v(mkind(o), mindex(o), s, len, 1);
                    rng = rs;
                }
                total = ops;
                memcpy(nor + FS_BASE, snap, sizeof snap);
                reboot();
                ops = 0;
                cut_at = 1 + (long)(rnd() % (uint32_t)total);
                save_v(mkind(o), mindex(o), s, len, 1);
                {
                    const long at = cut_at;
                    reboot();
                    if (at == total && current_is(mkind(o), mindex(o), s, len))
                        mpush(o, s, len);        /* (cut on the header itself, and it held) */
                }
                ncuts++;
            } else if (r < 78) {
                if (fs_prepare() < 0)
                    ok = 0, printf("  step %d: prepare failed\n", step);
            } else if (r < 83) {
                if (fs_delete(mkind(o), mindex(o)) != 0)
                    ok = 0;
                model[o].n = 0;
            } else if (r < 88) {
                if (fs_begin(mkind(o), mindex(o), 1u + rnd() % 3000u, 1) == 0) {
                    fs_put("partial", 7);
                    fs_abort();
                }
            } else {
                reboot();
            }
            if (!mcheck())
                ok = 0, printf("  step %d (op %u): an object not as the model has it\n", step, r);
        }
        printf("  (%d steps: %d saves, %d cut short, %d refused while playing)\n", step, nsaves, ncuts, nbusy);
        bad += check("fs: 6,000 random operations, every object as the model", ok);
    }
    return bad;
}

int main(void)
{
    char a[600], b[600], got[600];
    int bad = 0, n;
    memset(nor, 0xFF, sizeof nor);
    memset(a, 'A', sizeof a);
    memset(b, 'B', sizeof b);
    bad += check("empty flash loads nothing", st_load(OBJ_PROJECT0, got, sizeof got) < 0);
    bad += check("save A", st_save(OBJ_PROJECT0, a, sizeof a) == 0);
    n = st_load(OBJ_PROJECT0, got, sizeof got);
    bad += check("load returns A", n == (int)sizeof a && !memcmp(got, a, sizeof a));
    bad += check("save B", st_save(OBJ_PROJECT0, b, sizeof b) == 0);
    n = st_load(OBJ_PROJECT0, got, sizeof got);
    bad += check("load returns B (newer seq)", n == (int)sizeof b && !memcmp(got, b, sizeof b));
    bad += check("(1.5.0) its seq: 2", st_seq(OBJ_PROJECT0) == 2u && st_seq(OBJ_PANEL) == 0u);
    fail_after = 1;                                  /* payload page 1 written, the rest torn */
    st_save(OBJ_PROJECT0, a, sizeof a);
    fail_after = -1;
    n = st_load(OBJ_PROJECT0, got, sizeof got);
    bad += check("torn save keeps B", n == (int)sizeof b && !memcmp(got, b, sizeof b));
    fail_after = 3;                                  /* all payload pages, header torn */
    st_save(OBJ_PROJECT0, a, sizeof a);
    fail_after = -1;
    n = st_load(OBJ_PROJECT0, got, sizeof got);
    bad += check("header not written keeps B", n == (int)sizeof b && !memcmp(got, b, sizeof b));
    bad += check("save A again", st_save(OBJ_PROJECT0, a, sizeof a) == 0);
    n = st_load(OBJ_PROJECT0, got, sizeof got);
    bad += check("load returns A", n == (int)sizeof a && !memcmp(got, a, sizeof a));
    n = st_load(OBJ_PROJECT0, got, 100);
    bad += check("(1.5.0) a load shorter than the payload: its start, CRC whole", n == 100 && !memcmp(got, a, 100));
    nor[st_sector(OBJ_PROJECT0, 0) + ST_PAYLOAD_OFF + 10] ^= 0x01;   /* bit rot in one copy */
    nor[st_sector(OBJ_PROJECT0, 1) + ST_PAYLOAD_OFF + 10] ^= 0x01;
    n = st_load(OBJ_PROJECT0, got, sizeof got);
    bad += check("both copies corrupt -> nothing", n < 0);
    bad += check("other objects untouched", st_load(OBJ_PANEL, got, sizeof got) < 0);
    bad += check("settings save/load",
                 st_save(OBJ_SETTINGS, "hello", 5) == 0 && st_load(OBJ_SETTINGS, got, 5) == 5 && !memcmp(got, "hello", 5));
    bad += check("CRC-32 is zlib's (check value 0xCBF43926)", st_crc32("123456789", 9) == 0xCBF43926u);
    memset(nor, 0xFF, sizeof nor);
    st_save(OBJ_PANEL, a, sizeof a);
    st_save(OBJ_PANEL, b, sizeof b);              /* B is newer, in the other copy */
    {
        st_hdr_t h;
        int cur = st_current(OBJ_PANEL, &h);
        nor[st_sector(OBJ_PANEL, (uint32_t)cur) + ST_PAYLOAD_OFF + 300] ^= 0x10;   /* rot in the newer copy */
    }
    n = st_load(OBJ_PANEL, got, sizeof got);
    bad += check("newer copy rotten -> the older one loads", n == (int)sizeof a && !memcmp(got, a, sizeof a));
    bad += check("save over the rotten copy", st_save(OBJ_PANEL, b, sizeof b) == 0);
    n = st_load(OBJ_PANEL, got, sizeof got);
    bad += check("... loads the new data", n == (int)sizeof b && !memcmp(got, b, sizeof b));
    nor[st_sector(OBJ_PANEL, 0) + 8] ^= 0x01;    /* both headers broken */
    nor[st_sector(OBJ_PANEL, 1) + 8] ^= 0x01;
    bad += check("both headers broken -> nothing", st_load(OBJ_PANEL, got, sizeof got) < 0);
    {   /* AMB-1 map: every object in a data region, none in the sample slots (0xA0000..0xDBFFF), no overlaps */
        uint32_t o, p, ok = 1;
        for (o = 0; o < OBJ_NOBJ; o++) {
            uint32_t a0 = st_sector(o, 0), a1 = st_sector(o, 1) + 4096;
            int in_data = (a0 >= 0x97000 && a1 <= 0xA0000) || (a0 >= 0xDC000 && a1 <= 0xE0000) ||
                          (a0 >= 0xFC000 && a1 <= 0xFF000);
            if (!in_data || a1 - a0 != 8192)
                ok = 0;
            for (p = 0; p < o; p++)
                if (a0 < st_sector(p, 1) + 4096 && st_sector(p, 0) < a1)
                    ok = 0;
        }
        bad += check("objects in data regions, clear of the sample slots", (int)ok);
    }
    bad += frame_store_tests();
    printf("%s\n", bad ? "STORAGE TEST FAILED" : "storage test passed");
    return bad != 0;
}
