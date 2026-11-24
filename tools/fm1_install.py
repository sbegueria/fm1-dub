#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""Install a Felucca package (.fwsc) on an FM-1 over USB-MIDI.

The same update as the web installer (web/fm1ota.js): step 1, the
running firmware reads parts of the package and starts the update loader;
step 2, the loader reads the whole package and writes it. In both steps the
device asks (SysEx read requests on the logical image) and we answer. Then
the FM-1 restarts and the installed identity is checked.

  fm1_install.py PACKAGE.fwsc [--port NAME] [--yes] [--force]
  fm1_install.py --info [--port NAME]

If the FM-1 is still in update mode (an earlier install was cut off), the
install finishes the write. Needs mido with python-rtmidi.

When the loader stops answering or drops off USB mid-write, the installer
waits for it and resumes (see "Stalls" below) before it gives up. Every
install writes a log of the session (each request, its timing, the gaps, the
end) to ~/fm1-install-logs/ (or --log-dir, or $FM1_INSTALL_LOGS).

Stalls. The protocol is device-driven: the loader asks (cmd 0x30 reads), we
answer. The loader (firmware/loader) asks a read again after 5 s without an
answer, 3 times; after that (15 s) its session ends and it waits, in its main
loop, for a new handshake and upgrade command. A new session starts again from
the package's header and reads the whole package again; the sectors already
equal are compared, not erased or written. So "resume" means: the same reads
again, only the flash work left is done. Its state is in RAM (the session) and
in the flash (the app area written so far, the staged loader at 0xE0000, the
update record at 0xE4F00). Here: no request for 6 s -> the last answer again
(once); no request for 17 s (the loader has given up its session) -> a
handshake: if the loader answers, the upgrade command again; if it doesn't, or
its port is gone, wait up to 30 s for a loader on USB-MIDI and start it again;
at most 3 times. With a package without Felucca's loader (--force: M-VAVE's
own loader, whose silent phases are not known) the 6 s answer is not repeated
and the handshake waits for the long idle (180 s).

