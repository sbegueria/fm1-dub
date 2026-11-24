// SPDX-License-Identifier: GPL-3.0-only
// (1.5.0) The Sets page's "Make a set" (web/dub1makeset.js) in Node, and its twin tools/dub1_set.py make:
//  - sources read: a set, the pieces (WORK), a whole backup's objects; Mode and Transition the first's;
//  - a set made from three pieces of two sets, in a new order, renamed: its pieces the sources' byte for byte (only their
//    place and name new), each piece's cues with it under its new place, a piece left out without its cues, the
//    reversed order's cues reversed; the set's name, Mode, Transition, setlen; the sirens' knobs and Tune the first's;
//  - refused: 0 or 9 pieces, a name too wide, too long, with a space in front or a mark the FM-1 hasn't (the set's and
//    a piece's), pieces of two formats; a first source with no GLOB: none written, Mode and Transition left alone;
//  - read back by both codecs (dub1container.js and tools/dub1_container.py: the same dump), by the cue editor (no
//    problems), made the same by dub1_set.py make (byte for byte, a backup's set too), loaded by the simulator as the
//    FM-1 loads a set (its names, Mode, Transition, setlen, the cues piece by piece), and sent over the set protocol
//    (COMMIT's check: kept).
//   node tests/host/make_set_test.mjs     (from the repo root; needs build/host/amb_host: host/build_host.sh)
import { execFileSync } from "child_process";
import fs from "fs";
import path from "path";
import * as M from "../../web/dub1makeset.js";
import { parse, build, dump, bundle, name as fieldText } from "../../web/dub1container.js";
import { describe } from "../../web/dub1sets.js";
import { Edit, place, T } from "../../web/dub1cues.js";

const HOST = "build/host/amb_host", PY = process.env.PYTHON || "python3";
let failed = 0;
const ok = (c, what) => { console.log(`${what.padEnd(76)} ${c ? "ok" : "FAIL"}`); if (!c) failed++; };
const same = (a, b) => a.length === b.length && a.every((x, i) => x === b[i]);
const throws = (fn, code, check = () => true) => { try { fn(); return false; } catch (e) { return e.code === code && check(e); } };
if (!fs.existsSync(HOST)) { console.log("no build/host/amb_host: run host/build_host.sh"); process.exit(1); }
fs.mkdirSync("build/host", { recursive: true });
const tmp = fs.mkdtempSync("build/host/mk-");   // (short: the simulator's file arguments are 64 bytes)
const host = (script) => {
  const f = path.join(tmp, "s.amb");
  fs.writeFileSync(f, script);
  return execFileSync(HOST, [f, tmp], { encoding: "latin1", maxBuffer: 1 << 26 });
};
const py = (...a) => execFileSync(PY, a, { encoding: "utf8" });
const put = (name, b) => { const f = path.join(tmp, name); fs.writeFileSync(f, b); return f; };
const chunks = (b, id) => parse(b).chunks.filter((c) => c.id === id);
const pieceBody = (c) => JSON.stringify({ ...c, index: 0, name: 0, pings: Array.from(c.pings) });

// ---------------------------------------------------------------------------------------------------- sources
const winterB = new Uint8Array(fs.readFileSync("sets/winter-set.dub1")), fourB = new Uint8Array(fs.readFileSync("sets/four-to-five.dub1"));
const workB = new Uint8Array(fs.readFileSync("tests/host/work-1.5.0.dub1"));
const winter = M.readSource(winterB), four = M.readSource(fourB), work = M.readSource(workB);
ok(winter.name === "Winter Set" && winter.pieces.length === 5 && winter.mode === 2 && winter.trans === 2 &&
   winter.pieces.map((p) => p.cues ? p.cues.cues.length : 0).join() === "4,9,8,12,6", "the Winter Set: 5 pieces, Set mode, Mix, its cues");
ok(four.mode === 0 && four.trans === 0 && four.pieces.every((p) => !p.cues) && work.kind === 1 && work.pieces.length === 8,
   "4 to 5 a.m.: Endless, LP, no cues; the pieces (WORK): eight");
ok(M.P_SMODE === T.song[0] && T.params[M.P_SMODE].name === "Song mode" && M.MODES.join() === T.smodes.join(),
   "Mode's place in GLOB: the firmware's (Song mode, Endless / Song / Set)");
ok(throws(() => M.readSource(bundle([{ kind: 1, index: 0, bytes: workB }])), "file") && throws(() => M.readSource(new Uint8Array(64)), "file"),
   "a whole backup or not a .dub1: not a source (its objects are)");

// ------------------------------------------------------------------------------------- a set made: three pieces
const made = M.makeSet({ name: "Mix", mode: 1, trans: 1, pieces: [
  { src: winter, slot: 3, name: "Aurora 2" }, { src: four, slot: 1, name: "Pressure" }, { src: winter, slot: 0, name: "Hoarfrost" }] });
