/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
 * X0X / AMB-1: object map changed (see below). DUB-1 1.5.0: the frame store (below the A/B objects). */
/* Persistent storage on the SPI NOR.
 *
 * Every object has an A/B sector pair. A save goes to the copy that is not
 * the current one: erase the sector, program the payload (from offset 256),
 * then the 32-byte header at offset 0 last. The header is the commit record;
 * on load the valid copy with the highest seq wins, so a torn write leaves
 * the previous copy in charge.
 *
 * Flash access goes through three hooks (also used by the host test):
 *   st_read(off, dst, n)   st_erase(off)   st_prog(off, src, n)
 * (1.5.0) st_prog's source is always RAM (the driver reads it with the flash stopped): the callers' own (the panel,
 * the project's page buffer, a header on the stack). No payload-sized buffer any more: CRCs are taken as the bytes
 * stream past in 256-byte reads (st_buf, 3,840 B, gone).
 */
#define ST_MAGIC 0x30583058u                   /* "X0X0": never mistaken for a Felucca object */
#define ST_SECTOR 4096u
#define ST_PAYLOAD_OFF 256u
#define ST_PAYLOAD_MAX (ST_SECTOR - ST_PAYLOAD_OFF)

/* flash map, AMB-1 (plat.h OBJ_*): the project at 0xFC000 (Felucca's FL_GLOB), the panel calibration at
 * 0xDC000 (Felucca's user-preset area). Nothing above 0xDFFFF: 0xE0000.. is the update loader's
 * staging area. The magic is X0X's, so a project is told apart by its own magic (project.c).
 * (1.5.0: OBJ_PROJ only read, the 1.4 save migrated into the frame store's WORK; 0x97000..0xDAFFF the frames) */
typedef struct {
    uint32_t magic;
    uint16_t type, slot;
    uint32_t seq, len, crc, rsv[2];
    uint32_t hcrc;
} st_hdr_t;

static int st_read(uint32_t off, void *dst, uint32_t n);
static int st_erase(uint32_t off);
static int st_prog(uint32_t off, const void *src, uint32_t n);

static uint32_t st_crc_up(uint32_t c, const void *p, uint32_t n)   /* zlib CRC-32, 4 bits per step; c from ~0, ~ at the end */
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
static uint32_t st_crc32(const void *p, uint32_t n) { return ~st_crc_up(0xFFFFFFFFu, p, n); }

/* n bytes from off: their CRC-32 into *crc, copied to dst as far as max (dst 0: CRC only). 256 at a time on the
 * stack, so audio keeps up between the reads (main.c's windows) and nothing payload-sized is needed */
static int st_stream(uint32_t off, uint32_t n, uint8_t *dst, uint32_t max, uint32_t *crc)
{
    uint8_t b[256];
    uint32_t c = 0xFFFFFFFFu, at = 0, i;
    while (at < n) {
        const uint32_t k = n - at > 256u ? 256u : n - at;
        if (st_read(off + at, b, k))
            return -1;
        c = st_crc_up(c, b, k);
        if (dst)
            for (i = 0; i < k && at + i < max; i++)
                dst[at + i] = b[i];
        at += k;
    }
    *crc = ~c;
    return 0;
}

static uint32_t st_sector(uint32_t obj, uint32_t copy)  /* flash offset of copy A (0) / B (1) */
{
    return (obj == OBJ_PANEL ? 0xDC000u : 0xFC000u) + copy * ST_SECTOR;
}

static int st_head(uint32_t obj, uint32_t copy, st_hdr_t *h)   /* commit record valid: 0 */
{
    if (st_read(st_sector(obj, copy), h, sizeof *h))
        return -1;
    if (h->magic != ST_MAGIC || h->type != obj || h->len > ST_PAYLOAD_MAX ||
        h->hcrc != st_crc32(h, sizeof *h - 4u))
        return -1;
    return 0;
}

