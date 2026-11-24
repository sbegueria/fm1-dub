// SPDX-License-Identifier: GPL-3.0-only
// (1.5.0, step 7b) The Sets page's cue editor's model (web/dub1cues.js) in Node:
//  - its tables are the firmware's (tools/gen_cue_tables.py --check, run by tests/run_tests.sh as "cue-tables");
//  - places: part, bar and 16th to the CUES chunk's pos and back; what each op takes (normal);
//  - a set's cues read, added, changed, deleted, kept sorted by place (equal places in the order given); 64 a piece at
//    most; a cue the FM-1 wouldn't take refused; the file unchanged written byte for byte as it came;
//  - every cue checked as the firmware checks one: 40,000 cues (each op, its targets and values at and past their
//    ends, lengths) through the editor's cueWhy and the simulator's cue_ok ("cuecheck"): the same answer each;
//  - an edited set (every op) the same as tests/host/cues-edited.dub1 (made so: --write), the same dump and bytes in the
//    C, Python and JS codecs, loaded by the simulator as the FM-1 loads a set (its cues the editor's, cue for cue), and
//    sent over the set protocol (COMMIT's check: kept); a cue past its part left out where the player leaves it out
//    (where), and the emulator's tests play one (tests/host/emu_cues_test.mjs).
//   node tests/host/cue_editor_test.mjs [--write]     (from the repo root; needs build/host/amb_host)
import { execFileSync } from "child_process";
import fs from "fs";
import os from "os";
import path from "path";
import * as C from "../../web/dub1cues.js";
import { parse, dump, Refused } from "../../web/dub1container.js";

const HOST = "build/host/amb_host", FIX = "tests/host/cues-edited.dub1", WINTER = "sets/winter-set.dub1";
let failed = 0;
const ok = (c, what) => { console.log(`${what.padEnd(76)} ${c ? "ok" : "FAIL"}`); if (!c) failed++; };
const same = (a, b) => a.length === b.length && a.every((x, i) => x === b[i]);
const throws = (fn, code) => { try { fn(); return false; } catch (e) { return code === undefined || e.code === code; } };
fs.mkdirSync("build/host", { recursive: true });
const tmp = fs.mkdtempSync("build/host/cue-");   // (short: the simulator's file arguments are 64 bytes)
const host = (script) => {
  const f = path.join(tmp, "s.amb");
  fs.writeFileSync(f, script);
  return execFileSync(HOST, [f, tmp], { encoding: "latin1", maxBuffer: 1 << 26 });
};
if (!fs.existsSync(HOST)) { console.log("no build/host/amb_host: run host/build_host.sh"); process.exit(1); }

// ---------------------------------------------------------------------------------------------------- places, ops
ok(C.pos(C.MAIN, 16, 0) === (1 << 14 | 16 << 4) && C.pos(C.BAR, 1023, 15) === 65535 && C.pos(C.INTRO, 0, 0) === 0,
   "pos: main bar 16 (the 17th), the last place, the first");
ok(JSON.stringify(C.place(C.pos(C.OUTRO_P, 3, 9))) === JSON.stringify({ part: 2, bar: 3, step: 9 }), "place: pos back to its part, bar, 16th");
ok(C.OPS.length === 12 && C.T.ops.join() === "mute,solo,move,fx,siren,sirk,macro,knob,outro,next,lock,hold",
   "the 12 ops, in the firmware's order");
ok(C.targets(C.KNOB).length === C.T.params.filter((p) => p.cue).length && !C.targets(C.KNOB).some((t) => [16, 17].includes(t.arg)),
   "a knob's targets: every parameter but Tune, MIDI out, Song mode");
const motion = C.targets(C.KNOB).find((t) => t.arg === 15);
ok(motion.group === "Form" && motion.name === "Motion", "Motion's knob: the Form page's Motion (the manual's label)");
ok(C.range(C.MACRO, 0).text(-30) === "Shut 60" && C.range(C.MACRO, 0).text(0) === "Off" && C.range(C.SIRK, 1 * 4 + 1).text(18) === "12 Hz",
   "a value as the screen shows it: Filter -30 Shut 60 (0..100), Off; Whoop's Rate 12 Hz");
