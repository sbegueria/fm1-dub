/* SPDX-License-Identifier: GPL-3.0-only */
/* (1.5.0, step 4) the set protocol (firmware/src/app/setwire.c) on its own, against a computer doing what
 * tools/dub1_set.py does, with the real frame store (storage.c) on a simulated NOR and the real container codec; the
 * project's side (project_set_*, project_work_bytes) stubbed: a set is one whose container reads and says SET. Every
 * frame within usb.c's (640 bytes in between F0 and F7, 192 packets of 3 out); HELLO, STATUS; BUSY while playing and
 * while asking; BEGIN / DATA (512-byte chunks) / COMMIT of sets up to a frame's 16,128 bytes; a chunk sent again when
 * its answer was lost; a bad checksum unanswered; the NAKs; the question's yes (sealed, read back the same in 256-byte
 * chunks), no, 30 s, the music started; a transfer gone quiet; a power cut in a transfer (the set as it was); APPLY,
 * DELETE, LIST of eight sets and WORK; (Decisions 4) RESTORE: one question, its yes keeping the sets it named and the
 * pieces unasked, the rest asked, its ends (all came, 10 s, CANCEL, the music), a COMMIT's answer lost; (Decisions 5)
 * its stop at the first set that fails on the flash (a BEGIN, a seal), the sets kept before it; then 400 transfers over a
 * wire that loses, garbles and repeats frames, each
 * with the retries the tool makes: every set kept exactly as sent, or not at all.
 *   cc -o set_wire_test tests/host/set_wire_test.c && ./set_wire_test */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

/* ---------------------------------------------------------------- the flash: a NOR, a power cut at an op --- */
static uint8_t nor[0x100000];
static long ops, cut_at = -1;
static int dead;
static int st_read(uint32_t off, void *dst, uint32_t n) { memcpy(dst, nor + off, n); return 0; }
static int cut_now(void)
{
    if (dead)
        return -1;
    if (cut_at >= 0 && ++ops == cut_at) {
        dead = 1;
        return -1;
    }
    return 0;
}
static int st_erase(uint32_t off)
{
    if ((off & 0xFFFu) || cut_now())
        return -9;
    memset(nor + off, 0xFF, 4096);
    return 0;
}
static int st_prog(uint32_t off, const void *src, uint32_t n)
{
    const uint8_t *s = src;
    uint32_t i;
    if ((off & 0xFFu) + n > 256u || cut_now())
        return -9;
    for (i = 0; i < n; i++)
        nor[off + i] &= s[i];
    return 0;
}
#include "../../firmware/src/app/plat.h"
#include "../../firmware/src/storage.c"
#include "../../firmware/src/app/container.c"
#include "../../firmware/src/app/names.h"

/* ---------------------------------------------------------------- what setwire.c takes from the app --- */
#define EN_VERSION "TEST"
#define PROJ_FORMAT 37u
#define PROJ_NAME 12
#define NPIECES 8
static volatile uint8_t en_playing;
static struct {
    char name[PROJ_NAME + 1];
} proj = {"Live"};
void plat_fs_scan(void) { fs_scan(); }
int plat_fs_find(uint32_t kind, uint32_t index, uint32_t nth, uint32_t *len) { return fs_find(kind, index, nth, len); }
int plat_fs_read(int frame, uint32_t off, void *dst, uint32_t n) { return fs_read(frame, off, dst, n); }
int plat_fs_begin(uint32_t kind, uint32_t index, uint32_t len, int may_erase) { return fs_begin(kind, index, len, may_erase); }
int plat_fs_put(const void *src, uint32_t n) { return fs_put(src, n); }
int plat_fs_finish(void) { return fs_finish(); }
static int seal_fail;                            /* (Decisions 5) the next seal fails, as the flash would (no header) */
int plat_fs_seal(void)
{
    if (seal_fail) {
        seal_fail = 0;
        fs_abort();
        return -9;
    }
    return fs_seal();
}
int plat_fs_open(void) { return fs_open_frame(); }
void plat_fs_abort(void) { fs_abort(); }
int plat_fs_delete(uint32_t kind, uint32_t index) { return fs_delete(kind, index); }
/* project.c's side, stubbed: the container read (shape, CRC), a SET, its META's name (names.h's) */
static int checks, takes, take_rc;
static int fget(void *ctx, uint32_t off, void *dst, uint32_t n) { return fs_read(*(const int *)ctx, off, dst, n); }
static int project_set_info(int f, uint32_t len, char *name, uint32_t *np)
{
    d1c_r_t r;
    d1c_meta_t m;
    uint32_t at = 0, id, pay, n, px;
    int rc = d1c_open(&r, fget, &f, len);
    if (rc)
        return rc;
    if (r.h.kind != FS_SET)
        return D1C_ESHAPE;
    if (r.h.format > PROJ_FORMAT)
        return D1C_ENEWER;
    name[0] = 0;
    *np = r.h.npieces;
    while (d1c_next(&r, &at, &id, &pay, &n))
        if (id == D1C_META) {
            if ((rc = d1c_read_meta(&r, pay, n, &m)) != 0)
                return rc;
            if (!name_fits(m.name, D1C_NAME, &px))
                return D1C_EBAD;
            memcpy(name, m.name, PROJ_NAME);
            name[PROJ_NAME] = 0;
        }
    return 0;
}
static int project_set_check(int f, uint32_t len, char *name, uint32_t *np)
{
    checks++;
    return project_set_info(f, len, name, np);
}
static int project_set_take(int f, uint32_t len)
{
    (void)f, (void)len;
    takes++;
    return take_rc;
}
/* (Decisions 4) a restore's pieces: a container that reads and says WORK; taken: counted */
static int wtakes, wtake_rc, wtake_open;
static int project_work_check(int f, uint32_t len, char *name)
{
    d1c_r_t r;
    d1c_meta_t m;
    uint32_t at = 0, id, pay, n;
    int rc = d1c_open(&r, fget, &f, len);
    if (rc)
        return rc;
    if (r.h.kind != FS_WORK)
        return D1C_ESHAPE;
    name[0] = 0;
    while (d1c_next(&r, &at, &id, &pay, &n))
        if (id == D1C_META && d1c_read_meta(&r, pay, n, &m) == 0) {
            memcpy(name, m.name, PROJ_NAME);
            name[PROJ_NAME] = 0;
        }
    return 0;
}
static int project_work_take(void)
{
    wtake_open += plat_fs_open() >= 0;           /* (taken with their frame still open: a fault) */
    wtakes++;
    return wtake_rc;
}
static uint8_t work[4184];                       /* (WORK as it is: any bytes, the same at every chunk) */
static uint32_t project_work_bytes(uint32_t off, uint8_t *dst, uint32_t n)
{
    if (n)
        memcpy(dst, work + off, n);
    return sizeof work;
}
/* (the Set page's Store, sxw_store: the simulator's scenarios test it, storeknob.amb; stubbed here) */
static int project_set_name(int f, uint32_t len, char *name)
{
    uint32_t np;
    return project_set_info(f, len, name, &np);
}
static int project_save(void) { return 0; }
static int project_set_write(uint32_t ix, uint32_t n, const char *name, uint32_t *len)
{
    (void)ix, (void)n, (void)name, (void)len;
    return -1;
}
/* the wire out: the last frame (usb.c's send ring holds 192 packets of 3 bytes) */
static uint8_t tx[4096];
static uint32_t tx_n, tx_count;
static int fails;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); fails++; } } while (0)
static int sxw_wire(const uint8_t *p, uint32_t n)
{
    CHECK(n <= 192u * 3u, "an answer of %u bytes: over usb.c's send ring", n);
    memcpy(tx, p, n);
    tx_n = n, tx_count++;
    return 0;
}
#include "../../firmware/src/app/setwire.c"

