/* SPDX-License-Identifier: GPL-3.0-only */
/* DUB-1's container (1.5.0, STORAGE-1.5.0.md 4.2): the one format of a frame's payload and of a .dub1 file, the same
 * bytes. Only the codec: bytes in and out, fields checked for shape (lengths, padding, the CRC); what the fields mean
 * (a piece's values, their format, their ranges) is project.c's. Shared by the device (main.c's unity build, through
 * project.c), amb_host and the web emulator (the same project.c); tools/dub1_container.py and web/dub1container.js are
 * its twins (tests/host/container_twins.sh: the three read and write the same bytes).
 *
 * All little-endian. A 32-byte header, then chunks, the last END:
 *   header   "D1ST", u16 version (1), u16 header length (32), u32 total length (END included), u32 proj format (the
 *            writer's PROJ_FORMAT), u16 kind (plat.h's FS_*: 1 WORK, 2 SET ...), u16 pieces (PIEC chunks), u32 flags
 *            (D1C_F_*), u32 reserved x 2 (0)
 *   chunk    4-byte id, u32 length, the payload, zeros to a multiple of 4. A reader skips an id it doesn't know.
 *   META 44  name[16] (NUL-padded; up to 12 characters, the user's), the writer's version[16] (EN_VERSION), u32 source
 *            (D1C_SRC_*), u32 migrated (OBJ_PROJ's seq when 1.4's save was migrated; 0 none), u32 saves (a count)
 *   GLOB     (WORK; optional in a set) u8 piece, leds, screen, values, trans, nsir, nsk, setlen (the set's pieces,
 *            0 all: step 4, was 0); u16 pool, u16 npar;
 *            i8 sir[nsir x nsk]; i16 par[npar]: what project_t holds beside the pieces
 *   PIEC     u8 index (slot), kind (D1C_FULL, D1C_RECIPE, D1C_DELTA), recipe, screen, pings_ok, flags; u16 nparams,
 *            u16 pool, u16 npings; u32 seed (0: sets carry no seeds, the user's); name[16]; u8 pings[npings];
 *            FULL: i16 par[nparams], every parameter by its enum index (amb.h's: append-only, a stable id);
 *            DELTA: nparams x (u16 index, i16 value) on today's recipe; RECIPE: nothing (nparams 0)
 *   CUES     (step 7) a piece's cue list, one chunk a piece that has cues: u8 index (its PIEC's slot), u8 count
 *            (1..64), u16 flags (0); count x 8-byte cues, sorted by pos (equal ones in the order they fire):
 *              u16 pos   part (bits 14-15: 0 intro, 1 main, 2 outro, 3 the bar from PLAY) | bar in it (bits 4-13,
 *                        0..1023) | step (bits 0-3: the semicorchea (16th) in the bar)
 *              u8 op, u8 arg, i16 val, u16 len (what each means: project.c's cue_ok, ui.c's player)
 *   END 4    u32 CRC-32 (zlib's) of every byte before this chunk
 * Shape is strict (a known chunk's length exact, padding zero, the header 32 bytes): what passes reads back the
 * same bytes; anything else is refused, never half applied. */
#define D1C_MAGIC 0x54533144u                  /* "D1ST" */
#define D1C_VERSION 1u
#define D1C_HDR 32u
#define D1C_ID(a, b, c, d) ((uint32_t)(a) | (uint32_t)(b) << 8 | (uint32_t)(c) << 16 | (uint32_t)(d) << 24)
#define D1C_META D1C_ID('M', 'E', 'T', 'A')
#define D1C_GLOB D1C_ID('G', 'L', 'O', 'B')
#define D1C_PIEC D1C_ID('P', 'I', 'E', 'C')
#define D1C_CUES D1C_ID('C', 'U', 'E', 'S')
#define D1C_END D1C_ID('E', 'N', 'D', ' ')
#define D1C_NAME 16u                           /* a name field; the user's names up to 12 characters (the decisions) */
#define D1C_META_LEN 44u
#define D1C_PIEC_FIX 32u
#define D1C_GLOB_FIX 12u
#define D1C_MAXSIR 64u                         /* nsir x nsk at most */
#define D1C_CUES_FIX 4u                        /* (step 7) a CUES chunk's head; then its cues, 8 bytes each */
#define D1C_CUE 8u
#define D1C_MAXCUE 64u                         /* cues a piece at most (SETFILES.md's 64) */
enum { D1C_FULL, D1C_RECIPE, D1C_DELTA, D1C_NKIND };
enum { D1C_SRC_DEVICE, D1C_SRC_WEB, D1C_SRC_HOST };
#define D1C_F_WAVE 1u                          /* (proj format 30 only) the LFO pages' layout B (PROJ_WAVE_MARK) */
#define D1C_F_NOCUES 2u                        /* (step 7, a WORK's) the player's "cues off" (the Set page's Cues:
                                                * global, one switch; 0 in a set, never read from one) */