const n1 = C.normal({ pos: 5, op: C.OUTRO, arg: 3, val: 9, len: 4 }), n2 = C.normal({ pos: 5, op: C.MOVE, arg: 2, val: 7, len: 0 });
ok(n1.arg === 0 && n1.val === 0 && n1.len === 0 && n2.val === 0 && n2.len === 1, "normal: what an op doesn't take is 0; a hold at least a 16th");

// ---------------------------------------------------------------------------------------------- the model, edited
const winter = new Uint8Array(fs.readFileSync(WINTER));
let e = new C.Edit(winter);
ok(e.name === "Winter Set" && e.pieces.length === 5 && e.pieces.map((p) => p.cues.length).join() === "4,9,8,12,6" && !e.problems.length,
   "the Winter Set read: 5 pieces, 39 cues, none the FM-1 would refuse");
ok(e.mode === 2 && e.trans === 2 && e.pieces[0].song.main === 280, "its Song mode Set, Transition Mix, Hoarfrost's main 280 bars");
ok(same(e.bytes(), winter), "unchanged: written as it came, byte for byte");
const i0 = e.add(0, { pos: C.pos(C.MAIN, 64, 2), op: C.MUTE, arg: 4, val: 1, len: 0 });   // (main 64.2 has the chord throw)
ok(i0 === 3 && e.pieces[0].cues[2].op === C.MOVE && e.pieces[0].cues[3].op === C.MUTE, "add at an equal place: after the one there");
const i1 = e.add(0, { pos: C.pos(C.INTRO, 2, 0), op: C.SOLO, arg: 2, val: 1, len: 0 });
ok(i1 === 1 && e.pieces[0].cues.every((q, i, a) => !i || a[i - 1].pos <= q.pos), "add: in its place, the list sorted");
const i2 = e.change(0, i1, { pos: C.pos(C.OUTRO_P, 1, 0), op: C.SOLO, arg: 2, val: 0, len: 0 });
ok(e.pieces[0].cues[i2].pos === C.pos(C.OUTRO_P, 1, 0) && C.listWhy(e.pieces[0].cues).length === 0, "change: moved to its new place");
e.remove(0, i2);
ok(e.pieces[0].cues.length === 5 && e.dirty, "delete");
ok(throws(() => e.add(0, { pos: 0, op: C.KNOB, arg: 16, val: 0, len: 0 }), "global") &&
   throws(() => e.add(0, { pos: 0, op: C.MACRO, arg: 0, val: 51, len: 0 }), "val") &&
   throws(() => e.add(0, { pos: 0, op: C.FX, arg: 16, val: 0, len: 4 }), "arg") &&
   throws(() => e.add(0, { pos: 0, op: 12, arg: 0, val: 0, len: 0 }), "op"),
   "refused: Tune ridden, the filter past +50, a 17th effect, an op the FM-1 doesn't know");
const full = new C.Edit(winter);
for (let k = full.pieces[1].cues.length; k < 64; k++) full.add(1, { pos: C.pos(C.MAIN, k, 0), op: C.NEXT, arg: 0, val: 0, len: 0 });
ok(full.pieces[1].cues.length === 64 && throws(() => full.add(1, { pos: 0, op: C.NEXT, arg: 0, val: 0, len: 0 }), "full"),
   "64 cues a piece: the 65th refused");
full.pieces[1].cues.push({ pos: 0, op: C.NEXT, arg: 0, val: 0, len: 0 });
ok(full.check().some((p) => p.why === "count") && full.check().some((p) => p.why === "order") && throws(() => full.bytes()),
   "a list of 65, out of order: refused by check and bytes");
// where the player leaves a cue out (cues.c's cue_when): past its part, an outro in Endless, the hand-over's last bar
const song = { intro: 16, main: 32, outro: 16 };
const W = (part, bar, mode = 1, ho = false) => C.where({ pos: C.pos(part, bar, 0) }, song, mode, ho);
ok(W(C.MAIN, 31).at === 47 && W(C.MAIN, 32).why === "pastmain" && W(C.INTRO, 16).why === "pastintro" &&
   W(C.OUTRO_P, 16).why === "pastoutro" && W(C.BAR, 64).why === "pastend" && W(C.OUTRO_P, 0, 0).why === "endless" &&
   W(C.MAIN, 100, 0).at === 100 && W(C.OUTRO_P, 15, 2, true).why === "handover" && W(C.OUTRO_P, 14, 2, true).at === 62,
   "where: past the main, the intro, the outro, the song; Endless; a Mix's last bar");

