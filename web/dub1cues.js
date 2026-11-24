// SPDX-License-Identifier: GPL-3.0-only
// (1.5.0, step 7b) The Sets page's cue editor, its model: a set's (or the pieces') cue lists read from a .dub1, edited
// (added, changed, deleted, kept sorted by place) and written back, every cue checked exactly as the FM-1 checks it
// (firmware/src/app/project.c's cue_ok, with the firmware's own tables: dub1cuetables.js) and every list as the codec
// does (container.c: 1..64 cues, sorted by place; one list a piece, for a piece the file has), so a set this page
// accepts is never refused by the device. No DOM here: the page draws (web/dub1_sets.html), the tests drive this
// (tests/host/cue_editor_test.mjs).
//   A cue: { pos, op, arg, val, len } as the CUES chunk has it (container.c; STORAGE-1.5.0.md 2.4): pos part << 14 |
//   bar << 4 | step (part 0 intro, 1 main, 2 outro, 3 the bar from PLAY; bar 0..1023 in that part; step 0..15, the
//   16th in the bar); op app.h's CUE_*; arg its target; val its value; len in 16ths (a hold's length, a ride's glide).
import { parse, build, Refused } from "./dub1container.js";
import { T } from "./dub1cuetables.js";

export { T };
export const MUTE = 0, SOLO = 1, MOVE = 2, FX = 3, SIREN = 4, SIRK = 5, MACRO = 6, KNOB = 7, OUTRO = 8, NEXT = 9,
  LOCK = 10, HOLD = 11, NOPS = 12;
export const INTRO = 0, MAIN = 1, OUTRO_P = 2, BAR = 3;
export const MAXBAR = 1023, NSTEP = 16, MAXLEN = 65535, MAXCUE = T.maxcue;
export const FRAME_MAX = 16128;                      // (a set's bytes at most: the FM-1's frame; WHY 4)
const [P_SMODE, P_SINTRO, P_SMAIN, P_SOUTRO] = T.song;
// what each op takes: the value ("bool" 0 / 1, "range" its target's, "none" 0) and the length ("none" 0, "held" >= 1,
// "any" 0 or more: a siren's 0 a tap, a ride's 0 at once)
export const OPS = [
  { op: MUTE, key: "mute", val: "bool", len: "none" }, { op: SOLO, key: "solo", val: "bool", len: "none" },
  { op: MOVE, key: "move", val: "none", len: "held" }, { op: FX, key: "fx", val: "none", len: "held" },
  { op: SIREN, key: "siren", val: "none", len: "any" }, { op: SIRK, key: "sirk", val: "range", len: "any" },
  { op: MACRO, key: "macro", val: "range", len: "any" }, { op: KNOB, key: "knob", val: "range", len: "any" },
  { op: OUTRO, key: "outro", val: "none", len: "none", noarg: true }, { op: NEXT, key: "next", val: "none", len: "none", noarg: true },
  { op: LOCK, key: "lock", val: "bool", len: "none", noarg: true }, { op: HOLD, key: "hold", val: "bool", len: "none", noarg: true },
];
if (T.ops.length !== NOPS || OPS.some((o, i) => T.ops[i] !== o.key)) throw new Error("dub1cues.js: the ops are not the firmware's");

export const pos = (part, bar, step) => ((part & 3) << 14) | ((bar & 1023) << 4) | (step & 15);
export const place = (p) => ({ part: p >>> 14, bar: (p >>> 4) & 1023, step: p & 15 });
const int = (v, lo, hi) => Number.isInteger(v) && v >= lo && v <= hi;