#define D1C_F_SETS 4u                          /* (a WORK's, the user's 2026-11-23) the sets the FM-1 comes with
                                                * offered on this unit, once (project.c's first_sets); 0 in a set.
                                                * Older readers skip it */
/* errors (< 0) */
#define D1C_EIO (-1)                           /* a read or write failed */
#define D1C_EMAGIC (-2)                        /* not a container */
#define D1C_EVERSION (-3)                      /* a newer container version */
#define D1C_ESHAPE (-4)                        /* lengths, padding, chunk order */
#define D1C_ECRC (-5)
#define D1C_ENEWER (-6)                        /* made for a newer DUB-1 (format, parameters) */
#define D1C_EBAD (-7)                          /* a value out of its range (project.c) */

typedef struct {
    uint16_t version, hlen, kind, npieces;
    uint32_t total, format, flags, rsv[2];
} d1c_hdr_t;
typedef struct {
    char name[D1C_NAME], fw[D1C_NAME];
    uint32_t source, migrated, saves;
} d1c_meta_t;
typedef struct {
    uint8_t piece, leds, screen, values, trans, nsir, nsk, setlen;
    uint16_t pool, npar;
    int8_t sir[D1C_MAXSIR];
    uint32_t par_at;                           /* (read) where par[] starts */
} d1c_glob_t;
typedef struct {
    uint8_t index, kind, recipe, screen, pings_ok, flags;
    uint16_t nparams, pool, npings;
    uint32_t seed;
    char name[D1C_NAME];
    uint32_t pings_at, data_at;                /* (read) where the pings and the parameters start */
} d1c_piece_t;
typedef struct {                               /* (step 7) a cue, as the chunk has it (8 bytes) */
    uint16_t pos;
    uint8_t op, arg;
    int16_t val;
    uint16_t len;
} d1c_cue_t;
typedef struct {
    uint8_t index, count;
    uint16_t flags;
    uint32_t cues_at;                          /* (read) where the cues start */
} d1c_cues_t;
#define D1C_POS(part, bar, step) ((uint16_t)((uint32_t)(part) << 14 | ((uint32_t)(bar) & 1023u) << 4 | ((uint32_t)(step) & 15u)))

static uint32_t d1c_crc_up(uint32_t c, const void *p, uint32_t n)   /* zlib CRC-32 (storage.c's, kept apart so the
                                                  * codec stands alone in its test) */
{
    static const uint32_t T[16] = {
        0x00000000u, 0x1DB71064u, 0x3B6E20C8u, 0x26D930ACu, 0x76DC4190u, 0x6B6B51F4u, 0x4DB26158u, 0x5005713Cu,
        0xEDB88320u, 0xF00F9344u, 0xD6D6A3E8u, 0xCB61B38Cu, 0x9B64C2B0u, 0x86D3D2D4u, 0xA00AE278u, 0xBDBDF21Cu};
    const uint8_t *b = p;
    while (n--) {
        c ^= *b++;
        c = (c >> 4) ^ T[c & 15u];
        c = (c >> 4) ^ T[c & 15u];
    }
    return c;
}

/* ------------------------------------------------------------------------------------------------- writing --- */
/* bytes go to put (0: counted only, the first of two passes: the total length before the header is written) */
typedef int (*d1c_put_fn)(void *ctx, const void *p, uint32_t n);
typedef struct {
    d1c_put_fn put;
    void *ctx;
    uint32_t len, crc;
    int err;
} d1c_w_t;

static void d1c_w_init(d1c_w_t *w, d1c_put_fn put, void *ctx)
{
    w->put = put, w->ctx = ctx, w->len = 0, w->crc = 0xFFFFFFFFu, w->err = 0;
}
static void d1c_bytes(d1c_w_t *w, const void *p, uint32_t n)
{
    if (w->err || !n)
        return;
    if (w->put && w->put(w->ctx, p, n))
        w->err = D1C_EIO;
    w->crc = d1c_crc_up(w->crc, p, n);
    w->len += n;
}
static void d1c_u8(d1c_w_t *w, uint32_t v)
{
    uint8_t b = (uint8_t)v;
    d1c_bytes(w, &b, 1);
}
static void d1c_u16(d1c_w_t *w, uint32_t v)
{
    uint8_t b[2] = {(uint8_t)v, (uint8_t)(v >> 8)};
    d1c_bytes(w, b, 2);
}
static void d1c_u32(d1c_w_t *w, uint32_t v)
{
    uint8_t b[4] = {(uint8_t)v, (uint8_t)(v >> 8), (uint8_t)(v >> 16), (uint8_t)(v >> 24)};
    d1c_bytes(w, b, 4);
}
static void d1c_i16s(d1c_w_t *w, const int16_t *v, uint32_t n)   /* (32 at a time: the device's stack, not a buffer) */
{
    uint8_t b[64];
    uint32_t i, k;
    while (n) {
        k = n > 32u ? 32u : n;
        for (i = 0; i < k; i++)
            b[2 * i] = (uint8_t)v[i], b[2 * i + 1] = (uint8_t)((uint16_t)v[i] >> 8);
        d1c_bytes(w, b, 2 * k);
        v += k, n -= k;
    }
}
static void d1c_name(d1c_w_t *w, const char *s) { d1c_bytes(w, s, D1C_NAME); }   /* a name field, its 16 bytes as
                                                  * they are (NUL-padded by the caller: d1c_name_set) */
static void d1c_name_set(char *d, const char *s)  /* a string into a name field, NUL-padded (cut at 16) */
{
    uint32_t i, e = 0;
    for (i = 0; i < D1C_NAME; i++) {
        if (!e && (!s || !s[i]))
            e = 1;
        d[i] = e ? 0 : s[i];
    }
}
static void d1c_chunk(d1c_w_t *w, uint32_t id, uint32_t len) { d1c_u32(w, id), d1c_u32(w, len); }
static void d1c_pad(d1c_w_t *w)
{
    static const uint8_t Z[3] = {0, 0, 0};
    d1c_bytes(w, Z, (4u - (w->len & 3u)) & 3u);
}
static void d1c_header(d1c_w_t *w, const d1c_hdr_t *h)   /* (the total: the counting pass's length) */
{
    d1c_u32(w, D1C_MAGIC), d1c_u16(w, D1C_VERSION), d1c_u16(w, D1C_HDR), d1c_u32(w, h->total), d1c_u32(w, h->format);
    d1c_u16(w, h->kind), d1c_u16(w, h->npieces), d1c_u32(w, h->flags), d1c_u32(w, h->rsv[0]), d1c_u32(w, h->rsv[1]);
}
static void d1c_meta(d1c_w_t *w, const d1c_meta_t *m)
{
    d1c_chunk(w, D1C_META, D1C_META_LEN);
    d1c_name(w, m->name), d1c_name(w, m->fw), d1c_u32(w, m->source), d1c_u32(w, m->migrated), d1c_u32(w, m->saves);
}
static void d1c_glob(d1c_w_t *w, const d1c_glob_t *g, const int16_t *par)
{
    const uint32_t ns = (uint32_t)g->nsir * g->nsk;
    d1c_chunk(w, D1C_GLOB, D1C_GLOB_FIX + ns + 2u * g->npar);
    d1c_u8(w, g->piece), d1c_u8(w, g->leds), d1c_u8(w, g->screen), d1c_u8(w, g->values), d1c_u8(w, g->trans);
    d1c_u8(w, g->nsir), d1c_u8(w, g->nsk), d1c_u8(w, g->setlen), d1c_u16(w, g->pool), d1c_u16(w, g->npar);
    d1c_bytes(w, g->sir, ns);
    d1c_i16s(w, par, g->npar);
    d1c_pad(w);
}
/* a piece: FULL par[nparams]; DELTA par[2 x nparams] as (index, value) pairs; RECIPE none */
static void d1c_piece(d1c_w_t *w, const d1c_piece_t *p, const uint8_t *pings, const int16_t *par)
{
    const uint32_t nd = p->kind == D1C_FULL ? p->nparams : p->kind == D1C_DELTA ? 2u * p->nparams : 0u;
    d1c_chunk(w, D1C_PIEC, D1C_PIEC_FIX + p->npings + 2u * nd);
    d1c_u8(w, p->index), d1c_u8(w, p->kind), d1c_u8(w, p->recipe), d1c_u8(w, p->screen), d1c_u8(w, p->pings_ok);
    d1c_u8(w, p->flags), d1c_u16(w, p->nparams), d1c_u16(w, p->pool), d1c_u16(w, p->npings), d1c_u32(w, p->seed);
    d1c_name(w, p->name);
    d1c_bytes(w, pings, p->npings);
    d1c_i16s(w, par, nd);
    d1c_pad(w);
}
/* (step 7) a cue list: its head, then its cues (d1c_cue_put, count of them: 4 + 8n, no padding) */
static void d1c_cues_head(d1c_w_t *w, uint32_t index, uint32_t count, uint32_t flags)
{
    d1c_chunk(w, D1C_CUES, D1C_CUES_FIX + D1C_CUE * count);
    d1c_u8(w, index), d1c_u8(w, count), d1c_u16(w, flags);
}
static void d1c_cue_put(d1c_w_t *w, const d1c_cue_t *q)
{
    d1c_u16(w, q->pos), d1c_u8(w, q->op), d1c_u8(w, q->arg), d1c_u16(w, (uint16_t)q->val), d1c_u16(w, q->len);
}
static void d1c_raw(d1c_w_t *w, uint32_t id, const void *p, uint32_t n)   /* a chunk as bytes (an unknown one) */
{
    d1c_chunk(w, id, n);
    d1c_bytes(w, p, n);
    d1c_pad(w);
}
static void d1c_end(d1c_w_t *w)                  /* the CRC of everything before it */
{
    const uint32_t c = ~w->crc;
    d1c_chunk(w, D1C_END, 4);
    d1c_u32(w, c);
}

/* ------------------------------------------------------------------------------------------------- reading --- */
/* bytes come from get(ctx, offset, dst, n): a frame (plat_fs_read) or memory (a file) */
typedef int (*d1c_get_fn)(void *ctx, uint32_t off, void *dst, uint32_t n);
typedef struct {
    d1c_get_fn get;
    void *ctx;
    uint32_t size, end;                        /* size: the bytes there are; end: where END starts */
    d1c_hdr_t h;
} d1c_r_t;

static uint32_t d1c_le(const uint8_t *b, uint32_t n)
{
    uint32_t v = 0;
    while (n--)
        v = v << 8 | b[n];
    return v;
}
static int d1c_get(const d1c_r_t *r, uint32_t off, void *dst, uint32_t n)
{
    if (off > r->size || n > r->size - off || r->get(r->ctx, off, dst, n))
        return D1C_EIO;
    return 0;
}

/* the header and every chunk's frame checked, the CRC over all of it (streamed, 64 bytes at a time): 0, or < 0 */
static int d1c_open(d1c_r_t *r, d1c_get_fn get, void *ctx, uint32_t size)
{
    uint8_t b[64];
    uint32_t at, c = 0xFFFFFFFFu, i;
    r->get = get, r->ctx = ctx, r->size = size, r->end = 0;
    if (size < D1C_HDR + 12u || d1c_get(r, 0, b, D1C_HDR))
        return size < D1C_HDR + 12u ? D1C_ESHAPE : D1C_EIO;
    if (d1c_le(b, 4) != D1C_MAGIC)
        return D1C_EMAGIC;
    r->h.version = (uint16_t)d1c_le(b + 4, 2), r->h.hlen = (uint16_t)d1c_le(b + 6, 2), r->h.total = d1c_le(b + 8, 4);
    r->h.format = d1c_le(b + 12, 4), r->h.kind = (uint16_t)d1c_le(b + 16, 2), r->h.npieces = (uint16_t)d1c_le(b + 18, 2);
    r->h.flags = d1c_le(b + 20, 4), r->h.rsv[0] = d1c_le(b + 24, 4), r->h.rsv[1] = d1c_le(b + 28, 4);
    if (r->h.version != D1C_VERSION)
        return r->h.version > D1C_VERSION ? D1C_EVERSION : D1C_ESHAPE;
    if (r->h.hlen != D1C_HDR || r->h.total != size || (size & 3u))
        return D1C_ESHAPE;
    for (at = D1C_HDR;;) {                       /* (the chunks' frames: each inside, END last and at the end) */
        uint32_t id, len;
        if (size - at < 8u || d1c_get(r, at, b, 8))
            return D1C_ESHAPE;
        id = d1c_le(b, 4), len = d1c_le(b + 4, 4);
        if (id == D1C_END) {
            if (len != 4u || size - at != 12u)
                return D1C_ESHAPE;
            r->end = at;
            break;
        }
        if (len > size - at - 8u || ((len + 3u) & ~3u) > size - at - 8u)
            return D1C_ESHAPE;
        if (len & 3u) {                          /* (the padding zeros) */
            const uint32_t np = 4u - (len & 3u);
            if (d1c_get(r, at + 8u + len, b, np))
                return D1C_EIO;
            for (i = 0; i < np; i++)
                if (b[i])
                    return D1C_ESHAPE;
        }
        at += 8u + ((len + 3u) & ~3u);
    }
    for (at = 0; at < r->end; at += i) {         /* (the CRC) */
        i = r->end - at > sizeof b ? (uint32_t)sizeof b : r->end - at;
        if (d1c_get(r, at, b, i))
            return D1C_EIO;
        c = d1c_crc_up(c, b, i);
    }
    if (d1c_get(r, r->end + 8u, b, 4) || d1c_le(b, 4) != ~c)
        return D1C_ECRC;
    return 0;
}

/* the chunks in turn (after d1c_open): *at 0 to start; 1 and the chunk's id, payload offset and length; 0 at END */
static int d1c_next(const d1c_r_t *r, uint32_t *at, uint32_t *id, uint32_t *pay, uint32_t *len)
{
    uint8_t b[8];
    uint32_t a = *at ? *at : D1C_HDR;
    if (a >= r->end || d1c_get(r, a, b, 8))
        return 0;
    *id = d1c_le(b, 4), *len = d1c_le(b + 4, 4), *pay = a + 8u;
    *at = a + 8u + ((*len + 3u) & ~3u);
    return 1;
}

static void d1c_name_in(char *d, const uint8_t *s)   /* a name field as a string (16 bytes, NUL-terminated if shorter) */
{
    uint32_t i;
    for (i = 0; i < D1C_NAME; i++)
        d[i] = (char)s[i];
}
static int d1c_read_meta(const d1c_r_t *r, uint32_t pay, uint32_t len, d1c_meta_t *m)
{
    uint8_t b[D1C_META_LEN];
    if (len != D1C_META_LEN)
        return D1C_ESHAPE;
    if (d1c_get(r, pay, b, sizeof b))
        return D1C_EIO;
    d1c_name_in(m->name, b), d1c_name_in(m->fw, b + 16);
    m->source = d1c_le(b + 32, 4), m->migrated = d1c_le(b + 36, 4), m->saves = d1c_le(b + 40, 4);
    return 0;
}
static int d1c_read_glob(const d1c_r_t *r, uint32_t pay, uint32_t len, d1c_glob_t *g)
{
    uint8_t b[D1C_GLOB_FIX];
    uint32_t ns;
    if (len < D1C_GLOB_FIX || d1c_get(r, pay, b, sizeof b))
        return len < D1C_GLOB_FIX ? D1C_ESHAPE : D1C_EIO;
    g->piece = b[0], g->leds = b[1], g->screen = b[2], g->values = b[3], g->trans = b[4], g->nsir = b[5], g->nsk = b[6];
    g->setlen = b[7], g->pool = (uint16_t)d1c_le(b + 8, 2), g->npar = (uint16_t)d1c_le(b + 10, 2);
    ns = (uint32_t)g->nsir * g->nsk;
    if (ns > D1C_MAXSIR || len != D1C_GLOB_FIX + ns + 2u * g->npar)
        return D1C_ESHAPE;
    if (ns && d1c_get(r, pay + D1C_GLOB_FIX, g->sir, ns))
        return D1C_EIO;
    g->par_at = pay + D1C_GLOB_FIX + ns;
    return 0;
}
static int d1c_read_piece(const d1c_r_t *r, uint32_t pay, uint32_t len, d1c_piece_t *p)
{
    uint8_t b[D1C_PIEC_FIX];
    uint32_t nd;
    if (len < D1C_PIEC_FIX || d1c_get(r, pay, b, sizeof b))
        return len < D1C_PIEC_FIX ? D1C_ESHAPE : D1C_EIO;
    p->index = b[0], p->kind = b[1], p->recipe = b[2], p->screen = b[3], p->pings_ok = b[4], p->flags = b[5];
    p->nparams = (uint16_t)d1c_le(b + 6, 2), p->pool = (uint16_t)d1c_le(b + 8, 2), p->npings = (uint16_t)d1c_le(b + 10, 2);
    p->seed = d1c_le(b + 12, 4);
    d1c_name_in(p->name, b + 16);
    if (p->kind >= D1C_NKIND)
        return D1C_ESHAPE;
    nd = p->kind == D1C_FULL ? p->nparams : p->kind == D1C_DELTA ? 2u * p->nparams : 0u;
    if ((p->kind == D1C_RECIPE && p->nparams) || len != D1C_PIEC_FIX + p->npings + 2u * nd)
        return D1C_ESHAPE;
    p->pings_at = pay + D1C_PIEC_FIX;
    p->data_at = p->pings_at + p->npings;
    return 0;
}
static void d1c_cue_in(const uint8_t *b, d1c_cue_t *q)   /* (step 7) 8 bytes as a cue */
{
    q->pos = (uint16_t)d1c_le(b, 2), q->op = b[2], q->arg = b[3], q->val = (int16_t)(uint16_t)d1c_le(b + 4, 2);
    q->len = (uint16_t)d1c_le(b + 6, 2);
}
/* (step 7) a CUES chunk's shape: its length its count's, 1 to 64 cues, sorted by pos (read through, 8 at a time) */
static int d1c_read_cues(const d1c_r_t *r, uint32_t pay, uint32_t len, d1c_cues_t *c)
{
    uint8_t b[64];
    uint32_t i, k, n, at, last = 0;
    if (len < D1C_CUES_FIX || d1c_get(r, pay, b, D1C_CUES_FIX))
        return len < D1C_CUES_FIX ? D1C_ESHAPE : D1C_EIO;
    c->index = b[0], c->count = b[1], c->flags = (uint16_t)d1c_le(b + 2, 2);
    if (!c->count || c->count > D1C_MAXCUE || len != D1C_CUES_FIX + D1C_CUE * c->count)
        return D1C_ESHAPE;
    c->cues_at = pay + D1C_CUES_FIX;
    for (at = c->cues_at, n = c->count; n; n -= k, at += D1C_CUE * k) {
        k = n > 8u ? 8u : n;
        if (d1c_get(r, at, b, D1C_CUE * k))
            return D1C_EIO;
        for (i = 0; i < k; i++) {
            const uint32_t p = d1c_le(b + D1C_CUE * i, 2);
            if (p < last)
                return D1C_ESHAPE;
            last = p;
        }
    }
    return 0;
}
static int d1c_read_cue(const d1c_r_t *r, const d1c_cues_t *c, uint32_t i, d1c_cue_t *q)   /* cue i of the list */
{
    uint8_t b[D1C_CUE];
    if (i >= c->count || d1c_get(r, c->cues_at + D1C_CUE * i, b, D1C_CUE))
        return D1C_EIO;
    d1c_cue_in(b, q);
    return 0;
}
static int d1c_read_i16s(const d1c_r_t *r, uint32_t at, int16_t *v, uint32_t n)   /* n little-endian i16 from at */
{
    uint8_t b[64];
    uint32_t i, k;
    while (n) {
        k = n > 32u ? 32u : n;
        if (d1c_get(r, at, b, 2u * k))
            return D1C_EIO;
        for (i = 0; i < k; i++)
            v[i] = (int16_t)(uint16_t)(b[2 * i] | b[2 * i + 1] << 8);
        v += k, n -= k, at += 2u * k;
    }
    return 0;
}