/* ---------------------------------------------------------------- the computer's side --- */
static uint32_t now = 1000, seq;
static uint8_t ans[1024];
static uint32_t ans_n, ans_cmd;
static uint32_t rnd_s = 7u;
static uint32_t rnd(void)
{
    rnd_s ^= rnd_s << 13;
    rnd_s ^= rnd_s >> 17;
    rnd_s ^= rnd_s << 5;
    return rnd_s;
}
/* a request as tools/dub1_set.py frames it; how the wire treats it: 0 fine, 1 lost on the way in, 2 garbled (a bit
 * flipped: the checksum then wrong, or a byte the 7-bit range: either way unanswered), 3 its answer lost. 1: answered */
static int ask_how(uint32_t cmd, const uint8_t *body, uint32_t n, int how)
{
    static uint8_t f[1024];
    uint32_t w = 0, s = 0, i;
    const uint32_t c0 = tx_count;
    f[w++] = 0x7D, f[w++] = 0x44, f[w++] = 0x31, f[w++] = (uint8_t)cmd, f[w++] = (uint8_t)seq;
    w += sxw_pack7(body, n, f + w);
    for (i = 3; i < w; i++)
        s += f[i];
    f[w++] = (uint8_t)(s & 0x7Fu);
    CHECK(w <= 640u, "a request of %u bytes: over usb.c's frame", w);
    if (how == 1)
        return 0;
    if (how == 2)
        f[5 + rnd() % (w - 5u)] ^= (uint8_t)(1u << (rnd() % 7u));
    CHECK(sxw_frame(f, w, now), "a set frame not taken");
    if (tx_count == c0)
        return 0;
    for (s = 0, i = 4; i + 2u < tx_n; i++)
        s += tx[i];
    CHECK(tx[0] == 0xF0 && tx[1] == 0x7D && tx[2] == 0x44 && tx[3] == 0x31 && tx[5] == (seq & 0x7Fu) &&
          tx[tx_n - 1u] == 0xF7 && (s & 0x7Fu) == tx[tx_n - 2u], "an answer's frame");
    if (how == 3)
        return 0;
    ans_cmd = tx[4];
    ans_n = sxw_unpack7(tx + 6, tx_n - 8u, ans, sizeof ans);
    return 1;
}
static int ask(uint32_t cmd, const uint8_t *body, uint32_t n)
{
    seq = (seq + 1u) & 0x7Fu;
    return ask_how(cmd, body, n, 0);
}
static uint32_t rd16(const uint8_t *p) { return p[0] | (uint32_t)p[1] << 8; }

/* a set container of n bytes (rounded up to 4; n >= 160: META with its name, a PIEC, an unknown chunk to fill it) */
static int mput(void *ctx, const void *p, uint32_t n)
{
    uint8_t **d = ctx;
    memcpy(*d, p, n);
    *d += n;
    return 0;
}
static uint32_t make_set(uint8_t *b, uint32_t n, const char *name, uint32_t kind, uint32_t format, uint32_t salt)
{
    static uint8_t fill[16384];
    d1c_w_t w;
    d1c_hdr_t h;
    d1c_meta_t m;
    d1c_piece_t pc;
    int16_t par[4] = {1, 2, 3, (int16_t)salt};
    uint8_t *at = b;
    uint32_t i, pass, total = 0, nf = n - 32u - 52u - 48u - 8u - 12u;
    for (i = 0; i < nf; i++)
        fill[i] = (uint8_t)(salt * 31u + i * 7u);
    for (pass = 0; pass < 2u; pass++) {
        at = b;
        d1c_w_init(&w, pass ? mput : 0, &at);
        memset(&h, 0, sizeof h);
        h.total = total, h.format = format, h.kind = (uint16_t)kind, h.npieces = 1;
        d1c_header(&w, &h);
        memset(&m, 0, sizeof m);
        d1c_name_set(m.name, name);
        d1c_meta(&w, &m);
        memset(&pc, 0, sizeof pc);
        pc.kind = D1C_FULL, pc.nparams = 4;
        d1c_piece(&w, &pc, 0, par);
        d1c_raw(&w, D1C_ID('X', 'F', 'I', 'L'), fill, nf);
        d1c_end(&w);
        total = w.len;
    }
    return total;
}
/* the set sent as the tool sends it: BEGIN, DATA in 512-byte chunks, COMMIT, each request tried up to 5 times (no
 * answer: again); hows: how the wire treats each try (0: all fine). The answer to COMMIT (or the NAK, or 0 none) */
