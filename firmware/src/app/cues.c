/* SPDX-License-Identifier: GPL-3.0-only */
/* (1.5.0, step 7) The cue player: a set's moves at set times in a song (build/design/STORAGE-1.5.0.md 5.5, SETFILES.md
 * "Cues"), played on the engine's own clock (en_bars: the semicorcheas (16ths) since PLAY, again from 0 at a Set's
 * next song) with the calls PERFORM's keys and knobs make. Part of ui.c (the unity build): it plays into the same
 * state the keys do (the mutes, the solos; the moves and effects it holds are its own, sent with the keys': perf_send,
 * perf_fx_send), so you can play on top: a key, a knob, PERFORM as ever; a knob you turn while a cue rides it is
 * yours from then on.
 *
 * Only the playing piece's list is in RAM (64 cues, 512 bytes), read from where it is (project.c's cues_read: the WORK
 * frame, or a set's) when the piece changes: a Set's Segue or Mix sets the next piece at the outro's last bar
 * (set_tick, en_tr_cue), so its list is read then, a bar before its downbeat, and nothing of it plays till that
 * downbeat. A cue's place: its part's first bar (the intro from PLAY, the main from en_song_b0, the outro from
 * en_song_bo once it is planned) plus its bar and step; it fires when the engine has played every step before it
 * (as `bar N` in a film script: just before that step's tick). A cue whose part ends first is dropped (an intro's past
 * the main, a main's past the outro, any past the song's end: Decisions 1); an outro's waits till the outro is planned
 * (never in Endless). Moves held (a throw, Open, Freeze, a siren held) end after their length, or with the song: when
 * it stops, at a Set's hand-over, when the piece is changed, or the cues turned off (then a ride of the DJ filter or
 * another master action lands on its value, a knob's ride gives the piece's own value back: a cue never changes the
 * piece itself, nothing to save). One switch turns them all off: proj.cues_off (the Set page's Cues), saved
 * with the player's settings; and DUB1_CUES (ui.c, 0 in 1.5.0's builds) at build time: the list read, none played. (Decisions 7) A cue shows what its key or knob would (cue_show: the card, or the header's
 * name, the filter's ring) and the screen reader says none of it; the mutes and solos the cues changed are put back as
 * they were when the music stops (cue_unmute; a Mix, a Segue or a Set's rest carry them on, as before) */
#define CUE_NEVER 0xFFFFFFFFu
static void big_show(int k, const char *name, uint16_t col);
enum { CA_FREE, CA_MOVE, CA_FX, CA_SHOLD, CA_SOFF };


/* the step cue q is due at (since PLAY), and the step its part ends at (*end; CUE_NEVER: not known); CUE_NEVER: its
 * part not planned (an outro's before the outro is) */
static uint32_t cue_when(const d1c_cue_t *q, uint32_t *end)
{
    const uint32_t part = (uint32_t)q->pos >> 14, rel = ((uint32_t)q->pos >> 4 & 1023u) * 16u + (q->pos & 15u);
    const uint32_t b0 = en_song_b0 * 16u, bo = en_song_bo * 16u, be = en_song_be * 16u;   /* (bo, be 0: none planned) */
    uint32_t base = 0;
    *end = be ? be : CUE_NEVER;
    if (part == CUE_P_INTRO) {
        *end = b0;                               /* (no intro, Endless's or Intro None: none of them) */
    } else if (part == CUE_P_MAIN) {
        base = b0;
        if (bo)
            *end = bo;
    } else if (part == CUE_P_OUTRO) {
        if (!bo)
            return CUE_NEVER;
        base = bo;
    }
    return base + rel;
}

static void cue_post(uint32_t op, uint32_t arg, int v)   /* a ride's value: as the knob would send it */
{
    if (op == CUE_MACRO) {
        ui.mac[arg] = (int8_t)v;                 /* (the ring shows it; never saved, as the knob's) */
        en_macro((int)arg, v);
    } else if (op == CUE_SIRK) {
        en_sir_set((int)(arg / EN_NSK), (int)(arg % EN_NSK), v);   /* (the engine's only: proj.sir, saved, the piece's) */
        cue.sirk |= (uint16_t)(1u << arg);
    } else {
        en_set((int)arg, v);                     /* (the engine's only: proj.par stays the piece's) */
        cue.knobs[arg >> 5] |= 1u << (arg & 31u);
    }
}