static int st_body(uint32_t obj, uint32_t copy, const st_hdr_t *h, void *dst, uint32_t max)   /* CRC ok: 0 (the
                                                  * payload into dst as far as max; dst 0: checked only) */
{
    uint32_t c;
    if (st_stream(st_sector(obj, copy) + ST_PAYLOAD_OFF, h->len, dst, max, &c) || c != h->crc)
        return -1;
    return 0;
}

/* the current copy: the valid one with the highest seq (A on a tie), -1 when neither is valid. Headers first, then
 * the winner's payload: read into dst (up to max; 0: only checked), the other copy's when its CRC fails; *h gets
 * the header of the one that held */
static int st_pick(uint32_t obj, st_hdr_t *h, void *dst, uint32_t max)
{
    st_hdr_t a, b;
    int va = st_head(obj, 0, &a) == 0, vb = st_head(obj, 1, &b) == 0;
    if (vb && (!va || b.seq > a.seq)) {
        if (st_body(obj, 1, &b, dst, max) == 0) {
            *h = b;
            return 1;
        }
        vb = 0;
    }
    if (va && st_body(obj, 0, &a, dst, max) == 0) {
        *h = a;
        return 0;
    }
    if (vb && st_body(obj, 1, &b, dst, max) == 0) {
        *h = b;
        return 1;
    }
    return -1;
}
static int st_current(uint32_t obj, st_hdr_t *h) { return st_pick(obj, h, 0, 0); }

/* load object into dst (up to max bytes); returns the length, or -1 */
static int st_load(uint32_t obj, void *dst, uint32_t max)
{
    st_hdr_t h;
    if (st_pick(obj, &h, dst, max) < 0)
        return -1;
    return (int)(h.len > max ? max : h.len);
}

static uint32_t st_seq(uint32_t obj)              /* (1.5.0) the current copy's seq; 0: none (a first save is 1) */
{
    st_hdr_t h;
    return st_current(obj, &h) < 0 ? 0u : h.seq;
}

static int st_save(uint32_t obj, const void *src, uint32_t len)   /* (src in RAM: the driver's) */
{
    uint32_t seq, base, off;
    int cur, rc;
    st_hdr_t h;
    if (len > ST_PAYLOAD_MAX)
        return -1;
    cur = st_current(obj, &h);
    seq = cur < 0 ? 0u : h.seq;
    base = st_sector(obj, cur == 0 ? 1u : 0u);       /* write the other copy */
    if ((rc = st_erase(base)) != 0)
        return rc;
    for (off = 0; off < len; off += 256u) {
        uint32_t n = len - off > 256u ? 256u : len - off;
        if ((rc = st_prog(base + ST_PAYLOAD_OFF + off, (const uint8_t *)src + off, n)) != 0)
            return rc;
    }
    h.magic = ST_MAGIC;
    h.type = (uint16_t)obj;
    h.slot = (uint16_t)(cur == 0 ? 1 : 0);
    h.seq = seq + 1u;
    h.len = len;
    h.crc = st_crc32(src, len);
    h.rsv[0] = h.rsv[1] = 0xFFFFFFFFu;
    h.hcrc = st_crc32(&h, sizeof h - 4u);
    if ((rc = st_prog(base, &h, sizeof h)) != 0)       /* the commit record, last */
        return rc;
    {   /* read back: a write-protected or failing part must not report SAVED */
        st_hdr_t chk;
        uint32_t c = cur == 0 ? 1u : 0u;
        if (st_head(obj, c, &chk) || chk.seq != h.seq || st_body(obj, c, &chk, 0, 0))
            return -7;
    }
    return 0;
}

/* ------------------------------------------------------------------------------------------------ frame store ---
 * (1.5.0, STORAGE-1.5.0.md 3C and 5.1) 17 frames of 4 sectors (16 KB) at 0x97000..0xDAFFF, each holding one version
 * of one object, (kind, index): the working project (WORK), later a stored set, a song, a sound set. The A/B rule
 * above, for N objects in N + k frames: a frame's 32-byte header (st_hdr_t: magic "D1FR", type the kind, slot the
 * index, seq global) is programmed LAST, after the payload is programmed and read back; on load the valid version with
 * the highest seq whose payload CRC holds wins (a torn save, a rotten payload: the version before). A save goes to a
 * frame no object's newest version is in: a headerless one first (never used, or a save cut short), then the oldest
 * superseded version, the second newest of an object last (it is that object's undo, and the boot guard's fallback):
 * the 17 frames share the wear. The payload is streamed in (fs_put, a page at a time: one 256-B buffer), its CRC
 * taken on the way; nothing set-sized in RAM. Erasing silences the audio (st_erase): a save erases only the sectors
 * its length needs, and fs_prepare erases ahead (while stopped) so a save while playing finds its sectors erased and
 * only programs pages (the device test: page writes miss no audio, an erase ~10 blocks). */
