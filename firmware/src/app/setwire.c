/* SPDX-License-Identifier: GPL-3.0-only */
/* DUB-1 1.5.0, the set protocol (STORAGE-1.5.0.md 6.3, section 10's step 4): the sets on the device, over SysEx on the
 * USB-MIDI port, from the web page (step 5) or tools/dub1_set.py. Part of the unity build after project.c (its sets'
 * side: project_set_*, project_work_bytes) and before ui.c (the question's card: sxw_asking, sxw_answer, sxw_tick); the
 * host simulator and tests/host/set_wire_test.c include it too. The includer gives sxw_wire(p, n): one whole F0 .. F7
 * frame to the host (the device: usb.c's ota_wire_send).
 *
 * A frame: F0 7D 44 31 cmd seq pack7(body) chk F7 ("D1"; manufacturer 0x7D as the screen reader's F0 7D 53 52), pack7
 * ota.c's 8-to-7 (LSB first), chk the sum of the bytes from cmd to the last packed one, & 0x7F. The answer: the same cmd
 * and seq (an ACK, its body below), or BUSY (7E: cmd, why, a few words) or NAK (7F: cmd, why, a detail). A frame whose
 * checksum is wrong gets no answer (the host sends it again: 2 s, 3 tries). One request at a time; usb.c keeps one
 * frame of 640 bytes, so the host's chunks are up to 512 bytes (DATA: 514, packed 588 + 6) and ours up to 256 (READ: 260,
 * packed 298 + 8: 102 packets of usb.c's 192). The store test: Chrome sent 17 KB/s and got 8.6 KB/s back, no errors.
 *   01 HELLO     -> u8 protocol 1, u16 512, u16 256, u32 PROJ_FORMAT, u16 PLAT_FS_MAX, u8 sets 8, u8 state (1 playing,
 *                   2 asking, 4 a set coming in, 8 a restore open), u8 the last question's op and state, 0,
 *                   char version[32] ("DUB-1 x", NUL-padded; was 16: "DUB-1 1.5.0-BETA1" is 17)
 *   02 LIST      -> u8 n, then n x (u8 kind, u8 index, u16 length, u8 pieces, u8 flags (1 stored, 2 live), name[16]):
 *                   WORK (the project as it is now: live) and every stored set
 *   03 STATUS    -> u8 op, state (SXW_S_*), kind, index, pieces, name[16], flags: the last question, as it stands;
 *                   (beta2, the user's) flags 1: a set stored into the set loaded (the pieces still the old one's),
 *                   2: the question is ui.c's LOAD IT NOW? (an APPLY after such a yes: the store answered yes)
 *   10 READ      u8 kind, u8 index, u16 off, u16 n (<= 256) -> u16 off, u16 total, the bytes: WORK made on the fly
 *                   from the project as it is (no save needed; deterministic, the same at every chunk), a set as stored
 *   20 BEGIN     u8 kind (2: a set), u8 index (0..7), u16 length, u32 CRC-32 (zlib's) -> the same: a frame open
 *                   (erased as far as the length needs), the old version of that set kept till the new one counts.
 *                   (Decisions 4) kind 1, index 0: the pieces (a backup's WORK), only in a restore's yes
 *   21 DATA      u16 off, the bytes (1..512), in order -> u16 off, u16 n, u16 got (a chunk sent again, its answer
 *                   lost: answered again, not written twice)
 *   22 COMMIT    -> u8 state 1 (asking), pieces, name[16]: the bytes read back, their CRC the host's, the set checked
 *                   whole (as loading it would take it); then the device asks: STORE SET n? <name>. GLO yes (the
 *                   header programmed: it counts), any other button, key or knob no (the frame left headerless: free);
 *                   30 s unanswered: no. The host polls STATUS for the answer. (beta2, the user's) Its yes into the
 *                   set loaded (sxw_live): after the outcome, the FM-1 asks LOAD IT NOW? (an APPLY of it: ui.c)
 *                   (2026-11-23) The Set page's KNOB 3, Store, asks the same from the FM-1 itself (sxw_store): the
 *                   pieces now written as a set, then this question; STATUS the same (op COMMIT), flags 0
 *   24 CANCEL    -> the transfer dropped, a question asked withdrawn (no)
 *   30 APPLY     u8 kind 2, u8 index -> as COMMIT: LOAD SET n? <name>; yes: its pieces into the slots (project_set_take)
 *                   and WORK saved. (The Set page's KNOB 4, Load, asks the same from the FM-1 itself: sxw_load)
 *   31 DELETE    u8 kind 2, u8 index -> as COMMIT: DELETE SET n? <name>; yes: every version of it erased
 *   32 RESTORE   u8 sets (a mask: set n+1 bit n), u8 the pieces (0/1) -> the question (kind 1 with the pieces, index
 *                   the mask, pieces how many sets): RESTORE BACKUP? n sets + the pieces (Decisions 4: one question for
 *                   a whole backup). Its yes opens a restore: exactly those sets, once each, and the pieces once, are
 *                   kept as their COMMIT comes, without asking again (sent, read back and checked whole as ever; COMMIT
 *                   answers state 2, yes, or 5, failed); the pieces (BEGIN kind 1) into a frame of their own, decoded as
 *                   a WORK, dropped, and taken into the slots as APPLY takes a set, then WORK saved (with the device's
 *                   own META, never the backup's: the 1.4 check at boot reads it). The restore ends when all have come,
 *                   on CANCEL, APPLY, DELETE or another RESTORE, when the music starts, or after 10 s with nothing sent;
 *                   (Decisions 5) at the first of its sets or pieces that fails on the flash (no frame, an erase, a
 *                   program, the read back, the seal: that NAK, or COMMIT's state 5), the rest not kept; a set it
 *                   doesn't name is asked as ever
 * While the music plays, everything but HELLO, STATUS, LIST and READ gets BUSY "stop first" (an erase is a silent gap:
 * the decision; the reads are allowed, Decisions 3: a backup mid-set, the flash read in main.c's 256-byte IRQ-off
 * windows, ~0.3 ms each, WORK made from RAM); while a question waits, the rest gets BUSY "answer on the FM-1". The store is held from BEGIN to the answer: the
 * project's saves wait (project_save: PLAT_FS_HELD; autosave saves it after), 10 s without DATA drops the transfer. */