static void cue_ride(const d1c_cue_t *q, uint32_t t)   /* a ride from where it is to q's value over q's len */
{
    uint32_t i, k = CUE_NRIDE;
    int from;
    for (i = 0; i < CUE_NRIDE; i++)              /* (its target's slot: a ride on it, or its last value) */
        if (cue.ride[i].op == q->op && cue.ride[i].arg == q->arg && cue.ride[i].len)
            k = i;
    if (k == CUE_NRIDE)
        for (i = 0; i < CUE_NRIDE && k == CUE_NRIDE; i++)   /* (a free one, else one done, else the first) */
            if (!cue.ride[i].len)
                k = i;
    if (k == CUE_NRIDE)
        for (i = 0; i < CUE_NRIDE && k == CUE_NRIDE; i++)
            if (!cue.ride[i].on)
                k = i;
    if (k == CUE_NRIDE)
        k = 0;
    if (cue.ride[k].len && cue.ride[k].op == q->op && cue.ride[k].arg == q->arg)
        from = cue.ride[k].now;
    else
        from = q->op == CUE_MACRO ? ui.mac[q->arg]
             : q->op == CUE_SIRK ? proj.sir[q->arg / EN_NSK][q->arg % EN_NSK] : proj.par[q->arg];
    cue.ride[k].op = q->op, cue.ride[k].arg = q->arg, cue.ride[k].t0 = t, cue.ride[k].len = q->len ? q->len : 1u;
    cue.ride[k].from = (int16_t)from, cue.ride[k].to = q->val, cue.ride[k].now = (int16_t)from, cue.ride[k].on = 1;
}

static void cue_rides(uint32_t b)                /* each ride's value at step b (its first step at once) */
{
    uint32_t i;
    for (i = 0; i < CUE_NRIDE; i++) {
        const uint32_t len = cue.ride[i].len;
        int32_t v, d;
        uint32_t k;
        if (!cue.ride[i].on || b < cue.ride[i].t0)
            continue;
        k = b - cue.ride[i].t0 + 1u;
        d = cue.ride[i].to - cue.ride[i].from;
        if (k >= len) {
            v = cue.ride[i].to, cue.ride[i].on = 0;
        } else {                                 /* (each value as a knob's detent: the first at once, then at even
                                                  * steps; rounded away from where it started) */
            const int32_t num = d * (int32_t)k;
            v = cue.ride[i].from + (num + (num < 0 ? -((int32_t)len - 1) : (int32_t)len - 1)) / (int32_t)len;
        }
        if (v != cue.ride[i].now || !cue.ride[i].on) {
            cue.ride[i].now = (int16_t)v;
            cue_post(cue.ride[i].op, cue.ride[i].arg, v);
        }
    }
}

static uint32_t cue_free(void)                  /* the things due that can still be kept */
{
    uint32_t i, n = 0;
    for (i = 0; i < CUE_NACT; i++)
        n += cue.act[i].what == CA_FREE;
    return n;
}
static void cue_act(uint32_t at, uint32_t what, uint32_t arg)   /* something due at step at (a hold's end; the caller
                                                  * made sure there's room: a hold never begun that can't end) */
{
    uint32_t i;
    for (i = 0; i < CUE_NACT; i++)
        if (cue.act[i].what == CA_FREE) {
            cue.act[i].at = at, cue.act[i].what = (uint8_t)what, cue.act[i].arg = (uint8_t)arg;
            return;
        }
}
static void cue_do(uint32_t what, uint32_t arg)
{
    if (what == CA_MOVE) {
        cue.moves &= ~(1u << arg);
        perf_send();
    } else if (what == CA_FX) {
        cue.fx &= ~(1u << arg);
        perf_fx_send();
    } else if (cue.siren == arg + 1u && !ui.skey) {   /* (a siren the keys took over: theirs) */
        if (what == CA_SHOLD) {
            en_siren((int)arg, EN_SIR_HOLD);
            cue.sheld = 1;
            card_show(PERF_SNAME[arg], PERF_SHOLD[arg], K_FROST, 0);   /* (Decisions 7: the held one's card, unsaid) */
        } else {
            if (cue.sheld)
                en_siren((int)arg, EN_SIR_OFF);
            cue.siren = 0, cue.sheld = 0;
        }
    }
}
static void cue_acts(uint32_t b)
{
    uint32_t i;
    for (i = 0; i < CUE_NACT; i++)
        if (cue.act[i].what != CA_FREE && cue.act[i].at <= b) {
            const uint32_t w = cue.act[i].what;
            cue.act[i].what = CA_FREE;
            cue_do(w, cue.act[i].arg);
        }
}