// ----------------------------------------------------------------------- every cue checked as the firmware checks it
let s = 12345;
const r = (n) => { s ^= s << 13; s >>>= 0; s ^= s >>> 17; s ^= s << 5; s >>>= 0; return s % n; };
const cues = [];
const vals = (lo, hi) => [lo - 1, lo, hi, hi + 1, lo + r(hi - lo + 1), -32768, 32767, 0, 1, 2].filter((v) => v >= -32768 && v <= 32767);
for (let op = 0; op <= 13; op++) {
  for (let arg = 0; arg < 256; arg += arg < 180 ? 1 : 7) {
    const rg = C.range(op, arg), vs = rg ? vals(rg.lo, rg.hi) : [0, 1, 2, -1, r(65536) - 32768];
    for (const val of vs) for (const len of [0, 1, 2, 3, r(65536)]) cues.push({ pos: r(65536), op, arg, val, len });
  }
}
while (cues.length < 40000) cues.push({ pos: r(65536), op: r(14), arg: r(256), val: r(65536) - 32768, len: r(4) ? r(40) : r(65536) });
fs.writeFileSync(path.join(tmp, "cues.txt"), cues.map((q) => `${q.pos} ${q.op} ${q.arg} ${q.val} ${q.len}`).join("\n") + "\n");
host(`cuecheck ${path.join(tmp, "cues.txt")} ${path.join(tmp, "ok.txt")}\n`);
const fw = fs.readFileSync(path.join(tmp, "ok.txt"), "utf8").trim().split("\n");
const diff = cues.filter((q, i) => (fw[i] === "1") !== C.cueOk(q));
ok(fw.length === cues.length && !diff.length, `cueWhy and the firmware's cue_ok: the same for ${cues.length} cues ` +
   `(${fw.filter((x) => x === "1").length} taken)`);
if (diff.length) console.log("   e.g.", JSON.stringify(diff.slice(0, 5)));

// ------------------------------------------------- an edited set: the fixture, the three codecs, the FM-1 takes it
e = new C.Edit(winter);
e.change(0, 0, { ...e.pieces[0].cues[0], val: -25 });                         // Hoarfrost: the filter to Shut 25
e.add(0, { pos: C.pos(C.MAIN, 16, 0), op: C.MUTE, arg: 4, val: 1, len: 0 });   // the hats out at the main's bar 17
e.add(0, { pos: C.pos(C.MAIN, 24, 0), op: C.MUTE, arg: 4, val: 0, len: 0 });   // and back 8 bars on
e.remove(2, e.pieces[2].cues.length - 1);                                      // Snowfield: its last cue gone
const th = [                                                                    // Thaw: every other op
  { pos: C.pos(C.MAIN, 8, 4), op: C.KNOB, arg: 15, val: 60, len: 64 }, { pos: C.pos(C.MAIN, 12, 0), op: C.SIRK, arg: 3, val: 80, len: 16 },
  { pos: C.pos(C.MAIN, 12, 0), op: C.SIREN, arg: 0, val: 0, len: 32 }, { pos: C.pos(C.MAIN, 20, 0), op: C.LOCK, arg: 0, val: 1, len: 0 },
  { pos: C.pos(C.MAIN, 28, 0), op: C.LOCK, arg: 0, val: 0, len: 0 }, { pos: C.pos(C.MAIN, 30, 0), op: C.HOLD, arg: 0, val: 1, len: 0 },
  { pos: C.pos(C.MAIN, 31, 0), op: C.HOLD, arg: 0, val: 0, len: 0 }, { pos: C.pos(C.MAIN, 40, 0), op: C.FX, arg: 6, val: 0, len: 8 },
  { pos: C.pos(C.MAIN, 44, 0), op: C.SOLO, arg: 2, val: 1, len: 0 }, { pos: C.pos(C.MAIN, 45, 0), op: C.SOLO, arg: 2, val: 0, len: 0 },
  { pos: C.pos(C.MAIN, 48, 0), op: C.MOVE, arg: 3, val: 0, len: 16 }, { pos: C.pos(C.MAIN, 56, 0), op: C.NEXT, arg: 0, val: 0, len: 0 },
  { pos: C.pos(C.BAR, 400, 0), op: C.OUTRO, arg: 0, val: 0, len: 0 }, { pos: C.pos(C.OUTRO_P, 2, 0), op: C.SIREN, arg: 1, val: 0, len: 0 },
];
for (const q of th) e.add(4, q);
const ed = e.bytes();
if (process.argv.includes("--write")) fs.writeFileSync(FIX, ed);
ok(fs.existsSync(FIX) && same(ed, new Uint8Array(fs.readFileSync(FIX))), `the edited Winter Set: ${FIX} (${ed.length} bytes), byte for byte`);
const back = new C.Edit(ed);
ok(back.pieces.map((p) => p.cues.length).join() === "6,9,7,12,20" && back.pieces[0].cues[0].val === -25 && !back.check().length,
   "read back: 6, 9, 7, 12, 20 cues; the filter's -25");