const mc = parse(made), mp = chunks(made, "PIEC"), mq = chunks(made, "CUES"), mg = chunks(made, "GLOB")[0], mm = chunks(made, "META")[0];
ok(mc.header.kind === 2 && mc.header.npieces === 3 && mc.header.format === 37 && mp.map((p) => p.index).join() === "0,1,2",
   "made: a set of 3 pieces, format 37, its pieces in places 1 to 3");
ok(mp.map((p) => fieldText(p.name)).join("|") === "Aurora 2|Pressure|Hoarfrost" && fieldText(mm.name) === "Mix" && mm.source === 1,
   "its names: the set's, each piece's as typed; META's source the web");
const srcPiec = (s, slot) => s.pieces.find((p) => p.slot === slot).chunk;
ok(pieceBody(mp[0]) === pieceBody(srcPiec(winter, 3)) && pieceBody(mp[1]) === pieceBody(srcPiec(four, 1)) &&
   pieceBody(mp[2]) === pieceBody(srcPiec(winter, 0)), "each piece its source's, value for value (its sound untouched)");
ok(mq.length === 2 && mq[0].index === 0 && mq[1].index === 2 &&
   JSON.stringify(mq[0].cues) === JSON.stringify(winter.pieces[3].cues.cues) &&
   JSON.stringify(mq[1].cues) === JSON.stringify(winter.pieces[0].cues.cues), "the cues with their pieces: Aurora's at 1, Hoarfrost's at 3, none at 2");
ok(mg.trans === 1 && mg.par[M.P_SMODE] === 1 && mg.setlen === 3 && JSON.stringify(mg.sir) === JSON.stringify(winter.glob.sir) &&
   mg.par[T.song[4] - 0] === winter.glob.par[T.song[4]] && mg.par.length === winter.glob.par.length,
   "GLOB: Mode Song, Transition Segue, setlen 3; the sirens' knobs and the rest the first's");
const ed = new Edit(made);
ok(!ed.problems.length && ed.pieces.map((p) => p.cues.length).join() === "12,0,4" && ed.mode === 1 && ed.trans === 1,
   "the cue editor reads it: no problems, 12, 0, 4 cues, Song, Segue");
const def = M.makeSet({ name: "Mix", pieces: [{ src: winter, slot: 1 }, { src: four, slot: 0 }] }), dg = chunks(def, "GLOB")[0];
ok(dg.trans === 2 && dg.par[M.P_SMODE] === 2 && dg.setlen === 2 && chunks(def, "PIEC")[0].name.every((x, i) => x === srcPiec(winter, 1).name[i]),
   "Mode and Transition not chosen: the first source's; a name not typed: its own");
const rev = M.makeSet({ name: "Reversed", pieces: [4, 3, 2, 1, 0].map((slot) => ({ src: winter, slot })) });
ok(new Edit(rev).pieces.map((p) => p.cues.length).join() === "6,12,8,9,4" && chunks(rev, "GLOB")[0].setlen === 5,
   "the Winter Set reversed: its cues reversed with their pieces");
const one = M.makeSet({ name: "Thaw", pieces: [{ src: winter, slot: 4 }] });
ok(chunks(one, "CUES").length === 1 && chunks(one, "CUES")[0].index === 0 && chunks(one, "CUES")[0].cues.length === 6,
   "Thaw alone: its 6 cues at place 1, the others' left out with their pieces");
const eight = M.makeSet({ name: "", pieces: work.pieces.map((p) => ({ src: work, slot: p.slot, name: "" })) });
ok(parse(eight).header.npieces === 8 && chunks(eight, "GLOB")[0].setlen === 0 && chunks(eight, "META")[0].name.every((x) => !x),
   "eight from the pieces now, no names: setlen 0 (all eight), empty names kept empty");

// --------------------------------------------------------------------------------------------------- refused
const p1 = [{ src: winter, slot: 0 }];
ok(throws(() => M.makeSet({ name: "x", pieces: [] }), "count") &&
   throws(() => M.makeSet({ name: "x", pieces: [...work.pieces.map((p) => ({ src: work, slot: p.slot })), ...p1] }), "count", (e) => e.n === 9),
   "0 pieces or 9: refused (a set takes 1 to 8)");
for (const bad of ["Woodward Dub", "ABCDEFGHIJKLM", " Lead", "Café", "a*b"]) {
  ok(throws(() => M.makeSet({ name: bad, pieces: p1 }), "name", (e) => e.which === -1) &&
     throws(() => M.makeSet({ name: "ok", pieces: [p1[0], { src: winter, slot: 1, name: bad }] }), "name", (e) => e.which === 1) &&
     !M.nameCheck(bad).ok, `a name the FM-1 wouldn't take refused (the set's, a piece's): "${bad}"`);
}
ok(M.nameCheck("Hoarfrost").ok && M.nameCheck("Hoarfrost").width > 0 && M.nameCheck("").ok && M.nameCheck("#2 (a/b),_!").ok,
   "names it takes: letters, marks, empty");
const old = (() => { const c = parse(fourB); c.header.format = 36; return M.readSource(build(c)); })();
ok(throws(() => M.makeSet({ name: "x", pieces: [p1[0], { src: old, slot: 0 }] }), "format", (e) => e.format === 36 && e.want === 37) &&
   old.mode === null && old.trans === 0, "pieces of two formats: refused; an older one's Mode not read (its place unknown)");