/* (Decisions 7) a cue shows what its key or knob would show (the card in the middle: "CHORD THROW"; a grid effect's,
 * dub hold's and a knob's name in the header; a master action's ring), and says nothing: the screen reader is told only
 * of what you do (say_as with no flags: the header, not the reader) */
static void cue_show(const d1c_cue_t *q)
{
    const uint32_t a = q->arg;
    uint32_t i;
    char n[24];
    const char *s;
    switch (q->op) {
    case CUE_MUTE:
        card_show(PERF_NAME[a], q->val ? "muted" : "on", q->val ? K_DIM : PERF_COL[a], 0);
        break;
    case CUE_SOLO:
        if (ui.solo >> a & 1u)
            card_show("SOLO", PERF_LOW[a], K_CLAP, 0);
        else if (ui.solo)
            card_show(PERF_NAME[a], "solo off", K_CLAP, 0);
        else
            card_show("SOLO", "off: all heard", K_CLAP, 0);
        break;
    case CUE_MOVE:
        for (i = 0; i < PERF_NB; i++)
            if (PERF_BTHR[i] == 1u << a)
                card_show(PERF_THRNAME[i], PERF_THRWHAT[i], K_FROST, 0);
        for (i = 0; i < PERF_NM; i++)
            if (PERF_MBIT[i] == 1u << a)
                card_show(PERF_NAME[PERF_NP + i], PERF_WHAT[i], K_FROST, 0);
        break;
    case CUE_FX:
        for (i = 0; i < PERF_NF && PERF_FX[i] != a; i++)
            ;
        if (i < PERF_NF)                         /* (one of PERFORM's four: its key's card) */
            card_show(PERF_NAME[PERF_F0 + i], "held", K_FROST, 0);
        else                                     /* (the grid's: its name in the header, as its key) */
            say_as(EN_FX_NAME[a], 0, 0);
        break;
    case CUE_SIREN: card_show(PERF_SNAME[a], PERF_STAP[a], K_FROST, 0); break;
    case CUE_SIRK:                               /* ("WAIL ECHO": the siren's knob, in the header) */
        for (i = 0, s = PERF_SNAME[a / EN_NSK]; *s; )
            n[i++] = *s++;
        n[i++] = ' ';
        for (s = knob_name(K_SPITCH + (int)(a % EN_NSK)); *s && i < sizeof n - 1; s++)
            n[i++] = (char)(*s >= 'a' && *s <= 'z' ? *s - 32 : *s);
        n[i] = 0;
        say_as(n, 0, 0);
        break;
    case CUE_MACRO: big_show(K_FILTER + (int)a, knob_name(K_FILTER + (int)a), K_ACC); break;   /* (its ring rides) */
    case CUE_KNOB:                               /* (the engine's only: the name, not a ring showing the piece's value) */
        for (i = 0, s = knob_name((int)a); *s && i < sizeof n - 1; s++)
            n[i++] = (char)(*s >= 'a' && *s <= 'z' ? *s - 32 : *s);
        n[i] = 0;
        say_as(n, 0, 0);
        break;
    case CUE_OUTRO: card_show("OUTRO", "from the next phrase", K_ACC, 0); break;
    case CUE_NEXT: card_show("NEXT CHORD", en_playing ? "on the bar" : "", K_PULSE, 0); break;
    case CUE_LOCK: card_show("LOCK", q->val ? "the groove kept" : "the groove free", K_ACC, 0); break;
    case CUE_HOLD: say_as("DUB HOLD", q->val ? "ON" : "OFF", 0); break;
    default: break;
    }
}