static int (*wire_how)(void);
static int fine(void) { return 0; }
static int rough(void)                           /* (a bad wire: 3% lost, 3% garbled, 4% the answer lost) */
{
    const uint32_t r = rnd() % 100u;
    return r < 3u ? 1 : r < 6u ? 2 : r < 10u ? 3 : 0;
}
static int lose_at;
static int lose_nth(void) { return --lose_at == 0 ? 3 : 0; }   /* (the lose_at'th try's answer lost) */
static int try_ask(uint32_t cmd, const uint8_t *body, uint32_t n)
{
    int t;
    seq = (seq + 1u) & 0x7Fu;
    for (t = 0; t < 5; t++) {
        if (ask_how(cmd, body, n, wire_how()))
            return 1;
        now += 2000u;                            /* (2 s, no answer: again) */
    }
    return 0;
}
static uint32_t send_obj(uint32_t kind, uint32_t slot, const uint8_t *b, uint32_t n)   /* (kind 1: a restore's pieces) */
{
    uint8_t q[600];
    const uint32_t crc = ~d1c_crc_up(0xFFFFFFFFu, b, n);
    uint32_t off;
    q[0] = (uint8_t)kind, q[1] = (uint8_t)slot, q[2] = (uint8_t)n, q[3] = (uint8_t)(n >> 8);
    q[4] = (uint8_t)crc, q[5] = (uint8_t)(crc >> 8), q[6] = (uint8_t)(crc >> 16), q[7] = (uint8_t)(crc >> 24);
    if (!try_ask(SXW_BEGIN, q, 8))
        return 0;
    if (ans_cmd != SXW_BEGIN)
        return ans_cmd;
    for (off = 0; off < n; off += 512u) {
        const uint32_t k = n - off > 512u ? 512u : n - off;
        q[0] = (uint8_t)off, q[1] = (uint8_t)(off >> 8);
        memcpy(q + 2, b + off, k);
        if (!try_ask(SXW_DATA, q, 2u + k))
            return 0;
        if (ans_cmd != SXW_DATA)
            return ans_cmd;
        CHECK(rd16(ans) == off && rd16(ans + 2) == k && rd16(ans + 4) == off + k, "DATA's answer at %u", off);
        now += 30u;
    }
    if (!try_ask(SXW_COMMIT, 0, 0))
        return 0;
    if (ans_cmd == SXW_BUSY && ans[1] == SXW_B_ASKING && try_ask(SXW_STATUS, 0, 0))
        return ans[0] == SXW_COMMIT && ans[1] == SXW_S_ASKING && ans[3] == slot ? SXW_COMMIT : 0;   /* (COMMIT's
                                                  * answer lost, sent again: asking already) */
    if (ans_cmd == SXW_NAK && ans[1] == SXW_E_OPEN && try_ask(SXW_STATUS, 0, 0))
        return ans[0] == SXW_COMMIT && ans[1] == SXW_S_YES && ans[2] == kind && ans[3] == slot ? SXW_COMMIT : 0;   /* (a
                                                  * restore's: kept at once, its answer lost; sent again: nothing open) */
    return ans_cmd;
}
static uint32_t send_set(uint32_t slot, const uint8_t *b, uint32_t n) { return send_obj(FS_SET, slot, b, n); }
static int stored_is(uint32_t slot, const uint8_t *b, uint32_t n)   /* set slot stored as b: 1 */
{
    static uint8_t got[16384];
    uint32_t len;
    const int f = fs_find(FS_SET, slot, 0, &len);
    return f >= 0 && len == n && !fs_read(f, 0, got, n) && !memcmp(got, b, n);
}
static void reboot(void)
{
    dead = 0, cut_at = -1, ops = 0;
    fs.scanned = 0;
    fs_scan();
}