#define SXW_H0 0x7Du
#define SXW_H1 0x44u
#define SXW_H2 0x31u
enum { SXW_HELLO = 0x01, SXW_LIST = 0x02, SXW_STATUS = 0x03, SXW_READ = 0x10, SXW_BEGIN = 0x20, SXW_DATA = 0x21,
       SXW_COMMIT = 0x22, SXW_CANCEL = 0x24, SXW_APPLY = 0x30, SXW_DELETE = 0x31, SXW_RESTORE = 0x32, SXW_BUSY = 0x7E,
       SXW_NAK = 0x7F };
#define SXW_PROTO 1u
#define SXW_IN 512u                              /* the host's chunks, bytes at most */
#define SXW_OUT 256u                             /* ours */
#define SXW_NSETS PLAT_FS_SETS                   /* sets on the device (the decision) */
#define SXW_ASK_MS 30000u                        /* a question unanswered this long: no */
#define SXW_IDLE_MS 10000u                       /* a transfer with no DATA this long: dropped; a restore with nothing
                                                  * sent: ended */
/* NAK's why */
enum { SXW_E_BAD = 1,                            /* a request's fields (length, kind, index, offset) */
       SXW_E_CMD,                                /* no such command */
       SXW_E_NONE,                               /* no such set stored */
       SXW_E_BIG,                                /* longer than a frame holds */
       SXW_E_NEWER,                              /* made for a newer DUB-1 (its format) */
       SXW_E_SET,                                /* not a set this DUB-1 takes (shape, CRC, a value: the detail project.c's) */
       SXW_E_FULL,                               /* no frame free */
       SXW_E_FLASH,                              /* the flash: an erase, a program or the read back failed (or none) */
       SXW_E_ORDER,                              /* DATA not where the transfer is (the detail: u16 got), COMMIT short */
       SXW_E_OPEN };                             /* no transfer open */
/* BUSY's why */
enum { SXW_B_PLAYING = 1, SXW_B_ASKING };
/* the last question's state (STATUS) */
enum { SXW_S_IDLE, SXW_S_ASKING, SXW_S_YES, SXW_S_NO, SXW_S_TIMEOUT, SXW_S_FAILED, SXW_S_PLAYING };

static int sxw_wire(const uint8_t *p, uint32_t n);   /* (the includer's: one frame to the host) */
#ifndef SXW_DEC
static uint8_t sxw_dec_own[640];
#define SXW_DEC sxw_dec_own                        /* (the device: ota.c's ota_dec, the same thread, never at once) */
#endif
#define SXW_BODY (4u + SXW_OUT)                  /* a body out, bytes at most (READ's) */
static uint8_t sxw_out[6u + (SXW_BODY * 8u + 6u) / 7u + 2u];   /* a frame out: 306 bytes at most (102 of usb.c's packets) */