static void cue_fire(const d1c_cue_t *q, uint32_t t)   /* q, due at step t: as its key or knob */
{
    const uint32_t a = q->arg, m = 1u << a;
    int shown = 1;                               /* (0: nothing done, nothing shown) */
    switch (q->op) {
    case CUE_MUTE:
        if (!(cue.mset & m))                     /* (Decisions 7: how it was before the cues touched it) */
            cue.mwas = (uint8_t)((cue.mwas & ~m) | (ui.mutes & m));
        cue.mset |= (uint8_t)m;
        ui.mutes = q->val ? ui.mutes | m : ui.mutes & ~m;
        perf_mutes();
        break;
    case CUE_SOLO:
        if (!(cue.sset & m))
            cue.swas = (uint8_t)((cue.swas & ~m) | (ui.solo & m));
        cue.sset |= (uint8_t)m;
        ui.solo = q->val ? ui.solo | m : ui.solo & ~m;
        perf_mutes();
        break;
    case CUE_MOVE:                               /* (eight holds at once at most: a ninth not begun) */
        if (cue_free()) {
            cue.moves |= m;
            perf_send();
            cue_act(t + q->len, CA_MOVE, a);
        } else {
            shown = 0;
        }
        break;
    case CUE_FX:
        if (cue_free()) {
            cue.fx |= m;
            perf_fx_send();
            cue_act(t + q->len, CA_FX, a);
        } else {
            shown = 0;
        }
        break;
    case CUE_SIREN:                              /* (a tap: the short one; held: the slow one after 2 steps, as the key's
                                                  * third of a second; then let go) */
        en_siren((int)a, EN_SIR_TAP);
        if (q->len > 2u && cue_free() >= 2u) {
            cue.siren = (uint8_t)(a + 1u), cue.sheld = 0;
            cue_act(t + 2u, CA_SHOLD, a);
            cue_act(t + q->len, CA_SOFF, a);
        }
        break;
    case CUE_SIRK:
    case CUE_MACRO:
    case CUE_KNOB: cue_ride(q, t); break;
    case CUE_OUTRO:
        if (!en_tr_cue && en_song_ph != EN_SG_OUTRO && en_song_ph != EN_SG_END && !en_song_ask)
            en_outro();
        else
            shown = 0;
        break;
    case CUE_NEXT: en_next(); break;
    case CUE_LOCK:
        ui.lock = (uint8_t)q->val;
        en_lock(ui.lock);
        break;
    case CUE_HOLD:
        if (!!en_frozen != !!q->val)
            en_freeze(q->val);
        break;
    default: shown = 0; break;
    }
    if (shown)
        cue_show(q);
}

/* the moves the cues hold let go, their rides ended (the master actions' on their values, the knobs' the piece's
 * own again) */
static void cue_end(void)
{
    uint32_t i;
    for (i = 0; i < CUE_NRIDE; i++) {
        if (cue.ride[i].on && cue.ride[i].op == CUE_MACRO)
            cue_post(CUE_MACRO, cue.ride[i].arg, cue.ride[i].to);
        cue.ride[i].on = 0, cue.ride[i].len = 0;
    }
    for (i = 0; i < CUE_NACT; i++)
        cue.act[i].what = CA_FREE;
    if (cue.moves) {
        cue.moves = 0;
        perf_send();
    }
    if (cue.fx) {
        cue.fx = 0;
        perf_fx_send();
    }
    if (cue.siren && cue.sheld && !ui.skey)
        en_siren(cue.siren - 1, EN_SIR_OFF);
    cue.siren = 0, cue.sheld = 0;
    for (i = 0; i < P_NPARAMS; i++)
        if (cue.knobs[i >> 5] >> (i & 31u) & 1u)
            en_set((int)i, proj.par[i]);
    memset(cue.knobs, 0, sizeof cue.knobs);
    for (i = 0; i < EN_NSIR * EN_NSK; i++)
        if (cue.sirk >> i & 1u)
            en_sir_set((int)(i / EN_NSK), (int)(i % EN_NSK), proj.sir[i / EN_NSK][i % EN_NSK]);
    cue.sirk = 0;
    cue.live = 0;
}

/* (Decisions 7) the music stopped: the mutes and solos the cues changed put back as they were before (those you tapped
 * since are yours, kept). Not at a Set's own rest between two songs (Transition LP: the next comes by itself, as a
 * Mix or a Segue carries them over) */
static void cue_unmute(void)
{
    if (cue.mset | cue.sset) {
        ui.mutes = (ui.mutes & ~(uint32_t)cue.mset) | (cue.mwas & cue.mset);
        ui.solo = (ui.solo & ~(uint32_t)cue.sset) | (cue.swas & cue.sset);
        perf_mutes();
    }
    cue.mset = cue.sset = cue.mwas = cue.swas = 0;
}
static void cue_stopped(void)                    /* (each pass: the music's stop, its fall seen) */
{
    const uint32_t last = proj.setlen ? proj.setlen : NPIECES;
    if (cue.played && !en_playing &&
        !(proj.par[P_SMODE] == 2 && en_song_ph == EN_SG_END && proj.piece + 1u < last))
        cue_unmute();
    cue.played = en_playing;
}

