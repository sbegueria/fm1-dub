// SPDX-License-Identifier: GPL-3.0-only
// (1.5.0, step 5) the Sets page's protocol logic (web/dub1sets.js, with web/dub1container.js) against the host simulator
// (build/host/amb_host's "sxpipe": the whole firmware, its set protocol fed frame by frame as the USB would, its time
// running as the real one), as tests/host/dub1_set_test.py drives tools/dub1_set.py: HELLO, LIST, a set sent (the
// FM-1 asked: GLO yes, HOME no, CANCEL, 30 s), read back the same, a whole backup in one file while the music plays
// (WORK and every set, each the same as read alone), a backup's pieces sent as a named set, load (the simulator's own
// checks after), delete, BUSY while playing for the rest (the page's words), the NAKs in the page's words (none
// stored, a newer DUB-1's), the files the page refuses itself (a name too wide, not a .dub1, a whole backup), and the
// backup made by the Python tool read the same here; (Decisions 4) restore everything: refused while playing, HOME
// says no to the whole, GLO once keeps its sets and pieces unasked, the files the page refuses for it; (Decisions 5) a
// restore stopped at the first set that fails on the flash (which were kept, which never sent), and versionOf (HELLO's
// version string: a test build told from the release).
//   node tests/host/dub1_sets_test.mjs      (from the repo root; needs build/host/amb_host: host/build_host.sh)
import { execFileSync } from "child_process";
import fs from "fs";
import os from "os";
import path from "path";
import { HostLink, HOST } from "./hostlink.mjs";
import * as X from "../../web/dub1sets.js";
import { parse, build, unbundle, bundle } from "../../web/dub1container.js";

X.DELAY.poll = 50;
let failed = 0;
const ok = (c, what) => { console.log(`${what.padEnd(72)} ${c ? "ok" : "FAIL"}`); if (!c) failed++; };
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const same = (a, b) => a.length === b.length && a.every((x, i) => x === b[i]);