#define FS_MAGIC 0x52463144u                   /* "D1FR": not "X0X0", not "DUB1": nothing older is taken for a frame */
#define FS_BASE 0x97000u
#define FS_N 17u                               /* frames: 0x97000 + f x 0x4000 (the last ends at 0xDAFFF) */
#define FS_SIZE 0x4000u
#define FS_SECS (FS_SIZE / ST_SECTOR)
#define FS_MAX (FS_SIZE - ST_PAYLOAD_OFF)      /* 16,128 B a payload */
#define FS_PREP 2u                             /* frames kept erased ahead (two saves while playing, then stop) */
#define FS_BUSY (-11)                          /* fs_begin: no erased frame, and may not erase (playing) */
#define FS_FULL (-12)                          /* every frame some object's newest version */
static struct {
    struct {
        uint16_t kind, index;                  /* kind 0: no valid header (free) */
        uint32_t seq;
    } f[FS_N];
    uint32_t top;                              /* the highest seq of any valid header */
    uint32_t blank[(FS_N * FS_SECS + 31u) / 32u];   /* sector known erased (erased or read so, this boot): bit f x 4 + s */
    uint8_t scanned;
    int8_t wf;                                 /* the frame open for writing (fs_begin), -1 none */
    uint16_t wkind, windex;
    uint32_t wlen, woff, wcrc;
    int werr;
    uint8_t wdone;                             /* fs_finish done: the bytes all in, read back; fs_seal next */
    uint8_t keep;                              /* (1.5.0, step 7) a frame + 1 never picked for a save (0: none): a restore's
                                                * pieces, dropped, their cues still copied from it into the new WORK */
    uint8_t page[256] __attribute__((aligned(4)));   /* the page being filled (the driver's RAM source) */
} fs;

static uint32_t fs_base(uint32_t f) { return FS_BASE + f * FS_SIZE; }
static int fs_blank_is(uint32_t f, uint32_t s) { return (int)(fs.blank[(f * FS_SECS + s) >> 5] >> ((f * FS_SECS + s) & 31u) & 1u); }
static void fs_blank_set(uint32_t f, uint32_t s, int on)
{
    const uint32_t b = f * FS_SECS + s, m = 1u << (b & 31u);
    fs.blank[b >> 5] = on ? fs.blank[b >> 5] | m : fs.blank[b >> 5] & ~m;
}

static int fs_head(uint32_t f, st_hdr_t *h)      /* a valid frame header: 0 */
{
    if (st_read(fs_base(f), h, sizeof *h))
        return -1;
    if (h->magic != FS_MAGIC || h->type == 0 || h->len == 0 || h->len > FS_MAX || h->seq == 0 ||
        h->hcrc != st_crc32(h, sizeof *h - 4u))
        return -1;
    return 0;
}

static void fs_scan(void)                        /* the 17 headers: the frame table (boot, and the host's reboot) */
{
    uint32_t f;
    st_hdr_t h;
    for (f = 0; f < (sizeof fs.blank / sizeof fs.blank[0]); f++)
        fs.blank[f] = 0;
    fs.top = 0;
    fs.wf = -1;
    for (f = 0; f < FS_N; f++) {
        if (fs_head(f, &h) == 0) {
            fs.f[f].kind = h.type, fs.f[f].index = h.slot, fs.f[f].seq = h.seq;
            if (h.seq > fs.top)
                fs.top = h.seq;
        } else {
            fs.f[f].kind = 0, fs.f[f].index = 0, fs.f[f].seq = 0;
        }
    }
    fs.scanned = 1;
}

