#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""(1.5.0, step 4) tools/dub1_set.py against the host simulator (build/host/amb_host's "sxpipe": the whole firmware, its
set protocol fed frame by frame as the USB would, its time running as the real one): list, send (the FM-1 asked: GLO
yes, HOME no), read back the same, back up WORK (and send a backup as a set), load (its pieces in the slots, as the
simulator checks), delete, LIST and READ while playing (BUSY for the rest), the refusals the tool makes itself (a name too wide), the CLI;
(Decisions 4) restore a whole backup after one question (HOME: none of it; GLO: its sets and pieces); (Decisions 5) a
restore stopped at the first set that fails on the flash, the tool and the CLI saying which were kept. Needs
build/host/amb_host (host/build_host.sh). Run from the repo root:
  python3 tests/host/dub1_set_test.py"""
import io
import os
import sys
import tempfile
import threading
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import dub1_container as C  # noqa: E402
import dub1_set as S  # noqa: E402

S.DELAY.update(poll=0.05, ask=20.0)
HOST = str(ROOT / "build/host/amb_host")
failed = 0


def ok(cond, what):
    global failed
    print(f"{what:<70} {'ok' if cond else 'FAIL'}")
    failed += not cond


def main():
    if not os.path.exists(HOST):
        print("no build/host/amb_host: run host/build_host.sh")
        return 1
    tmp = tempfile.mkdtemp(prefix="dub1_set_test_")
    five, three = os.path.join(tmp, "five.dub1"), os.path.join(tmp, "three.dub1")
    # (setsdelete: the sets the FM-1 comes with, stored at its first boot, deleted: the empty store this test was made for)
    start = S.HOST_START + "".join(x + "\n" for x in [
        "setsdelete", "name 0 Winter", "name 1 Night Drive", "name -1 Echo Set", "slotrecipe 2 7",
        "dub1set %s 5" % five, "name -1 Three", "dub1set %s 3 noglob" % three, "name -1 -"])
    link = S.HostLink(HOST, start, tmp)
    dev = S.Device(link)
    try:
        hi = dev.hello()
        ok(hi["proto"] == 1 and hi["format"] == 37 and hi["chunk_in"] == 512 and hi["sets"] == 8 and not hi["playing"],
           "HELLO: protocol 1, format 37, 512-byte chunks, 8 sets, stopped")
        ls = dev.list()
        ok(len(ls) == 1 and ls[0][0] == S.WORK and ls[0][2] == 4184, "LIST: the pieces now (WORK, 4184 bytes), no sets")

        data = open(five, "rb").read()
        st = dev.send_set(2, data, on_ask=lambda: link.cmd("tap GLO"))
        ok(st == S.S_YES, "send to set 3, GLO on the FM-1: kept")
        ls = dev.list()
        ok([(k, i, n, p, nm) for k, i, n, p, _, nm in ls[1:]] == [(S.SET, 2, len(data), 5, "Echo Set")],
           "LIST: set 3, Echo Set, 5 pieces")
        ok(dev.read(S.SET, 2) == data, "read set 3 back: the same bytes")
        st = dev.send_set(4, open(three, "rb").read(), on_ask=lambda: link.cmd("tap HOME"))
        ok(st == S.S_NO and len(dev.list()) == 2, "send to set 5, HOME on the FM-1: not kept")

        work = dev.read(S.WORK, 0)
        w = C.parse(work)
        ok(w["header"]["kind"] == S.WORK and w["header"]["npieces"] == 8, "backup: WORK as it is, 8 pieces")
        as_set = S.as_set(work, "Backup")
        ok(C.parse(as_set)["header"]["kind"] == S.SET, "a backup made a set, named")
        st = dev.send_set(7, as_set, on_ask=lambda: link.cmd("tap GLO"))
        ok(st == S.S_YES and dev.read(S.SET, 7) == as_set, "the backup sent as set 8")

        st, nm = dev.ask_op(S.APPLY, 2, on_ask=lambda: link.cmd("tap GLO"))
        time.sleep(0.3)
        for line in ("expect setlen 5", "expect recipe2 7", "expectname 1 Night Drive", "expect piece 0",
                     "expect dirty 0", "workcheck"):
            link.cmd(line)
        ok(st == S.S_YES and nm == "Echo Set", "load set 3: asked (Echo Set), GLO: loaded")
        st, nm = dev.ask_op(S.DELETE, 7, on_ask=lambda: link.cmd("turn K1 1"))
        ok(st == S.S_NO and len(dev.list()) == 3, "delete set 8, a knob turned: not deleted")
        st, nm = dev.ask_op(S.DELETE, 7, on_ask=lambda: link.cmd("tap GLO"))
        ok(st == S.S_YES and nm == "Backup" and len(dev.list()) == 2, "delete set 8, GLO: gone")
        try:
            dev.ask_op(S.APPLY, 6)
            ok(False, "load of an empty set: refused")
        except S.SetError as e:
            ok(e.code == "refused" and "no such set" in str(e), "load of an empty set: refused (%s)" % e)

        link.cmd("wait 3200")                    # (the delete's outcome on the screen, 3 s: a button then only takes it away)
        link.cmd("tap PLAY")
        time.sleep(0.2)
        ok(len(dev.list()) == 2, "LIST while playing (Decisions 3: read-only)")
        ok(C.parse(dev.read(S.WORK, 0))["header"]["kind"] == S.WORK and dev.read(S.SET, 2) == data,
           "a backup while playing: WORK and set 3 read")
        try:
            dev.send_set(4, open(three, "rb").read())
            ok(False, "send while playing: busy")
        except S.SetError as e:
            ok(e.code == "busy" and "stop first" in str(e), "send while playing: %s" % e)
        ok(dev.hello()["playing"], "HELLO while playing: says so")
        link.cmd("tap PLAY")
        time.sleep(0.2)

        try:
            S.as_set(open(five, "rb").read(), "Woodward Dub")
            ok(False, "a name too wide refused by the tool")
        except S.SetError as e:
            ok(e.code == "file" and "87 px" in str(e), "a name too wide refused by the tool (%s)" % e)
        try:
            S.as_set(b"not a container")
            ok(False, "not a .dub1: refused")
        except S.SetError as e:
            ok(e.code == "file", "not a .dub1: refused")

        out = io.StringIO()   # the CLI, on the same simulator
        rc = S.main(["list"], out=out, link=link)
        ok(rc == 0 and "set 3: Echo Set, 5 pieces" in out.getvalue(), "CLI list")
        path = os.path.join(tmp, "b.dub1")
        out = io.StringIO()
        rc = S.main(["backup", path], out=out, link=link)
        ok(rc == 0 and C.parse(open(path, "rb").read())["header"]["kind"] == S.WORK, "CLI backup")
        out = io.StringIO()
        rc = S.main(["backup", path, "--all"], out=out, link=link)
        objs = C.unbundle(open(path, "rb").read()) if rc == 0 else []
        ok(rc == 0 and [(k, i) for k, i, _ in objs] == [(S.WORK, 0), (S.SET, 2)] and objs[1][2] == data,
           "CLI backup --all: the pieces and set 3 in one file")
        out = io.StringIO()
        th = threading.Thread(target=lambda: (time.sleep(0.6), link.cmd("tap OCT+")))
        th.start()
        rc = S.main(["send", "6", three], out=out, link=link)
        th.join()
        ok(rc == 1 and "Answer on the FM-1" in out.getvalue(), "CLI send, asked, answered no (OCT+): exit 1")

        # (Decisions 4) restore: a backup of the pieces, set 3 and set 6 back after one question (HOME: none of it)
        whole = os.path.join(tmp, "whole.dub1")
        with open(whole, "wb") as f:
            f.write(C.bundle([(S.WORK, 0, work), (S.SET, 2, data), (S.SET, 5, open(three, "rb").read())], hi["version"]))
        sets, wk, mask = S.restore_plan(open(whole, "rb").read(), 37)
        ok(mask == 0x24 and [o["index"] for o in sets] == [2, 5] and wk and sets[1]["name"] == "Three",
           "restore's plan: sets 3 and 6 and the pieces")
        link.cmd("name 0 Changed")
        st, done = dev.restore(open(whole, "rb").read(), on_ask=lambda: link.cmd("tap HOME"))
        ok(st == S.S_NO and not done and not any(i == 5 for k, i, *_ in dev.list() if k == S.SET),
           "restore, HOME on the FM-1: nothing sent")
        out = io.StringIO()
        th = threading.Thread(target=lambda: (time.sleep(0.6), link.cmd("tap GLO")))
        th.start()
        rc = S.main(["restore", whole], out=out, link=link)
        th.join()
        time.sleep(0.3)
        ls = dev.list()
        ok(rc == 0 and "restored: 2 sets and the pieces" in out.getvalue() and
           [(i, nm) for k, i, _, _, _, nm in ls if k == S.SET] == [(2, "Echo Set"), (5, "Three")] and
           dev.read(S.SET, 5) == open(three, "rb").read(), "CLI restore, GLO once: sets 3 and 6 and the pieces")
        for line in ("expectname 0 Winter", "expect dirty 0", "workcheck"):
            link.cmd(line)
        # (Decisions 5) a restore stops at the first set that fails on the flash (its power lost from set 6 on):
        # set 3 restored, set 6 failed, the pieces never sent; the CLI says so and exits 1
        for cli in (False, True):
            on_item = lambda i, n, o: link.cmd("powercut 1") if i == 1 else None
            if not cli:
                try:
                    dev.restore(open(whole, "rb").read(), on_ask=lambda: link.cmd("tap GLO"), on_item=on_item)
                    ok(False, "restore, the flash failing at set 6: stopped")
                except S.SetError as e:
                    ok(e.code == "stopped" and [(k, i) for k, i, nm, s in e.done] == [(S.SET, 2)] and
                       e.failed[:2] == (S.SET, 5) and e.left == [(S.WORK, 0, wk["name"])],
                       "restore, the flash failing at set 6: stopped there (%s)" % e)
            else:
                out = io.StringIO()
                real = S.Device.restore
                S.Device.restore = lambda self, data, a, item, bar: real(self, data, a, lambda i, n, o: (
                    item(i, n, o), on_item(i, n, o)), bar)
                th = threading.Thread(target=lambda: (time.sleep(0.6), link.cmd("tap GLO")))
                th.start()
                try:
                    rc = S.main(["restore", whole], out=out, link=link)
                finally:
                    S.Device.restore = real
                th.join()
                ok(rc == 1 and "restored before it stopped: set 3" in out.getvalue() and
                   "not sent: the pieces" in out.getvalue(),
                   "CLI restore, the flash failing at set 6: which were restored, which not sent; exit 1")
            for line in ("reboot", "wait 300", "tap PLAY", "wait 300", "expect playing 0"):
                link.cmd(line)
            ok(dev.read(S.SET, 2) == data and not dev.hello()["restoring"], "after it: set 3 kept, no restore open")
        try:
            S.restore_plan(data)
            ok(False, "restore of a set, not a backup: refused")
        except S.SetError as e:
            ok(e.code == "file", "restore of a set, not a backup: refused (%s)" % e)
        log = "\n".join(link.log)
        ok("FAIL" not in log and "ok setlen == 5" in log and "ok workcheck" in log,
           "the simulator's checks (expect, workcheck) all held")
        if "FAIL" in log:
            print("\n".join(l for l in link.log if "FAIL" in l))
    finally:
        link.close()
    print("dub1_set: %s" % ("%d FAILED" % failed if failed else "ok"))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
