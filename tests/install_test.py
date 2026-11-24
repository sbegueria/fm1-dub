#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""tools/fm1_install.py against a simulated FM-1 (no hardware, no mido).
The fake device is the one in web/test_web.mjs: identity on the handshake,
then "device asks, host answers" reads of the logical image. Run from the repo root:
  python3 tests/install_test.py
Also checks logical_image/product_of against web/fm1pkg.js (needs node and
build/amb.fwsc, skipped otherwise)."""
import io
import os
import queue
import re
import shutil
import subprocess
import sys
import tempfile
import threading
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import fm1_install as I  # noqa: E402

I.DELAY.update(open=0, start=0.01, reply=0, loader=0.01, reboot=0.01, retry=0.02, poll=0.05, hs=0.1,
               idle_check=0.4, idle_write=0.4, wait_loader=2, wait_reboot=2,
               resend=0.15, quiet=0.4, reconnect=1.0, gap=0.1)
LOGS = Path(tempfile.mkdtemp(prefix="felucca-install-logs-"))
os.environ["FM1_INSTALL_LOGS"] = str(LOGS)       # the session logs go here, not to ~/fm1-install-logs
failed = 0


def ok(cond, what):
    global failed
    print(f"{what:<64} {'ok' if cond else 'FAIL'}")
    failed += not cond


# ---------------------------------------------------------- simulated FM-1 ---

class FakeLink:
    def __init__(self, dev, gen):
        self.dev, self.gen, self.q, self.closed = dev, gen, queue.Queue(), False

    @property
    def lost(self):
        return self.gen != self.dev.gen or not self.dev.connected

    def send(self, pkt):
        if self.lost:
            return False
        self.dev.sent.append(bytes(pkt))
        self.dev.rx(bytes(pkt))
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
        self.closed = True


class FakeFM1:
    """a MIDI backend with one FM-1 on it"""

    """the write's stalls, as the loader (firmware/loader) would show them; each once, at that many answers:
      drop_at      an answer lost on the way (the device keeps waiting for it: the host's resend gets through)
      quiet_at     the loader gives its session up (as after its 3 x 5 s) and waits for a handshake and the
                   upgrade command in its main loop
      replug_at    drops off USB and comes back as the loader (a new port) replug_delay s later
      unplug_after gone for good; stall_after: the loader stops asking for good, still on USB"""

    def __init__(self, image, identity="FM-1_015", name="FM-1", unplug_after=None, after_write="FM-1_900",
                 stall_after=None, bad_addr=None, drop_at=None, quiet_at=None, replug_at=None, replug_delay=0.3):
        self.image, self.unplug_after, self.after_write = image, unplug_after, after_write
        self.stall_after, self.bad_addr = stall_after, bad_addr
        self.drop_at, self.quiet_at, self.replug_at, self.replug_delay = drop_at, quiet_at, replug_at, replug_delay
        self.dropped = 0
        self.served = self.bad = self.upgrades = 0
        self.sent, self.links, self.gen, self.lock = [], [], 0, threading.Lock()
        self.boot(identity, name)

    def boot(self, identity, name):
        with self.lock:
            self.gen += 1
            self.identity, self.name, self.connected = identity, name, True
            self.waiting, self.queue = None, []

    def input_names(self):
        return [self.name] if self.connected else []

    output_names = input_names

    def open(self, in_name, out_name):
        if in_name != self.name or not self.connected:
            raise IOError("no such port")
        link = FakeLink(self, self.gen)
        self.links.append(link)
        return link

    def tx(self, pkt):
        for link in self.links:
            if link.gen == self.gen and not link.closed:
                link.q.put(bytes(pkt))

    def rx(self, d):
        if d == I.HS_QUERY:
            t = self.identity.encode()
            self.tx(b"\xF0" + I.pack7(bytes([0, 0x59, 0x11, 0, 0, 0]) + t + bytes(28 - len(t))) + b"\xF7")
        elif d == I.UPGRADE:
            self.upgrades += 1
            first = self.bad_addr if self.bad_addr is not None else 0
            self.queue = ([(first + k * 512, 512) for k in range(6)] + [(I.FINISH_WRITE, 8)]
                          if self.identity.startswith("ota-")
                          else [(0, 64), (0x40, 160), (0x1000, 512), (I.FINISH_CHECK, 8)])
            self.next()
        elif self.waiting:
            u = I.unpack7(d[1:-1])
            addr, n = self.waiting
            if self.drop_at is not None and self.served == self.drop_at:
                self.drop_at = None                 # this answer never arrived
                self.dropped += 1
                return
            fin = addr >= I.FINISH_CHECK
            want = b"success\0" if fin else self.image[addr:addr + n]
            if u[14:14 + len(want)] != want or len(u) != 15 + len(want):
                self.bad += 1
            self.waiting = None
            self.served += 1
            if self.unplug_after and self.served >= self.unplug_after:
                self.connected = False
                return
            if self.quiet_at is not None and self.served >= self.quiet_at:
                self.quiet_at = None                # session given up: the main loop waits for an upgrade command
                self.queue = []
                return
            if self.replug_at is not None and self.served >= self.replug_at:
                self.replug_at = None
                self.connected = False
                threading.Timer(self.replug_delay, self.boot, (self.identity, "Felucca Update")).start()
                return
            if self.stall_after and self.served >= self.stall_after:
                return
            if addr == I.FINISH_CHECK:
                threading.Timer(0.05, self.boot, ("ota-FM-1_900", "Felucca Update")).start()
            elif addr == I.FINISH_WRITE:
                threading.Timer(0.05, self.boot, (self.after_write, "Felucca")).start()
            else:
                self.next()

    def next(self):
        if not self.queue:
            return
        self.waiting = addr, n = self.queue.pop(0)
        u = bytearray([0, 0x59, 0x30, 0, 0, 0, 0]) + addr.to_bytes(4, "little") + bytes([n & 0xFF, n >> 8, 0])
        u.append(~sum(u[6:14]) & 0xFF)
        self.tx(b"\xF0" + I.pack7(u) + b"\xF7")


# ----------------------------------------------------------------- helpers ---

def package(product="FM-1_900", marker=True, size=0x2000):
    raw = bytearray((i * 7) & 0xFF for i in range(size + I.BLOCKS))
    for i in range(I.BLOCKS):
        raw[i * I.BLK + I.KEEP] = (ord(product[i]) + i + 1) & 0xFF if i < len(product) else 0x7D
    raw[0x1800:0x1800 + 16] = I.LOADER_MARK if marker else bytes(16)
    return bytes(raw)


TMP = Path(tempfile.mkdtemp(prefix="felucca-install-"))


def pkgfile(name, raw):
    p = TMP / name
    p.write_bytes(raw)
    return str(p)


def logfile(text):
    """the log the installer named in its output, read"""
    m = re.search(r"the session's log: (\S+)", text)
    return Path(m[1]).read_text() if m and Path(m[1]).exists() else None


def cli(args, dev, answer=True):
    out, err = io.StringIO(), io.StringIO()
    old, sys.stderr = sys.stderr, err
    try:
        rc = I.main(args, backend=dev, out=out, ask=lambda _p: answer)
    finally:
        sys.stderr = old
    return rc, out.getvalue(), err.getvalue()


# ------------------------------------------------------------------- tests ---

def wire():
    data = bytes(range(256)) * 3
    ok(I.unpack7(I.pack7(data))[:len(data)] == data, "pack7 / unpack7 round trip")
    pkt = I.response(0x12345, b"\x01\x02\x03", fl=5)
    u = I.unpack7(pkt[1:-1])
    ok(u[:3] == b"\x00\x59\x30" and int.from_bytes(u[7:11], "little") == 0x12345 and u[14:17] == b"\x01\x02\x03"
       and ~sum(u[6:17]) & 0xFF == u[17], "response: header, address, data, checksum")
    raw = package("FM-1_906")
    ok(I.product_of(raw) == "FM-1_906" and len(I.logical_image(raw)) == len(raw) - 20, "product_of / logical_image (synthetic)")
    raw = package("FM-1_9001000")                  # X0X from 0.10: major, minor, patch, two digits each
    ok(I.product_of(raw) == "FM-1_9001000", "product_of: a 12-character identity")


def installs():
    raw = package()
    image = I.logical_image(raw)
    p = pkgfile("ok.fwsc", raw)

    dev = FakeFM1(image)
    rc, out, err = cli([p, "--yes"], dev)
    ok(rc == 0 and dev.bad == 0 and dev.upgrades == 2 and dev.served == 11 and "done: the FM-1 runs FM-1_900" in out
       and "100%" in out, f"install: running -> loader -> Felucca ({dev.served} reads)")

    raw10 = package("FM-1_9001000")
    dev = FakeFM1(I.logical_image(raw10), identity="FM-1_909", after_write="FM-1_9001000")
    rc, out, err = cli([pkgfile("x10.fwsc", raw10), "--yes"], dev)
    ok(rc == 0 and dev.bad == 0 and "done: the FM-1 runs FM-1_9001000" in out,
       "install: 0.9 (FM-1_909) -> 0.10 (FM-1_9001000), the longer identity verified after")

    dev = FakeFM1(image, identity="ota-FM-1_900", name="Felucca Update")
    rc, out, err = cli([p, "--yes"], dev)
    ok(rc == 0 and dev.bad == 0 and dev.upgrades == 1 and "update mode" in out, "install: device already in update mode -> finishes the write")

    dev = FakeFM1(image)
    rc, out, err = cli([p], dev, answer=False)
    ok(rc == 1 and dev.upgrades == 0 and "cancelled" in out, "install: answer no -> nothing sent but the handshake")

    dev = FakeFM1(image, identity="FM-1_905", name="Felucca")
    rc, out, err = cli(["--info"], dev)
    ok(rc == 0 and "FM-1_905" in out and "running" in out and dev.upgrades == 0, "--info: identity of the connected FM-1")

    dev = FakeFM1(image, identity="ota-FM-1_900", name="Felucca Update")
    rc, out, err = cli(["--info"], dev)
    ok(rc == 0 and "update loader" in out, "--info: device in update mode")


def errors():
    raw = package()
    image = I.logical_image(raw)
    p = pkgfile("ok.fwsc", raw)

    rc, out, err = cli([p, "--yes"], FakeFM1(image, name="IAC Driver Bus 1"))
    ok(rc == 3 and "not found" in err and "IAC Driver" in err, "no FM-1 port -> exit 3, lists the MIDI inputs")
    dev = FakeFM1(image)
    rc, out, err = cli([p, "--yes", "--port", "Felucca"], dev)
    ok(rc == 3 and dev.upgrades == 0, "--port that matches nothing -> exit 3")
    dev = FakeFM1(image, name="My Interface")
    rc, out, err = cli(["--info", "--port", "my int"], dev)
    ok(rc == 0 and "FM-1_015" in out, "--port picks a port the default match skips")

    t0 = time.monotonic()
    rc, out, err = cli([p, "--yes"], FakeFM1(image, unplug_after=2))
    ok(rc == 4 and "disconnected" in err and "nothing was written" in err and time.monotonic() - t0 < 3,
       "unplugged in step 1 -> exit 4 'lost', nothing written")
    t0 = time.monotonic()
    rc, out, err = cli([p, "--yes"], FakeFM1(image, identity="ota-FM-1_900", name="Felucca Update", unplug_after=3))
    took = time.monotonic() - t0
    ok(rc == 4 and "did not come back within" in err and "Run the install again" in err and 1.0 <= took < 3,
       f"unplugged during the write, never back -> waits {I.DELAY['reconnect']:.0f} s for it, exit 4 ({took:.1f} s)")
    ok("straight into the computer" in err and "download mode" in err and "M-Flasher" in err
       and "keeps the bootloader" in err and I.MANUAL_RESCUE in err and "stays in update mode" not in err,
       "... the honest message: no hub, replug, the download mode, the rescue or M-Flasher")
    log = logfile(err)
    ok(log is not None and "req #3 " in log and "the port is gone" in log and "the loader did not come back" in log
       and log.rstrip().endswith("end"), "... its log saved and named, with the last requests and how it ended")
    dev = FakeFM1(image, identity="ota-FM-1_900", name="Felucca Update", stall_after=3)
    rc, out, err = cli([p, "--yes"], dev)
    ok(rc == 4 and "stopped answering" in err and f"after {I.RESUMES} resumes" in err and dev.upgrades == 1 + I.RESUMES,
       f"loader stops asking for good -> {I.RESUMES} resumes, then exit 4")
    rc, out, err = cli([p, "--yes"], FakeFM1(image, identity="ota-FM-1_900", name="Felucca Update", bad_addr=len(image)))
    ok(rc == 4 and "outside the package" in err, "read past the package -> exit 4")

    rc, out, err = cli([p, "--yes"], FakeFM1(image, after_write="FM-1_015"))
    ok(rc == 6 and "reports FM-1_015" in err, "another identity after the restart -> exit 6")
    dev = FakeFM1(image, identity="XY-9_001", name="usb-midi")
    rc, out, err = cli([p, "--yes"], dev)
    ok(rc == 6 and dev.upgrades == 0, "another model -> exit 6, nothing sent")

    class Stuck(FakeFM1):          # the loader never shows up
        def boot(self, identity, name):
            super().boot(identity, name)
            if identity.startswith("ota-"):
                self.connected = False
    rc, out, err = cli([p, "--yes"], Stuck(image))
    ok(rc == 5 and "loader did not appear" in err, "no loader after step 1 -> exit 5")

    plain = pkgfile("plain.fwsc", package(marker=False))
    dev = FakeFM1(image)
    rc, out, err = cli([plain, "--yes"], dev)
    ok(rc == 2 and "no Felucca update loader" in err and dev.sent == [], "package without the loader marker -> exit 2, no MIDI")
    dev = FakeFM1(I.logical_image(package(marker=False)))
    rc, out, err = cli([plain, "--yes", "--force"], dev)
    ok(rc == 0 and dev.bad == 0, "... installs with --force")
    rc, out, err = cli([pkgfile("short.fwsc", b"\0" * 100), "--yes"], FakeFM1(image))
    ok(rc == 2 and "too short" in err, "not a package -> exit 2")
    rc, out, err = cli([str(TMP / "missing.fwsc"), "--yes"], FakeFM1(image))
    ok(rc == 2, "missing file -> exit 2")


def stalls():
    """the write's stalls, each in a whole install (running -> loader -> Felucca): the installer waits, resumes, and
    the log says what happened"""
    raw = package()
    image = I.logical_image(raw)
    p = pkgfile("ok.fwsc", raw)

    dev = FakeFM1(image)
    rc, out, err = cli([p, "--yes"], dev)
    log = logfile(out)
    reqs = re.findall(r"req #(\d+) +(0x[0-9a-f]{8}) \+(\d+) +([\d.]+) ms", log or "")
    ok(rc == 0 and log is not None and len(reqs) == dev.served and reqs[-1][1] == "0xf0000000"
       and "step 1 (check)" in log and "step 2 (write)" in log and "done: FM-1_900" in log,
       f"the log of a good install: every request ({len(reqs)}), its address and timing, the steps, the end")
    ok("straight into the computer, not through a USB hub" in out and "switched on" in out,
       "before the install: plug it straight in (no hub), keep it switched on")

    dev = FakeFM1(image, drop_at=6)               # the 2nd read of the write: its answer lost
    rc, out, err = cli([p, "--yes"], dev)
    log = logfile(out) or ""
    ok(rc == 0 and dev.dropped == 1 and dev.bad == 0 and dev.upgrades == 2 and "the last answer again" in log
       and "resume" not in log, "an answer lost -> sent again after the pause, the write goes on (no restart)")

    dev = FakeFM1(image, quiet_at=7)              # the loader gives its session up mid-write
    rc, out, err = cli([p, "--yes"], dev)
    log = logfile(out) or ""
    ok(rc == 0 and dev.bad == 0 and dev.upgrades == 3 and "handshake on the same port: ota-FM-1_900" in log
       and "resume 1" in log and "waiting for it" in out and "the write starts again" in out,
       "the loader goes quiet mid-write -> a handshake, the upgrade again, the write finishes")
    ok(re.search(r"GAP (0\.[4-9]|[1-9])", log) is not None, "... the log marks the gap")

    dev = FakeFM1(image, replug_at=8)             # off USB and back as the loader
    rc, out, err = cli([p, "--yes"], dev)
    log = logfile(out) or ""
    ok(rc == 0 and dev.bad == 0 and dev.upgrades == 3 and "the port is gone" in log
       and "MIDI inputs: (none)" in log and "found ota-FM-1_900 on Felucca Update" in log and "resume 1" in log
       and "done: the FM-1 runs FM-1_900" in out,
       "the loader drops off USB and comes back -> found again, the write finishes")

    dev = FakeFM1(image, replug_at=8, replug_delay=3.0)    # back too late
    rc, out, err = cli([p, "--yes"], dev)
    ok(rc == 4 and "did not come back within" in err and logfile(err) is not None,
       "... back only after the wait -> exit 4, the honest message, the log")

    plain = pkgfile("plain.fwsc", package(marker=False))
    dev = FakeFM1(I.logical_image(package(marker=False)), drop_at=6)
    rc, out, err = cli([plain, "--yes", "--force"], dev)
    log = logfile(out) or logfile(err) or ""
    ok(rc == 0 and "the last answer again" not in log and "another loader" in log and "resume 1" in log,
       "another loader (--force): no answer sent again (its pauses not known); after the long idle, a restart")


def against_js():
    clean = ROOT / "build/amb.fwsc"
    if not shutil.which("node") or not clean.exists():
        print("logical image vs fm1pkg.js: skipped (needs node and build/amb.fwsc)")
        return
    js = ("import { logicalImage, productOf } from %r; import { readFileSync } from 'node:fs';"
          "const p = readFileSync(process.argv[1]); process.stderr.write(productOf(p));"
          "process.stdout.write(logicalImage(p));") % str(ROOT / "web/fm1pkg.js")
    r = subprocess.run(["node", "--input-type=module", "-e", js, str(clean)], capture_output=True, check=True)
    raw = clean.read_bytes()
    ok(I.logical_image(raw) == r.stdout and I.product_of(raw) == r.stderr.decode(),
       f"logical_image / product_of == fm1pkg.js ({clean.name}, {I.product_of(raw)})")
    ok(I.LOADER_MARK in raw, f"{clean.name} carries the Felucca loader marker")


wire()
installs()
errors()
stalls()
against_js()
shutil.rmtree(TMP, ignore_errors=True)
shutil.rmtree(LOGS, ignore_errors=True)
print(f"INSTALL TESTS FAILED ({failed})" if failed else "install tests passed")
sys.exit(1 if failed else 0)