/* the frame of (kind, index)'s version below seq `below` with the highest seq, by header only; -1 none */
static int fs_version(uint32_t kind, uint32_t index, uint32_t below)
{
    uint32_t f, best = 0;
    int bf = -1;
    for (f = 0; f < FS_N; f++)
        if (fs.f[f].kind == kind && fs.f[f].index == index && fs.f[f].seq < below && fs.f[f].seq > best)
            best = fs.f[f].seq, bf = (int)f;
    return bf;
}

/* (kind, index)'s nth newest version whose payload CRC holds (0: the current one, 1: the one before ...): its frame,
 * *len its length; -1 none */
static int fs_find(uint32_t kind, uint32_t index, uint32_t nth, uint32_t *len)
{
    uint32_t below = 0xFFFFFFFFu, c;
    int f;
    st_hdr_t h;
    if (!fs.scanned)
        fs_scan();
    while ((f = fs_version(kind, index, below)) >= 0) {
        below = fs.f[f].seq;
        if (fs_head((uint32_t)f, &h) || h.seq != fs.f[f].seq || h.type != kind || h.slot != index)
            continue;                            /* (changed under the table: skipped) */
        if (st_stream(fs_base((uint32_t)f) + ST_PAYLOAD_OFF, h.len, 0, 0, &c) || c != h.crc)
            continue;                            /* (rotten: the version before) */
        if (nth-- == 0) {
            *len = h.len;
            return f;
        }
    }
    return -1;
}

/* (1.5.0, the Set page's Load) (kind, index) has a version, by the headers alone (the frame table, in RAM: no payload
 * read, so cheap enough for every frame's drawing); a payload gone bad is found when it is read (fs_find) */
static int fs_has(uint32_t kind, uint32_t index)
{
    if (!fs.scanned)
        fs_scan();
    return fs_version(kind, index, 0xFFFFFFFFu) >= 0;
}

static int fs_read(int f, uint32_t off, void *dst, uint32_t n)   /* inside the frame's payload: 0 */
{
    if (f < 0 || (uint32_t)f >= FS_N || off > FS_MAX || n > FS_MAX - off)
        return -1;
    return st_read(fs_base((uint32_t)f) + ST_PAYLOAD_OFF + off, dst, n) ? -1 : 0;
}

/* where a save goes: a frame holding no object's newest version. Rank: 0 headerless and known erased, 1 headerless,
 * 2 a superseded version (the oldest first), 3 an object's second newest (the oldest first); skip_ready: not rank 0
 * (fs_prepare's next). -1: none */
static int fs_pick(int skip_ready)
{
    uint32_t f, s;
    int best = -1, br = 9;
    uint32_t bseq = 0xFFFFFFFFu;
    for (f = 0; f < FS_N; f++) {
        int r;
        if (fs.keep && f == fs.keep - 1u)
            continue;
        if (fs.f[f].kind == 0) {
            int ready = 1;
            for (s = 0; s < FS_SECS; s++)
                ready &= fs_blank_is(f, s);
            if (ready && skip_ready)
                continue;
            r = ready ? 0 : 1;
        } else {
            const int newest = fs_version(fs.f[f].kind, fs.f[f].index, 0xFFFFFFFFu);
            if (newest == (int)f)
                continue;                        /* (an object's current: never) */
            r = fs_version(fs.f[f].kind, fs.f[f].index, fs.f[newest].seq) == (int)f ? 3 : 2;
        }
        if (r < br || (r == br && r >= 2 && fs.f[f].seq < bseq)) {
            best = (int)f, br = r, bseq = fs.f[f].seq;
        }
    }
    return best;
}

static int fs_sector_blank(uint32_t f, uint32_t s)   /* read so (and remembered): 1 */
{
    uint32_t b[64], at, i;
    if (fs_blank_is(f, s))
        return 1;
    for (at = 0; at < ST_SECTOR; at += sizeof b) {
        if (st_read(fs_base(f) + s * ST_SECTOR + at, b, sizeof b))
            return 0;
        for (i = 0; i < sizeof b / 4u; i++)
            if (b[i] != 0xFFFFFFFFu)
                return 0;
    }
    fs_blank_set(f, s, 1);
    return 1;
}