Exit codes: 0 done, 1 cancelled or other error, 2 bad arguments or package,
3 FM-1 not found, 4 connection lost or the device stopped, 5 timeout (no
loader / no restart), 6 wrong model, or another identity after the install.
"""
import argparse
import datetime
import os
import platform
import queue
import re
import sys
import time

HS_QUERY = bytes([0xF0, 0x00, 0x32, 0x45, 0x00, 0x00, 0x00, 0x40, 0x7F, 0xF7])
UPGRADE = bytes([0xF0, 0x22, 0x24, 0x35, 0x7F, 0xF7])
FINISH_CHECK, FINISH_WRITE = 0xE0000000, 0xF0000000
MAXDATA = 512
BLOCKS, BLK, KEEP = 20, 0x30, 0x2F
LOADER_MARK = b"FELUCCA-LOADER-1"
PORT_RE = re.compile(r"fm-1|felucca|ota|composite|sinco|usb-midi", re.I)   # never probe other gear

# seconds; the tests shorten them. resend: no request for this long -> the last answer again (once);
# quiet: no request for this long with Felucca's loader (it gives a read up after 3 x 5 s) -> a handshake and
# the upgrade command again; idle_write: the same with another loader; reconnect: how long to wait for a
# loader to come back on USB; gap: a pause between two requests that the log marks
DELAY = {"open": 0.3, "start": 2.0, "reply": 0.01, "loader": 3.0, "reboot": 3.0, "retry": 1.0,
         "poll": 1.0, "hs": 1.0, "idle_check": 8.0, "idle_write": 180.0,
         "wait_loader": 30.0, "wait_reboot": 40.0,
         "resend": 6.0, "quiet": 17.0, "reconnect": 30.0, "gap": 1.0}
RESUMES = 3                     # restarts of the write after a stall, at most
MANUAL_RESCUE = "https://sbegueria.github.io/fm1-dub/manual/#recovering-an-fm-1-that-won-t-start"
BEFORE = ("Before you start: plug the FM-1 straight into the computer, not through a USB hub, and keep it\n"
          "switched on (charged, or on USB power) until the install has finished.")
WHAT_NOW = ("What now:\n"
            "  1. Unplug the USB cable and plug it straight into the computer (not through a hub). Leave the FM-1\n"
            "     switched on.\n"
            "  2. Run the install again: if the FM-1 is still in update mode, it finishes the write.\n"
            "  3. If the FM-1 is not found at all and its screen stays blank, it may have dropped into the chip's\n"
            "     own download mode, which no installer can reach. Recover it with the rescue\n"
            f"     ({MANUAL_RESCUE})\n"
            "     or with M-Flasher's FLASH FIRMWARE (it keeps the bootloader), then install again.")

EXIT = {"usage": 2, "badpkg": 2, "notfound": 3, "model": 6, "lost": 4, "stopped": 4, "badreq": 4,
        "noloader": 5, "noreturn": 5, "mismatch": 6}


class SessionLog:
    """the install's journal: each request (address, length, the time since the one before), the gaps, the
    stalls and what was done about them, the end. Kept in memory; save() writes it to a file."""

    def __init__(self):
        self.t0 = time.monotonic()
        self.lines, self.last_req, self.reqs = [], None, 0
        self.keep = False           # main() saves it once an install is under way (not for --info or a no)
        self.event(f"fm1_install {datetime.datetime.now().isoformat(timespec='seconds')}  "
                   f"{platform.platform()}  python {platform.python_version()}")

    def event(self, msg):
        self.lines.append(f"{time.monotonic() - self.t0:9.3f}  {msg}")

    def request(self, addr, n):
        now = time.monotonic()
        dt = now - self.last_req if self.last_req is not None else 0.0
        self.last_req = now
        self.reqs += 1
        gap = f"  GAP {dt:.1f} s" if dt >= DELAY["gap"] else ""
        self.event(f"req #{self.reqs:<5d} {addr:#010x} +{n:<4d} {dt * 1000:8.1f} ms{gap}")

    def text(self):
        return "\n".join(self.lines) + "\n"

    def save(self, folder=None):
        """-> the file's path, or None if it could not be written"""
        folder = folder or os.environ.get("FM1_INSTALL_LOGS") or os.path.join(os.path.expanduser("~"), "fm1-install-logs")
        try:
            os.makedirs(folder, exist_ok=True)
            stem = os.path.join(folder, datetime.datetime.now().strftime("%Y%m%d-%H%M%S"))
            path, k = stem + ".log", 1
            while os.path.exists(path):
                path, k = f"{stem}-{k}.log", k + 1
            with open(path, "w") as f:
                f.write(self.text())
            return path
        except OSError:
            return None


class InstallError(Exception):
    def __init__(self, code, msg):
        super().__init__(msg)
        self.code = code


# ------------------------------------------------------------ wire format ---

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
        while nb >= 8:
            out.append(acc & 0xFF)
            acc >>= 8
            nb -= 8
    return bytes(out)


def response(addr, data, fl=0):
    n = len(data)
    body = bytearray([0x00, 0x59, 0x30, (n + 8) & 0xFF, ((n + 8) >> 8) & 0xFF, 0, fl])
    body += addr.to_bytes(4, "little") + bytes([n & 0xFF, (n >> 8) & 0xFF, 0]) + bytes(data)
    body.append(~sum(body[6:]) & 0xFF)
    return b"\xF0" + pack7(body) + b"\xF7"


def parse_request(pkt):
    """-> (flags, addr, len) or None"""
    if len(pkt) < 2 or pkt[0] != 0xF0 or pkt[-1] != 0xF7:
        return None
    u = unpack7(pkt[1:-1])
    if len(u) != 15 or u[:3] != b"\x00\x59\x30" or ~sum(u[6:14]) & 0xFF != u[14]:
        return None
    return u[6], int.from_bytes(u[7:11], "little"), int.from_bytes(u[11:14], "little")


class Identity:
    def __init__(self, text, model, version):
        self.text, self.model, self.version = text, model, version

    @property
    def loader(self):
        return self.model.lower().startswith("ota-")


def parse_identity(pkt):
    if len(pkt) < 2 or pkt[0] != 0xF0 or pkt[-1] != 0xF7:
        return None
    d = unpack7(pkt[1:-1])
    if len(d) != 34 or d[:3] != b"\x00\x59\x11":
        return None
    txt = d[6:33].rstrip(b"\0").decode("latin-1")
    m = re.fullmatch(r"([^_]+)_(\d+)", txt)
    return Identity(txt, m[1], int(m[2])) if m else None