static struct {
    uint8_t op, state, kind, index, npieces;     /* the last question (op: SXW_COMMIT, SXW_APPLY, SXW_DELETE), its set */
    char name[PROJ_NAME + 1];
    int frame;                                   /* its frame: the set coming in, or stored */
    uint32_t flen;
    uint8_t open;                                /* a set coming in: BEGIN .. COMMIT */
    uint32_t len, crc, got, ccrc;                /* its length and CRC (the host's), the bytes in, their CRC so far */
    uint32_t t;                                  /* the last DATA; the question asked at */
    int rframe;                                  /* READ's set: its frame (its newest version: never reused), -1 none */
    uint8_t rindex;
    uint32_t rlen;
    uint32_t in, bad;                            /* frames taken; refused (a checksum: no answer) */
    uint8_t unsaved;                             /* (APPLY's yes) taken, but WORK not saved (the flash) */
    uint8_t rq_sets, rq_work;                    /* (Decisions 4) RESTORE's question: the sets (a mask), the pieces */
    uint8_t rs_on, rs_sets, rs_work;             /* its yes: a restore open, what is still to come */
    uint8_t rs_done;                             /* kept in it so far */
    uint8_t rs_note;                             /* (for ui.c, which clears it) 1 a set kept, 2 the pieces taken, 3 taken
                                                  * not saved, 4 one failed */
    uint8_t rs_end;                              /* (ui.c clears it) the restore ended: 1 all came, 2 short, 3 stopped
                                                  * (Decisions 5: one failed on the flash) */
    uint8_t rs_kept;                             /* the sets kept in it so far (a mask, as RESTORE's) */
    uint8_t rs_fkind, rs_fix;                    /* (rs_end 3) the one that failed: FS_SET and its number, or FS_WORK */
    uint8_t took;                                /* (ui.c clears it) the pieces taken: the engine to take them */
    uint32_t rs_t;                               /* the restore's last request */
    uint32_t now;                                /* the time last seen (a frame's, the tick's) */
} sxw = {0, 0, 0, 0, 0, {0}, -1, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static uint32_t sxw_pack7(const uint8_t *in, uint32_t n, uint8_t *out)   /* (ota.c's) 8-to-7, LSB first */
{
    uint32_t acc = 0, nb = 0, o = 0;
    while (n--) {
        acc |= (uint32_t)*in++ << nb;
        nb += 8u;
        while (nb >= 7u) {
            out[o++] = (uint8_t)(acc & 0x7Fu);
            acc >>= 7;
            nb -= 7u;
        }
    }
    if (nb)
        out[o++] = (uint8_t)(acc & 0x7Fu);
    return o;
}
static uint32_t sxw_unpack7(const uint8_t *in, uint32_t n, uint8_t *out, uint32_t max)
{
    uint32_t acc = 0, nb = 0, o = 0;
    while (n--) {
        acc |= (uint32_t)(*in++ & 0x7Fu) << nb;
        nb += 7u;
        if (nb >= 8u) {
            if (o < max)
                out[o] = (uint8_t)acc;
            o++;
            acc >>= 8;
            nb -= 8u;
        }
    }
    return o;
}
/* (beta2, the user's: a set stored into the set loaded didn't change what plays, the pieces its copy) the set loaded
 * into the pieces: 0..7 (an APPLY's yes, the Set page's Load), -1 not known (the boot, a restore's pieces: then the
 * set stored whose name the pieces carry, sxw_is_live) */
static int8_t sxw_live = -1;
static uint8_t sxw_stlive;                       /* the last COMMIT's set: the set loaded (STATUS's flags 1) */
static uint8_t sxw_again;                        /* the question up is LOAD IT NOW? (STATUS's flags 2; ui.c's title) */
static uint8_t sxw_local;                        /* (the Set page's Store) the STORE question up is the FM-1's own: the
                                                  * pieces now (sxw_store), its yes the set loaded, no LOAD IT NOW? */
static uint8_t sxw_repl;                         /* (its question) the set it goes into used: replaced, */
static char sxw_rname[PROJ_NAME + 1];            /* that one's name ("": none, or not one this DUB-1 reads) */
static uint32_t sxw_rd16(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8; }
static void sxw_wr16(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v, p[1] = (uint8_t)(v >> 8); }

/* the answer: SXW_DEC[0 .. n) as the body of one frame (cmd, seq as asked) */
static void sxw_send(uint32_t cmd, uint32_t seq, uint32_t n)
{
    uint32_t w = 0, i, s = 0;
    if (n > SXW_BODY)
        return;
    sxw_out[w++] = 0xF0;
    sxw_out[w++] = SXW_H0;
    sxw_out[w++] = SXW_H1;
    sxw_out[w++] = SXW_H2;
    sxw_out[w++] = (uint8_t)(cmd & 0x7Fu);
    sxw_out[w++] = (uint8_t)(seq & 0x7Fu);
    w += sxw_pack7(SXW_DEC, n, sxw_out + w);
    for (i = 4; i < w; i++)
        s += sxw_out[i];
    sxw_out[w++] = (uint8_t)(s & 0x7Fu);
    sxw_out[w++] = 0xF7;
    sxw_wire(sxw_out, w);
}
static void sxw_nak(uint32_t cmd, uint32_t seq, uint32_t why, uint32_t detail)
{
    SXW_DEC[0] = (uint8_t)cmd, SXW_DEC[1] = (uint8_t)why, SXW_DEC[2] = (uint8_t)detail, SXW_DEC[3] = (uint8_t)(detail >> 8);
    sxw_send(SXW_NAK, seq, 4);
}
static void sxw_busy(uint32_t cmd, uint32_t seq, uint32_t why)
{
    static const char *const W[3] = {"", "stop first", "answer on the FM-1"};
    const char *t = W[why < 3u ? why : 0];
    uint32_t n = 2;
    SXW_DEC[0] = (uint8_t)cmd, SXW_DEC[1] = (uint8_t)why;
    while (*t)
        SXW_DEC[n++] = (uint8_t)*t++;
    sxw_send(SXW_BUSY, seq, n);
}
static uint32_t sxw_name(uint8_t *d, const char *s)   /* a name field (16 bytes, NUL-padded): 16 */
{
    uint32_t i, e = 0;
    for (i = 0; i < 16u; i++) {
        if (!e && !s[i])
            e = 1;
        d[i] = e ? 0 : (uint8_t)s[i];
    }
    return 16;
}
static uint32_t sxw_question(uint8_t *d)         /* op, state, kind, index, pieces, name[16], flags */
{
    d[0] = sxw.op, d[1] = sxw.state, d[2] = sxw.kind, d[3] = sxw.index, d[4] = sxw.npieces;
    sxw_name(d + 5, sxw.name);
    d[21] = (uint8_t)((sxw.op == SXW_COMMIT && sxw_stlive ? 1u : 0u) | (sxw_again ? 2u : 0u));
    return 22;
}
/* set ix the set loaded? (sxw_live; not known: the set stored there now named as the pieces, a name at all) */
static int sxw_is_live(uint32_t ix)
{
    char nm[PROJ_NAME + 1];
    uint32_t len, np, i;
    int f;
    if (sxw_live >= 0)
        return (uint32_t)sxw_live == ix;
    if (!proj.name[0] || (f = plat_fs_find(FS_SET, ix, 0, &len)) < 0 || project_set_info(f, len, nm, &np))
        return 0;
    for (i = 0; nm[i] && nm[i] == proj.name[i]; i++)
        ;
    return nm[i] == proj.name[i];
}

static void sxw_drop(void)                       /* the set coming in dropped (its frame left headerless: free) */
{
    if (sxw.open || (sxw.op == SXW_COMMIT && sxw.state == SXW_S_ASKING))
        plat_fs_abort();
    sxw.open = 0;
}
static void sxw_ask(uint32_t op, uint32_t index, int frame, uint32_t len, const char *name, uint32_t np, uint32_t now)
{
    uint32_t i;
    sxw.op = (uint8_t)op, sxw.state = SXW_S_ASKING, sxw.kind = FS_SET, sxw.index = (uint8_t)index;
    sxw.npieces = (uint8_t)np, sxw.frame = frame, sxw.flen = len, sxw.t = now;
    sxw_stlive = sxw_again = sxw_local = 0;
    for (i = 0; i < PROJ_NAME && name[i]; i++)
        sxw.name[i] = name[i];
    sxw.name[i] = 0;
}
static uint32_t sxw_set_err(int rc) { return rc == D1C_ENEWER ? SXW_E_NEWER : SXW_E_SET; }
/* (Decisions 5) a set (kind 2, its number) or the pieces (1) a restore still wants failed on the flash (no frame, an
 * erase, a program, the read back, the seal): the restore stops there, the rest not kept (the computer stops sending
 * them too); ui.c says which failed and which were kept. Anything else: nothing */
static void sxw_rs_stop(uint32_t kind, uint32_t index)
{
    if (!sxw.rs_on || (kind == FS_WORK ? !sxw.rs_work : kind != FS_SET || !(sxw.rs_sets >> index & 1u)))
        return;
    sxw.rs_fkind = (uint8_t)kind, sxw.rs_fix = (uint8_t)index;
    sxw.rs_on = sxw.rs_sets = sxw.rs_work = 0;
    sxw.rs_end = 3;
}
/* (Decisions 4) a restore's set (kind 2) or pieces (1) kept without a question: STATUS says so as a yes would; the
 * last to come ends it; (Decisions 5) one that failed stops it */
static void sxw_kept(uint32_t kind, uint32_t index, const char *name, uint32_t np, int ok, uint32_t now)
{
    sxw_ask(SXW_COMMIT, index, -1, 0, name, np, now);
    sxw.kind = (uint8_t)kind;
    sxw.state = ok ? SXW_S_YES : SXW_S_FAILED;
    sxw.rs_t = now;
    if (!ok) {
        sxw_rs_stop(kind, index);
        return;
    }
    if (kind == FS_WORK)
        sxw.rs_work = 0;
    else
        sxw.rs_sets &= (uint8_t)~(1u << index), sxw.rs_kept |= (uint8_t)(1u << index);
    sxw.rs_done++;
    if (!sxw.rs_sets && !sxw.rs_work)
        sxw.rs_on = 0, sxw.rs_end = 1;
}
static void sxw_rs_close(void)                   /* a restore ended before all came */
{
    if (sxw.rs_on)
        sxw.rs_end = 2;
    sxw.rs_on = sxw.rs_sets = sxw.rs_work = 0;
}

/* (APPLY, DELETE; and the Set page's Load, sxw_load) the question about stored set ix: LOAD SET n? (op SXW_APPLY: the
 * set checked whole, as loading it would take it) or DELETE SET n?. 0: asked; else NAK's why (*det its detail) */
static uint32_t sxw_ask_stored(uint32_t op, uint32_t ix, uint32_t now, uint32_t *det)
{
    char nm[PROJ_NAME + 1];
    uint32_t len, np = 0;
    int f, rc;
    *det = 0;
    if (sxw.open)                                /* (a set coming in: dropped, as CANCEL) */
        sxw_drop();
    sxw_rs_close();                              /* (a restore open: ended) */
    if ((f = plat_fs_find(FS_SET, ix, 0, &len)) < 0)
        return SXW_E_NONE;
    nm[0] = 0;
    if (op == SXW_APPLY && (rc = project_set_check(f, len, nm, &np)) != 0) {
        *det = (uint32_t)-rc;
        return sxw_set_err(rc);
    }
    if (op == SXW_DELETE && project_set_info(f, len, nm, &np))
        nm[0] = 0, np = 0;                       /* (one this DUB-1 can't read can still go) */
    sxw_ask(op, ix, f, len, nm, np, now);
    return 0;
}
/* (the user's, 2026-11-22: the Set page's KNOB 4, Load) a stored set loaded from the FM-1 itself: the question APPLY
 * asks (LOAD SET n? <name>), answered and taken as APPLY's (sxw_answer, project_set_take), STATUS the same (op
 * SXW_APPLY), so the Sets page sees what it would after its own. As APPLY, only while stopped; and never over the
 * computer's own work (a question up, a set coming in, a restore open: those are not dropped from here, as the
 * computer's APPLY drops its own). 0: asked; -1 playing; -2 the computer busy; else NAK's why (no such set, not one
 * this DUB-1 loads, made by a newer one) */
static int sxw_load(uint32_t ix, uint32_t now)
{
    uint32_t det;
    if (en_playing)
        return -1;
    if (sxw.state == SXW_S_ASKING || sxw.open || sxw.rs_on)
        return -2;
    if (ix >= SXW_NSETS)
        return SXW_E_BAD;
    return (int)sxw_ask_stored(SXW_APPLY, ix, now, &det);
}
/* (beta2, the user's) a set just stored into the set loaded: LOAD IT NOW? (sxw_load's question, its title ui.c's) */
static int sxw_load_again(uint32_t ix, uint32_t now)
{
    const int rc = sxw_load(ix, now);
    sxw_again = rc == 0;
    return rc;
}

/* (the user's, 2026-11-23: the Set page's KNOB 3, Store) the set loaded into the pieces, as far as it is still known:
 * sxw_live, or (after a boot) the first stored set whose name the pieces carry (META's name read cheaply: no CRC, for
 * the knob's picture); -1 none */
static int sxw_live_find(void)
{
    char nm[PROJ_NAME + 1];
    uint32_t ix, len, i;
    int f;
    if (sxw_live >= 0)
        return sxw_live;
    if (!proj.name[0])
        return -1;
    for (ix = 0; ix < SXW_NSETS; ix++) {
        if ((f = plat_fs_find(FS_SET, ix, 0, &len)) < 0 || project_set_name(f, len, nm))
            continue;
        for (i = 0; nm[i] && nm[i] == proj.name[i]; i++)
            ;
        if (nm[i] == proj.name[i])
            return (int)ix;
    }
    return -1;
}
/* the pieces as they are now kept as stored set ix (name, pieces 1 .. n) from the FM-1 itself: the pieces saved first
 * (WORK, as GLO held saves: the set and the pieces the same), the set written into a frame of its own and checked whole
 * (project_set_write: the old version kept till the new one counts), then the question a set sent from the computer
 * asks (STORE SET n? <name>: COMMIT's, its screen, answer, outcome and STATUS; GLO yes, anything else no, 30 s): yes
 * seals it (sxw_answer: then the set loaded, no LOAD IT NOW?: the pieces are it), no or no answer drops it. As Load,
 * only while stopped (the erases) and never over the computer's own work. 0: asked; -1 playing; -2 the computer busy;
 * -3 the pieces not saved (the flash); -4 the set not written (the flash, or not read back as written) */
static int sxw_store(uint32_t ix, const char *name, uint32_t n, uint32_t now)
{
    uint32_t len, np;
    int f;
    if (en_playing)
        return -1;
    if (sxw.state == SXW_S_ASKING || sxw.open || sxw.rs_on)
        return -2;
    if (ix >= SXW_NSETS || n < 1u || n > NPIECES)
        return SXW_E_BAD;
    sxw_repl = 0, sxw_rname[0] = 0;
    if ((f = plat_fs_find(FS_SET, ix, 0, &len)) >= 0) {
        sxw_repl = 1;
        if (project_set_info(f, len, sxw_rname, &np))
            sxw_rname[0] = 0;
    }
    if (project_save() != 0)
        return -3;
    sxw.rframe = -1;
    if ((f = project_set_write(ix, n, name, &len)) < 0)
        return -4;
    sxw_ask(SXW_COMMIT, ix, f, len, name, n, now);
    sxw_local = 1;
    return 0;
}

/* ------------------------------------------------------------------------------------------- the requests --- */
/* a frame (usb.c's: the bytes between F0 and F7). 1: ours (answered, or a bad one ignored); 0: someone else's */
static int sxw_frame(const uint8_t *p, uint32_t n, uint32_t now)
{
    uint8_t *const d = SXW_DEC;
    uint32_t s = 0, i, cmd, seq, k;
    if (n < 6u || p[0] != SXW_H0 || p[1] != SXW_H1 || p[2] != SXW_H2)
        return 0;
    cmd = p[3], seq = p[4];
    for (i = 3; i + 1u < n; i++)
        s += p[i];
    if ((s & 0x7Fu) != p[n - 1u] || (k = sxw_unpack7(p + 5, n - 6u, d, sizeof SXW_DEC)) > sizeof SXW_DEC) {
        sxw.bad++;                               /* (no answer: the host sends it again) */
        return 1;
    }
    sxw.in++;
    sxw.now = now;
    if (cmd == SXW_HELLO) {
        static const char V[] = "DUB-1 " EN_VERSION;
        d[0] = SXW_PROTO;
        sxw_wr16(d + 1, SXW_IN), sxw_wr16(d + 3, SXW_OUT);
        d[5] = (uint8_t)PROJ_FORMAT, d[6] = (uint8_t)(PROJ_FORMAT >> 8), d[7] = 0, d[8] = 0;
        sxw_wr16(d + 9, PLAT_FS_MAX);
        d[11] = SXW_NSETS;
        d[12] = (uint8_t)((en_playing ? 1u : 0u) | (sxw.state == SXW_S_ASKING ? 2u : 0u) | (sxw.open ? 4u : 0u) |
                          (sxw.rs_on ? 8u : 0u));
        d[13] = sxw.op, d[14] = sxw.state, d[15] = 0;
        for (i = 0; i < 32u; i++)
            d[16 + i] = i < sizeof V - 1u ? (uint8_t)V[i] : 0;
        sxw_send(cmd, seq, 48);
        return 1;
    }
    if (cmd == SXW_STATUS) {
        sxw_send(cmd, seq, sxw_question(d));
        return 1;
    }
    if (en_playing && cmd != SXW_LIST && cmd != SXW_READ) {   /* (the reads go on: Decisions 3) */
        sxw_busy(cmd, seq, SXW_B_PLAYING);
        return 1;
    }
    if (sxw.state == SXW_S_ASKING && cmd != SXW_CANCEL) {
        sxw_busy(cmd, seq, SXW_B_ASKING);
        return 1;
    }
    switch (cmd) {
    case SXW_LIST: {                             /* WORK (live), then each set stored */
        char nm[PROJ_NAME + 1];
        uint32_t m = 0, len, np, ix;
        uint8_t *e = d + 1;
        e[0] = FS_WORK, e[1] = 0;
        sxw_wr16(e + 2, project_work_bytes(0, 0, 0));
        e[4] = NPIECES, e[5] = 2u | (plat_fs_find(FS_WORK, 0, 0, &len) >= 0 ? 1u : 0u);
        sxw_name(e + 6, proj.name);
        e += 22, m++;
        for (ix = 0; ix < SXW_NSETS; ix++) {
            const int f = plat_fs_find(FS_SET, ix, 0, &len);
            if (f < 0)
                continue;
            if (project_set_info(f, len, nm, &np))
                nm[0] = 0, np = 0;               /* (there, but not one this DUB-1 reads: listed, to be deleted) */
            e[0] = FS_SET, e[1] = (uint8_t)ix;
            sxw_wr16(e + 2, len);
            e[4] = (uint8_t)np, e[5] = 1;
            sxw_name(e + 6, nm);
            e += 22, m++;
        }
        d[0] = (uint8_t)m;
        sxw_send(cmd, seq, 1u + 22u * m);
        break;
    }
    case SXW_READ: {                             /* u8 kind, u8 index, u16 off, u16 n */
        const uint32_t kind = d[0], ix = d[1], off = sxw_rd16(d + 2);
        uint32_t want = sxw_rd16(d + 4), total;
        if (k != 6u || want == 0u || want > SXW_OUT || (kind != FS_WORK && kind != FS_SET) ||
            (kind == FS_WORK ? ix != 0u : ix >= SXW_NSETS)) {
            sxw_nak(cmd, seq, SXW_E_BAD, 0);
            break;
        }
        if (kind == FS_WORK) {
            total = project_work_bytes(0, 0, 0);
        } else {
            if (sxw.rframe < 0 || sxw.rindex != ix || off == 0u)   /* (found again at each first chunk: a set
                                                  * changed since is read as it is now) */
                sxw.rframe = plat_fs_find(FS_SET, ix, 0, &sxw.rlen), sxw.rindex = (uint8_t)ix;
            if (sxw.rframe < 0) {
                sxw_nak(cmd, seq, SXW_E_NONE, 0);
                break;
            }
            total = sxw.rlen;
        }
        if (off >= total) {
            sxw_nak(cmd, seq, SXW_E_BAD, total);
            break;
        }
        if (want > total - off)
            want = total - off;
        sxw_wr16(d, off), sxw_wr16(d + 2, total);
        if (kind == FS_WORK ? project_work_bytes(off, d + 4, want) != total : plat_fs_read(sxw.rframe, off, d + 4, want) != 0) {
            sxw_nak(cmd, seq, SXW_E_FLASH, 0);
            break;
        }
        sxw_send(cmd, seq, 4u + want);
        break;
    }
    case SXW_BEGIN: {                            /* u8 kind, u8 index, u16 len, u32 crc */
        const uint32_t kind = d[0], ix = d[1], len = sxw_rd16(d + 2), crc = sxw_rd16(d + 4) | sxw_rd16(d + 6) << 16;
        int rc;
        if (k != 8u || len < 44u || (kind == FS_SET ? ix >= SXW_NSETS : kind != FS_WORK || ix != 0u || !sxw.rs_work)) {
            sxw_nak(cmd, seq, SXW_E_BAD, 0);     /* (the pieces only in a restore's yes) */
            break;
        }
        if (len > PLAT_FS_MAX) {
            sxw_nak(cmd, seq, SXW_E_BIG, PLAT_FS_MAX);
            break;
        }
        sxw_drop();                              /* (one open before: a BEGIN sent again, its answer lost) */
        sxw.rframe = -1;
        if (sxw.rs_on)
            sxw.rs_t = now;
        if ((rc = plat_fs_begin(kind == FS_SET ? FS_SET : FS_INBOX, ix, len, 1)) != 0) {   /* (the pieces: a frame of
                                                  * their own, never sealed: read, then dropped) */
            sxw_rs_stop(kind, ix);               /* (Decisions 5: a restore's, no frame for it: stopped) */
            sxw_nak(cmd, seq, rc == -12 ? SXW_E_FULL : SXW_E_FLASH, (uint32_t)-rc);
            break;
        }
        sxw.open = 1, sxw.kind = (uint8_t)kind, sxw.index = (uint8_t)ix;
        sxw.len = len, sxw.crc = crc, sxw.got = 0, sxw.ccrc = 0xFFFFFFFFu, sxw.t = now;
        sxw_send(cmd, seq, 8);                   /* (the request's fields, as they came) */
        break;
    }
    case SXW_DATA: {                             /* u16 off, the bytes */
        const uint32_t off = k >= 2u ? sxw_rd16(d) : 0u, m = k >= 2u ? k - 2u : 0u;
        int rc;
        if (!sxw.open) {
            sxw_nak(cmd, seq, SXW_E_OPEN, 0);
            break;
        }
        if (m == 0u || m > SXW_IN) {
            sxw_nak(cmd, seq, SXW_E_BAD, sxw.got);
            break;
        }
        if (off + m == sxw.got && off < sxw.got) { /* (sent again, its answer lost: written already) */
        } else if (off != sxw.got || m > sxw.len - sxw.got) {
            sxw_nak(cmd, seq, SXW_E_ORDER, sxw.got);
            break;
        } else {
            if (off == 0u && m >= 20u && (d[2] != 'D' || d[3] != '1' || d[4] != 'S' || d[5] != 'T' ||
                                          sxw_rd16(d + 2 + 16) != sxw.kind)) {
                sxw_drop();                      /* (not a set (the pieces: not a WORK): said at once, not after the
                                                  * whole of it) */
                sxw_nak(cmd, seq, SXW_E_SET, 0);
                break;
            }
            if (off == 0u && m >= 20u && (sxw_rd16(d + 2 + 12) | sxw_rd16(d + 2 + 14) << 16) > PROJ_FORMAT) {
                sxw_drop();
                sxw_nak(cmd, seq, SXW_E_NEWER, sxw_rd16(d + 2 + 12));
                break;
            }
            if ((rc = plat_fs_put(d + 2, m)) != 0) {
                sxw_drop();
                sxw_rs_stop(sxw.kind, sxw.index);
                sxw_nak(cmd, seq, SXW_E_FLASH, (uint32_t)-rc);
                break;
            }
            sxw.ccrc = d1c_crc_up(sxw.ccrc, d + 2, m);
            sxw.got += m;
        }
        sxw.t = sxw.rs_t = now;
        sxw_wr16(d, off), sxw_wr16(d + 2, m), sxw_wr16(d + 4, sxw.got);
        sxw_send(cmd, seq, 6);
        break;
    }
    case SXW_COMMIT: {
        char nm[PROJ_NAME + 1];
        uint32_t np;
        int f, rc;
        if (!sxw.open) {
            sxw_nak(cmd, seq, SXW_E_OPEN, 0);
            break;
        }
        if (sxw.got != sxw.len) {
            sxw_nak(cmd, seq, SXW_E_ORDER, sxw.got);
            break;
        }
        if (~sxw.ccrc != sxw.crc) {
            sxw_drop();
            sxw_nak(cmd, seq, SXW_E_BAD, 1);
            break;
        }
        if ((f = plat_fs_finish()) < 0) {        /* (the last page, read back; the frame still open, headerless) */
            sxw.open = 0;
            sxw_rs_stop(sxw.kind, sxw.index);
            sxw_nak(cmd, seq, SXW_E_FLASH, (uint32_t)-f);
            break;
        }
        np = NPIECES;
        if ((rc = sxw.kind == FS_WORK ? project_work_check(f, sxw.len, nm) : project_set_check(f, sxw.len, nm, &np)) != 0) {
            sxw_drop();
            sxw_nak(cmd, seq, sxw_set_err(rc), (uint32_t)-rc);
            break;
        }
        if (sxw.kind == FS_WORK) {               /* (a restore's pieces: decoded, their frame dropped, then taken) */
            sxw_drop();
            rc = project_work_take();
            sxw.took = rc >= 0, sxw.unsaved = (uint8_t)(rc > 0);
            if (rc >= 0)
                sxw_live = -1;                   /* (the backup's pieces: their set not known) */
            sxw.rs_note = rc < 0 ? 4u : rc > 0 ? 3u : 2u;
            sxw_kept(FS_WORK, 0, nm, np, rc >= 0, now);
        } else if (sxw.rs_on && (sxw.rs_sets >> sxw.index & 1u)) {   /* (a restore's set: its yes given already) */
            sxw.open = 0, sxw.rframe = -1;
            rc = plat_fs_seal();
            sxw.rs_note = rc == 0 ? 1u : 4u;
            sxw_kept(FS_SET, sxw.index, nm, np, rc == 0, now);
        } else {
            sxw.open = 0;
            sxw_ask(SXW_COMMIT, sxw.index, f, sxw.len, nm, np, now);
            sxw_stlive = (uint8_t)sxw_is_live(sxw.index);   /* (before its yes: the old version still the one found) */
        }
        sxw_send(cmd, seq, sxw_question(d));
        break;
    }
    case SXW_CANCEL:
        sxw_drop();
        if (sxw.state == SXW_S_ASKING)
            sxw.state = SXW_S_NO;
        sxw_rs_close();
        sxw_send(cmd, seq, sxw_question(d));
        break;
    case SXW_RESTORE: {                          /* u8 sets (a mask), u8 the pieces */
        uint32_t m = 0;
        if (k != 2u || d[1] > 1u || (!d[0] && !d[1])) {
            sxw_nak(cmd, seq, SXW_E_BAD, 0);
            break;
        }
        sxw_drop();
        sxw_rs_close();
        sxw.rq_sets = d[0], sxw.rq_work = d[1];
        for (i = 0; i < SXW_NSETS; i++)
            m += sxw.rq_sets >> i & 1u;
        sxw_ask(SXW_RESTORE, sxw.rq_sets, -1, 0, "", m, now);
        sxw.kind = sxw.rq_work ? FS_WORK : 0u;
        sxw_send(cmd, seq, sxw_question(d));
        break;
    }
    case SXW_APPLY:
    case SXW_DELETE: {                           /* u8 kind 2, u8 index */
        uint32_t why, det;
        if (k != 2u || d[0] != FS_SET || d[1] >= SXW_NSETS) {
            sxw_nak(cmd, seq, SXW_E_BAD, 0);
            break;
        }
        if ((why = sxw_ask_stored(cmd, d[1], now, &det)) != 0) {
            sxw_nak(cmd, seq, why, det);
            break;
        }
        sxw_send(cmd, seq, sxw_question(d));
        break;
    }
    default:
        sxw_nak(cmd, seq, SXW_E_CMD, 0);
        break;
    }
    return 1;
}

/* ------------------------------------------------------------------------------------ the question (ui.c) --- */
static int sxw_asking(void) { return sxw.state == SXW_S_ASKING ? sxw.op : 0; }   /* the question waiting: its op, 0 none */

/* every frame (the UI's): a question unanswered too long, or the music started (MIDI): no; a transfer gone quiet:
 * dropped */
static void sxw_tick(uint32_t now)
{
    sxw.now = now;
    if (sxw.open && now - sxw.t > SXW_IDLE_MS)
        sxw_drop();
    if (sxw.rs_on && !sxw.open && (en_playing || now - sxw.rs_t > SXW_IDLE_MS))
        sxw_rs_close();                          /* (a restore gone quiet, or the music started: ended) */
    if (sxw.state == SXW_S_ASKING && (en_playing || now - sxw.t > SXW_ASK_MS)) {
        if (sxw.op == SXW_COMMIT)
            plat_fs_abort();
        sxw.state = en_playing ? SXW_S_PLAYING : SXW_S_TIMEOUT;
    }
}

/* the answer (GLO: yes, anything else no): done. SXW_S_YES (a set stored, loaded into the slots and saved, deleted),
 * SXW_S_NO, or SXW_S_FAILED (the flash, or the set no longer read) */
static int sxw_answer(int yes)
{
    int ok = 0;
    if (sxw.state != SXW_S_ASKING)
        return sxw.state;
    if (!yes) {
        if (sxw.op == SXW_COMMIT)
            plat_fs_abort();
        return sxw.state = SXW_S_NO;
    }
    sxw.rframe = -1;
    if (sxw.op == SXW_COMMIT) {
        ok = plat_fs_seal() == 0;
        if (ok && sxw_local)                     /* (the Set page's Store: the pieces are that set now) */
            sxw_live = (int8_t)sxw.index;
    } else if (sxw.op == SXW_APPLY) {
        const int rc = project_set_take(sxw.frame, sxw.flen);
        ok = rc >= 0, sxw.unsaved = (uint8_t)(rc > 0);
        if (ok)
            sxw_live = (int8_t)sxw.index;
    }
    else if (sxw.op == SXW_DELETE) {
        ok = plat_fs_delete(FS_SET, sxw.index) == 0;
        if (ok && sxw_live == (int)sxw.index)
            sxw_live = -1;
    }
    else if (sxw.op == SXW_RESTORE) {            /* (a restore open: its sets and pieces kept as they come) */
        sxw.rs_on = 1, sxw.rs_sets = sxw.rq_sets, sxw.rs_work = sxw.rq_work;
        sxw.rs_done = sxw.rs_note = sxw.rs_end = sxw.rs_kept = 0, sxw.rs_t = sxw.now;
        ok = 1;
    }
    return sxw.state = ok ? SXW_S_YES : SXW_S_FAILED;
}
