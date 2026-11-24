#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""DUB-1's sets on an FM-1, over USB-MIDI (1.5.0): list them, back up the pieces as they are, read a stored set, send
one, load one into the pieces, delete one. Listing and reading work while the FM-1 plays; sending, loading and
deleting need it stopped, and it asks on its screen before it keeps, loads or deletes a set (GLO: yes; any other button,
key or knob: no; 30 s: no).

  dub1_set.py list
  dub1_set.py backup [FILE] [--all]  the pieces as they are now (WORK) to FILE (dub1-backup-DATE.dub1); --all: and every
                                     stored set, all in one file (a BACKUP, as the web page's Back up makes)
  dub1_set.py read SET FILE          stored set SET (1..8) to FILE
  dub1_set.py send SET FILE [--name NAME]   FILE (a set, or a backup sent as a set) to set SET; then GLO on the FM-1
  dub1_set.py load SET               set SET into the pieces (its pieces into slots 1..n); GLO on the FM-1
  dub1_set.py delete SET             GLO on the FM-1
  dub1_set.py restore FILE           a whole backup (backup --all's, the web page's) back: every set it holds into its
                                     own number and its pieces into the pieces, after one question on the FM-1 (RESTORE
                                     BACKUP? n sets + the pieces: GLO); sets it doesn't hold stay as they are
  dub1_set.py make OUT --from SPEC [--from SPEC ...] [--name NAME] [--piece-names "A|B|..."] [--mode M]
                  [--transition T]   a new set from the pieces of others (no FM-1 needed: files only), as the web
                                     page's Make a set: SPEC is FILE[@pieces|@N][:i,j,k], a set or the pieces (a
                                     backup's: @pieces its pieces, @N its set N), and which of its pieces (1..8) in that
                                     order (all by default); 1 to 8 pieces in all, in the order given. --piece-names:
                                     the pieces' new names in order, "|" between them (an empty one keeps its name).
                                     --mode endless|song|set, --transition lp|segue|mix (else the first file's); the
                                     sirens' knobs and Tune the first file's; each piece's cues kept with it
  --port NAME     the MIDI port (part of its name); else the first FM-1 that answers
  --host AMB_HOST the host simulator instead of an FM-1 (build/host/amb_host: its "sxpipe"; for the tests)

The set protocol (firmware/src/app/setwire.c): F0 7D 44 31 cmd seq pack7(body) chk F7, one request at a time, each
answered (2 s, 3 tries); 512-byte chunks to the FM-1, 256 back. Needs mido with python-rtmidi (as fm1_install.py).
The files are .dub1 containers (tools/dub1_container.py): what the web page reads and writes too.

Exit codes: 0 done, 1 not done (answered no, 30 s, refused), 2 bad arguments or file, 3 FM-1 not found, 4 connection
lost, 5 busy (playing: stop first).
"""
import argparse
import os
import queue
import subprocess
import sys
import tempfile
import threading
import time
import zlib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import dub1_container as D  # noqa: E402

H = bytes([0x7D, 0x44, 0x31])
HELLO, LIST, STATUS, READ, BEGIN, DATA, COMMIT, CANCEL, APPLY, DELETE, RESTORE, BUSY, NAK = (
    0x01, 0x02, 0x03, 0x10, 0x20, 0x21, 0x22, 0x24, 0x30, 0x31, 0x32, 0x7E, 0x7F)
NSETS = 8
WORK, SET = 1, 2
CHUNK_IN, CHUNK_OUT = 512, 256
S_IDLE, S_ASKING, S_YES, S_NO, S_TIMEOUT, S_FAILED, S_PLAYING = range(7)
STATE = {S_IDLE: "nothing asked", S_ASKING: "asking", S_YES: "done", S_NO: "answered no on the FM-1",
         S_TIMEOUT: "not answered on the FM-1 (30 s)", S_FAILED: "the FM-1's flash failed",
         S_PLAYING: "not answered: the music started"}
WHY = {1: "a bad request", 2: "a command this DUB-1 doesn't know (update it?)", 3: "no such set on the FM-1",
       4: "too big for a frame (16,128 bytes)", 5: "made for a newer DUB-1: update the FM-1",
       6: "not a set this DUB-1 takes", 7: "no room left on the FM-1", 8: "the FM-1's flash failed",
       9: "out of step (sent again from the start?)", 10: "nothing being sent"}
DELAY = {"reply": 2.0, "tries": 3, "poll": 0.25, "ask": 35.0, "open": 0.3}   # seconds; the tests shorten them
EXIT = {"usage": 2, "file": 2, "notfound": 3, "lost": 4, "busy": 5, "refused": 1, "no": 1}


class SetError(Exception):
    def __init__(self, code, msg):
        super().__init__(msg)
        self.code = code


# ------------------------------------------------------------ the wire ---

def pack7(data):
    out, acc, nb = [], 0, 0
    for b in data:
        acc |= b << nb
        nb += 8
        while nb >= 7:
            out.append(acc & 0x7F)
            acc >>= 7
            nb -= 7
    if nb:
        out.append(acc & 0x7F)
    return bytes(out)


def unpack7(s):
    out, acc, nb = [], 0, 0
    for b in s:
        acc |= (b & 0x7F) << nb
        nb += 7
        if nb >= 8:
            out.append(acc & 0xFF)
            acc >>= 8
            nb -= 8
    return bytes(out)


def frame(cmd, seq, body=b""):
    f = bytes([0xF0, *H, cmd & 0x7F, seq & 0x7F]) + pack7(body)
    return f + bytes([sum(f[4:]) & 0x7F, 0xF7])


def parse(pkt):
    """an answer: (cmd, seq, body), or None (someone else's, or garbled)"""
    if len(pkt) < 8 or pkt[0] != 0xF0 or pkt[-1] != 0xF7 or pkt[1:4] != H:
        return None
    if sum(pkt[4:-2]) & 0x7F != pkt[-2]:
        return None
    return pkt[4], pkt[5], unpack7(pkt[6:-2])


def u16(b, at):
    return b[at] | b[at + 1] << 8


def text(field):
    return bytes(field).split(b"\0", 1)[0].decode("latin-1")


def what(kind, index):
    """a set or the pieces, as the tool says it: set 3, the pieces"""
    return "set %d" % (index + 1) if kind == SET else "the pieces"


# ------------------------------------------------------------ the links ---

HOST_START = "wait 600\ntap PLAY\nwait 300\n"   # (the simulator plays from the start, as the FM-1: stopped first)


class HostLink:
    """the host simulator (build/host/amb_host) as an FM-1: its "sxpipe" takes frames and script lines on stdin and
    prints its answers ("sxout F0 .. F7"); cmd() runs a script line there (the tests press its buttons so). script:
    the lines run before (HOST_START: stopped)"""

    def __init__(self, amb_host, script=HOST_START, outdir=None):
        self.dir = outdir or tempfile.mkdtemp(prefix="dub1_set_")
        path = os.path.join(self.dir, "start.amb")
        with open(path, "w") as f:
            f.write(script + "sxpipe\n")
        self.q, self.log, self.lock = queue.Queue(), [], threading.Lock()
        self.acks = queue.Queue()
        self.p = subprocess.Popen([amb_host, path, self.dir], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                  stderr=subprocess.STDOUT, bufsize=0)
        self.ready = threading.Event()
        threading.Thread(target=self._rx, daemon=True).start()
        if not self.ready.wait(30):
            raise SetError("notfound", "the host simulator did not start its pipe")

    def _rx(self):
        for raw in iter(self.p.stdout.readline, b""):
            ln = raw.decode("latin-1").rstrip("\n")
            if ln.startswith("sxout "):
                self.q.put(bytes(int(x, 16) for x in ln[6:].split()))
            elif ln == "sxpipe":
                self.ready.set()
            elif ln.startswith("> ") or ln == "sxdone":
                self.acks.put(ln)
            else:
                self.log.append(ln)
        self.ready.set()

    @property
    def lost(self):
        return self.p.poll() is not None

    def _line(self, s):
        with self.lock:
            try:
                self.p.stdin.write((s + "\n").encode())
                self.p.stdin.flush()
            except OSError:
                return False
        return True

    def send(self, pkt):
        return self._line("sx " + " ".join("%02X" % b for b in pkt))

    def cmd(self, line, wait=True):
        """a script line run in the simulator (it has run once its echo comes back)"""
        while not self.acks.empty():
            self.acks.get_nowait()
        if not self._line(line):
            return False
        if wait:
            end = time.monotonic() + 30
            while time.monotonic() < end:
                try:
                    if self.acks.get(timeout=1) == "> " + line:
                        return True
                except queue.Empty:
                    pass
            return False
        return True

    def read(self, timeout):
        try:
            return self.q.get(timeout=max(timeout, 0))
        except queue.Empty:
            return None

    def drain(self):
        while not self.q.empty():
            self.q.get_nowait()

    def close(self):
        try:
            self.p.stdin.close()
            self.p.wait(timeout=10)
        except Exception:
            self.p.kill()


def open_device(port=None):
    """the first MIDI port (fm1_install.py's names) whose device answers HELLO as a DUB-1: a Device"""
    import fm1_install as I
    try:
        backend = I.MidoBackend()
    except ImportError:
        raise SetError("usage", "needs mido and python-rtmidi (pip install mido python-rtmidi)")
    names = backend.input_names()
    cands = [n for n in names if port.lower() in n.lower()] if port else [n for n in names if I.PORT_RE.search(n)]
    outs = backend.output_names()
    for name in cands:
        out = I.pair_output(name, outs)
        if not out:
            continue
        try:
            link = backend.open(name, out)
        except Exception:
            continue
        dev = Device(link)
        try:
            dev.hello(tries=1)
            return dev
        except SetError:
            link.close()
    raise SetError("notfound", "no DUB-1 1.5 answered (USB data cable? another app using the port? an older DUB-1?)"
                   "\n  MIDI inputs: " + (", ".join(names) if names else "(none)"))


# ------------------------------------------------------------ the device ---

class Device:
    """the set protocol over a link (send(pkt), read(timeout), drain(), lost)"""

    def __init__(self, link):
        self.link, self.seq = link, 0

    def request(self, cmd, body=b"", tries=None):
        """one request, its answer (cmd, body): tried again when unanswered (a frame lost or garbled either way)"""
        self.seq = (self.seq + 1) & 0x7F
        pkt = frame(cmd, self.seq, body)
        for _ in range(tries or DELAY["tries"]):
            self.link.drain()
            if not self.link.send(pkt):
                raise SetError("lost", "the FM-1 was disconnected")
            end = time.monotonic() + DELAY["reply"]
            while (left := end - time.monotonic()) > 0:
                a = self.link.read(left)
                if a is None:
                    break
                r = parse(a)
                if r and r[1] == self.seq and r[0] in (cmd, BUSY, NAK):
                    return r[0], r[2]
            if self.link.lost:
                raise SetError("lost", "the FM-1 was disconnected")
        raise SetError("lost", "the FM-1 doesn't answer (a DUB-1 1.5 there?)")

    def ok(self, cmd, body=b"", what=""):
        """a request that must be ACKed: its body; BUSY / NAK as a SetError"""
        c, b = self.request(cmd, body)
        if c == BUSY:
            raise SetError("busy", "the FM-1 is busy: " + bytes(b[2:]).decode("latin-1"))
        if c == NAK:
            why = WHY.get(b[1], "refused (%d)" % b[1])
            raise SetError("refused", (what + ": " if what else "") + why)
        return b

    def hello(self, tries=None):
        c, b = self.request(HELLO, tries=tries)
        if c != HELLO or len(b) < 32 or b[0] != 1:
            raise SetError("notfound", "not a DUB-1 1.5 set protocol")
        return dict(proto=b[0], chunk_in=u16(b, 1), chunk_out=u16(b, 3), format=u16(b, 5), max=u16(b, 9),
                    sets=b[11], playing=bool(b[12] & 1), asking=bool(b[12] & 2), restoring=bool(b[12] & 8),
                    version=text(b[16:48]))    # (the version 16 bytes before Decisions 4's, 32 since)

    def list(self):
        """[(kind, index, length, pieces, flags, name)]: WORK (live) first, then the stored sets"""
        b = self.ok(LIST)
        return [(b[1 + 22 * i], b[2 + 22 * i], u16(b, 3 + 22 * i), b[5 + 22 * i], b[6 + 22 * i],
                 text(b[7 + 22 * i:23 + 22 * i])) for i in range(b[0])]

    def read(self, kind, index, progress=lambda done, total: None, tries=3):
        """WORK as it is now (kind 1) or a stored set (2, index 0..7): its bytes (checked: a container). WORK is made
        on the fly at each chunk: while the music plays a knob turned or the next piece of a set meanwhile changes it
        between chunks, and the whole fails its CRC: read again (tries)"""
        for t in range(tries):
            out, total = bytearray(), 1
            while len(out) < total:
                off = len(out)
                b = self.ok(READ, bytes([kind, index, off & 255, off >> 8, CHUNK_OUT & 255, CHUNK_OUT >> 8]))
                if u16(b, 0) != off:
                    raise SetError("lost", "an answer out of step")
                total = u16(b, 2)
                out += b[4:]
                progress(len(out), total)
            try:
                D.parse(bytes(out))
                return bytes(out)
            except D.Refused as e:               # (garbled: never seen on the wire test; or WORK changed meanwhile)
                if t == tries - 1:
                    raise SetError("lost", "what came back doesn't read (%s)%s" % (
                        e, ": the pieces kept changing while the FM-1 played" if kind == WORK else ""))

    def wait_answer(self, on_ask=lambda: None, op=0):
        """the FM-1 asking: on_ask() once, then STATUS polled till it is answered: the state (S_*). (beta2) op: the
        question's; another asked already (LOAD IT NOW?, after a store's yes into the set loaded): yes. self.live
        after it: the set stored is the set loaded (STATUS's flags: the FM-1 keeps playing the old one)"""
        on_ask()
        end = time.monotonic() + DELAY["ask"]
        self.live = False
        while time.monotonic() < end:
            b = self.ok(STATUS)
            self.live = len(b) > 21 and bool(b[21] & 3)
            if op and b[0] != op:
                return S_YES
            if b[1] != S_ASKING:
                return b[1]
            time.sleep(DELAY["poll"])
        return S_TIMEOUT

    def send_set(self, index, data, on_ask=lambda: None, progress=lambda done, total: None, kind=SET):
        """data (a SET container) to set index (0..7): BEGIN, DATA, COMMIT, the FM-1's answer: the state (S_*).
        (kind WORK, index 0: a restore's pieces.) In a restore's yes the FM-1 keeps it at once: COMMIT answers S_YES
        (or S_FAILED), not asked"""
        crc = zlib.crc32(data) & 0xFFFFFFFF
        n = len(data)
        self.ok(BEGIN, bytes([kind, index, n & 255, n >> 8]) + crc.to_bytes(4, "little"), "BEGIN")
        for off in range(0, n, CHUNK_IN):
            b = self.ok(DATA, bytes([off & 255, off >> 8]) + data[off:off + CHUNK_IN], "DATA at %d" % off)
            if u16(b, 4) != min(off + CHUNK_IN, n):
                raise SetError("lost", "the FM-1 has %d bytes, not %d" % (u16(b, 4), min(off + CHUNK_IN, n)))
            progress(min(off + CHUNK_IN, n), n)
        c, b = self.request(COMMIT)
        if c == BUSY and b[1] == 2:              # (COMMIT's answer lost, sent again: asking already)
            s = self.ok(STATUS)
            if s[0] != COMMIT or s[3] != index:
                raise SetError("busy", "the FM-1 is asking something else")
        elif c == NAK and b[1] == 10:            # (a restore's, kept at once, its answer lost: nothing open now)
            s = self.ok(STATUS)
            if s[0] == COMMIT and s[2] == kind and s[3] == index and s[1] != S_ASKING:
                return s[1]
            self._refused(c, b, "COMMIT")
        elif c != COMMIT:
            self._refused(c, b, "COMMIT")
        elif b[1] != S_ASKING:
            return b[1]                          # (kept in a restore's yes, or failed)
        return self.wait_answer(on_ask, COMMIT)

    def restore(self, data, on_ask=lambda: None, on_item=lambda i, n, o: None, progress=lambda done, total: None):
        """(Decisions 4) a whole backup back: every set it holds into its own number, then its pieces, after one
        question (RESTORE). (the question's state, [(kind, index, name, state)] kept). (Decisions 5) It stops at the
        first not kept (the FM-1's flash failed, or anything else; the FM-1 ends the restore itself on a flash failure,
        and is told CANCEL anyway): SetError("stopped") with .done (kept before it), .failed ((kind, index, name,
        why)) and .left (the ones never sent: (kind, index, name))"""
        hi = self.hello()
        sets, work, mask = restore_plan(data, hi["format"])
        c, b = self.request(RESTORE, bytes([mask, 1 if work else 0]))
        if c != RESTORE:
            self._refused(c, b, "RESTORE")
        st = self.wait_answer(on_ask)
        done = []
        if st != S_YES:
            return st, done
        items = sets + ([work] if work else [])
        total, before = sum(len(o["bytes"]) for o in items), 0
        for i, o in enumerate(items):
            on_item(i, len(items), o)
            err, s = None, None
            try:
                s = self.send_set(o["index"], o["bytes"], kind=o["kind"],
                                  progress=lambda d, t, b0=before: progress(b0 + d, total))
            except SetError as e:
                err = e
            if err or s != S_YES:                # (stopped here: the rest never sent)
                if not (err and err.code == "lost"):
                    try:
                        self.request(CANCEL)
                    except SetError:
                        pass                     # (gone: the FM-1 ends it after 10 s)
                why = str(err) if err else STATE.get(s, "state %d" % s)
                e = SetError("stopped", "the restore stopped at %s: %s" % (what(o["kind"], o["index"]), why))
                e.done = done
                e.failed = (o["kind"], o["index"], o["name"], why)
                e.left = [(x["kind"], x["index"], x["name"]) for x in items[i + 1:]]
                raise e
            done.append((o["kind"], o["index"], o["name"], s))
            before += len(o["bytes"])
        return st, done

    def ask_op(self, cmd, index, on_ask=lambda: None):
        """APPLY or DELETE of set index: asked on the FM-1, its answer: the state (S_*), and the set's name"""
        c, b = self.request(cmd, bytes([SET, index]))
        if c == BUSY and b[1] == 2:
            b = self.ok(STATUS)
            if b[0] != cmd or b[3] != index:
                raise SetError("busy", "the FM-1 is asking something else")
        elif c != cmd:
            self._refused(c, b, "")
        return self.wait_answer(on_ask), text(b[5:21])

    def _refused(self, c, b, what):
        if c == BUSY:
            raise SetError("busy", "the FM-1 is busy: " + bytes(b[2:]).decode("latin-1"))
        raise SetError("refused", (what + ": " if what else "") + WHY.get(b[1], "refused (%d)" % b[1]))


# ------------------------------------------------------------ files ---

def as_set(data, name=None):
    """a .dub1 file as the set to send: a SET as it is, a WORK (a backup) made a set (its kind; nothing else changes),
    its name changed if asked; every name checked as the FM-1 checks it (names.h: never cut on its screen)"""
    try:
        c = D.parse(data)
    except D.Refused as e:
        raise SetError("file", "not a .dub1 file DUB-1 reads (%s)" % e)
    h = c["header"]
    if h["kind"] == D.BACKUP:
        raise SetError("file", "a whole backup: take one of its sets out first (dub1_container.py unbundle)")
    if h["kind"] not in (WORK, SET):
        raise SetError("file", "not a set (kind %d)" % h["kind"])
    changed = h["kind"] != SET
    h["kind"] = SET
    for ch in c["chunks"]:
        if ch["id"] == b"META" and name is not None:
            try:
                ch["name"], changed = D.name_field(name), True
            except D.Refused as e:
                raise SetError("file", str(e))
        if ch["id"] in (b"META", b"PIEC") and D.name_fits(ch["name"]) is None:
            raise SetError("file", "the name %r: no wider than the FM-1's header shows (%d px), up to %d characters" % (
                text(ch["name"]), D.NAME_PX, D.NAME_CHARS))
    if name is not None and not any(ch["id"] == b"META" for ch in c["chunks"]):
        raise SetError("file", "this file has no META to name")
    return D.build(c) if changed else bytes(data)


def restore_plan(data, fmt=0):
    """(Decisions 4) what a whole backup puts back: (sets by number, the pieces or None, the sets' mask: set n+1 bit
    n); each a dict(kind, index, bytes, name, pieces, format), read and its names checked as a send's (as_set), none
    made for a newer DUB-1 than fmt (the FM-1's; 0 unknown)"""
    try:
        objs = D.unbundle(data)
    except D.Refused as e:
        raise SetError("file", "not a whole backup DUB-1 reads (%s): restore takes the file backup --all made" % e)
    sets, work, mask = [], None, 0
    for kind, index, b in objs:
        one, pieces = kind == SET and index < NSETS and not mask >> index & 1, kind == WORK and work is None
        if not one and not pieces:
            continue
        as_set(b)                                # (its names: as a send checks them)
        c = D.parse(b)
        meta = next((ch for ch in c["chunks"] if ch["id"] == b"META"), None)
        it = dict(kind=kind, index=index if one else 0, bytes=bytes(b), name=text(meta["name"]) if meta else "",
                  pieces=c["header"]["npieces"], format=c["header"]["format"])
        if fmt and it["format"] > fmt:
            raise SetError("refused", "%s: %s (its format %d, this DUB-1's %d)" % (
                "set %d" % (index + 1) if one else "the pieces", WHY[5], it["format"], fmt))
        if one:
            sets.append(it)
            mask |= 1 << index
        else:
            work = it
    if not sets and work is None:
        raise SetError("file", "this backup holds nothing to restore")
    return sorted(sets, key=lambda o: o["index"]), work, mask


# ------------------------------------------------------------ make a set (web/dub1makeset.js's twin) ---

MODES, TRANS = ("endless", "song", "set"), ("lp", "segue", "mix")


def _tables():
    """(format, P_SMODE) from the page's cue tables (web/dub1cuetables.js: the firmware's), or (0, 0)"""
    import re
    try:
        js = open(os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "web", "dub1cuetables.js"),
                  encoding="utf-8").read()
        return int(re.search(r"^  format: (\d+)", js, re.M).group(1)), int(re.search(r"^  song: \[(\d+)", js, re.M).group(1))
    except (OSError, AttributeError):
        return 0, 0


def read_source(data):
    """a set or the pieces (WORK) as a source: dict(format, flags, meta, glob, pieces: [dict(chunk, cues, slot)])"""
    try:
        c = D.parse(data)
    except D.Refused as e:
        raise SetError("file", "not a .dub1 file DUB-1 reads (%s)" % e)
    h = c["header"]
    if h["kind"] not in (WORK, SET):
        raise SetError("file", "not a set or the pieces (kind %d)" % h["kind"])
    ch = c["chunks"]
    pieces = [dict(chunk=pc, slot=pc["index"], cues=next((q for q in ch if q["id"] == b"CUES" and q["index"] == pc["index"]),
                                                          None))
              for pc in sorted((x for x in ch if x["id"] == b"PIEC"), key=lambda x: x["index"])]
    return dict(format=h["format"], flags=h["flags"], meta=next((x for x in ch if x["id"] == b"META"), None),
                glob=next((x for x in ch if x["id"] == b"GLOB"), None), pieces=pieces)


def make_set(pieces, name="", mode=None, trans=None, base=None):
    """the new set's bytes: pieces [(source, slot, name or None: its own)] in order; mode 0..2, trans 0..2 (None: the
    base's); base the source its own settings come from (default: the first with a GLOB). As makeSet in
    web/dub1makeset.js, byte for byte"""
    n = len(pieces)
    if not 1 <= n <= NSETS:
        raise SetError("file", "a set has 1 to 8 pieces (%d chosen)" % n)
    first = pieces[0][0]
    fmt = first["format"]
    for src, _, _ in pieces:
        if src["format"] != fmt:
            raise SetError("file", "pieces made by two DUB-1 versions (formats %d and %d): a set takes pieces of one" % (
                fmt, src["format"]))

    def field(text, what):
        if not text:
            return bytes(16)
        try:
            return D.name_field(text)
        except D.Refused as e:
            raise SetError("file", "%s: %s" % (what, e))
    b = base or next((s for s, _, _ in pieces if s["glob"]), first)
    glob = b["glob"] if b["glob"] and b["format"] == fmt else next((s["glob"] for s, _, _ in pieces if s["glob"]), None)
    meta = b["meta"] or first["meta"]
    meta = dict(meta, name=field(name, "the set's name"), source=1) if meta else dict(
        id=b"META", name=field(name, "the set's name"), fw=bytes(16), source=1, migrated=0, saves=0)
    chunks = [meta]
    if glob:
        g = dict(glob, par=list(glob["par"]), sir=list(glob["sir"]), setlen=n if n < NSETS else 0)
        if trans is not None:
            g["trans"] = trans
        if mode is not None:
            tf, smode = _tables()
            if fmt != tf or len(g["par"]) <= smode:
                raise SetError("file", "Mode can't be changed in a set of format %d" % fmt)
            g["par"][smode] = mode
        chunks.append(g)
    cues = []
    for k, (src, slot, nm) in enumerate(pieces):
        s = next((x for x in src["pieces"] if x["slot"] == slot), None)
        if s is None:
            raise SetError("file", "no piece %d in that file" % (slot + 1))
        if nm is None:
            nm = text(s["chunk"]["name"])
        chunks.append(dict(s["chunk"], index=k, name=field(nm, "piece %d's name" % (k + 1))))
        if s["cues"]:
            cues.append(dict(s["cues"], index=k))
    data = D.build(dict(header=dict(format=fmt, kind=SET, npieces=n, flags=first["flags"] & 1, rsv=[0, 0]),
                        chunks=chunks + cues))
    if len(data) > 16128:
        raise SetError("file", "too big for the FM-1 (%d bytes; 16,128 at most)" % len(data))
    return data


def make_spec(spec):
    """FILE[@pieces|@N][:i,j,k] -> (source, [slots] or None)"""
    import re
    m = re.match(r"^(.*?)(?:@(pieces|[1-8]))?(?::([1-8](?:,[1-8])*))?$", spec)
    path, obj, which = m.group(1), m.group(2), m.group(3)
    try:
        raw = open(path, "rb").read()
    except OSError as e:
        raise SetError("file", "cannot read %s: %s" % (path, e.strerror))
    try:
        kind = D.parse(raw)["header"]["kind"]
    except D.Refused as e:
        raise SetError("file", "%s: not a .dub1 file DUB-1 reads (%s)" % (path, e))
    if kind == D.BACKUP:
        if not obj:
            raise SetError("file", "%s is a whole backup: say which, FILE@pieces or FILE@N (its set N)" % path)
        objs = D.unbundle(raw)
        o = next((b for k, ix, b in objs if (k == WORK if obj == "pieces" else k == SET and ix == int(obj) - 1)), None)
        if o is None:
            raise SetError("file", "%s holds no %s" % (path, "pieces" if obj == "pieces" else "set " + obj))
        raw = o
    elif obj:
        raise SetError("file", "%s is not a whole backup (@%s)" % (path, obj))
    return read_source(raw), [int(x) - 1 for x in which.split(",")] if which else None


def make(a, out):
    pieces = []
    for spec in a.sources:
        src, which = make_spec(spec)
        for slot in which if which is not None else [p["slot"] for p in src["pieces"]]:
            pieces.append([src, slot, None])
    if a.piece_names is not None:
        names = a.piece_names.split("|")
        if len(names) > len(pieces):
            raise SetError("usage", "%d names for %d pieces" % (len(names), len(pieces)))
        for p, nm in zip(pieces, names):
            p[2] = nm or None
    data = make_set([tuple(p) for p in pieces], a.name or "", None if a.mode is None else MODES.index(a.mode),
                    None if a.transition is None else TRANS.index(a.transition))
    with open(a.out, "wb") as f:
        f.write(data)
    print("saved %s: %d piece%s, %d bytes" % (a.out, len(pieces), "" if len(pieces) == 1 else "s", len(data)), file=out)
    return 0


# ------------------------------------------------------------ CLI ---

def slot(s):
    v = int(s)
    if not 1 <= v <= 8:
        raise argparse.ArgumentTypeError("a set is 1 to 8")
    return v - 1


def run(a, dev, out):
    def say_ask():
        print("Answer on the FM-1: GLO yes, any other button no (30 s)", file=out, flush=True)

    def bar(done, total):
        print("\r  %d of %d bytes" % (done, total), end="" if done < total else "\n", file=out, flush=True)

    hi = dev.hello()
    if a.cmd == "list":
        print("%s, project format %d%s" % (hi["version"], hi["format"], ", PLAYING (stop it to change sets)"
                                           if hi["playing"] else ""), file=out)
        for kind, ix, n, np, fl, nm in dev.list():
            if kind == WORK:
                print("  the pieces now: %s%d bytes" % ((nm + ", ") if nm else "", n), file=out)
            else:
                print("  set %d: %s, %d piece%s, %d bytes" % (ix + 1, nm or "(no name)", np, "" if np == 1 else "s", n),
                      file=out)
        return 0
    if a.cmd == "backup" and a.all:
        objs = [(k, ix, dev.read(k, ix, bar)) for k, ix, n, np, fl, nm in dev.list()]
        data = D.bundle(objs, hi["version"])
        path = a.file or time.strftime("dub1-backup-%Y%m%d-%H%M.dub1")
        with open(path, "wb") as f:
            f.write(data)
        print("saved %s (%d bytes: the pieces and %d set%s)" % (path, len(data), len(objs) - 1,
                                                             "" if len(objs) == 2 else "s"), file=out)
        return 0
    if a.cmd in ("backup", "read"):
        data = dev.read(WORK, 0, bar) if a.cmd == "backup" else dev.read(SET, a.set, bar)
        path = a.file or time.strftime("dub1-backup-%Y%m%d-%H%M.dub1")
        with open(path, "wb") as f:
            f.write(data)
        print("saved %s (%d bytes)" % (path, len(data)), file=out)
        return 0
    if a.cmd == "restore":
        try:
            raw = open(a.file, "rb").read()
        except OSError as e:
            raise SetError("file", "cannot read %s: %s" % (a.file, e.strerror))
        sets, work, mask = restore_plan(raw, hi["format"])
        print("restoring %s: %s%s" % (a.file, ", ".join("set %d %s" % (o["index"] + 1, o["name"] or "(no name)")
                                                         for o in sets),
                                      (", " if sets else "") + "the pieces" if work else ""), file=out)
        others = [ix + 1 for k, ix, n, np, fl, nm in dev.list() if k == SET and not mask >> ix & 1]
        if others:
            print("  sets %s on the FM-1 are not in it: they stay" % " ".join(map(str, others)), file=out)

        def item(i, n, o):
            print("  %d of %d: %s" % (i + 1, n, "set %d" % (o["index"] + 1) if o["kind"] == SET else "the pieces"),
                  file=out, flush=True)
        try:
            st, done = dev.restore(raw, say_ask, item, bar)
        except SetError as e:
            if e.code == "stopped":              # (Decisions 5: which were restored, which never sent)
                print("  restored before it stopped: %s" % (", ".join(what(k, ix) for k, ix, nm, s in e.done) or
                                                             "nothing"), file=out)
                if e.left:
                    print("  not sent: %s" % ", ".join(what(k, ix) for k, ix, nm in e.left), file=out)
            raise
        if st != S_YES:
            raise SetError("no", STATE.get(st, "state %d" % st))
        print("restored: %d set%s%s" % (len(sets), "" if len(sets) == 1 else "s", " and the pieces" if work else ""),
              file=out)
        return 0
    if a.cmd == "send":
        try:
            raw = open(a.file, "rb").read()
        except OSError as e:
            raise SetError("file", "cannot read %s: %s" % (a.file, e.strerror))
        data = as_set(raw, a.name)
        print("sending %s to set %d (%d bytes)" % (a.file, a.set + 1, len(data)), file=out)
        st = dev.send_set(a.set, data, say_ask, bar)
    else:
        st, nm = dev.ask_op(APPLY if a.cmd == "load" else DELETE, a.set, say_ask)
    if st != S_YES:
        raise SetError("no", STATE.get(st, "state %d" % st))
    print({"send": "kept as set %d", "load": "set %d loaded into the pieces", "delete": "set %d deleted"}[a.cmd] % (
        a.set + 1), file=out)
    if a.cmd == "send" and getattr(dev, "live", False):
        print("(the set loaded: the FM-1 keeps playing the old one until it's loaded again; it asks: LOAD IT NOW?)",
              file=out)
    return 0


def main(argv=None, out=sys.stdout, link=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", metavar="NAME", help="MIDI port to use (part of its name)")
    ap.add_argument("--host", metavar="AMB_HOST", help="the host simulator instead of an FM-1 (tests)")
    sub = ap.add_subparsers(dest="cmd", required=True)
    sub.add_parser("list")
    p = sub.add_parser("backup")
    p.add_argument("file", nargs="?")
    p.add_argument("--all", action="store_true", help="every stored set too, in one file")
    p = sub.add_parser("read")
    p.add_argument("set", type=slot)
    p.add_argument("file")
    p = sub.add_parser("send")
    p.add_argument("set", type=slot)
    p.add_argument("file")
    p.add_argument("--name", help="the set's name (as the FM-1 shows it)")
    for c in ("load", "delete"):
        sub.add_parser(c).add_argument("set", type=slot)
    sub.add_parser("restore").add_argument("file")
    p = sub.add_parser("make")
    p.add_argument("out")
    p.add_argument("--from", dest="sources", action="append", required=True, metavar="FILE[@pieces|@N][:i,j,k]")
    p.add_argument("--name", help="the set's name")
    p.add_argument("--piece-names", help='the pieces\' names in order, "|" between them')
    p.add_argument("--mode", choices=MODES)
    p.add_argument("--transition", choices=TRANS)
    a = ap.parse_args(argv)
    own = None
    try:
        if a.cmd == "make":                      # (files only: no FM-1)
            return make(a, out)
        if link is not None:
            dev = Device(link)
        elif a.host:
            own = HostLink(a.host)
            dev = Device(own)
        else:
            dev = open_device(a.port)
            own = dev.link
        return run(a, dev, out)
    except SetError as e:
        print("error: %s" % e, file=sys.stderr)
        return EXIT.get(e.code, 1)
    except KeyboardInterrupt:
        print("\ninterrupted", file=sys.stderr)
        return 130
    finally:
        if own is not None:
            own.close()


if __name__ == "__main__":
    sys.exit(main())