# --------------------------------------------------------------- package ---

def product_of(raw):
    """the package identity ("FM-1_906"): one marker byte after each of the first 20 blocks"""
    if len(raw) < BLOCKS * BLK:
        raise InstallError("badpkg", "not an FM-1 package (too short)")
    return "".join(chr((m - i - 1) & 0xFF) for i in range(BLOCKS) if (m := raw[i * BLK + KEEP]) != 0x7D)


def logical_image(raw):
    """the image the device reads during an update: the package without the 20 marker bytes"""
    out = bytearray()
    for i in range(BLOCKS):
        out += raw[i * BLK:i * BLK + KEEP]
    return bytes(out + raw[BLOCKS * BLK:])


def model_of(text):
    return text.split("_")[0].removeprefix("ota-")


# ------------------------------------------------------------------ MIDI ---

class MidoLink:
    """one MIDI in/out pair with a SysEx queue"""

    def __init__(self, backend, in_name, out_name):
        import mido
        self.backend, self.name, self.mido = backend, in_name, mido
        self.q = queue.Queue()
        self.inp = mido.open_input(in_name, callback=self._rx)
        try:
            self.out = mido.open_output(out_name)
        except Exception:
            self.inp.close()
            raise
        time.sleep(DELAY["open"])   # CoreMIDI may drop a message sent just after opening

    def _rx(self, msg):
        if msg.type == "sysex":
            self.q.put(bytes([0xF0, *msg.data, 0xF7]))

    @property
    def lost(self):
        return self.name not in self.backend.input_names()

    def send(self, pkt):
        try:
            self.out.send(self.mido.Message("sysex", data=pkt[1:-1]))
            return True
        except Exception:
            return False

    def read(self, timeout):
        try:
            return self.q.get(timeout=max(timeout, 0))
        except queue.Empty:
            return None

    def drain(self):
        while not self.q.empty():
            self.q.get_nowait()

    def close(self):
        for p in (self.inp, self.out):
            try:
                p.close()
            except Exception:
                pass


class MidoBackend:
    def __init__(self):
        import mido
        self.mido = mido

    def input_names(self):
        return list(dict.fromkeys(self.mido.get_input_names()))

    def output_names(self):
        return list(dict.fromkeys(self.mido.get_output_names()))

    def open(self, in_name, out_name):
        return MidoLink(self, in_name, out_name)


def pair_output(name, outs):
    if name in outs:
        return name
    base = re.sub(r"\s+\d+$", "", name)     # Windows numbers each port
    same = [o for o in outs if re.sub(r"\s+\d+$", "", o) == base]
    if len(same) == 1:
        return same[0]
    return outs[0] if len(outs) == 1 else None


# --------------------------------------------------------------- updater ---

class Device:
    def __init__(self, link, ident, name):
        self.link, self.id, self.name = link, ident, name


def handshake(link, tries=3):
    link.drain()
    for _ in range(tries):
        if not link.send(HS_QUERY):
            return None
        end = time.monotonic() + DELAY["hs"]
        while (left := end - time.monotonic()) > 0:
            p = link.read(left)
            if p is None:
                break
            ident = parse_identity(p)
            if ident:
                return ident
    return None