static int fs_clear(uint32_t f, uint32_t nsec, int may_erase)   /* sectors 0..nsec-1 of frame f erased: 0 */
{
    uint32_t s;
    int rc;
    for (s = 0; s < nsec; s++) {
        if (fs_sector_blank(f, s))
            continue;
        if (!may_erase)
            return FS_BUSY;
        if (s == 0)
            fs.f[f].kind = 0, fs.f[f].index = 0, fs.f[f].seq = 0;   /* (its header gone) */
        if ((rc = st_erase(fs_base(f) + s * ST_SECTOR)) != 0)
            return rc;
        fs_blank_set(f, s, 1);
    }
    return 0;
}

/* open a frame for a new version of (kind, index), len bytes; may_erase 0 (playing): only a frame already erased as
 * far as len needs (FS_BUSY otherwise). 0: open, the bytes go in with fs_put, then fs_commit (or fs_abort) */
static int fs_begin(uint32_t kind, uint32_t index, uint32_t len, int may_erase)
{
    uint32_t nsec, s;
    int f, rc;
    if (!fs.scanned)
        fs_scan();
    fs.wf = -1;
    if (kind == 0 || kind > 0xFFFFu || index > 0xFFFFu || len == 0 || len > FS_MAX)
        return -2;
    if ((f = fs_pick(0)) < 0)
        return FS_FULL;
    nsec = (ST_PAYLOAD_OFF + len + ST_SECTOR - 1u) / ST_SECTOR;
    if ((rc = fs_clear((uint32_t)f, nsec, may_erase)) != 0)
        return rc;
    for (s = 0; s < nsec; s++)
        fs_blank_set((uint32_t)f, s, 0);         /* (written from now on) */
    fs.f[f].kind = 0, fs.f[f].index = 0, fs.f[f].seq = 0;
    fs.wf = (int8_t)f, fs.wkind = (uint16_t)kind, fs.windex = (uint16_t)index;
    fs.wlen = len, fs.woff = 0, fs.wcrc = 0xFFFFFFFFu, fs.werr = 0, fs.wdone = 0;
    return 0;
}

static int fs_put(const void *src, uint32_t n)   /* the next n bytes of the open version: 0 */
{
    const uint8_t *p = src;
    if (fs.wf < 0)
        return -1;
    if (fs.werr)
        return fs.werr;
    if (fs.wdone || n > fs.wlen - fs.woff)
        return fs.werr = -3;
    fs.wcrc = st_crc_up(fs.wcrc, p, n);
    while (n--) {
        fs.page[fs.woff & 255u] = *p++;
        if ((++fs.woff & 255u) == 0u) {
            int rc = st_prog(fs_base((uint32_t)fs.wf) + ST_PAYLOAD_OFF + fs.woff - 256u, fs.page, 256u);
            if (rc)
                return fs.werr = rc;
        }
    }
    return 0;
}

/* the open version's bytes all in: the last page, the payload read back and its CRC checked. The frame stays open,
 * headerless (nothing current changed), for fs_seal; a version may be looked at first (fs_read: (1.5.0) a set sent in,
 * checked whole and confirmed on the device before it counts). The frame (>= 0), or < 0 and it is left free */
static int fs_finish(void)
{
    const int f = fs.wf;
    uint32_t c, tail;
    int rc;
    if (f < 0)
        return -1;
    if (fs.werr || fs.woff != fs.wlen || fs.wdone) {
        fs.wf = -1;
        return fs.werr ? fs.werr : -3;
    }
    tail = fs.woff & 255u;
    if (tail && (rc = st_prog(fs_base((uint32_t)f) + ST_PAYLOAD_OFF + fs.woff - tail, fs.page, tail)) != 0) {
        fs.wf = -1;
        return rc;
    }
    if (st_stream(fs_base((uint32_t)f) + ST_PAYLOAD_OFF, fs.wlen, 0, 0, &c) || c != ~fs.wcrc) {
        fs.wf = -1;
        return -7;                               /* (read back: a failing part must not report SAVED) */
    }
    fs.wdone = 1;
    return f;
}

