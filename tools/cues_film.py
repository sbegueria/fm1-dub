#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""(1.5.0, step 7) A film script's performer moves as cues (SETFILES.md "Cues", written (a)).

A tests/film script (`bar N` + tap / key / turn / spin lines) is run by the host simulator as it is, its film, WAV and
logs left out, with `cuerec` from the film's start (after `splashrun`): every move as the engine got it, on its clock
(host/amb_host.c crec_poll). Those are compiled into each piece's cues (app.h's CUE_* ops) and written into the set
file made from the same script (tools/make_sets.sh), as CUES chunks (container.c): the set then plays the film's moves.

  cues_film.py compile SCRIPT SET SLOTS [--work DIR] [--host BIN] [--events FILE]
      SCRIPT  the film script (read only)            SET  the .dub1 SET made from it (its CUES replaced)
      SLOTS   the script's pieces in the set's order (0..7, "3,4,5,6,7": the set's piece 1 is the script's slot 4)
      --events FILE: the moves recorded already (no run)
  cues_film.py compare FILM_EVENTS SET_EVENTS [--slots SLOTS]
      the moves of the film's run against a run of the set (its cues played): each matched by piece, kind and target,
      the difference in semicorcheas (16ths) and in ms

How a move becomes a cue (the engine's state, as `cuerec` saw it change):
  MUTE, SOLO              the part's state, at once (CUE_MUTE / CUE_SOLO)
  MOVE, FX (on .. off)    held from on to off (CUE_MOVE / CUE_FX, len its semicorcheas (16ths), 1 at least); one not let
                          go before its song ends: held to the end
  SKEY (a siren's key)    a tap (let go before its slow one began): CUE_SIREN len 0; held: CUE_SIREN len (3 at least)
  MAC, SIRK, PAR          a knob's turns, one ride each run of them (no gap over 6 s, none 3x the one before, one way):
                          to its last value, over the run's semicorcheas plus one gap (its first step at once, as the
                          first detent); a jump of more than 3 (a spin): at once (CUE_MACRO / CUE_SIRK / CUE_KNOB)
  LOCK, HOLD, OUTRO       at once (CUE_LOCK / CUE_HOLD / CUE_OUTRO)
A move's place: the semicorchea (16th) it fell in, rounded to the nearer step line (the film's keys come within ms of
`bar N`; a throw begins 300 ms after its key, as on the FM-1), then the song's part it is in (intro, main, outro, as
the engine had them) and its bar and step there. Not mapped: a move after its song ended (the music stopped), in a
Set's last bar after the hand-over was cued (the next piece already set: its list in RAM), or a bar past 1023.
"""
import os
import subprocess
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import dub1_container as D

OPS = ["mute", "solo", "move", "fx", "siren", "sirk", "macro", "knob", "outro", "next", "lock", "hold"]
OP = {n: i for i, n in enumerate(OPS)}
PARTS = "imob"
SKIP = {"film", "filmstop", "wav", "wavstop", "tlog", "tlogstop", "stems", "shot"}
MOVE_NAME = ["chord throw", "Open", "Shut", "Drop", "Echo", "hats throw", "pings throw", "bass throw", "kick throw"]
FX_NAME = ["Throw", "Time x2", "Tape stop", "Swell", "Low-pass", "High-pass", "Wah", "Phone", "Verb", "Crash", "Freeze",
           "Wash", "Stutter", "Gate", "Siren", "Riser"]
SIREN_NAME = ["Wail", "Whoop", "Hi-lo", "Laser"]
MAC_NAME = ["the DJ filter", "Energy", "Wet", "Duck"]
SK_NAME = ["Pitch", "Rate", "Depth", "Echo"]


class Ev:
    __slots__ = ("ms", "bars", "frac", "piece", "ph", "b0", "bo", "be", "playing", "trcue", "what", "arg", "val", "t")

    def __init__(self, f):
        (self.ms, self.bars, self.frac, self.piece, self.ph, self.b0, self.bo, self.be, self.playing, self.trcue) = \
            [int(x) for x in f[:10]]
        self.what, self.arg, self.val = f[10], int(f[11]), int(f[12])
        self.t = self.bars + (1 if self.frac >= 500 else 0)   # (the nearer step line)


def read_events(path):
    out = []
    for ln in open(path):
        if ln.startswith("#") or not ln.strip():
            continue
        out.append(Ev(ln.split()))
    return out


def place(e, t=None):
    """(part, bar, step) of step t (e's by default) in e's song, or None"""
    t = e.t if t is None else t
    b0, bo = e.b0 * 16, e.bo * 16
    if e.ph == 0:                                # (Endless: no parts; the main from PLAY)
        part, rel = 1, t
    elif t < b0:
        part, rel = 0, t
    elif bo and t >= bo:
        part, rel = 2, t - bo
    else:
        part, rel = 1, t - b0
    if rel // 16 > 1023:
        if t // 16 > 1023:
            return None
        part, rel = 3, t
    return part, rel // 16, rel % 16


def why_not(e, t=None):
    """why a move at e can't be a cue (None: it can)"""
    t = e.t if t is None else t
    if not e.playing:
        return "after its song ended (the music stopped)"
    if e.trcue:
        return "in the last bar after the hand-over was cued (the next piece's list already in RAM)"
    if e.be and t >= e.be * 16:
        return "past its song's end"
    if place(e, t) is None:
        return "past bar 1023"
    return None


def describe(op, arg, val, ln):
    if op == OP["mute"]:
        return "%s %s" % (["chord", "pings", "bass", "kick", "hats", "noise"][arg], "muted" if val else "on")
    if op == OP["solo"]:
        return "%s %s" % (["chord", "pings", "bass", "kick", "hats", "noise"][arg], "solo" if val else "solo off")
    if op == OP["move"]:
        return "%s held %d" % (MOVE_NAME[arg], ln)
    if op == OP["fx"]:
        return "%s held %d" % (FX_NAME[arg], ln)
    if op == OP["siren"]:
        return "%s %s" % (SIREN_NAME[arg], "tapped" if not ln else "held %d" % ln)
    if op == OP["macro"]:
        return "%s to %d over %d" % (MAC_NAME[arg], val, ln)
    if op == OP["sirk"]:
        return "%s's %s to %d over %d" % (SIREN_NAME[arg // 4], SK_NAME[arg % 4], val, ln)
    if op == OP["knob"]:
        return "parameter %d to %d over %d" % (arg, val, ln)
    return "%s %d" % (OPS[op], val)


def started(evs):
    """the moves made in the ms before the music starts (a film's first, as PLAY or the launch screen starts it: the
    engine plays a block later) as the song's first step's"""
    out, pending = [], []
    for k, e in enumerate(evs):
        if e.what == "PLAY" and e.val == 1:
            out.append(e)
            nxt = next((x for x in evs[k + 1:] if x.playing), None)
            for p in pending:
                if e.ms - p.ms <= 1000 and nxt is not None:
                    p.playing, p.bars, p.frac, p.t = 1, 0, 0, 0
                    p.ph, p.b0, p.bo, p.be, p.piece = nxt.ph, nxt.b0, nxt.bo, nxt.be, nxt.piece
                out.append(p)
            pending = []
        elif not e.playing and e.what not in ("PIECE", "PLAY"):
            pending.append(e)
        else:
            out += pending
            pending = []
            out.append(e)
    return out + pending


def compile_events(evs):
    """{piece: [cue, ...]} (cue: [pos, op, arg, val, len, note]) and [(piece, what, why), ...] not mapped"""
    cues, lost = {}, []
    evs = started(evs)
    runs, cur = [], []
    last_bars = None
    for e in evs:                                # (a run: a piece from its start to its end, PLAY to stop or the next)
        boundary = e.what in ("PIECE", "PLAY") or (last_bars is not None and e.bars < last_bars)
        if boundary and cur:
            runs.append(cur)
            cur = []
        if e.what not in ("PIECE", "PLAY"):
            cur.append(e)
        last_bars = e.bars
    if cur:
        runs.append(cur)

    def add(e, op, arg, val, ln, t=None, note=""):
        why = why_not(e, t)
        name = describe(op, arg, val, ln)
        if why:
            lost.append((e.piece, "bar %d: %s" % ((e.t if t is None else t) // 16, name), why))
            return
        part, bar, step = place(e, t)
        cues.setdefault(e.piece, []).append([D_pos(part, bar, step), op, arg, val, ln, name + note])

    for run in runs:
        held, skey, rides = {}, {}, {}
        end_t = max([e.be * 16 for e in run if e.be] or [max(e.t for e in run)])   # (its song's end, if planned)
        for e in run:
            w = e.what
            if w in ("MUTE", "SOLO"):
                add(e, OP[w.lower()], e.arg, e.val, 0)
            elif w in ("MOVE", "FX"):
                k = (w, e.arg)
                if e.val:
                    held[k] = e
                elif k in held:
                    s = held.pop(k)
                    add(s, OP[w.lower()], e.arg, 0, max(1, e.t - s.t))
            elif w == "SKEY":
                if e.val == 1:
                    skey[e.arg] = [e, False]
                elif e.val == 2 and e.arg in skey:
                    skey[e.arg][1] = True
                elif e.val == 0 and e.arg in skey:
                    s, hold = skey.pop(e.arg)
                    add(s, OP["siren"], e.arg, 0, max(3, e.t - s.t) if hold else 0)
            elif w in ("MAC", "SIRK", "PAR"):
                rides.setdefault((w, e.arg), []).append(e)
            elif w in ("LOCK", "HOLD"):
                add(e, OP[w.lower()], 0, e.val, 0)
            elif w == "OUTRO":
                add(e, OP["outro"], 0, 0, 0)
        for (w, arg), s in held.items():         # (never let go in its song: held to its end)
            add(s, OP[w.lower()], arg, 0, max(1, end_t - s.t), note=" (to its song's end)")
        for arg, (s, hold) in skey.items():
            add(s, OP["siren"], arg, 0, max(3, end_t - s.t) if hold else 0, note=" (to its song's end)")
        op_of = {"MAC": OP["macro"], "SIRK": OP["sirk"], "PAR": OP["knob"]}
        for (w, arg), es in rides.items():
            groups, g = [], []
            last = 0 if w == "MAC" else None     # (the master actions start at 0: the launch screen's run, a boot)
            for e in es:
                jump = last is not None and abs(e.val - last) > 3
                last = e.val
                if jump:                         # (a spin: a cue of its own, at once)
                    if g:
                        groups.append(g)
                    groups.append([e])
                    g = []
                    continue
                if g:
                    gap = e.ms - g[-1].ms
                    prev = g[-1].ms - g[-2].ms if len(g) > 1 else None
                    d0 = g[-1].val - (g[-2].val if len(g) > 1 else g[-1].val)
                    d1 = e.val - g[-1].val
                    if gap > 6000 or (prev and gap > 3 * prev and gap > 300) or (d0 and d1 and (d0 > 0) != (d1 > 0)):
                        groups.append(g)
                        g = []
                g.append(e)
            if g:
                groups.append(g)
            for g in groups:
                if len(g) == 1:
                    add(g[0], op_of[w], arg, g[0].val, 0)
                else:
                    span = g[-1].t - g[0].t
                    gap = max(1, round(span / (len(g) - 1)))
                    add(g[0], op_of[w], arg, g[-1].val, span + gap,
                        note=" (%d turns over %.1f s)" % (len(g), (g[-1].ms - g[0].ms) / 1000.0))
    for p in cues:                               # (by place, the order they came in kept)
        cues[p].sort(key=lambda c: c[0])
    return cues, lost


def D_pos(part, bar, step):
    return part << 14 | (bar & 1023) << 4 | (step & 15)


def pos_text(pos):
    return "%s %d.%d" % (["intro", "main", "outro", "bar"][pos >> 14], pos >> 4 & 1023, pos & 15)


def run_script(script, work, host):
    """the script run as it is (no film), cuerec from its film's start: the events file"""
    os.makedirs(work, exist_ok=True)
    lines = open(script).read().split("\n")
    out, put = [], False
    has_run = any(ln.split()[:1] == ["splashrun"] for ln in lines)
    ev = os.path.join(work, "film-events.txt")
    for ln in lines:
        w = ln.split()[:1]
        if w and w[0] == "film":
            out.append("filmclock")              # (its clock, for `at`: no film written)
            continue
        if w and w[0] in SKIP:
            continue
        out.append(ln)
        if not put and w and w[0] == ("splashrun" if has_run else "splash"):
            out.append("cuerec %s" % ev)
            put = True
    if not put:
        out.insert(0, "cuerec %s" % ev)
    out.append("cuerec off")
    sc = os.path.join(work, "film-rec.amb")
    open(sc, "w").write("\n".join(out) + "\n")
    log = os.path.join(work, "film-rec.log")
    with open(log, "w") as f:
        rc = subprocess.call([host, sc, work], stdout=f, stderr=subprocess.STDOUT)
    bad = [ln for ln in open(log) if ln.startswith("FAIL") or ln.startswith("line ")]
    if bad:
        sys.stderr.write("".join(bad[:10]))
        sys.stderr.write("the script's run failed (rc %d): %s\n" % (rc, log))
        sys.exit(1)
    return ev


def write_set(path, slots, cues):
    c = D.parse(open(path, "rb").read())
    if c["header"]["kind"] != 2:
        raise SystemExit("%s: not a SET" % path)
    chunks = [ch for ch in c["chunks"] if ch["id"] != b"CUES"]
    n = 0
    for i, s in enumerate(slots):
        q = cues.get(s, [])
        if not q:
            continue
        if len(q) > 64:
            raise SystemExit("piece %d: %d cues, 64 at most" % (i + 1, len(q)))
        chunks.append(dict(id=b"CUES", index=i, flags=0, cues=[x[:5] for x in q]))
        n += len(q)
    c["chunks"] = chunks
    open(path, "wb").write(D.build(c))
    return n


def compile_main(args):
    script, setf, slots = args[0], args[1], [int(x) for x in args[2].split(",")]
    opt = dict(zip(args[3::2], args[4::2]))
    work = opt.get("--work") or tempfile.mkdtemp(prefix="cues-")
    host = opt.get("--host", "build/host/amb_host_cues")   # (the cues played: DUB1_CUES=1)
    ev = opt.get("--events") or run_script(script, work, host)
    cues, lost = compile_events(read_events(ev))
    n = write_set(setf, slots, cues)
    names = [D.name(ch["name"]) for ch in D.parse(open(setf, "rb").read())["chunks"] if ch["id"] == b"PIEC"]
    print("%s: %d cues from %s" % (setf, n, script))
    for i, s in enumerate(slots):
        q = cues.get(s, [])
        print("  piece %d (%s, the script's slot %d): %d cues" % (i + 1, (names[i] if i < len(names) else "") or "no name",
                                                                    s + 1, len(q)))
        for c in q:
            print("    %-14s %s" % (pos_text(c[0]), c[5]))
    others = sorted(set(cues) - set(slots))
    for s in others:
        print("  (the script's slot %d, not in the set: %d moves left out)" % (s + 1, len(cues[s])))
    if lost:
        print("  not mapped:")
        for p, what, why in lost:
            print("    slot %d, %s: %s" % (p + 1, what, why))
    return 0


def runs_of(evs, order_of):
    """the events of each piece's run (its song from the top: a run per piece, the film's and the set's alike), by the
    piece's place in the set; each run's events while it plays, the hand-over's last bar left out"""
    out, cur, last = {}, None, None
    for e in started(evs):
        if e.what == "PLAY" or (last is not None and e.bars < last) or e.what == "PIECE":
            cur = None
        last = e.bars
        if e.what in ("PLAY", "PIECE") or e.piece not in order_of:
            continue
        if cur is None:
            cur = out.setdefault(order_of[e.piece], [])
        cur.append(e)
    return out


def curve(evs, what, arg, start, end):
    """a knob's value at each semicorchea (16th) start .. end-1 (None before its first change: the value it had)"""
    v, out, k = None, [], 0
    ch = [e for e in evs if e.what == what and e.arg == arg]
    for t in range(start, end):
        while k < len(ch) and ch[k].bars <= t:
            v = ch[k].val
            k += 1
        out.append(v)
    return out


def loudness(wavf, ms0, t_ms, span_ms):
    """the RMS level (dBFS) of a WAV (16-bit stereo, 44.1 kHz) over [t, t + span) ms, ms0 the WAV's start"""
    import math
    import struct as st
    import wave
    w = wavf
    a = int((t_ms - ms0) * 44.1)
    n = int(span_ms * 44.1)
    if a < 0 or a + n > w.getnframes():
        return None
    w.setpos(a)
    raw = w.readframes(n)
    x = st.unpack("<%dh" % (len(raw) // 2), raw)
    e = sum(v * v for v in x[::8]) / max(1, len(x[::8]))
    return 10 * math.log10(e / 32768.0 ** 2 + 1e-12)


def compare_main(args):
    """the film's moves against the set's run, piece by piece: the moves (a mute, a solo, a throw or an effect held and
    let go, a siren sounding) matched in order, how far apart in semicorcheas (16ths); the rides (the master actions,
    the knobs) as their value at each semicorchea (16th); with the WAVs, each move's level change in both"""
    film, run = read_events(args[0]), read_events(args[1])
    opt = dict(zip(args[2::2], args[3::2]))
    slots = [int(x) for x in opt.get("--slots", "0,1,2,3,4,5,6,7").split(",")]
    strict = opt.get("--strict") == "1"
    a = runs_of(film, {s: i for i, s in enumerate(slots)})
    b = runs_of(run, {i: i for i in range(len(slots))})
    wavs = {}
    for k, f in (("film", args[0]), ("set", args[1])):
        for ln in open(f):
            if ln.startswith("# wav "):
                wavs[k] = (ln.split()[2], int(ln.split()[3]))
    wa = wb = None
    if opt.get("--film-wav") and opt.get("--set-wav") and "film" in wavs and "set" in wavs:
        import wave
        wa, wb = wave.open(opt["--film-wav"], "rb"), wave.open(opt["--set-wav"], "rb")
    disc = ("MUTE", "SOLO", "MOVE", "FX", "SIRON")
    diffs, only_f, only_s, lv = [], [], [], []
    rides = []
    for p in sorted(set(a) | set(b)):
        x, y = a.get(p, []), b.get(p, [])
        keys = sorted({(e.what, e.arg, e.val) for e in x + y if e.what in disc and not (e.what == "SIRON" and not e.arg)
                       and e.playing and not e.trcue})
        for k in keys:
            ex = [e for e in x if (e.what, e.arg, e.val) == k and e.playing and not e.trcue]
            ey = [e for e in y if (e.what, e.arg, e.val) == k and e.playing and not e.trcue]
            for i in range(max(len(ex), len(ey))):
                if i >= len(ex):
                    only_s.append((p, ey[i]))
                elif i >= len(ey):
                    only_f.append((p, ex[i]))
                else:
                    diffs.append((p, ex[i], ey[i]))
                    if wa is not None and k[2] != 0:
                        la = [loudness(wa, wavs["film"][1], ex[i].ms + d, 2000) for d in (-2000, 0)]
                        lb = [loudness(wb, wavs["set"][1], ey[i].ms + d, 2000) for d in (-2000, 0)]
                        if None not in la + lb:
                            lv.append((p, k, la[1] - la[0], lb[1] - lb[0]))
        if x and y:
            end = min(max(e.bars for e in x), max(e.bars for e in y))
            for what in ("MAC", "PAR"):
                for arg in sorted({e.arg for e in x + y if e.what == what}):
                    cx, cy = curve(x, what, arg, 0, end), curve(y, what, arg, 0, end)
                    both = [(u, v) for u, v in zip(cx, cy) if u is not None and v is not None]
                    if both:
                        d = [abs(u - v) for u, v in both]
                        rides.append((p, what, arg, len(both), sum(1 for z in d if z == 0), sum(d) / len(d), max(d)))
    print("moves (a mute, a solo, a throw or an effect held and let go, a siren): %d matched; film only %d, set only %d" % (
        len(diffs), len(only_f), len(only_s)))
    bad = len(only_f) + len(only_s)
    if diffs:
        ds = [d[2].t - d[1].t for d in diffs]
        dt = [((d[2].bars + d[2].frac / 1000.0) - (d[1].bars + d[1].frac / 1000.0)) for d in diffs]
        print("  on the same semicorchea (16th): %d of %d; one apart: %d; more: %d" % (
            sum(1 for z in ds if z == 0), len(ds), sum(1 for z in ds if abs(z) == 1), sum(1 for z in ds if abs(z) > 1)))
        print("  the set's against the film's, in semicorcheas (16ths): mean %+.2f, mean |.| %.2f, max |.| %.2f "
              "(at 120: x 125 ms)" % (sum(dt) / len(dt), sum(abs(z) for z in dt) / len(dt), max(abs(z) for z in dt)))
        bad += sum(1 for z in ds if abs(z) > 1)
        for p, x, y in diffs:
            if abs(y.t - x.t) > 1:
                print("  apart: piece %d %s %d %d: film %d.%d, set %d.%d" % (p + 1, x.what, x.arg, x.val, x.bars // 16,
                                                                            x.bars % 16, y.bars // 16, y.bars % 16))
    for p, e in only_f:
        print("  film only: piece %d %s %d %d at bar %d.%d%s" % (p + 1, e.what, e.arg, e.val, e.bars // 16, e.bars % 16,
                                                                  "" if e.playing else " (stopped)"))
    for p, e in only_s:
        print("  set only: piece %d %s %d %d at bar %d.%d" % (p + 1, e.what, e.arg, e.val, e.bars // 16, e.bars % 16))
    for p, what, arg, n, same, mean, mx in rides:
        name = MAC_NAME[arg] if what == "MAC" else "parameter %d" % arg
        print("  ride: piece %d %s: %d semicorcheas (16ths) compared, the same value %d (%.1f%%), mean |diff| %.2f, "
              "max %d" % (p + 1, name, n, same, 100.0 * same / n, mean, mx))
        if strict and mx > 2:
            bad += 1
    if lv:
        dd = [abs(u - v) for _, _, u, v in lv]
        print("  level change at the moves (2 s after against 2 s before), film against set: mean |diff| %.1f dB, "
              "max %.1f dB, %d moves" % (sum(dd) / len(dd), max(dd), len(dd)))
        for p, k, u, v in lv:
            print("    piece %d %s %d: film %+.1f dB, set %+.1f dB" % (p + 1, k[0], k[1], u, v))
    if strict and bad:
        print("FAIL: %d moves not where the film has them" % bad)
        return 1
    return 0


def main(argv):
    if len(argv) >= 5 and argv[1] == "compile":
        return compile_main(argv[2:])
    if len(argv) >= 4 and argv[1] == "compare":
        return compare_main(argv[2:])
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv))
