/* SPDX-License-Identifier: GPL-3.0-only */
/* (1.5.0) firmware/src/app/container.c alone, on the host: containers made of every chunk (an unknown one too) read
 * back field for field and written again byte for byte; a broken one refused (magic, version, a length, the padding,
 * the CRC, END not last); 200,000 mutated containers read through every reader without a fault (built with ASan and
 * UBSan by run_tests.sh), each refused unless its CRC was mended. And the C third of the twins' test
 * (tests/host/container_twins.sh: tools/dub1_container.py, web/dub1container.js):
 *   container_test                 the tests
 *   container_test emit DIR N      N sample containers, DIR/c000.dub1 ..
 *   container_test dump FILE       the canonical dump (one line a chunk; the twins print the same)
 *   container_test recode IN OUT   read and written again (the same bytes, for a container that reads) */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../firmware/src/app/container.c"

static uint32_t rng = 1u;
static uint32_t rnd(void)
{
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return rng;
}

/* ---- memory in and out ---- */
typedef struct {
    uint8_t *b;
    uint32_t n, cap;
} mem_t;
static int mem_put(void *ctx, const void *p, uint32_t n)
{
    mem_t *m = ctx;
    if (m->n + n > m->cap)
        return -1;
    memcpy(m->b + m->n, p, n);
    m->n += n;
    return 0;
}
static int mem_get(void *ctx, uint32_t off, void *dst, uint32_t n)
{
    memcpy(dst, (const uint8_t *)ctx + off, n);
    return 0;
}