static void cue_touch(int k)                     /* (knob_set) a knob turned by hand: a cue's ride on it ends there */
{
    uint32_t i, op = CUE_NOPS, arg = 0;
    if (k >= K_FILTER && k <= K_SPACE)
        op = CUE_MACRO, arg = (uint32_t)(k - K_FILTER);
    else if (k >= K_SPITCH && k <= K_SECHO)
        op = CUE_SIRK, arg = (uint32_t)(sir_page % EN_NSIR) * EN_NSK + (uint32_t)(k - K_SPITCH);
    else if (k >= 0 && k < P_NPARAMS)
        op = CUE_KNOB, arg = (uint32_t)k;
    for (i = 0; i < CUE_NRIDE; i++)
        if (cue.ride[i].op == op && cue.ride[i].arg == arg)
            cue.ride[i].on = 0, cue.ride[i].len = 0;
}

#ifdef EN_NO_CUES                                /* (host/stems.c: the UI's second copy draws pictures only) */
static void cue_tick(void) {}
#else
/* the playing piece's list: read from where it is (cuesrc_t), unless it's the one in RAM already (a save only moved it:
 * then only where it is now noted, nothing restarted). 1: a new list */
static int cue_load(void)
{
    const cuesrc_t *s = &proj.cues[proj.piece];
    if (cue.slot == (int8_t)proj.piece) {
        uint8_t b[8 * D1C_CUE];
        uint32_t i, k, j, same = s->n == cue.n;
        if (!memcmp(&cue.src, s, sizeof *s))
            return 0;
        for (i = 0; same && i < cue.n; i += k) {   /* (8 at a time: no second list in RAM) */
            k = cue.n - i > 8u ? 8u : cue.n - i;
            if (cue_get(s, i, b, k))
                same = 0;
            for (j = 0; same && j < k; j++) {
                d1c_cue_t c;
                d1c_cue_in(b + D1C_CUE * j, &c);
                same = !memcmp(&c, &cue.q[i + j], sizeof c);
            }
        }
        if (same) {
            cue.src = *s;
            return 0;
        }
    }
    if (cue.live)
        cue_end();
    cue.slot = (int8_t)proj.piece;
    cue.src = *s;
    cue.n = (uint8_t)cues_read(s, cue.q);
    cue.done = 0;
    return 1;
}

static void cue_tick(void)                       /* (each pass of the main loop: input's) */
{
    const uint32_t b = en_bars;
    uint32_t i, t, e;
    if (cue.slot != (int8_t)proj.piece || memcmp(&cue.src, &proj.cues[proj.piece], sizeof cue.src))
        cue_load();                              /* (the piece changed, or its list moved or came from a set) */
    cue_stopped();
    if (!DUB1_CUES || proj.cues_off || !en_playing) {   /* (DUB1_CUES 0, the release's: never armed, none fired) */
        if (cue.live)
            cue_end();
        cue.bars = b;
        return;
    }
    if (en_tr_cue) {                             /* (a Set's hand-over under way: the next list waits for its downbeat) */
        cue.bars = b;
        return;
    }
    if (cue.live && b == cue.bars)
        return;
    if (!cue.live || b < cue.bars) {             /* (a run starts: from the top, PLAY or a Set's next song, every cue
                                                  * ahead; cues on, or a piece chosen, in the middle: those past left) */
        const int top = b < cue.bars || b == 0u;
        if (cue.live)
            cue_end();
        cue.done = 0;
        for (i = 0; i < cue.n && !top; i++)
            if ((t = cue_when(&cue.q[i], &e)) != CUE_NEVER && t < b)
                cue.done |= (uint64_t)1 << i;
        cue.live = 1;
    }
    cue.bars = b;
    cue_acts(b);
    for (i = 0; i < cue.n; i++) {
        if (cue.done >> i & 1u)
            continue;
        if ((t = cue_when(&cue.q[i], &e)) == CUE_NEVER)
            continue;
        if (t >= e) {                            /* (its part over first: dropped) */
            cue.done |= (uint64_t)1 << i;
            continue;
        }
        if (t <= b) {
            cue.done |= (uint64_t)1 << i;
            cue_fire(&cue.q[i], t);
        }
    }
    cue_acts(b);                                 /* (a hold of length 0 can't be; a siren's within the step: now) */
    cue_rides(b);
}
#endif