// the targets an op takes: [{ arg, name, group? }] (a knob's: each parameter a cue may ride, by its first page)
export function targets(op) {
  switch (op) {
    case MUTE: case SOLO: return T.parts.map((n, i) => ({ arg: i, name: n }));
    case MOVE: return T.moves.map((m, i) => ({ arg: i, name: m[0] }));
    case FX: return T.fx.map((n, i) => ({ arg: i, name: n }));
    case SIREN: return T.sirens.map((s, i) => ({ arg: i, name: s.name }));
    case SIRK: return T.sirens.flatMap((s, i) => s.knobs.map((k, j) => ({ arg: i * T.nsk + j, name: `${s.name} ${k.name}`, group: s.name })));
    case MACRO: return T.macros.map((m, i) => ({ arg: i, name: m.name }));
    case KNOB: return T.params.filter((p) => p.cue).map((p) => ({ arg: p.i, name: p.at.length ? p.at[0][1] : p.name,
      group: p.at.length ? p.at[0][0] : "" }));
    default: return [{ arg: 0, name: "" }];
  }
}
// a ride's range and the screen's text for a value: { lo, hi, def, text(v) } (null: the op takes no range)
export function range(op, arg) {
  let r = null;
  if (op === SIRK && arg < T.nsir * T.nsk) r = T.sirens[Math.floor(arg / T.nsk)].knobs[arg % T.nsk];
  else if (op === MACRO && arg < T.nmac) r = { ...T.macros[arg], def: 0 };
  else if (op === KNOB && arg < T.npar) r = T.params[arg];
  if (!r) return null;
  return { lo: r.lo, hi: r.hi, def: r.def, text: (v) => (r.text && v >= r.lo && v <= r.hi ? r.text[v - r.lo] : String(v)) };
}

// cue q as the FM-1 takes it (project.c's cue_ok, word for word): null, or why not (a key the page words)
export function cueWhy(q) {
  if (!q || !int(q.pos, 0, 65535) || !int(q.op, 0, 255) || !int(q.arg, 0, 255) || !int(q.val, -32768, 32767) ||
      !int(q.len, 0, MAXLEN)) return "shape";
  const v = q.val, a = q.arg, len = q.len;
  switch (q.op) {
    case MUTE: case SOLO:
      return a >= T.npart ? "arg" : v !== 0 && v !== 1 ? "val" : len ? "len0" : null;
    case MOVE: return a >= T.nmove ? "arg" : v ? "val" : !len ? "len1" : null;
    case FX: return a >= T.nfx ? "arg" : v ? "val" : !len ? "len1" : null;
    case SIREN: return a >= T.nsir ? "arg" : v ? "val" : null;
    case SIRK: case MACRO: case KNOB: {
      if (q.op === KNOB && a < T.npar && !T.params[a].cue) return "global";
      const r = range(q.op, a);
      return !r ? "arg" : v < r.lo || v > r.hi ? "val" : null;
    }
    case OUTRO: case NEXT: return a ? "arg" : v ? "val" : len ? "len0" : null;
    case LOCK: case HOLD: return a ? "arg" : v !== 0 && v !== 1 ? "val" : len ? "len0" : null;
    default: return "op";
  }
}
export const cueOk = (q) => cueWhy(q) === null;
// a cue made whole for its op (the page's form: a field the op doesn't take is 0; a ride's value in its range)
export function normal(q) {
  const o = OPS[q.op], c = { pos: q.pos, op: q.op, arg: o && o.noarg ? 0 : q.arg | 0, val: q.val | 0, len: q.len | 0 };
  if (!o) return c;
  if (o.val === "none") c.val = 0;
  if (o.val === "bool") c.val = c.val ? 1 : 0;
  if (o.len === "none") c.len = 0;
  if (o.len === "held" && c.len < 1) c.len = 1;
  return c;
}
// sorted by place, cues at the same place in the order given (as they fire): a new array
export const sorted = (list) => list.map((q, i) => [q, i]).sort((x, y) => x[0].pos - y[0].pos || x[1] - y[1]).map((x) => x[0]);
// a list as the FM-1 takes it: [] or the problems [{ i (-1: the list), why }]
export function listWhy(list) {
  const out = [];
  if (list.length > MAXCUE) out.push({ i: -1, why: "count" });
  list.forEach((q, i) => {
    const w = cueWhy(q);
    if (w) out.push({ i, why: w });
    else if (i && q.pos < list[i - 1].pos) out.push({ i, why: "order" });
  });
  return out;
}