/* ---- a random container: every kind of chunk ---- */
static void rnd_name(char *d)
{
    static const char C[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 -.'&!";
    uint32_t i, n = rnd() % 13u;
    char s[17];
    for (i = 0; i < n; i++)
        s[i] = C[rnd() % (sizeof C - 1)];
    s[n] = 0;
    d1c_name_set(d, s);
}
static void emit_random(d1c_w_t *w, uint32_t total, uint32_t seed)
{
    d1c_hdr_t h;
    d1c_meta_t m;
    d1c_glob_t g;
    d1c_piece_t p;
    int16_t par[600];
    uint8_t pings[200], raw[64];
    uint32_t i, k, np;
    rng = seed * 2654435761u + 7u;
    memset(&h, 0, sizeof h);
    h.total = total, h.format = 8u + rnd() % 40u, h.kind = (uint16_t)(1u + rnd() % 5u), h.flags = rnd() % 4u;
    np = rnd() % 9u;
    h.npieces = (uint16_t)np;
    if (rnd() % 4 == 0)
        h.rsv[0] = rnd(), h.rsv[1] = rnd();
    d1c_header(w, &h);
    if (rnd() % 4) {
        memset(&m, 0, sizeof m);
        rnd_name(m.name);
        d1c_name_set(m.fw, rnd() % 2 ? "1.5.0" : "DEV");
        m.source = rnd() % 3u, m.migrated = rnd() % 3 ? 0u : rnd(), m.saves = rnd() % 1000u;
        d1c_meta(w, &m);
    }
    if (rnd() % 3) {
        memset(&g, 0, sizeof g);
        g.piece = (uint8_t)rnd(), g.leds = (uint8_t)rnd(), g.screen = (uint8_t)rnd(), g.values = (uint8_t)rnd();
        g.trans = (uint8_t)rnd(), g.nsir = (uint8_t)(rnd() % 5u), g.nsk = (uint8_t)(rnd() % 5u), g.setlen = (uint8_t)(rnd() % 2u);
        g.pool = (uint16_t)rnd(), g.npar = (uint16_t)(rnd() % 200u);
        for (i = 0; i < (uint32_t)g.nsir * g.nsk; i++)
            g.sir[i] = (int8_t)rnd();
        for (i = 0; i < g.npar; i++)
            par[i] = (int16_t)rnd();
        d1c_glob(w, &g, par);
    }
    for (k = 0; k < np + (rnd() % 3 == 0); k++) {   /* (now and then a piece more than the header says: shape only) */
        if (rnd() % 5 == 0) {                         /* an unknown chunk, between */
            const uint32_t n = rnd() % 40u;
            for (i = 0; i < n; i++)
                raw[i] = (uint8_t)rnd();
            d1c_raw(w, D1C_ID('X', (char)('A' + rnd() % 26u), 'Y', 'Z'), raw, n);
        }
        memset(&p, 0, sizeof p);
        p.index = (uint8_t)(rnd() % 8u), p.kind = (uint8_t)(rnd() % D1C_NKIND), p.recipe = (uint8_t)(rnd() % 8u);
        p.screen = (uint8_t)rnd(), p.pings_ok = (uint8_t)(rnd() % 2u), p.flags = (uint8_t)rnd();
        p.nparams = (uint16_t)(p.kind == D1C_RECIPE ? 0u : rnd() % 200u), p.pool = (uint16_t)rnd();
        p.npings = (uint16_t)(rnd() % 100u), p.seed = rnd() % 2 ? 0u : rnd();
        rnd_name(p.name);
        for (i = 0; i < p.npings; i++)
            pings[i] = (uint8_t)rnd();
        for (i = 0; i < 2u * p.nparams; i++)
            par[i] = (int16_t)rnd();
        d1c_piece(w, &p, pings, par);
        if (rnd() % 3 == 0) {                         /* (step 7) a cue list, its positions sorted */
            const uint32_t n = 1u + (rnd() % 4 ? rnd() % 12u : rnd() % 64u);
            uint32_t pos = rnd() % 4 ? rnd() % 4096u : rnd() % 65536u;
            d1c_cues_head(w, rnd() % 8u, n, rnd() % 8 ? 0u : rnd() % 65536u);
            for (i = 0; i < n; i++) {
                d1c_cue_t q;
                pos += rnd() % 3 ? rnd() % 40u : 0u;
                q.pos = (uint16_t)(pos > 65535u ? 65535u : pos), q.op = (uint8_t)rnd(), q.arg = (uint8_t)rnd();
                q.val = (int16_t)rnd(), q.len = (uint16_t)(rnd() % 2 ? rnd() % 64u : rnd());
                d1c_cue_put(w, &q);
            }
        }
    }
    d1c_end(w);
}
static uint32_t make_random(uint8_t *b, uint32_t cap, uint32_t seed)
{
    d1c_w_t w;
    mem_t m = {b, 0, cap};
    uint32_t total;
    d1c_w_init(&w, 0, 0);
    emit_random(&w, 0, seed);                     /* (counted) */
    total = w.len;
    d1c_w_init(&w, mem_put, &m);
    emit_random(&w, total, seed);
    return w.err ? 0u : m.n;
}

/* ---- read everything a container has (the readers all run: the fuzz's target), written again into w (0: not) ---- */
static int walk(const uint8_t *b, uint32_t n, d1c_w_t *w, FILE *dump)
{
    d1c_r_t r;
    uint32_t at = 0, id, pay, len, i;
    int rc;
    static int16_t v[2 * 65536];
    static uint8_t raw[1 << 16];
    if ((rc = d1c_open(&r, mem_get, (void *)b, n)) != 0)
        return rc;
    if (w)
        d1c_header(w, &r.h);
    if (dump)
        fprintf(dump, "D1ST version=%u hlen=%u total=%u format=%u kind=%u npieces=%u flags=%u rsv=%u,%u\n", r.h.version,
                r.h.hlen, r.h.total, r.h.format, r.h.kind, r.h.npieces, r.h.flags, r.h.rsv[0], r.h.rsv[1]);
    while (d1c_next(&r, &at, &id, &pay, &len)) {
        if (id == D1C_META) {
            d1c_meta_t m;
            if ((rc = d1c_read_meta(&r, pay, len, &m)) != 0)
                return rc;
            if (w)
                d1c_meta(w, &m);
            if (dump) {
                fprintf(dump, "META name=");
                for (i = 0; i < 16; i++)
                    fprintf(dump, "%02x", (uint8_t)m.name[i]);
                fprintf(dump, " fw=");
                for (i = 0; i < 16; i++)
                    fprintf(dump, "%02x", (uint8_t)m.fw[i]);
                fprintf(dump, " source=%u migrated=%u saves=%u\n", m.source, m.migrated, m.saves);
            }
        } else if (id == D1C_GLOB) {
            d1c_glob_t g;
            if ((rc = d1c_read_glob(&r, pay, len, &g)) != 0 || (rc = d1c_read_i16s(&r, g.par_at, v, g.npar)) != 0)
                return rc;
            if (w)
                d1c_glob(w, &g, v);
            if (dump) {
                fprintf(dump, "GLOB piece=%u leds=%u screen=%u values=%u trans=%u nsir=%u nsk=%u setlen=%u pool=%u npar=%u sir=",
                        g.piece, g.leds, g.screen, g.values, g.trans, g.nsir, g.nsk, g.setlen, g.pool, g.npar);
                for (i = 0; i < (uint32_t)g.nsir * g.nsk; i++)
                    fprintf(dump, "%s%d", i ? "," : "", g.sir[i]);
                fprintf(dump, "%s par=", g.nsir * g.nsk ? "" : "-");
                for (i = 0; i < g.npar; i++)
                    fprintf(dump, "%s%d", i ? "," : "", v[i]);
                fprintf(dump, "%s\n", g.npar ? "" : "-");
            }
        } else if (id == D1C_PIEC) {
            d1c_piece_t p;
            const uint32_t nd = 0;
            (void)nd;
            if ((rc = d1c_read_piece(&r, pay, len, &p)) != 0)
                return rc;
            if (p.npings && r.get(r.ctx, p.pings_at, raw, p.npings))
                return D1C_EIO;
            {
                const uint32_t k = p.kind == D1C_FULL ? p.nparams : p.kind == D1C_DELTA ? 2u * p.nparams : 0u;
                if ((rc = d1c_read_i16s(&r, p.data_at, v, k)) != 0)
                    return rc;
                if (w)
                    d1c_piece(w, &p, raw, v);
                if (dump) {
                    fprintf(dump, "PIEC index=%u kind=%u recipe=%u screen=%u pings_ok=%u flags=%u nparams=%u pool=%u npings=%u "
                            "seed=%u name=", p.index, p.kind, p.recipe, p.screen, p.pings_ok, p.flags, p.nparams, p.pool,
                            p.npings, p.seed);
                    for (i = 0; i < 16; i++)
                        fprintf(dump, "%02x", (uint8_t)p.name[i]);
                    fprintf(dump, " pings=");
                    for (i = 0; i < p.npings; i++)
                        fprintf(dump, "%02x", raw[i]);
                    fprintf(dump, "%s data=", p.npings ? "" : "-");
                    for (i = 0; i < p.nparams && p.kind != D1C_RECIPE; i++) {
                        if (p.kind == D1C_DELTA)
                            fprintf(dump, "%s%u:%d", i ? "," : "", (uint16_t)v[2 * i], v[2 * i + 1]);
                        else
                            fprintf(dump, "%s%d", i ? "," : "", v[i]);
                    }
                    fprintf(dump, "%s\n", p.nparams && p.kind != D1C_RECIPE ? "" : "-");
                }
            }
        } else if (id == D1C_CUES) {          /* (step 7) */
            d1c_cues_t c;
            d1c_cue_t q;
            if ((rc = d1c_read_cues(&r, pay, len, &c)) != 0)
                return rc;
            if (w)
                d1c_cues_head(w, c.index, c.count, c.flags);
            if (dump)
                fprintf(dump, "CUES index=%u count=%u flags=%u cues=", c.index, c.count, c.flags);
            for (i = 0; i < c.count; i++) {
                if ((rc = d1c_read_cue(&r, &c, i, &q)) != 0)
                    return rc;
                if (w)
                    d1c_cue_put(w, &q);
                if (dump)
                    fprintf(dump, "%s%u:%u:%u:%d:%u", i ? "," : "", q.pos, q.op, q.arg, q.val, q.len);
            }
            if (dump)
                fprintf(dump, "\n");
        } else {
            if (len > sizeof raw || (len && r.get(r.ctx, pay, raw, len)))
                return D1C_EIO;
            if (w)
                d1c_raw(w, id, raw, len);
            if (dump) {
                fprintf(dump, "CHNK id=%02x%02x%02x%02x len=%u data=", id & 255u, id >> 8 & 255u, id >> 16 & 255u, id >> 24,
                        len);
                for (i = 0; i < len; i++)
                    fprintf(dump, "%02x", raw[i]);
                fprintf(dump, "%s\n", len ? "" : "-");
            }
        }
    }
    if (w)
        d1c_end(w);
    if (dump)
        fprintf(dump, "END crc=%08x\n", (uint32_t)(b[r.end + 8] | b[r.end + 9] << 8 | b[r.end + 10] << 16 | (uint32_t)b[r.end + 11] << 24));
    return 0;
}

static uint32_t recode(const uint8_t *b, uint32_t n, uint8_t *o, uint32_t cap, int *rc)
{
    d1c_w_t w;
    mem_t m = {o, 0, cap};
    d1c_w_init(&w, mem_put, &m);
    *rc = walk(b, n, &w, 0);
    return *rc || w.err ? 0u : m.n;
}

static uint8_t *slurp(const char *path, uint32_t *n)
{
    FILE *f = fopen(path, "rb");
    uint8_t *b;
    long l;
    if (!f)
        return 0;
    fseek(f, 0, SEEK_END);
    l = ftell(f);
    fseek(f, 0, SEEK_SET);
    b = malloc((size_t)l + 1);
    *n = (uint32_t)fread(b, 1, (size_t)l, f);
    fclose(f);
    return b;
}

static int check(const char *what, int ok)
{
    printf("%-60s %s\n", what, ok ? "ok" : "FAIL");
    return ok ? 0 : 1;
}

static void fix_crc(uint8_t *b, uint32_t n)       /* (END the last 12 bytes: its CRC made right again) */
{
    uint32_t c;
    if (n < 12)
        return;
    c = ~d1c_crc_up(0xFFFFFFFFu, b, n - 12u);
    b[n - 4] = (uint8_t)c, b[n - 3] = (uint8_t)(c >> 8), b[n - 2] = (uint8_t)(c >> 16), b[n - 1] = (uint8_t)(c >> 24);
}

int main(int argc, char **argv)
{
    static uint8_t a[1 << 17], b[1 << 17], m[(1 << 17) + 64];
    uint32_t n, k, i;
    int bad = 0, rc;
    if (argc == 4 && !strcmp(argv[1], "emit")) {
        for (k = 0; k < (uint32_t)atoi(argv[3]); k++) {
            char p[512];
            FILE *f;
            n = make_random(a, sizeof a, 1000u + k);
            snprintf(p, sizeof p, "%s/c%03u.dub1", argv[2], k);
            if (!(f = fopen(p, "wb")) || fwrite(a, 1, n, f) != n)
                return 2;
            fclose(f);
        }
        return 0;
    }
    if (argc == 3 && !strcmp(argv[1], "dump")) {
        uint8_t *f = slurp(argv[2], &n);
        if (!f)
            return 2;
        rc = walk(f, n, 0, stdout);
        if (rc)
            printf("REFUSED %d\n", rc);
        return rc ? 1 : 0;
    }
    if (argc == 4 && !strcmp(argv[1], "recode")) {
        uint8_t *f = slurp(argv[2], &n);
        FILE *o;
        if (!f)
            return 2;
        k = recode(f, n, b, sizeof b, &rc);
        if (rc || !(o = fopen(argv[3], "wb")) || fwrite(b, 1, k, o) != k)
            return 1;
        fclose(o);
        return 0;
    }

    /* the shape: one by hand */
    {
        d1c_w_t w;
        mem_t mm = {a, 0, sizeof a};
        d1c_hdr_t h;
        d1c_meta_t me;
        d1c_glob_t g;
        d1c_piece_t p;
        int16_t par[4] = {1, -2, 300, -32768};
        uint8_t pings[3] = {9, 8, 7};
        d1c_r_t r;
        uint32_t at = 0, id, pay, len, cnt = 0;
        memset(&h, 0, sizeof h), memset(&me, 0, sizeof me), memset(&g, 0, sizeof g), memset(&p, 0, sizeof p);
        h.format = 37, h.kind = 1, h.npieces = 1;
        d1c_name_set(me.name, "Night Set"), d1c_name_set(me.fw, "1.5.0");
        g.npar = 4, g.nsir = 1, g.nsk = 3, g.sir[0] = -5, g.sir[1] = 6, g.sir[2] = 7;
        p.nparams = 4, p.npings = 3, p.index = 5;
        d1c_name_set(p.name, "Fog");
        for (k = 0; k < 2; k++) {
            d1c_w_init(&w, k ? mem_put : 0, &mm);
            h.total = k ? n : 0;
            d1c_header(&w, &h), d1c_meta(&w, &me), d1c_glob(&w, &g, par), d1c_piece(&w, &p, pings, par), d1c_end(&w);
            n = w.len;
        }
        bad += check("by hand: 32 + META 52 + GLOB 8+12+3+8 (+1) + PIEC 8+32+3+8 (+3) + END 12", n == 32 + 52 + 32 + 52 + 12 && mm.n == n);
        bad += check("by hand: it reads", d1c_open(&r, mem_get, a, n) == 0 && r.h.format == 37 && r.h.npieces == 1);
        while (d1c_next(&r, &at, &id, &pay, &len)) {
            cnt++;
            if (id == D1C_PIEC) {
                int16_t v[4];
                uint8_t pg[3];
                bad += check("by hand: the piece's fields", d1c_read_piece(&r, pay, len, &p) == 0 && p.index == 5 &&
                             !strcmp(p.name, "Fog") && d1c_read_i16s(&r, p.data_at, v, 4) == 0 && !memcmp(v, par, 8) &&
                             r.get(r.ctx, p.pings_at, pg, 3) == 0 && !memcmp(pg, pings, 3));
            } else if (id == D1C_GLOB) {
                bad += check("by hand: the globals' fields", d1c_read_glob(&r, pay, len, &g) == 0 && g.npar == 4 && g.sir[0] == -5);
            } else if (id == D1C_META) {
                bad += check("by hand: the name", d1c_read_meta(&r, pay, len, &me) == 0 && !strcmp(me.name, "Night Set"));
            }
        }
        bad += check("by hand: three chunks before END", cnt == 3);
        bad += check("CRC-32 is zlib's", ~d1c_crc_up(0xFFFFFFFFu, "123456789", 9) == 0xCBF43926u);
        memcpy(b, a, n);
        b[0] ^= 1;
        bad += check("refused: magic", d1c_open(&r, mem_get, b, n) == D1C_EMAGIC);
        memcpy(b, a, n);
        b[4] = 2;
        bad += check("refused: a newer container version", d1c_open(&r, mem_get, b, n) == D1C_EVERSION);
        bad += check("refused: cut short", d1c_open(&r, mem_get, a, n - 4) != 0);
        memcpy(b, a, n);
        b[32 + 52 + 8 + 12 + 3 + 8] = 1;          /* (the GLOB's padding byte) */
        fix_crc(b, n);
        bad += check("refused: padding not zero (CRC mended)", d1c_open(&r, mem_get, b, n) == D1C_ESHAPE);
        memcpy(b, a, n);
        b[100] ^= 0x10;
        bad += check("refused: a bit flipped (CRC)", d1c_open(&r, mem_get, b, n) == D1C_ECRC);
        memcpy(b, a, n);
        b[n - 12] = 'X';                          /* (no END) */
        bad += check("refused: no END", d1c_open(&r, mem_get, b, n) != 0);
        memcpy(b, a, n);
        b[32 + 4] = 45;                           /* (META's length 44 -> 45: then END not where it must be) */
        fix_crc(b, n);
        bad += check("refused: a chunk's length wrong (CRC mended)", d1c_open(&r, mem_get, b, n) != 0 ||
                     walk(b, n, 0, 0) != 0);
    }

    /* (step 7) a cue list: its shape (the count its length's, 1 to 64, sorted by pos); what's in a cue is project.c's */
    {
        d1c_w_t w;
        mem_t mm = {a, 0, sizeof a};
        d1c_hdr_t h;
        d1c_cue_t q[2] = {{D1C_POS(1, 16, 0), 6, 0, -30, 0}, {D1C_POS(1, 16, 4), 2, 0, 0, 16}}, g;
        d1c_cues_t c;
        d1c_r_t r;
        uint32_t at = 0, id, pay, len, cues_at = 0;
        memset(&h, 0, sizeof h);
        h.format = 37, h.kind = 2;
        for (k = 0; k < 2; k++) {
            d1c_w_init(&w, k ? mem_put : 0, &mm);
            h.total = k ? n : 0;
            d1c_header(&w, &h), d1c_cues_head(&w, 3, 2, 0), d1c_cue_put(&w, &q[0]), d1c_cue_put(&w, &q[1]), d1c_end(&w);
            n = w.len;
        }
        bad += check("cues by hand: 32 + CUES 8+4+16 + END 12", n == 32 + 28 + 12);
        bad += check("cues by hand: main +16, its 5th semicorchea (16th): pos 0x4104", q[1].pos == 0x4104);
        rc = d1c_open(&r, mem_get, a, n);
        if (!rc && d1c_next(&r, &at, &id, &pay, &len) && id == D1C_CUES && d1c_read_cues(&r, pay, len, &c) == 0)
            cues_at = c.cues_at;
        bad += check("cues by hand: read, their fields", cues_at && c.index == 3 && c.count == 2 &&
                     d1c_read_cue(&r, &c, 1, &g) == 0 && !memcmp(&g, &q[1], sizeof g) && d1c_read_cue(&r, &c, 2, &g) != 0);
        memcpy(b, a, n);                          /* (the two swapped: out of order) */
        memcpy(b + 44, a + 52, 8), memcpy(b + 52, a + 44, 8);
        fix_crc(b, n);
        bad += check("cues refused: out of order (CRC mended)", walk(b, n, 0, 0) == D1C_ESHAPE);
        memcpy(b, a, n);
        b[41] = 3;                                /* (the count 3, the length two's) */
        fix_crc(b, n);
        bad += check("cues refused: a count not its length's", walk(b, n, 0, 0) == D1C_ESHAPE);
        memcpy(b, a, n);
        b[41] = 0, b[36] = 4, b[37] = 0;          /* (none: the length the head's alone, END after it) */
        memmove(b + 44, b + 60, 12);
        b[8] = (uint8_t)(n - 16u);
        fix_crc(b, n - 16u);
        bad += check("cues refused: an empty list", walk(b, n - 16u, 0, 0) == D1C_ESHAPE);
    }

    /* random containers: read back the same, written again the same */
    {
        int ok = 1;
        for (k = 0; k < 2000 && ok; k++) {
            n = make_random(a, sizeof a, k);
            i = recode(a, n, b, sizeof b, &rc);
            if (!n || rc || i != n || memcmp(a, b, n))
                ok = 0, printf("  sample %u: n %u rc %d recoded %u\n", k, n, rc, i);
        }
        bad += check("2,000 random containers: read, written again, the same bytes", ok);
    }

    /* the fuzz: mutated, read through every reader; refused unless the CRC was mended (or nothing changed) */
    {
        int ok = 1;
        uint32_t refused = 0, read = 0;
        rng = 99;
        for (k = 0; k < 200000 && ok; k++) {
            const uint32_t seed = rnd();
            uint32_t len, op, mended;
            n = make_random(a, sizeof a, seed % 5000u);
            rng = seed;
            memcpy(m, a, n);
            len = n;
            op = rnd() % 8u;
            if (op == 0)
                m[rnd() % n] ^= (uint8_t)(1u << (rnd() % 8u));
            else if (op == 1)
                for (i = 0; i < 1u + rnd() % 8u; i++)
                    m[rnd() % n] = (uint8_t)rnd();
            else if (op == 2)
                len = rnd() % n;
            else if (op == 3) {
                const uint32_t x = 1u + rnd() % 64u;
                for (i = 0; i < x; i++)
                    m[n + i] = (uint8_t)rnd();
                len = n + x;
            } else if (op == 4) {                 /* (a chunk's length, or a count, set wild) */
                const uint32_t at = 32u + 4u * (rnd() % ((n - 32u) / 4u));
                const uint32_t v = rnd() % 3 ? rnd() % 1024u : rnd();
                m[at] = (uint8_t)v, m[at + 1] = (uint8_t)(v >> 8), m[at + 2] = (uint8_t)(v >> 16), m[at + 3] = (uint8_t)(v >> 24);
            } else if (op == 5) {                 /* (the header's total) */
                const uint32_t v = rnd() % 3 ? n + (rnd() % 9u) - 4u : rnd();
                m[8] = (uint8_t)v, m[9] = (uint8_t)(v >> 8), m[10] = (uint8_t)(v >> 16), m[11] = (uint8_t)(v >> 24);
            } else if (op == 6) {                 /* (a piece's or the globals' counts) */
                const uint32_t at = 32u + rnd() % (n - 32u);
                m[at] = (uint8_t)rnd();
            } else if (n > 96u) {
                memmove(m + 32, m + 32 + 4u * (rnd() % 8u), n - 32u - 32u);   /* (chunks shifted) */
            }
            mended = rnd() % 2u;
            if (mended) {
                fix_crc(m, len);
                if (len >= 12)                    /* (and the total, sometimes) */
                    if (rnd() % 2)
                        m[8] = (uint8_t)len, m[9] = (uint8_t)(len >> 8), m[10] = (uint8_t)(len >> 16), m[11] = (uint8_t)(len >> 24);
            }
            rc = walk(m, len, 0, 0);
            if (rc)
                refused++;
            else
                read++;
            if (!rc && !mended && !(len == n && !memcmp(m, a, n)))
                ok = 0, printf("  mutation %u (op %u): read with its CRC not mended\n", k, op);
        }
        printf("  (fuzz: %u refused, %u read; under ASan/UBSan when run_tests.sh builds it)\n", refused, read);
        bad += check("200,000 mutated containers: no fault, none read unmended", ok);
    }
    printf("%s\n", bad ? "CONTAINER TEST FAILED" : "container test passed");
    return bad != 0;
}