int main(void)
{
    static uint8_t a[16384], b2[16384], back[16384];
    uint8_t q[600];
    uint32_t i, n, na, nb, off, len;
    wire_how = fine;
    memset(nor, 0xFF, sizeof nor);
    for (i = 0; i < sizeof work; i++)
        work[i] = (uint8_t)(i * 13u + 5u);
    fs_scan();

    /* HELLO; another's SysEx left alone; a bad checksum unanswered; while playing, BUSY "stop first" but HELLO, STATUS,
     * LIST and READ (Decisions 3: read-only) */
    CHECK(ask(SXW_HELLO, 0, 0) && ans_cmd == SXW_HELLO && ans_n == 48u && ans[0] == 1 && rd16(ans + 1) == 512u &&
          rd16(ans + 3) == 256u && rd16(ans + 5) == PROJ_FORMAT && rd16(ans + 9) == PLAT_FS_MAX && ans[11] == 8 &&
          !memcmp(ans + 16, "DUB-1 TEST", 10), "HELLO");
    {
        static const uint8_t other[] = {0x7D, 0x53, 0x52, 0x01, 0x00, 0x01};
        CHECK(!sxw_frame(other, sizeof other, now), "a screen reader's frame taken");
    }
    seq++;
    CHECK(!ask_how(SXW_LIST, 0, 0, 2) && sxw.bad == 1u, "a garbled frame answered");
    en_playing = 1;
    CHECK(ask(SXW_LIST, 0, 0) && ans_cmd == SXW_LIST && ans[0] == 1u && ans[1] == FS_WORK, "LIST while playing");
    q[0] = FS_WORK, q[1] = 0, q[2] = 0, q[3] = 0, q[4] = 0, q[5] = 1;
    CHECK(ask(SXW_READ, q, 6) && ans_cmd == SXW_READ && rd16(ans + 2) > 0u && ans_n == 4u + 256u, "READ while playing");
    q[0] = FS_SET, q[1] = 0;
    CHECK(ask(SXW_APPLY, q, 2) && ans_cmd == SXW_BUSY && ans[0] == SXW_APPLY && ans[1] == SXW_B_PLAYING &&
          ans_n == 12u && !memcmp(ans + 2, "stop first", 10), "APPLY while playing: BUSY stop first");
    CHECK(ask(SXW_DELETE, q, 2) && ans_cmd == SXW_BUSY && ans[1] == SXW_B_PLAYING, "DELETE while playing: BUSY");
    CHECK(ask(SXW_HELLO, 0, 0) && ans_cmd == SXW_HELLO && (ans[12] & 1u), "HELLO while playing: said so");
    CHECK(ask(SXW_STATUS, 0, 0) && ans_cmd == SXW_STATUS && ans[1] == SXW_S_IDLE, "STATUS while playing");
    na = make_set(a, 3000, "Winter", FS_SET, PROJ_FORMAT, 1);
    CHECK(send_set(0, a, na) == SXW_BUSY, "BEGIN while playing: BUSY");
    en_playing = 0;

    /* BEGIN's refusals */
    q[0] = FS_WORK, q[1] = 0, q[2] = 0, q[3] = 4, q[4] = q[5] = q[6] = q[7] = 0;
    CHECK(ask(SXW_BEGIN, q, 8) && ans_cmd == SXW_NAK && ans[1] == SXW_E_BAD, "BEGIN of a WORK: NAK");
    q[0] = FS_SET, q[1] = 8;
    CHECK(ask(SXW_BEGIN, q, 8) && ans_cmd == SXW_NAK && ans[1] == SXW_E_BAD, "BEGIN of set 9: NAK");
    q[1] = 0, q[2] = (uint8_t)(PLAT_FS_MAX + 1u), q[3] = (uint8_t)((PLAT_FS_MAX + 1u) >> 8);
    CHECK(ask(SXW_BEGIN, q, 8) && ans_cmd == SXW_NAK && ans[1] == SXW_E_BIG && rd16(ans + 2) == PLAT_FS_MAX, "BEGIN too big");
    CHECK(ask(SXW_DATA, q, 4) && ans_cmd == SXW_NAK && ans[1] == SXW_E_OPEN, "DATA with nothing begun");
    CHECK(ask(SXW_COMMIT, 0, 0) && ans_cmd == SXW_NAK && ans[1] == SXW_E_OPEN, "COMMIT with nothing begun");

    /* the biggest set a frame holds, its third chunk's answer lost and sent again, a garbled one between: asked; yes */
    na = make_set(a, PLAT_FS_MAX, "Echo Set", FS_SET, PROJ_FORMAT, 2);
    CHECK(na == PLAT_FS_MAX, "a set of %u bytes", na);
    {
        const uint32_t crc = ~d1c_crc_up(0xFFFFFFFFu, a, na);
        q[0] = FS_SET, q[1] = 2, q[2] = (uint8_t)na, q[3] = (uint8_t)(na >> 8);
        q[4] = (uint8_t)crc, q[5] = (uint8_t)(crc >> 8), q[6] = (uint8_t)(crc >> 16), q[7] = (uint8_t)(crc >> 24);
        CHECK(ask(SXW_BEGIN, q, 8) && ans_cmd == SXW_BEGIN && !memcmp(ans, q, 8), "BEGIN");
        for (off = 0; off < na; off += 512u) {
            const uint32_t k = na - off > 512u ? 512u : na - off;
            q[0] = (uint8_t)off, q[1] = (uint8_t)(off >> 8);
            memcpy(q + 2, a + off, k);
            seq = (seq + 1u) & 0x7Fu;
            if (off == 1024u)
                CHECK(!ask_how(SXW_DATA, q, 2u + k, 3), "an answer to be lost");
            if (off == 2048u)
                CHECK(!ask_how(SXW_DATA, q, 2u + k, 2), "a garbled DATA answered");
            CHECK(ask_how(SXW_DATA, q, 2u + k, 0) && ans_cmd == SXW_DATA && rd16(ans + 4) == off + k, "DATA at %u", off);
            if (off == 4096u) {                  /* (out of order: refused, where it is said) */
                q[0] = 0, q[1] = 0x20;
                CHECK(ask(SXW_DATA, q, 2u + 512u) && ans_cmd == SXW_NAK && ans[1] == SXW_E_ORDER &&
                      rd16(ans + 2) == off + k, "DATA out of order");
                CHECK(ask(SXW_COMMIT, 0, 0) && ans_cmd == SXW_NAK && ans[1] == SXW_E_ORDER, "COMMIT short");
            }
        }
        q[0] = (uint8_t)na, q[1] = (uint8_t)(na >> 8);
        CHECK(ask(SXW_DATA, q, 3) && ans_cmd == SXW_NAK && ans[1] == SXW_E_ORDER, "DATA past the end");
        CHECK(fs_find(FS_SET, 2, 0, &len) < 0, "the set there before its yes");
        CHECK(ask(SXW_COMMIT, 0, 0) && ans_cmd == SXW_COMMIT && ans[1] == SXW_S_ASKING && ans[3] == 2 && ans[4] == 1 &&
              !memcmp(ans + 5, "Echo Set", 9), "COMMIT: asking, its name");
    }
    CHECK(sxw_asking() == SXW_COMMIT && !strcmp(sxw.name, "Echo Set"), "the question up");
    CHECK(ask(SXW_LIST, 0, 0) && ans_cmd == SXW_BUSY && ans[1] == SXW_B_ASKING, "LIST while asking: BUSY");
    CHECK(ask(SXW_STATUS, 0, 0) && ans[0] == SXW_COMMIT && ans[1] == SXW_S_ASKING, "STATUS: asking");
    CHECK(plat_fs_open() >= 0 && fs_prepare() == 0, "the store held while asking");
    CHECK(sxw_answer(1) == SXW_S_YES && stored_is(2, a, na), "yes: stored");
    CHECK(ask(SXW_STATUS, 0, 0) && ans[0] == SXW_COMMIT && ans[1] == SXW_S_YES, "STATUS: yes");
    reboot();
    CHECK(stored_is(2, a, na), "... and after a reboot");
    for (off = 0; off < na; off += 256u) {       /* READ: as stored, 256 bytes a time */
        q[0] = FS_SET, q[1] = 2, q[2] = (uint8_t)off, q[3] = (uint8_t)(off >> 8), q[4] = 0, q[5] = 1;
        if (!ask(SXW_READ, q, 6) || ans_cmd != SXW_READ || rd16(ans) != off || rd16(ans + 2) != na)
            break;
        memcpy(back + off, ans + 4, ans_n - 4u);
    }
    CHECK(off >= na && !memcmp(back, a, na), "READ back the same");
    q[2] = (uint8_t)na, q[3] = (uint8_t)(na >> 8);
    CHECK(ask(SXW_READ, q, 6) && ans_cmd == SXW_NAK && ans[1] == SXW_E_BAD, "READ past the end");
    q[0] = FS_SET, q[1] = 5, q[2] = q[3] = 0;
    CHECK(ask(SXW_READ, q, 6) && ans_cmd == SXW_NAK && ans[1] == SXW_E_NONE, "READ of none");
    for (off = 0, n = 0; off < sizeof work; off += 256u) {   /* WORK as it is */
        q[0] = FS_WORK, q[1] = 0, q[2] = (uint8_t)off, q[3] = (uint8_t)(off >> 8), q[4] = 0, q[5] = 1;
        n += ask(SXW_READ, q, 6) && ans_cmd == SXW_READ && rd16(ans + 2) == sizeof work &&
             !memcmp(ans + 4, work + off, ans_n - 4u);
    }
    CHECK(n == (sizeof work + 255u) / 256u, "READ WORK");

    /* no: the frame free again, the set as it was; 30 s; the music started; a transfer gone quiet; a CRC not the host's;
     * not a set; a newer DUB-1's */
    nb = make_set(b2, 5000, "Other", FS_SET, PROJ_FORMAT, 3);
    CHECK(send_set(2, b2, nb) == SXW_COMMIT && sxw_answer(0) == SXW_S_NO && stored_is(2, a, na) && plat_fs_open() < 0,
          "no: the old one kept");
    CHECK(send_set(2, b2, nb) == SXW_COMMIT, "again");
    sxw_tick(now + SXW_ASK_MS - 1u);
    CHECK(sxw_asking(), "29.9 s: still asking");
    sxw_tick(now + SXW_ASK_MS + 1u);
    CHECK(!sxw_asking() && sxw.state == SXW_S_TIMEOUT && stored_is(2, a, na) && plat_fs_open() < 0, "30 s: no");
    CHECK(send_set(2, b2, nb) == SXW_COMMIT, "again");
    en_playing = 1;
    sxw_tick(now);
    en_playing = 0;
    CHECK(!sxw_asking() && sxw.state == SXW_S_PLAYING && stored_is(2, a, na), "the music started: no");
    q[0] = FS_SET, q[1] = 2, q[2] = (uint8_t)nb, q[3] = (uint8_t)(nb >> 8), q[4] = q[5] = q[6] = q[7] = 0;
    CHECK(ask(SXW_BEGIN, q, 8) && ans_cmd == SXW_BEGIN, "a BEGIN");
    sxw_tick(now + SXW_IDLE_MS + 1u);
    CHECK(!sxw.open && plat_fs_open() < 0, "quiet 10 s: dropped");
    CHECK(ask(SXW_DATA, b2, 100) && ans_cmd == SXW_NAK && ans[1] == SXW_E_OPEN, "... DATA then: nothing open");
    CHECK(ask(SXW_BEGIN, q, 8) && ans_cmd == SXW_BEGIN, "a BEGIN, its CRC wrong");
    for (off = 0; off < nb; off += 512u) {
        const uint32_t k = nb - off > 512u ? 512u : nb - off;
        q[0] = (uint8_t)off, q[1] = (uint8_t)(off >> 8);
        memcpy(q + 2, b2 + off, k);
        ask(SXW_DATA, q, 2u + k);
    }
    CHECK(ask(SXW_COMMIT, 0, 0) && ans_cmd == SXW_NAK && ans[1] == SXW_E_BAD && plat_fs_open() < 0, "COMMIT: CRC not the host's");
    nb = make_set(b2, 2000, "Work", FS_WORK, PROJ_FORMAT, 4);
    CHECK(send_set(3, b2, nb) == SXW_NAK && ans[1] == SXW_E_SET && fs_find(FS_SET, 3, 0, &len) < 0, "a WORK: NAK at once");
    nb = make_set(b2, 2000, "Later", FS_SET, PROJ_FORMAT + 1u, 5);
    CHECK(send_set(3, b2, nb) == SXW_NAK && ans[1] == SXW_E_NEWER && rd16(ans + 2) == PROJ_FORMAT + 1u, "a newer DUB-1's: NAK");
    nb = make_set(b2, 2000, "WWWWWWWW", FS_SET, PROJ_FORMAT, 6);
    CHECK(send_set(3, b2, nb) == SXW_NAK && ans[1] == SXW_E_SET && fs_find(FS_SET, 3, 0, &len) < 0 && plat_fs_open() < 0,
          "a name too wide: NAK at COMMIT");

    /* a power cut while a set comes in: after it, the set as it was */
    nb = make_set(b2, 9000, "Cut", FS_SET, PROJ_FORMAT, 7);
    {
        int k, ok = 1;
        for (k = 1; k < 60 && ok; k += 3) {
            cut_at = k, ops = 0;
            send_set(2, b2, nb);
            if (sxw_asking())
                sxw_answer(1);
            reboot();
            sxw.open = 0, sxw.state = SXW_S_IDLE;
            ok = stored_is(2, a, na) || stored_is(2, b2, nb);
            if (ok && stored_is(2, b2, nb)) {    /* (it made it: the old one back for the next cut) */
                send_set(2, a, na);
                sxw_answer(1);
            }
        }
        CHECK(ok, "a power cut in a transfer: the set as it was (cut at op %d)", k);
    }

    /* APPLY and DELETE: asked, then done (or not); the store taken by eight sets and WORK: LIST all nine */
    q[0] = FS_SET, q[1] = 2;
    takes = 0, take_rc = 1;
    CHECK(ask(SXW_APPLY, q, 2) && ans_cmd == SXW_APPLY && ans[1] == SXW_S_ASKING && !memcmp(ans + 5, "Echo Set", 9),
          "APPLY: asking");
    CHECK(sxw_answer(1) == SXW_S_YES && takes == 1 && sxw.unsaved, "yes: taken, not saved");
    take_rc = 0;
    CHECK(ask(SXW_APPLY, q, 2) && sxw_answer(0) == SXW_S_NO && takes == 1, "no: not taken");
    q[1] = 6;
    CHECK(ask(SXW_APPLY, q, 2) && ans_cmd == SXW_NAK && ans[1] == SXW_E_NONE, "APPLY of none");
    for (i = 0; i < 8u; i++) {
        char nm[8] = "Set 0";
        nm[4] = (char)('1' + i);
        nb = make_set(b2, 1500u + 1000u * i, nm, FS_SET, PROJ_FORMAT, 10u + i);
        CHECK(send_set(i, b2, nb) == SXW_COMMIT && sxw_answer(1) == SXW_S_YES && stored_is(i, b2, nb), "set %u stored", i + 1u);
    }
    CHECK(ask(SXW_LIST, 0, 0) && ans_cmd == SXW_LIST && ans[0] == 9 && ans_n == 1u + 9u * 22u && ans[1] == FS_WORK &&
          rd16(ans + 3) == sizeof work && !memcmp(ans + 7, "Live", 5), "LIST: WORK and eight sets");
    for (i = 0; i < 8u; i++)
        CHECK(ans[1 + 22 * (i + 1)] == FS_SET && ans[2 + 22 * (i + 1)] == i && rd16(ans + 3 + 22 * (i + 1)) == 1500u + 1000u * i &&
              ans[7 + 22 * (i + 1) + 4] == '1' + i, "LIST's set %u", i + 1u);
    q[0] = FS_SET, q[1] = 3;
    CHECK(ask(SXW_DELETE, q, 2) && ans_cmd == SXW_DELETE && !memcmp(ans + 5, "Set 4", 6) && sxw_answer(1) == SXW_S_YES &&
          fs_find(FS_SET, 3, 0, &len) < 0, "DELETE: asked, gone");
    CHECK(ask(SXW_DELETE, q, 2) && ans_cmd == SXW_NAK && ans[1] == SXW_E_NONE, "DELETE of none");
    CHECK(ask(0x55, 0, 0) && ans_cmd == SXW_NAK && ans[1] == SXW_E_CMD, "an unknown command");

    /* (Decisions 4) a restore: one question (RESTORE BACKUP?), its yes keeps exactly the sets it named, once each, and
     * the pieces, without asking again; a set it didn't name, or one sent twice, asked as ever; the pieces only in it;
     * ended by all coming, 10 s quiet, CANCEL, the music; a COMMIT's answer lost (kept already: STATUS says so) */
    {
        static uint8_t w1[16384], s1[16384], s3[16384];
        const uint32_t nw = make_set(w1, 4184, "Backup", FS_WORK, PROJ_FORMAT, 40);
        const uint32_t n1 = make_set(s1, 3000, "Restored 1", FS_SET, PROJ_FORMAT, 41);
        const uint32_t n3 = make_set(s3, 1500, "Restored 3", FS_SET, PROJ_FORMAT, 43);
        en_playing = 1;
        q[0] = 0x05, q[1] = 1;
        CHECK(ask(SXW_RESTORE, q, 2) && ans_cmd == SXW_BUSY && ans[1] == SXW_B_PLAYING, "RESTORE while playing: BUSY");
        en_playing = 0;
        q[0] = 0, q[1] = 0;
        CHECK(ask(SXW_RESTORE, q, 2) && ans_cmd == SXW_NAK && ans[1] == SXW_E_BAD, "RESTORE of nothing: NAK");
        q[0] = 1, q[1] = 2;
        CHECK(ask(SXW_RESTORE, q, 2) && ans_cmd == SXW_NAK && ans[1] == SXW_E_BAD, "RESTORE's pieces 2: NAK");
        CHECK(send_obj(FS_WORK, 0, w1, nw) == SXW_NAK && ans[1] == SXW_E_BAD && wtakes == 0, "the pieces without a restore: NAK");
        q[0] = 0x05, q[1] = 1;
        CHECK(ask(SXW_RESTORE, q, 2) && ans_cmd == SXW_RESTORE && ans[0] == SXW_RESTORE && ans[1] == SXW_S_ASKING &&
              ans[2] == FS_WORK && ans[3] == 0x05 && ans[4] == 2 && sxw_asking() == SXW_RESTORE, "RESTORE: asking once");
        CHECK(ask(SXW_BEGIN, q, 8) && ans_cmd == SXW_BUSY && ans[1] == SXW_B_ASKING, "BEGIN while it asks: BUSY");
        CHECK(sxw_answer(0) == SXW_S_NO && !sxw.rs_on, "no: no restore");
        CHECK(send_obj(FS_WORK, 0, w1, nw) == SXW_NAK && ans[1] == SXW_E_BAD && wtakes == 0, "... the pieces then: NAK");
        q[0] = 0x05, q[1] = 1;
        CHECK(ask(SXW_RESTORE, q, 2) && sxw_answer(1) == SXW_S_YES && sxw.rs_on, "yes: a restore open");
        CHECK(ask(SXW_HELLO, 0, 0) && (ans[12] & 8u) && !(ans[12] & 2u), "HELLO: a restore open");
        CHECK(send_set(0, s1, n1) == SXW_COMMIT && ans[0] == SXW_COMMIT && ans[1] == SXW_S_YES && ans[3] == 0 &&
              !sxw_asking() && stored_is(0, s1, n1) && sxw.rs_note == 1u && plat_fs_open() < 0, "set 1: kept, not asked");
        nb = make_set(b2, 2500, "Not named", FS_SET, PROJ_FORMAT, 44);
        CHECK(send_set(1, b2, nb) == SXW_COMMIT && ans[1] == SXW_S_ASKING && sxw_asking() == SXW_COMMIT &&
              sxw_answer(0) == SXW_S_NO && !stored_is(1, b2, nb), "set 2, not in it: asked (no)");
        CHECK(send_set(0, b2, nb) == SXW_COMMIT && ans[1] == SXW_S_ASKING && sxw_answer(0) == SXW_S_NO && stored_is(0, s1, n1),
              "set 1 again: asked (no)");
        CHECK(send_obj(FS_WORK, 0, s3, n3) == SXW_NAK && ans[1] == SXW_E_SET && sxw.rs_work && plat_fs_open() < 0,
              "a set sent as the pieces: NAK at once");
        wtake_rc = 1;
        CHECK(send_obj(FS_WORK, 0, w1, nw) == SXW_COMMIT && ans[1] == SXW_S_YES && ans[2] == FS_WORK && ans[4] == NPIECES &&
              !memcmp(ans + 5, "Backup", 7) && wtakes == 1 && !wtake_open && sxw.took && sxw.unsaved && sxw.rs_note == 3u &&
              fs_find(FS_INBOX, 0, 0, &len) < 0 && sxw.rs_on, "the pieces: taken (not saved), their frame dropped");
        wtake_rc = 0;
        CHECK(send_obj(FS_WORK, 0, w1, nw) == SXW_NAK && ans[1] == SXW_E_BAD && wtakes == 1, "the pieces twice: NAK");
        CHECK(send_set(2, s3, n3) == SXW_COMMIT && ans[1] == SXW_S_YES && stored_is(2, s3, n3) && !sxw.rs_on &&
              sxw.rs_end == 1u && sxw.rs_done == 3u, "set 3, the last: kept, the restore ended (all came)");
        CHECK(ask(SXW_HELLO, 0, 0) && !(ans[12] & 8u), "HELLO: no restore");
        sxw.took = 0, sxw.rs_end = 0;
        q[0] = 0x80, q[1] = 0;                   /* (10 s quiet: ended; then set 8 asked) */
        CHECK(ask(SXW_RESTORE, q, 2) && ans[2] == 0 && ans[4] == 1 && sxw_answer(1) == SXW_S_YES, "RESTORE set 8: yes");
        sxw_tick(now + SXW_IDLE_MS - 1u);
        CHECK(sxw.rs_on, "9.9 s: open");
        sxw_tick(now + SXW_IDLE_MS + 1u);
        CHECK(!sxw.rs_on && sxw.rs_end == 2u, "10 s quiet: ended short");
        CHECK(send_set(7, b2, nb) == SXW_COMMIT && ans[1] == SXW_S_ASKING && sxw_answer(0) == SXW_S_NO, "... set 8 then asked");
        q[0] = 0x80, q[1] = 0;
        CHECK(ask(SXW_RESTORE, q, 2) && sxw_answer(1) == SXW_S_YES && ask(SXW_CANCEL, 0, 0) && !sxw.rs_on, "CANCEL ends it");
        q[0] = 0x80, q[1] = 0;
        CHECK(ask(SXW_RESTORE, q, 2) && sxw_answer(1) == SXW_S_YES, "again");
        en_playing = 1;
        sxw_tick(now);
        en_playing = 0;
        CHECK(!sxw.rs_on, "the music started: ended");
        q[0] = 0x80, q[1] = 0;                   /* (COMMIT's answer lost: sent again, nothing open; STATUS: kept) */
        CHECK(ask(SXW_RESTORE, q, 2) && sxw_answer(1) == SXW_S_YES, "again");
        lose_at = 5;                             /* (BEGIN, three DATA, the COMMIT) */
        wire_how = lose_nth;
        CHECK(send_set(7, s3, n3) == SXW_COMMIT && stored_is(7, s3, n3) && !sxw.rs_on && sxw.rs_end == 1u,
              "a restore's COMMIT answer lost: kept, said so");
        wire_how = fine;
    }

    /* (Decisions 5) a restore stops at the first of its sets that fails on the flash, and says which were kept: the
     * flash dead from set 5's BEGIN (no frame erased: NAK), or failing at set 5's seal (COMMIT: state 5); then set 6, which
     * it named, is asked as ever, and the pieces it named are refused (no restore any more) */
    {
        static uint8_t s4[16384], s5[16384], s6[16384];
        const uint32_t n4 = make_set(s4, 2100, "Kept 4", FS_SET, PROJ_FORMAT, 51);
        const uint32_t n5 = make_set(s5, 2200, "Fails 5", FS_SET, PROJ_FORMAT, 52);
        const uint32_t n6 = make_set(s6, 2300, "Never 6", FS_SET, PROJ_FORMAT, 53);
        static uint8_t w1[16384];
        const uint32_t nw = make_set(w1, 4184, "Backup", FS_WORK, PROJ_FORMAT, 54);
        int how;
        for (how = 0; how < 2; how++) {
            const int w0 = wtakes;
            sxw.rs_end = 0;
            q[0] = 0x38, q[1] = 1;                   /* (sets 4, 5, 6 and the pieces) */
            CHECK(ask(SXW_RESTORE, q, 2) && sxw_answer(1) == SXW_S_YES && sxw.rs_on, "RESTORE 4 5 6 + the pieces: yes");
            CHECK(send_set(3, s4, n4) == SXW_COMMIT && ans[1] == SXW_S_YES && stored_is(3, s4, n4) && sxw.rs_kept == 0x08,
                  "set 4: kept");
            if (how == 0) {                          /* (the flash dead from here: the first erase fails) */
                cut_at = 1, ops = 0;
                CHECK(send_set(4, s5, n5) == SXW_NAK && (ans[1] == SXW_E_FLASH || ans[1] == SXW_E_FULL),
                      "set 5, the flash dead: NAK (%u)", ans[1]);
            } else {                                 /* (sent whole, read back, then its seal failing) */
                seal_fail = 1;
                CHECK(send_set(4, s5, n5) == SXW_COMMIT && ans[1] == SXW_S_FAILED && !stored_is(4, s5, n5),
                      "set 5, its seal failing: COMMIT says failed (%u)", ans[1]);
            }
            CHECK(!sxw.rs_on && sxw.rs_end == 3u && sxw.rs_fkind == FS_SET && sxw.rs_fix == 4u && sxw.rs_kept == 0x08 &&
                  sxw.rs_done == 1u, "the restore stopped at set 5, set 4 kept (end %u, failed %u/%u, kept %02x)",
                  sxw.rs_end, sxw.rs_fkind, sxw.rs_fix, sxw.rs_kept);
            CHECK(ask(SXW_HELLO, 0, 0) && !(ans[12] & 8u), "HELLO: no restore");
            reboot();                                /* (the power back) */
            CHECK(stored_is(3, s4, n4) && !stored_is(4, s5, n5), "set 4 still kept, set 5 not as sent");
            CHECK(send_set(5, s6, n6) == SXW_COMMIT && ans[1] == SXW_S_ASKING && sxw_answer(0) == SXW_S_NO,
                  "set 6, named but after the stop: asked (no)");
            CHECK(send_obj(FS_WORK, 0, w1, nw) == SXW_NAK && ans[1] == SXW_E_BAD && wtakes == w0,
                  "the pieces, named but after the stop: NAK");
            CHECK(ask(SXW_CANCEL, 0, 0) && sxw.rs_end == 3u, "CANCEL after it (the tool's): nothing more");
        }
    }

    /* 400 sets over a bad wire: requests lost or garbled, answers lost (then the same one sent again), as the tool
     * retries; each kept exactly as sent (yes), or the one before (no) */
    {
        int k, kept = 0, refused = 0, lost = 0;
        static uint8_t prev[8][16384];
        static uint32_t prevn[8];
        for (i = 0; i < 8u; i++) {
            const int f = fs_find(FS_SET, i, 0, &prevn[i]);
            if (f >= 0)
                fs_read(f, 0, prev[i], prevn[i]);
            else
                prevn[i] = 0;
        }
        for (k = 0; k < 400; k++) {
            const uint32_t slot = rnd() % 8u, yes = rnd() % 4u != 0;
            uint32_t r;
            nb = make_set(b2, 200u + rnd() % (PLAT_FS_MAX - 200u), "Rough", FS_SET, PROJ_FORMAT, 100u + (uint32_t)k);
            wire_how = rough;
            r = send_set(slot, b2, nb);
            wire_how = fine;
            (void)r;
            if (sxw_asking()) {
                sxw_answer((int)yes);
                if (yes)
                    memcpy(prev[slot], b2, nb), prevn[slot] = nb, kept++;
                else
                    refused++;
            } else {
                lost++;
            }
            for (i = 0; i < 8u; i++)
                if (prevn[i] ? !stored_is(i, prev[i], prevn[i]) : fs_find(FS_SET, i, 0, &len) >= 0) {
                    CHECK(0, "rough wire, transfer %d: set %u not as it should be", k, i + 1u);
                    k = 400;
                    break;
                }
            CHECK(plat_fs_open() < 0 || sxw.open, "rough wire: a frame left open");
            now += 100u;
        }
        printf("  (rough wire: %d kept, %d answered no, %d given up; %u frames taken, %u garbled)\n", kept, refused, lost,
               sxw.in, sxw.bad);
    }
    printf(fails ? "set_wire: %d FAILED\n" : "set_wire: ok\n", fails);
    return fails != 0;
}