// a piece's song, in bars, as its file has it (FULL: its own values; RECIPE or DELTA: today's recipe, its changes):
// { intro, main, outro } (Intro 0: None)
function songOf(pc) {
  const v = [...T.recipes[pc.recipe] ? T.recipes[pc.recipe].song : [0, 0, 0]];
  if (pc.kind === 0) {
    if (pc.par.length > P_SOUTRO) [v[0], v[1], v[2]] = [pc.par[P_SINTRO], pc.par[P_SMAIN], pc.par[P_SOUTRO]];
  } else if (pc.kind === 2) {
    for (const [i, x] of pc.par) if (i >= P_SINTRO && i <= P_SOUTRO) v[i - P_SINTRO] = x;
  }
  return { intro: v[0] * 8, main: v[1] * 8, outro: v[2] * 8 };
}
const text = (f) => { let s = ""; for (const c of f) { if (!c) break; s += String.fromCharCode(c); } return s; };

// where cue q falls in its piece's song, as the player places it (cues.c's cue_when): { at (the bar from PLAY, or
// null: never heard), why (null, or why it's left out: "nointro", "pastintro", "pastmain", "pastoutro", "pastend",
// "endless", "handover") }. smode: 0 Endless, 1 Song, 2 Set; handover: a Set's Mix or Segue to a next piece (its
// outro's last bar is the next piece's: Decisions 7, 5)
export function where(q, song, smode, handover = false) {
  const { part, bar, step } = place(q.pos), b = bar + step / 16;
  const endless = smode === 0, end = endless ? Infinity : song.intro + song.main + song.outro;
  let at, why = null;
  if (part === INTRO) {
    at = b;
    if (endless || !song.intro) why = endless ? "endless" : "nointro";
    else if (bar >= song.intro) why = "pastintro";
  } else if (part === MAIN) {
    at = (endless ? 0 : song.intro) + b;
    if (!endless && bar >= song.main) why = "pastmain";
  } else if (part === OUTRO_P) {
    at = song.intro + song.main + b;
    if (endless) why = "endless";
    else if (bar >= song.outro) why = "pastoutro";
  } else {
    at = b;
    if (bar >= end) why = "pastend";
  }
  if (!why && !endless && handover && at >= end - 1) why = "handover";
  return { at: why ? null : at, why };
}