const noglob = M.readSource((() => { const c = parse(fourB); c.chunks = c.chunks.filter((x) => x.id !== "GLOB"); return build(c); })());
const ng = M.makeSet({ name: "Plain", pieces: [{ src: noglob, slot: 0 }, { src: noglob, slot: 2 }] });
ok(noglob.mode === null && noglob.trans === null && !chunks(ng, "GLOB").length && parse(ng).header.npieces === 2,
   "a source without a GLOB: none written (the FM-1 keeps its own Mode, Transition)");
const mixed = M.makeSet({ name: "M", pieces: [{ src: noglob, slot: 0 }, { src: winter, slot: 0 }] });
ok(chunks(mixed, "GLOB")[0].trans === 2, "the first with a GLOB gives it, when the first has none");
ok(M.makeFileName("Night (dub) #2") === "dub1-set-Night-dub-2.dub1" && M.makeFileName("") === "dub1-set-new.dub1", "its file name");

// ------------------------------------------------- both codecs, the Python twin, the simulator, the set protocol
const mf = put("mix.dub1", made);
ok(py("tools/dub1_container.py", "dump", mf) === dump(mc), "the made set's dump: the same in Python and JS");
const backup = put("bk.dub1", bundle([{ kind: 1, index: 0, bytes: workB }, { kind: 2, index: 2, bytes: winterB }], "DUB-1 1.5.0"));
const bd = describe(new Uint8Array(fs.readFileSync(backup)));
const fromBk = M.readSource(bd.objs[1].bytes), fromWork = M.readSource(bd.objs[0].bytes);
const jsTwin = M.makeSet({ name: "Mix", mode: 1, trans: 1, pieces: [{ src: fromBk, slot: 3, name: "Aurora 2" }, { src: four, slot: 1, name: "Pressure" },
  { src: fromBk, slot: 0, name: "Hoarfrost" }, { src: fromWork, slot: 5 }] });
const pyOut = path.join(tmp, "py.dub1");
py("tools/dub1_set.py", "make", pyOut, "--from", `${backup}@3:4`, "--from", "sets/four-to-five.dub1:2", "--from", `${backup}@3:1`,
   "--from", `${backup}@pieces:6`, "--name", "Mix", "--piece-names", "Aurora 2||Hoarfrost", "--mode", "song", "--transition", "segue");
ok(same(new Uint8Array(fs.readFileSync(pyOut)), jsTwin), "dub1_set.py make (a backup's set 3 and its pieces): the same bytes as the page");
py("tools/dub1_set.py", "make", pyOut, "--from", "sets/winter-set.dub1:5,4,3,2,1", "--name", "Reversed");
ok(same(new Uint8Array(fs.readFileSync(pyOut)), rev), "dub1_set.py make, the Winter Set reversed: the same bytes");
let refused = false;
try { execFileSync(PY, ["tools/dub1_set.py", "make", pyOut, "--from", "sets/winter-set.dub1:1", "--name", "Woodward Dub"], { stdio: "pipe" }); }
catch (e) { refused = e.status === 2 && /name/.test(String(e.stderr)); }
ok(refused, "dub1_set.py make: a name too wide refused (exit 2)");

const out = host(`setin ${mf}\nexpect setlen 3\nexpect trans 1\nexpect par${M.P_SMODE} 1\nexpectname 0 Aurora 2\nexpectname 1 Pressure\n` +
  "expectname 2 Hoarfrost\n" + [0, 1, 2].map((k) => `cuelist ${k}`).join("\n") + "\n");
const listed = [...out.matchAll(/cues of piece (\d): (\d+)\n((?: {4}.*\n)*)/g)].map((m) => m[3].trim().split("\n").filter(Boolean)
  .map((ln) => ln.trim().split(" ")));
const want = ed.pieces.map((p) => p.cues.map((q) => {
  const pl = place(q.pos);
  return ["imob"[pl.part], `${pl.bar}.${pl.step}`, T.ops[q.op], String(q.arg), String(q.val), String(q.len)];
}));
ok(/ok setin/.test(out) && !/FAIL/.test(out), "the simulator loads it as the FM-1 loads a set: its names, Mode Song, Segue, 3 pieces");
ok(JSON.stringify(listed) === JSON.stringify(want) && listed.map((l) => l.length).join() === "12,0,4", "its cues there, piece by piece: 12, 0, 4");
const wire = host(`wait 300\ntap PLAY\nwait 300\nsetsdelete\nsetsend 4 ${mf}\nwait 100\nsetstate asking\ntap GLO\nwait 100\nsetstate yes\nsethas 4 Mix\n`);
ok(!/FAIL/.test(wire), "sent over the set protocol: COMMIT's check passes, kept as set 5");
fs.rmSync(tmp, { recursive: true, force: true });
console.log(failed ? `make a set: ${failed} FAILED` : "make a set: all ok");
process.exit(failed ? 1 : 0);
