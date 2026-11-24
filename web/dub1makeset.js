// SPDX-License-Identifier: GPL-3.0-only
// (1.5.0) The Sets page's "Make a set", its model: a new set put together from the pieces of other ones (the pieces now
// read from the FM-1, a set stored there, a .dub1 file, a ready-made set), which of them go in, their order, their
// names and the set's own name, Mode and Transition. No DOM here: the page draws (web/dub1_sets.html), the tests drive
// this (tests/host/make_set_test.mjs); tools/dub1_set.py's "make" is its twin (the same bytes).
//   What is written: a SET container as the firmware reads one (project.c's proj_decode): the header (the first
//   source's format; kind SET; npieces n), META (the first source's, its name the set's, source 1: the web), GLOB (the
//   first source's that has one: the sirens' knobs, Tune; its Transition and Mode as chosen; setlen n, 0 for eight),
//   each piece's PIEC as it came (its sound untouched) with its index its new place (0 .. n-1) and its name as typed,
//   then each piece's CUES as it came, its index its piece's new place (a piece left out takes its cues with it). A
//   chunk the page doesn't know is left out (none today). The format is not changed: pieces of one format only.
import { parse, build, nameField, nameFits, name as fieldText } from "./dub1container.js";
import { T } from "./dub1cuetables.js";

export const WORK = 1, SET = 2, NPIECES = 8, FRAME_MAX = 16128, WEB = 1;
export const MODES = ["Endless", "Song", "Set"], TRANS = ["LP", "Segue", "Mix"];
export const P_SMODE = T.song[0];                    // (GLOB's par: the set's Mode, in today's format)
export const FORMAT = T.format;

// what went wrong: code "file" (not a set or the pieces), "count" (n: not 1..8), "name" (which: -1 the set's, else the
// piece's place; text), "format" (format, want), "size" (bytes)
export class MakeError extends Error {
  constructor(code, msg, extra = {}) { super(msg); this.code = code; Object.assign(this, extra); }
}

// a container's bytes (a set, or the pieces: WORK) as a source: { format, name, meta, glob (or null), flags, mode,
// trans (null: not in it), pieces: [{ chunk (its PIEC), cues (its CUES or null), slot, name, recipe }] }. A whole
// backup: its objects are sources each (the page picks one). Throws MakeError("file")
export function readSource(bytes) {
  let c;
  try { c = parse(bytes); } catch (e) { throw new MakeError("file", `Not a .dub1 file DUB-1 reads (${e.message}).`); }
  const h = c.header;
  if (h.kind !== WORK && h.kind !== SET) throw new MakeError("file", `Not a set or the pieces (kind ${h.kind}).`);
  const meta = c.chunks.find((ch) => ch.id === "META") || null, glob = c.chunks.find((ch) => ch.id === "GLOB") || null;
  const pieces = c.chunks.filter((ch) => ch.id === "PIEC").sort((a, b) => a.index - b.index).map((pc) => ({
    chunk: pc, cues: c.chunks.find((ch) => ch.id === "CUES" && ch.index === pc.index) || null, slot: pc.index,
    name: fieldText(pc.name), recipe: pc.recipe }));
  const mine = glob && h.format === FORMAT && glob.par.length > P_SMODE;
  return { format: h.format, kind: h.kind, name: meta ? fieldText(meta.name) : "", meta, glob, flags: h.flags, pieces,
    mode: mine ? glob.par[P_SMODE] : null, trans: glob ? glob.trans : null, modeHere: !!mine };
}

// a name typed as the FM-1 takes it (names.h): { ok, width (px), field } (an empty name: ok, width 0)
export function nameCheck(text) {
  if (!text) return { ok: true, width: 0, field: new Uint8Array(16) };
  try { const f = nameField(text); return { ok: true, width: nameFits(f), field: f }; } catch (_) {
    return { ok: false, width: -1, field: null };
  }
}

// the base a set takes its own settings from: the first source with a GLOB (else the first)
export function baseOf(sources) { return sources.find((s) => s.glob) || sources[0] || null; }

// the new set: { name, pieces: [{ src (a source), slot (its piece's slot there), name }] in order, mode, trans (null:
// as the base has it), base (default baseOf the pieces' sources) } -> its bytes (a SET container). Throws MakeError
export function makeSet({ name = "", pieces, mode = null, trans = null, base = null }) {
  const n = pieces.length;
  if (n < 1 || n > NPIECES) throw new MakeError("count", `A set has 1 to 8 pieces (${n} chosen).`, { n });
  const b = base || baseOf(pieces.map((p) => p.src));
  const first = pieces[0].src;
  const fmt = first.format;
  for (const p of pieces) {
    if (p.src.format !== fmt) throw new MakeError("format", `Pieces made by two DUB-1 versions (formats ${fmt} and ` +
      `${p.src.format}): a set takes pieces of one.`, { format: p.src.format, want: fmt });
  }
  const setName = nameCheck(name);
  if (!setName.ok) throw new MakeError("name", `The set's name "${name}" is not one the FM-1 takes.`, { which: -1, text: name });
  const glob0 = b && b.glob && b.format === fmt ? b.glob : (pieces.map((p) => p.src).find((s) => s.glob) || {}).glob;
  const chunks = [];
  const meta0 = (b && b.meta) || first.meta;
  chunks.push(meta0 ? { ...meta0, name: setName.field, source: WEB } :
    { id: "META", name: setName.field, fw: new Uint8Array(16), source: WEB, migrated: 0, saves: 0 });
  if (glob0) {
    const g = { ...glob0, par: glob0.par.slice(), sir: glob0.sir.slice(), setlen: n < NPIECES ? n : 0 };
    if (trans !== null) {
      if (!(trans >= 0 && trans < TRANS.length)) throw new MakeError("trans", `No such Transition (${trans}).`);
      g.trans = trans;
    }
    if (mode !== null) {
      if (!(mode >= 0 && mode < MODES.length)) throw new MakeError("mode", `No such Mode (${mode}).`);
      if (fmt !== FORMAT || g.par.length <= P_SMODE) throw new MakeError("mode", `Mode can't be changed in a set of format ${fmt}.`);
      g.par[P_SMODE] = mode;
    }
    chunks.push(g);
  }
  const cues = [];
  pieces.forEach((p, k) => {
    const s = p.src.pieces.find((x) => x.slot === p.slot);
    if (!s) throw new MakeError("file", `No piece ${p.slot + 1} in that source.`);
    const nm = nameCheck(p.name === undefined ? s.name : p.name);
    if (!nm.ok) throw new MakeError("name", `The name "${p.name}" (piece ${k + 1}) is not one the FM-1 takes.`, { which: k, text: p.name });
    chunks.push({ ...s.chunk, index: k, name: nm.field });
    if (s.cues) cues.push({ ...s.cues, index: k });
  });
  chunks.push(...cues);
  const out = build({ header: { format: fmt, kind: SET, npieces: n, flags: first.flags & 1, rsv: [0, 0] }, chunks });
  if (out.length > FRAME_MAX) throw new MakeError("size", `Too big for the FM-1 (${out.length} bytes; 16,128 at most).`, { bytes: out.length });
  parse(out);                                        // (it reads: the shape strict)
  return out;
}

// a file name for it: "dub1-set-Night-Pieces.dub1"
export function makeFileName(name) {
  const safe = (name || "").replace(/[^A-Za-z0-9]+/g, "-").replace(/^-|-$/g, "");
  return `dub1-set-${safe || "new"}.dub1`;
}