// a .dub1's cues to edit: Edit(bytes) (a SET or a WORK: the pieces; a whole backup: choose one of its objects first).
// Throws Refused (not a container), or Error with .code "kind" / "older" / "newer" (cues only in today's format)
export class Edit {
  constructor(bytes) {
    this.orig = bytes instanceof Uint8Array ? bytes : new Uint8Array(bytes);
    this.c = parse(this.orig);
    const h = this.c.header;
    const err = (code, m) => Object.assign(new Error(m), { code });
    if (h.kind !== 1 && h.kind !== 2) throw err("kind", `not a set or the pieces (kind ${h.kind})`);
    if (h.format < T.format) throw err("older", `made by an older DUB-1 (format ${h.format}): cues need format ${T.format}`);
    if (h.format > T.format) throw err("newer", `made for a newer DUB-1 (format ${h.format}, this page's ${T.format})`);
    const meta = this.c.chunks.find((ch) => ch.id === "META"), glob = this.c.chunks.find((ch) => ch.id === "GLOB");
    this.kind = h.kind;
    this.name = meta ? text(meta.name) : "";
    this.smode = glob && glob.par.length > P_SMODE ? glob.par[P_SMODE] : null;   // (null: the FM-1's own Song mode)
    this.trans = glob ? glob.trans : null;
    this.setlen = glob ? glob.setlen : 0;
    this.pieces = this.c.chunks.filter((ch) => ch.id === "PIEC").sort((x, y) => x.index - y.index).map((pc) => ({
      slot: pc.index, name: text(pc.name), recipe: pc.recipe,
      recipeName: T.recipes[pc.recipe] ? T.recipes[pc.recipe].name : `recipe ${pc.recipe + 1}`, song: songOf(pc),
      cues: [] }));
    this.problems = [];                              // (what the device would refuse in the file as it came)
    for (const ch of this.c.chunks) {
      if (ch.id !== "CUES") continue;
      const p = this.pieces.find((x) => x.slot === ch.index);
      if (!p || p.loaded) { this.problems.push({ slot: ch.index, i: -1, why: !p ? "noslot" : "twice" }); continue; }
      if (ch.flags) this.problems.push({ slot: ch.index, i: -1, why: "flags" });
      p.loaded = true;
      p.cues = ch.cues.map(([ps, op, arg, val, len]) => ({ pos: ps, op, arg, val, len }));
      for (const w of listWhy(p.cues)) this.problems.push({ slot: p.slot, ...w });
    }
    this.dirty = false;
  }
  piece(slot) { const p = this.pieces.find((x) => x.slot === slot); if (!p) throw new Error(`no piece ${slot + 1}`); return p; }
  // the Song mode the cues are placed with (the file's, else Set when it has fewer than eight, else Song)
  get mode() { return this.smode !== null ? this.smode : this.pieces.length < 8 ? 2 : 1; }
  handover(slot) {                                   // (Set mode, a Mix or Segue, a next piece after this one)
    const last = this.setlen || this.pieces.length;
    return this.mode === 2 && !!this.trans && slot + 1 < last;
  }
  where(slot, q) { const p = this.piece(slot); return where(q, p.song, this.mode, this.handover(slot)); }
  // add cue q to piece slot (kept sorted: after any at the same place): its index. Throws Error .code "full" / the why
  add(slot, q) {
    const p = this.piece(slot), c = normal(q), w = cueWhy(c);
    if (p.cues.length >= MAXCUE) throw Object.assign(new Error(`a piece has ${MAXCUE} cues at most`), { code: "full" });
    if (w) throw Object.assign(new Error(`not a cue the FM-1 takes (${w})`), { code: w });
    let i = p.cues.length;
    while (i > 0 && p.cues[i - 1].pos > c.pos) i--;
    p.cues.splice(i, 0, c);
    this.dirty = true;
    return i;
  }
  // cue i of piece slot changed to q (moved to its place): its new index
  change(slot, i, q) {
    const p = this.piece(slot), c = normal(q), w = cueWhy(c);
    if (!p.cues[i]) throw new Error(`no cue ${i + 1}`);
    if (w) throw Object.assign(new Error(`not a cue the FM-1 takes (${w})`), { code: w });
    p.cues.splice(i, 1);
    let k = p.cues.length;
    while (k > 0 && p.cues[k - 1].pos > c.pos) k--;
    p.cues.splice(k, 0, c);
    this.dirty = true;
    return k;
  }
  remove(slot, i) {
    const p = this.piece(slot);
    if (!p.cues[i]) throw new Error(`no cue ${i + 1}`);
    p.cues.splice(i, 1);
    this.dirty = true;
  }
  // every problem the FM-1 would refuse the file, as this page writes it, for: [] when it takes it. (A file that came
  // with a list for a piece it doesn't have, a second list for one, or a list's flags not 0: this.problems; written
  // again here, those lists are left out and the flags 0, so the rest is what's checked)
  check() {
    const out = [];
    for (const p of this.pieces) for (const w of listWhy(p.cues)) out.push({ slot: p.slot, ...w });
    if (!out.length && this.bytes(true).length > FRAME_MAX) out.push({ slot: -1, i: -1, why: "size" });
    return out;
  }
  // the file with its cues as they are now (unchanged: its own bytes): CUES after the pieces, a chunk a piece that has
  // cues, in slot order (as the firmware writes them); anything else in the file as it was. Throws Refused when the
  // FM-1 would refuse it
  bytes(unchecked = false) {
    if (!this.dirty && !this.problems.length) return this.orig;
    if (!unchecked) {
      const bad = this.check();
      if (bad.length) throw new Refused(`cues the FM-1 would refuse: ${bad.map((b) => `piece ${b.slot + 1}${b.i >= 0 ? " cue " + (b.i + 1) : ""} (${b.why})`).join(", ")}`);
    }
    const rest = this.c.chunks.filter((ch) => ch.id !== "CUES");
    let at = 0;
    rest.forEach((ch, k) => { if (ch.id === "PIEC") at = k + 1; });
    const cues = this.pieces.filter((p) => p.cues.length).map((p) => ({ id: "CUES", index: p.slot, flags: 0,
      cues: p.cues.map((q) => [q.pos, q.op, q.arg, q.val, q.len]) }));
    return build({ header: this.c.header, chunks: [...rest.slice(0, at), ...cues, ...rest.slice(at)] });
  }
}