const efile = path.join(tmp, "edited.dub1");
fs.writeFileSync(efile, ed);
execFileSync(process.env.CC || "cc", ["-O2", "-o", path.join(tmp, "ctwin"), "tests/host/container_test.c"]);
const dc = execFileSync(path.join(tmp, "ctwin"), ["dump", efile], { encoding: "utf8" });
const dp = execFileSync(process.env.PYTHON || "python3", ["tools/dub1_container.py", "dump", efile], { encoding: "utf8" });
ok(dc === dump(parse(ed)) && dp === dc, "the edited set's dump: the same in C, Python and JS");
execFileSync(path.join(tmp, "ctwin"), ["recode", efile, path.join(tmp, "re.c")]);
execFileSync(process.env.PYTHON || "python3", ["tools/dub1_container.py", "recode", efile, path.join(tmp, "re.py")]);
ok(same(new Uint8Array(fs.readFileSync(path.join(tmp, "re.c"))), ed) && same(new Uint8Array(fs.readFileSync(path.join(tmp, "re.py"))), ed),
   "written again by the C and the Python: the same bytes");
const out = host(`setin ${efile}\n` + [0, 1, 2, 3, 4].map((k) => `cuelist ${k}`).join("\n") + "\n");
const listed = [...out.matchAll(/cues of piece (\d): (\d+)\n((?: {4}.*\n)*)/g)].map((m) => m[3].trim().split("\n").filter(Boolean)
  .map((ln) => ln.trim().split(" ")));
const want = back.pieces.map((p) => p.cues.map((q) => {
  const pl = C.place(q.pos);
  return ["imob"[pl.part], `${pl.bar}.${pl.step}`, C.T.ops[q.op], String(q.arg), String(q.val), String(q.len)];
}));
ok(/ok setin/.test(out) && JSON.stringify(listed) === JSON.stringify(want), "the simulator loads it as the FM-1 loads a set: its cues the editor's, cue for cue");
const wire = host(`wait 300\ntap PLAY\nwait 300\nsetsend 3 ${efile}\nwait 100\nsetstate asking\ntap GLO\nwait 100\nsetstate yes\nsethas 3 Winter Set\n`);
ok(!/FAIL/.test(wire), "sent over the set protocol: COMMIT's check passes, kept as set 4");
// a file the FM-1 would refuse isn't written: a cue made bad by hand
const bad = new C.Edit(ed);
bad.pieces[4].cues[0].val = 101;
bad.dirty = true;
ok(throws(() => bad.bytes()) && bad.check()[0].why === "val", "a cue made bad by hand: not written (Motion 101)");
// an older or a newer file: not edited (cues are format 37's)
const older = new Uint8Array(fs.readFileSync("tests/host/work-1.5.0-step4.dub1"));
ok(parse(older).header.format === 37 ? !!new C.Edit(older) : throws(() => new C.Edit(older), "older"), "the pieces (WORK) of a backup: edited too");
fs.rmSync(tmp, { recursive: true, force: true });
console.log(failed ? `cue editor: ${failed} FAILED` : "cue editor: all ok");
process.exit(failed ? 1 : 0);