/* the finished version committed: its header programmed, LAST, and read back. 0: the newest version of its object;
 * else the frame is left headerless (free) */
static int fs_seal(void)
{
    const int f = fs.wf;
    int rc;
    st_hdr_t h, chk;
    if (f < 0 || !fs.wdone)
        return -1;
    fs.wf = -1;
    h.magic = FS_MAGIC;
    h.type = fs.wkind, h.slot = fs.windex;
    h.seq = fs.top + 1u;
    h.len = fs.wlen;
    h.crc = ~fs.wcrc;
    h.rsv[0] = h.rsv[1] = 0xFFFFFFFFu;
    h.hcrc = st_crc32(&h, sizeof h - 4u);
    if ((rc = st_prog(fs_base((uint32_t)f), &h, sizeof h)) != 0)   /* the commit record, last */
        return rc;
    if (fs_head((uint32_t)f, &chk) || chk.seq != h.seq || chk.crc != h.crc || chk.len != h.len)
        return -7;
    fs.f[f].kind = fs.wkind, fs.f[f].index = fs.windex, fs.f[f].seq = h.seq;
    fs.top = h.seq;
    return 0;
}

/* the open version complete: fs_finish, then fs_seal. 0: committed (the newest version of its object); else the frame
 * is left headerless (free) */
static int fs_commit(void)
{
    const int f = fs_finish();
    return f < 0 ? f : fs_seal();
}

static int fs_open_frame(void) { return fs.wf; }   /* (1.5.0) the frame being written (-1 none): a set held for the
                                                  * device's yes is still open, fs_begin would drop it */

static void fs_abort(void) { fs.wf = -1; }       /* (the frame left headerless: free) */
static void fs_keep(int f) { fs.keep = (uint8_t)(f >= 0 && (uint32_t)f < FS_N ? f + 1 : 0); }   /* (step 7) */

/* every version of (kind, index) gone: the first sector (the header) of each frame erased, the newest first (a cut
 * between leaves an older version: delete again). Erases: only while stopped. 0, or the erase's error */
static int fs_delete(uint32_t kind, uint32_t index)
{
    int f, rc;
    if (!fs.scanned)
        fs_scan();
    fs.wf = -1;
    while ((f = fs_version(kind, index, 0xFFFFFFFFu)) >= 0) {
        fs.f[f].kind = 0, fs.f[f].index = 0, fs.f[f].seq = 0;
        if ((rc = st_erase(fs_base((uint32_t)f))) != 0)
            return rc;
        fs_blank_set((uint32_t)f, 0, 1);
    }
    return 0;
}

/* (stopped: erases) FS_PREP frames erased ahead, whole, the ones the next saves will take: a save while playing then
 * programs pages only. Each frame read first: one already erased costs no erase (and a boot finds them so). Returns
 * the sectors erased; < 0 an error */
static int fs_prepare(void)
{
    uint32_t f, s, ready = 0;
    int n = 0, rc, p;
    if (!fs.scanned)
        fs_scan();
    if (fs.wf >= 0)
        return 0;                                /* (a save open: not now) */
    for (f = 0; f < FS_N; f++) {
        int r = fs.f[f].kind == 0;
        for (s = 0; s < FS_SECS && r; s++)
            r = fs_blank_is(f, s);
        ready += (uint32_t)r;
    }
    while (ready < FS_PREP && (p = fs_pick(1)) >= 0) {
        for (s = 0; s < FS_SECS; s++)
            n += !fs_blank_is((uint32_t)p, s) && !fs_sector_blank((uint32_t)p, s);
        if ((rc = fs_clear((uint32_t)p, FS_SECS, 1)) != 0)
            return rc;
        fs.f[p].kind = 0, fs.f[p].index = 0, fs.f[p].seq = 0;   /* (a superseded version: gone, free) */
        ready++;
    }
    return n;
}