class Updater:
    def __init__(self, backend, port=None, log=None):
        self.backend = backend
        self.port = port
        self.log = log or SessionLog()

    def candidates(self, only_port):
        names = self.backend.input_names()
        if self.port:
            mine = [n for n in names if self.port.lower() in n.lower()]
            if only_port:
                return mine
            return mine + [n for n in names if n not in mine and PORT_RE.search(n)]
        return [n for n in names if PORT_RE.search(n)]

    def find(self, want=None, only_port=True):
        """the first matching port that answers the handshake (and want(identity))"""
        outs = self.backend.output_names()
        for name in self.candidates(only_port):
            out = pair_output(name, outs)
            if not out:
                continue
            try:
                link = self.backend.open(name, out)
            except Exception:
                continue
            ident = handshake(link, 2)
            if ident and (want is None or want(ident)):
                return Device(link, ident, name)
            link.close()
        return None

    def wait_for(self, want, secs, tick=None):
        end, seen = time.monotonic() + secs, None
        while time.monotonic() < end:
            names = self.backend.input_names()
            if names != seen:                            # what came and went on USB, for the log
                self.log.event(f"MIDI inputs: {', '.join(names) or '(none)'}")
                seen = names
            dev = self.find(want, only_port=False)
            if dev:
                self.log.event(f"found {dev.id.text} on {dev.name}")
                return dev
            if tick:
                tick()
            time.sleep(DELAY["retry"])
        return None

    def serve(self, link, image, finish, idle, progress, resend=None, tick=None):
        """answer read requests until the device asks for `finish`; -> (served, finished, lost, last), last the
        time of the last request. resend: after this long without a request, the last answer again (once);
        tick(seconds since the last request) while it is that quiet"""
        served, last, answer, resent = 0, time.monotonic(), None, False
        while True:
            pkt = link.read(DELAY["poll"])
            if pkt is None:
                quiet = time.monotonic() - last
                if link.lost:
                    self.log.event(f"the port is gone, {quiet:.1f} s after the last request")
                    return served, False, True, last
                if quiet > idle:
                    self.log.event(f"no request for {quiet:.1f} s")
                    return served, False, False, last
                if resend is not None and quiet >= resend:
                    if answer and not resent:
                        resent = True
                        self.log.event(f"no request for {quiet:.1f} s: the last answer again")
                        link.send(answer)
                    if tick:
                        tick(quiet)
                continue
            r = parse_request(pkt)
            if not r:
                continue
            fl, addr, n = r
            last, resent = time.monotonic(), False
            self.log.request(addr, n)
            if addr in (FINISH_CHECK, FINISH_WRITE):
                answer = response(addr, b"success\0")
                link.send(answer)
                if addr == finish:
                    self.log.event(f"finish {addr:#x} answered")
                    return served, True, False, last
                continue
            if n > MAXDATA or addr + n > len(image):
                self.log.event(f"bad request {addr:#x}+{n}")
                raise InstallError("badreq", f"the device asked for {addr:#x}+{n}, outside the package")
            time.sleep(DELAY["reply"])
            answer = response(addr, image[addr:addr + n], fl)
            if not link.send(answer):
                self.log.event("the answer could not be sent: the port is gone")
                return served, False, True, last
            served += 1
            progress(served, addr + n)

    def check(self, dev, image, step):
        """step 1: the running firmware checks the package and starts the loader"""
        step("start", dev.id.text)
        self.log.event(f"step 1 (check) on {dev.id.text}, {dev.name}")
        dev.link.send(UPGRADE)
        time.sleep(DELAY["start"])
        served, finished, lost, _ = self.serve(dev.link, image, FINISH_CHECK, DELAY["idle_check"],
                                               lambda k, end: step("check", k))
        dev.link.close()
        if not finished:
            raise InstallError("lost" if lost else "stopped",
                               f"the FM-1 {'was disconnected' if lost else 'stopped answering'} after "
                               f"{served} requests; nothing was written. Plug it straight into the computer "
                               "(not through a hub) and run the install again")
        step("loader")
        time.sleep(DELAY["loader"])
        ota = self.wait_for(lambda i: i.loader, DELAY["wait_loader"])
        if not ota:
            self.log.event("no loader")
            raise InstallError("noloader", f"the update loader did not appear within {DELAY['wait_loader']:.0f} s.\n"
                                           + WHAT_NOW)
        return ota

    def write(self, ota, image, step, felucca=True, product=None):
        """step 2: the loader reads and writes the whole package, then restarts. A stall (no request for a
        while, or the port gone): the last answer again, a handshake and the upgrade command again, or a wait
        for the loader to come back on USB, at most RESUMES times (the module's docstring, "Stalls")"""
        link, name, total, resumes, reached = ota.link, ota.name, 0, 0, 0
        quiet = DELAY["quiet"] if felucca else DELAY["idle_write"]
        resend = DELAY["resend"] if felucca else None
        self.log.event(f"step 2 (write) on {ota.id.text}, {name}; {'Felucca' if felucca else 'another'} loader; "
                       f"package {len(image)} B")

        def prog(k, end):
            nonlocal reached
            reached = max(reached, end)
            step("write", min(99, end * 100 // len(image)))

        step("write", 0)
        while True:
            link.send(UPGRADE)
            self.log.event("upgrade command sent")
            time.sleep(DELAY["start"])
            served, finished, lost, since = self.serve(link, image, FINISH_WRITE, quiet, prog, resend,
                                                       lambda q: step("stall", int(q)))
            total += served
            if finished:
                link.close()
                step("write", 100)
                return
            why = "was disconnected" if lost else "stopped answering"
            pct = reached * 100 // len(image)
            if resumes >= RESUMES:
                link.close()
                self.log.event(f"gave up after {resumes} resumes")
                raise InstallError("lost" if lost else "stopped",
                                   f"the loader {why} again at {pct}% ({total} requests in all), after {resumes} "
                                   "resumes.\n" + WHAT_NOW)
            back = None
            if not lost:                                 # still on USB: is the loader there?
                ident = handshake(link, 2)
                self.log.event(f"handshake on the same port: {ident.text if ident else 'no answer'}")
                if ident and ident.loader:
                    back = Device(link, ident, name)
            if back is None:                             # gone, or mute: wait for it on USB
                link.close()
                self.log.event(f"waiting up to {DELAY['reconnect']:.0f} s for the loader on USB")
                step("stall", int(time.monotonic() - since))
                back = self.wait_for(lambda i: i.loader or i.text == product, DELAY["reconnect"],
                                     lambda: step("stall", int(time.monotonic() - since)))
                if back and not back.id.loader:          # the write had finished after all
                    back.link.close()
                    self.log.event(f"the FM-1 is back running {back.id.text}: the write had finished")
                    step("write", 100)
                    return
            if back is None:
                self.log.event("the loader did not come back")
                raise InstallError("lost" if lost else "stopped",
                                   f"the loader {why} at {pct}% ({total} requests) and did not come back within "
                                   f"{DELAY['reconnect']:.0f} s.\n" + WHAT_NOW)
            resumes += 1
            self.log.event(f"resume {resumes}: {back.id.text} on {back.name}, the write starts again")
            step("resume", resumes)
            link, name = back.link, back.name

    def verify(self, product, step):
        step("reboot")
        time.sleep(DELAY["reboot"])
        back = self.wait_for(lambda i: not i.loader, DELAY["wait_reboot"])
        if not back:
            self.log.event("the FM-1 did not come back after the write")
            raise InstallError("noreturn", "written, but the FM-1 did not come back: switch it off and on. If its "
                                           f"screen then stays blank, see {MANUAL_RESCUE}")
        back.link.close()
        if back.id.text != product:
            raise InstallError("mismatch", f"written, but the FM-1 reports {back.id.text} (expected {product})")
        step("done", back.id.text)
        return back.id.text

    def install(self, dev, image, product, step=lambda *a: None):
        """running firmware -> loader -> new firmware; dev from find(). A device already in
        update mode only needs the write."""
        if model_of(dev.id.text) != model_of(product):
            dev.link.close()
            raise InstallError("model", f"the device is {dev.id.text}, the package is for {product}")
        ota = dev if dev.id.loader else self.check(dev, image, step)
        self.write(ota, image, step, LOADER_MARK in image, product)
        out = self.verify(product, step)
        self.log.event(f"done: {out}")
        return out


# ------------------------------------------------------------------- CLI ---

def load_package(path, force):
    try:
        raw = open(path, "rb").read()
    except OSError as e:
        raise InstallError("badpkg", f"cannot read {path}: {e.strerror}")
    product = product_of(raw)
    if not re.fullmatch(r"[^_]+_\d+", product):
        raise InstallError("badpkg", f"{path}: not an FM-1 package (identity {product!r})")
    if LOADER_MARK not in raw and not force:
        raise InstallError("badpkg", f"{path}: no Felucca update loader in this package; "
                                     "only Felucca's own packages are installed (--force overrides)")
    return product, logical_image(raw)


def not_found(up):
    names = up.backend.input_names()
    where = f"no MIDI port matching {up.port!r} answered" if up.port else "FM-1 not found"
    return InstallError("notfound", f"{where} (USB data cable? another app using it?)"
                        f"\n  MIDI inputs: {', '.join(names) if names else '(none)'}")


class Progress:
    def __init__(self, out):
        self.out, self.line = out, False

    def __call__(self, k, a=None):
        if k == "check":
            self.put(f"checking the package ({a} reads)")
        elif k == "write":
            self.put(f"writing {a:3d}%")
        elif k == "stall":
            self.put(f"the FM-1 stopped answering; waiting for it ({a} s)...")
        else:
            self.end()
            msg = {"start": f"starting the update on {a}", "loader": "switching to update mode",
                   "resume": f"the FM-1 is back: the write starts again (resume {a} of {RESUMES}; "
                             "what is already written is only checked)",
                   "reboot": "restarting", "done": f"done: the FM-1 runs {a}"}[k]
            print(msg, file=self.out, flush=True)

    def put(self, s):
        print(f"\r{s}\033[K", end="", file=self.out, flush=True)
        self.line = True

    def end(self):
        if self.line:
            print(file=self.out, flush=True)
            self.line = False


def run(a, backend, out, ask, log=None):
    up = Updater(backend, a.port, log)
    if a.info:
        dev = up.find()
        if not dev:
            raise not_found(up)
        dev.link.close()
        mode = "update loader (update not finished)" if dev.id.loader else "running"
        print(f"{dev.id.text}  [{mode}]  port: {dev.name}", file=out)
        return 0
    product, image = load_package(a.package, a.force)
    print(f"package: {product}  ({a.package})", file=out)
    up.log.event(f"package {product} ({a.package}), {len(image)} B, "
                 f"{'Felucca loader' if LOADER_MARK in image else 'no Felucca loader (--force)'}")
    up.log.event(f"MIDI inputs: {', '.join(backend.input_names()) or '(none)'}")
    up.log.keep = True
    dev = up.find()
    if not dev:
        up.log.event("FM-1 not found")
        raise not_found(up)
    up.log.event(f"device {dev.id.text} on {dev.name}")
    print(f"device:  {dev.id.text}  ({dev.name})" + ("  in update mode: the write will be finished" if dev.id.loader else ""),
          file=out)
    print(BEFORE, file=out)
    if not a.yes and not ask("Install? Do not unplug the FM-1 while writing. [y/N] "):
        dev.link.close()
        print("cancelled", file=out)
        up.log.keep = False
        return 1
    prog = Progress(out)
    try:
        up.install(dev, image, product, prog)
    finally:
        prog.end()
    return 0


def ask_tty(prompt):
    try:
        return input(prompt).strip().lower() in ("y", "yes")
    except EOFError:
        return False


def main(argv=None, backend=None, out=sys.stdout, ask=ask_tty):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("package", nargs="?", help="Felucca package (.fwsc)")
    ap.add_argument("--info", action="store_true", help="print the identity of the connected FM-1")
    ap.add_argument("--port", metavar="NAME", help="MIDI port to use (part of its name)")
    ap.add_argument("--yes", action="store_true", help="do not ask for confirmation")
    ap.add_argument("--force", action="store_true", help="install a package without the Felucca loader marker")
    ap.add_argument("--log-dir", metavar="DIR", help="where the session's log goes (default ~/fm1-install-logs, "
                                                     "or $FM1_INSTALL_LOGS)")
    a = ap.parse_args(argv)
    if bool(a.info) == bool(a.package):
        ap.error("give a PACKAGE.fwsc or --info")
    log = SessionLog()

    def save_log(dest):
        if not log.keep:
            return
        log.event("end")
        path = log.save(a.log_dir)
        print(f"the session's log: {path}" if path else "the session's log could not be written", file=dest)

    try:
        if backend is None:
            try:
                backend = MidoBackend()
            except ImportError:
                raise InstallError("usage", "needs mido and python-rtmidi (pip install mido python-rtmidi)")
        rc = run(a, backend, out, ask, log)
        save_log(out)
        return rc
    except InstallError as e:
        log.event(f"error ({e.code}): {e}")
        print(f"error: {e}", file=sys.stderr)
        save_log(sys.stderr)
        return EXIT.get(e.code, 1)
    except KeyboardInterrupt:
        log.event("interrupted")
        print("\ninterrupted. If the FM-1 is in update mode (its screen blank), run the install again to finish.",
              file=sys.stderr)
        save_log(sys.stderr)
        return 130


if __name__ == "__main__":
    sys.exit(main())