async function main() {
  if (!fs.existsSync(HOST)) { console.log("no build/host/amb_host: run host/build_host.sh"); return 1; }
  const tmp = fs.mkdtempSync(path.join(os.tmpdir(), "dub1_sets_test_"));
  const five = path.join(tmp, "five.dub1"), three = path.join(tmp, "three.dub1");
  // (setsdelete: the sets the FM-1 comes with, stored at its first boot, deleted: the empty store this test was made for)
  const script = "wait 600\ntap PLAY\nwait 300\nsetsdelete\n" + ["name 0 Winter", "name 1 Night Drive",
    "name -1 Echo Set", "slotrecipe 2 7", `dub1set ${five} 5`, "name -1 Three", `dub1set ${three} 3 noglob`, "name -1 -"].join("\n") + "\n";
  const link = new HostLink(script, tmp);
  await link.ready;
  const dev = new X.Device(link);
  try {
    const hi = await dev.hello();
    ok(hi.proto === 1 && hi.format === 37 && hi.chunkIn === 512 && hi.sets === 8 && !hi.playing && /^DUB-1/.test(hi.version),
       "HELLO: protocol 1, format 37, 512-byte chunks, 8 sets, stopped");
    let ls = await dev.list();
    ok(ls.length === 1 && ls[0].kind === X.WORK && ls[0].length > 4000, "LIST: the pieces now (WORK), no sets");

    // the file shown before it is sent: its name, its pieces, Haze's piece in slot 3
    const data = new Uint8Array(fs.readFileSync(five)), d = X.describe(data);
    ok(d.kind === X.SET && d.name === "Echo Set" && d.pieces.length === 5 && d.pieces[0].name === "Winter" &&
       d.pieces[2].recipeName === "Haze" && d.pieces[2].name === "", "describe: Echo Set, 5 pieces, Haze's in slot 3");
    let asked = 0, ticks = [];
    let st = await dev.sendSet(2, data, { onAsk: () => { asked++; link.cmd("tap GLO"); }, onTick: (s) => ticks.push(s),
                                          progress: () => {} });
    ok(st === X.S_YES && asked === 1, "send to set 3, GLO on the FM-1: kept");
    ls = await dev.list();
    ok(ls.length === 2 && ls[1].index === 2 && ls[1].name === "Echo Set" && ls[1].pieces === 5 && ls[1].length === data.length,
       "LIST: set 3, Echo Set, 5 pieces");
    ok(same(await dev.read(X.SET, 2), data), "set 3 read back (download): the same bytes");
    st = await dev.sendSet(4, new Uint8Array(fs.readFileSync(three)), { onAsk: () => link.cmd("tap HOME") });
    ok(st === X.S_NO && (await dev.list()).length === 2, "send to set 5, HOME on the FM-1: not kept");
    st = await dev.sendSet(4, new Uint8Array(fs.readFileSync(three)), { onAsk: () => dev.cancel() });
    ok(st === X.S_NO, "send to set 5, the page's Cancel while it asks: not kept");
    st = await dev.sendSet(4, new Uint8Array(fs.readFileSync(three)), { onAsk: () => link.cmd("wait 31000") });
    ok(st === X.S_TIMEOUT && X.STATE_TEXT[st].includes("30 seconds"), "send to set 5, 30 s unanswered: not kept");

    // a whole backup while the music plays (Decisions 3): one file, WORK and set 3, each as read alone (after the
    // NOT DONE on the screen, 3 s: a button meanwhile only takes it away)
    await link.cmd("wait 3200");
    await link.cmd("tap PLAY");
    await sleep(200);
    ok((await dev.hello()).playing, "HELLO while playing: says so");
    let seen = 0;
    const bk = await dev.backup((done, total) => { seen = Math.max(seen, done / total); });
    const objs = unbundle(bk.bytes);
    ok(objs.length === 2 && objs[0].kind === X.WORK && objs[1].kind === X.SET && objs[1].index === 2 &&
       same(objs[1].bytes, data) && parse(bk.bytes).header.kind === 6 && seen === 1,
       "backup while playing: one file, WORK and set 3 (the same bytes), progress to 100%");
    const bd = X.describe(bk.bytes);
    ok(bd.kind === 6 && bd.objs.length === 2 && bd.objs[1].info.name === "Echo Set" && bd.version === hi.version,
       "describe a backup: its two objects, the FM-1's version");
    for (const [what, run] of [["send", () => dev.sendSet(5, data)], ["load", () => dev.askOp(X.APPLY, 2)],
                               ["delete", () => dev.askOp(X.DELETE, 2)]]) {
      try { await run(); ok(false, `${what} while playing: busy`); } catch (e) {
        ok(e.code === "busy" && e.why === 1 && /stop it first/.test(e.message), `${what} while playing: "${e.message}"`);
      }
    }
    await link.cmd("tap PLAY");
    await sleep(200);

    // the backup's pieces (WORK) sent as a set, named on the page
    const work = objs[0].bytes, named = X.asSet(work, "Dub Chamber");
    ok(parse(named).header.kind === X.SET && X.describe(named).name === "Dub Chamber", "the backup's pieces made a set, named Dub Chamber (79 px: fits)");
    st = await dev.sendSet(7, named, { onAsk: () => link.cmd("tap GLO") });
    ok(st === X.S_YES && same(await dev.read(X.SET, 7), named), "sent as set 8, read back the same");
    ok(!dev.live, "set 8 not the set loaded: nothing more said");

    // load set 3 into the pieces, the simulator's own checks after
    let r = await dev.askOp(X.APPLY, 2, { onAsk: () => link.cmd("tap GLO") });
    await sleep(300);
    for (const l of ["expect setlen 5", "expect recipe2 7", "expectname 1 Night Drive", "expect piece 0", "expect dirty 0", "workcheck"])
      await link.cmd(l);
    ok(r.state === X.S_YES && r.name === "Echo Set", "load set 3: asked (Echo Set), GLO: loaded");
    // (beta2, the user's) sent again into set 3, the set loaded: kept, said so (the old one plays on); the FM-1 then
    // asks LOAD IT NOW? once its STORED is over (HOME: no)
    st = await dev.sendSet(2, data, { onAsk: () => link.cmd("tap GLO") });
    ok(st === X.S_YES && dev.live, "sent again into set 3 (the set loaded): kept, the page told it's the set loaded");
    for (const l of ["wait 3300", "expect asking 48", "expect again 1", "tap HOME", "wait 3300", "expect outcome 0"])
      await link.cmd(l);
    ok((await dev.ok(X.STATUS))[1] === X.S_NO, "LOAD IT NOW? on the FM-1, HOME: not loaded");
    r = await dev.askOp(X.DELETE, 7, { onAsk: () => link.cmd("turn K1 1") });
    ok(r.state === X.S_NO && (await dev.list()).length === 3, "delete set 8, a knob turned: not deleted");
    r = await dev.askOp(X.DELETE, 7, { onAsk: () => link.cmd("tap GLO") });
    ok(r.state === X.S_YES && r.name === "Dub Chamber" && (await dev.list()).length === 2, "delete set 8, GLO: gone");

    // the NAKs, in the page's words
    try { await dev.askOp(X.APPLY, 6); ok(false, "load of an empty set: refused"); } catch (e) {
      ok(e.code === "refused" && e.why === 3 && /no such set/.test(e.message), `load of an empty set: "${e.message}"`);
    }
    {
      const c = parse(data);
      c.header.format = 99;
      try { await dev.sendSet(6, build(c)); ok(false, "a newer DUB-1's set: refused"); } catch (e) {
        ok(e.code === "refused" && e.why === 5 && /newer DUB-1/.test(e.message), `a newer DUB-1's set: "${e.message}"`);
      }
    }
    // what the page refuses itself
    for (const [what, f] of [["a name too wide (Woodward Dub, 92 px)", () => X.asSet(data, "Woodward Dub")],
                             ["not a .dub1", () => X.asSet(new Uint8Array(100))],
                             ["a whole backup sent as it is", () => X.asSet(bk.bytes)]]) {
      try { f(); ok(false, what); } catch (e) { ok(e.code === "file", `${what}: "${e.message}"`); }
    }
    // a backup the Python twin makes (as dub1_set.py backup --all) read the same here
    const py = process.env.PYTHON || "python3", pyfile = path.join(tmp, "py.dub1");
    try {
      execFileSync(py, ["tools/dub1_container.py", "bundle", pyfile, `1:0:${five}`, `2:2:${three}`]);
      const o = unbundle(new Uint8Array(fs.readFileSync(pyfile)));
      ok(o.length === 2 && same(o[0].bytes, data) && o[1].index === 2, "a backup the Python twin made: read here");
    } catch (e) { ok(false, "a backup the Python twin made: " + e.message); }

    // (Decisions 4) restore everything: a backup of the pieces, set 3 and set 6 back after one question; refused while
    // playing; HOME says no to the whole; a file that isn't a backup, or one made for a newer DUB-1, refused here
    {
      const whole = bundle([{ kind: X.WORK, index: 0, bytes: objs[0].bytes }, { kind: X.SET, index: 2, bytes: data },
                            { kind: X.SET, index: 5, bytes: new Uint8Array(fs.readFileSync(three)) }], hi.version);
      const plan = X.restorePlan(whole, 37);
      ok(plan.mask === 0x24 && plan.sets.length === 2 && plan.work && plan.sets[1].name === "Three",
         "restore's plan: sets 3 and 6 and the pieces");
      await link.cmd("name 0 Changed");
      await link.cmd("wait 3200");             // (the delete's DELETED on the screen, 3 s, gone first)
      await link.cmd("tap PLAY");
      await sleep(200);
      try { await dev.restore(whole); ok(false, "restore while playing: busy"); } catch (e) {
        ok(e.code === "busy" && e.why === 1, `restore while playing: "${e.message}"`);
      }
      await link.cmd("tap PLAY");
      await sleep(200);
      let asks = 0;
      r = await dev.restore(whole, { onAsk: () => { asks++; link.cmd("tap HOME"); } });
      ls = await dev.list();
      ok(r.state === X.S_NO && !r.done.length && asks === 1 && !ls.some((e) => e.index === 5),
         "restore, HOME on the FM-1: nothing sent, set 6 still empty");
      const items = [];
      let pr = 0;
      r = await dev.restore(whole, { onAsk: () => { asks++; link.cmd("tap GLO"); }, onItem: (i, n, o) => items.push(o.kind),
                                     progress: (d, t) => { pr = d / t; } });
      await sleep(300);
      ls = await dev.list();
      ok(r.state === X.S_YES && asks === 2 && r.done.length === 3 && r.done.every((x) => x.state === X.S_YES) &&
         items.join() === "2,2,1" && pr === 1, "restore, GLO once: sets 3 and 6, then the pieces, each kept unasked");
      ok(ls.some((e) => e.index === 5 && e.name === "Three") && ls.some((e) => e.index === 2 && e.name === "Echo Set") &&
         same(await dev.read(X.SET, 5), new Uint8Array(fs.readFileSync(three))), "LIST after: set 6 Three, set 3 Echo Set");
      for (const l of ["expectname 0 Winter", "expect dirty 0", "workcheck"])
        await link.cmd(l);
      ok(!(await dev.hello()).restoring, "HELLO after: no restore open");
      for (const [what, f, code] of [["a set, not a backup", () => X.restorePlan(data), "file"],
                                     ["a backup made for a newer DUB-1", () => X.restorePlan(whole, 36), "refused"]]) {
        try { f(); ok(false, what); } catch (e) { ok(e.code === code, `restore of ${what}: "${e.message}"`); }
      }
      // (Decisions 5) it stops at the first set that fails on the flash (its power lost from set 6 on): set 3
      // restored, set 6 failed, the pieces never sent
      try {
        await dev.restore(whole, { onAsk: () => link.cmd("tap GLO"),
                                   onItem: (i) => { if (i === 1) link.cmd("powercut 1"); } });
        ok(false, "restore, the flash failing at set 6: stopped");
      } catch (e) {
        ok(e.code === "stopped" && e.done.map((x) => x.index).join() === "2" && e.failed.kind === X.SET &&
           e.failed.index === 5 && e.failed.err && e.failed.err.why === 8 && e.left.length === 1 && e.left[0].kind === X.WORK,
           `restore, the flash failing at set 6: stopped there ("${e.message}")`);
      }
      for (const l of ["reboot", "wait 300", "tap PLAY", "wait 300", "expect playing 0"])
        await link.cmd(l);
      ok(same(await dev.read(X.SET, 2), data) && !(await dev.hello()).restoring, "after it: set 3 kept, no restore open");
    }

    // (Decisions 5) the installer's check: HELLO's version string, a test build told from the release (Decisions 6: any
    // suffix a test build)
    {
      const v = (s) => JSON.stringify(X.versionOf(s));
      ok(v("DUB-1 1.5.0-BETA1") === JSON.stringify({ text: "1.5.0-BETA1", release: "1.5.0", pre: "BETA1", test: true, dev: false }) &&
         v("DUB-1 1.5.0") === JSON.stringify({ text: "1.5.0", release: "1.5.0", pre: "", test: false, dev: false }) &&
         v("DUB-1 1.5.2-storetest") === JSON.stringify({ text: "1.5.2-storetest", release: "1.5.2", pre: "storetest", test: true, dev: false }) &&
         v("DUB-1 1.5.0-rc1") === JSON.stringify({ text: "1.5.0-rc1", release: "1.5.0", pre: "rc1", test: true, dev: false }) &&
         v("DUB-1 DEV") === JSON.stringify({ text: "DEV", release: "", pre: "", test: false, dev: true }) &&
         X.versionOf("") === null && X.versionOf("FM-1 V15") === null, "versionOf: a beta, a release, a test build, a release candidate (all with a suffix: test builds), DEV, none");
      ok(X.versionOf((await dev.hello()).version).dev, `the simulator's HELLO: "${(await dev.hello()).version}", a development build`);
    }

    await link.cmd("wait 100");
    const fails = link.log.filter((l) => l.includes("FAIL"));
    ok(!fails.length && link.log.some((l) => l.includes("ok setlen == 5")) && link.log.some((l) => l.includes("ok workcheck")),
       "the simulator's checks (expect, workcheck) all held");
    if (fails.length) console.log(fails.join("\n"));
  } catch (e) {
    ok(false, "unexpected: " + (e && e.stack || e));
  } finally {
    await link.close();
    fs.rmSync(tmp, { recursive: true, force: true });
  }
  console.log(`dub1_sets: ${failed ? failed + " FAILED" : "ok"}`);
  return failed ? 1 : 0;
}
process.exit(await main());
