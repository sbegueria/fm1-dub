// SPDX-License-Identifier: GPL-3.0-only
// DUB-1's sets on an FM-1, over Web MIDI (1.5.0, step 5; STORAGE-1.5.0.md 6.3): the set protocol of
// firmware/src/app/setwire.c, for the Sets page (web/dub1_sets.html) and the emulator. The JS twin of
// tools/dub1_set.py's Device and as_set: list, read (WORK made on the fly, a set as stored), a whole backup in one file
// (WORK and every set: dub1container.js's bundle), send a set (BEGIN, DATA, COMMIT), load one (APPLY), delete one,
// restore a whole backup (Decisions 4: RESTORE, one question, then its sets and pieces kept as they come), and the
// FM-1's question waited for (STATUS polled). No DOM here: the page draws, this talks (tests/host/dub1_sets_test.mjs
// drives it against the host simulator, as dub1_set_test.py the Python tool).
//
// A frame: F0 7D 44 31 cmd seq pack7(body) chk F7 (pack7 ota.c's 8-to-7, LSB first; chk the sum of the bytes from cmd
// to the last packed one, & 0x7F). One request at a time, each answered: the same cmd (an ACK), BUSY (7E: cmd, why,
// words: 1 playing "stop first", 2 asking "answer on the FM-1") or NAK (7F: cmd, why, u16 detail). An answer lost or
// garbled: sent again (2 s, 3 tries). While the FM-1 plays, LIST and READ are answered (Decisions 3), the rest BUSY.
import { parse, build, crc32, Refused, nameField, nameFits, name as fieldText, bundle, unbundle, BACKUP } from "./dub1container.js";

export const HELLO = 0x01, LIST = 0x02, STATUS = 0x03, READ = 0x10, BEGIN = 0x20, DATA = 0x21, COMMIT = 0x22,
  CANCEL = 0x24, APPLY = 0x30, DELETE = 0x31, RESTORE = 0x32, BUSY = 0x7e, NAK = 0x7f;
export const WORK = 1, SET = 2, NSETS = 8, CHUNK_IN = 512, CHUNK_OUT = 256, ASK_S = 30;
export const S_IDLE = 0, S_ASKING = 1, S_YES = 2, S_NO = 3, S_TIMEOUT = 4, S_FAILED = 5, S_PLAYING = 6;
export const STATE_TEXT = {
  [S_IDLE]: "nothing asked", [S_ASKING]: "asking", [S_YES]: "done", [S_NO]: "answered no on the FM-1",
  [S_TIMEOUT]: "not answered on the FM-1 in 30 seconds", [S_FAILED]: "the FM-1's flash failed",
  [S_PLAYING]: "not answered: the music started",
};
// NAK's why, as the page says it
export const WHY_TEXT = {
  1: "the FM-1 did not take the request (a bad request, or the data changed on the way: try again)",
  2: "this DUB-1 does not know that request: update it",
  3: "there is no such set on the FM-1",
  4: "too big for the FM-1 (16,128 bytes at most)",
  5: "made for a newer DUB-1: update the FM-1 first",
  6: "not a set this DUB-1 takes (a value it doesn't know, or a damaged file)",
  7: "no room left on the FM-1",
  8: "the FM-1's flash failed",
  9: "out of step: send it again",
  10: "nothing was being sent",
};
export const BUSY_TEXT = { 1: "The FM-1 is playing: stop it first (PLAY), then try again.",
  2: "The FM-1 is asking something: answer it there first." };
export const RECIPES = ["Basic", "Chamber", "Steppers", "Fog", "Concrete", "Night Drive", "Pressure", "Haze"];
export const DELAY = { reply: 2000, tries: 3, poll: 250, ask: 35000 };   // ms (the tests shorten them)
export const PORT_RE = /fm-1|felucca|ota|composite|sinco|usb-midi/i;  // (fm1ota.js's: never probe other gear)

// what went wrong: code "busy" (why 1 playing, 2 asking), "refused" (why, detail: NAK), "lost", "notfound", "file",
// "no" (the FM-1 answered no, or not), "denied" (no SysEx allowed)
export class SetError extends Error {
  constructor(code, msg, why = 0, detail = 0) { super(msg); this.code = code; this.why = why; this.detail = detail; }
}

export function pack7(data) {
  const out = []; let acc = 0, nb = 0;
  for (const b of data) {
    acc |= b << nb; nb += 8;
    while (nb >= 7) { out.push(acc & 0x7f); acc >>>= 7; nb -= 7; }
  }
  if (nb) out.push(acc & 0x7f);
  return out;
}
export function unpack7(s) {
  const out = []; let acc = 0, nb = 0;
  for (const b of s) {
    acc |= (b & 0x7f) << nb; nb += 7;
    if (nb >= 8) { out.push(acc & 0xff); acc >>>= 8; nb -= 8; }
  }
  return out;
}
export function sxFrame(cmd, seq, body = []) {
  const f = [0xf0, 0x7d, 0x44, 0x31, cmd & 0x7f, seq & 0x7f, ...pack7(body)];
  let s = 0;
  for (let i = 4; i < f.length; i++) s += f[i];
  f.push(s & 0x7f, 0xf7);
  return f;
}
// an answer: { cmd, seq, body (Uint8Array) }, or null (someone else's, or garbled)
export function sxParse(p) {
  if (!p || p.length < 8 || p[0] !== 0xf0 || p[p.length - 1] !== 0xf7 || p[1] !== 0x7d || p[2] !== 0x44 || p[3] !== 0x31)
    return null;
  let s = 0;
  for (let i = 4; i < p.length - 2; i++) s += p[i];
  if ((s & 0x7f) !== p[p.length - 2]) return null;
  return { cmd: p[4], seq: p[5], body: Uint8Array.from(unpack7(Array.from(p).slice(6, -2))) };
}
const u16 = (b, at) => b[at] | (b[at + 1] << 8);
const text = (b) => fieldText(b);

// one Web MIDI in/out pair, the set protocol's frames queued (anything else left: notes, the screen reader's)
export class MidiLink {
  constructor(input, output) {
    this.input = input; this.output = output; this.q = []; this.waiter = null; this.name = input.name;
    this.onmsg = (ev) => {
      const d = ev.data;
      if (d[0] !== 0xf0 || d[1] !== 0x7d || d[2] !== 0x44 || d[3] !== 0x31) return;
      this.q.push(Array.from(d));
      if (this.waiter) { const w = this.waiter; this.waiter = null; w(); }
    };
    input.addEventListener ? input.addEventListener("midimessage", this.onmsg) : (input.onmidimessage = this.onmsg);
  }
  get lost() { return this.input.state === "disconnected" || this.output.state === "disconnected"; }
  send(bytes) { try { this.output.send(bytes); return true; } catch (_) { return false; } }
  drain() { this.q.length = 0; }
  async read(ms) {
    if (!this.q.length) {
      let tm;
      await Promise.race([new Promise((r) => { this.waiter = r; }), new Promise((r) => { tm = setTimeout(r, ms); })]);
      clearTimeout(tm);
      this.waiter = null;
    }
    return this.q.shift() || null;
  }
  close() {
    if (this.input.removeEventListener) this.input.removeEventListener("midimessage", this.onmsg);
    else this.input.onmidimessage = null;
  }
}

// (Decisions 5) HELLO's version string ("DUB-1 1.5.0-BETA1", EN_VERSION after "DUB-1 ") read: { text "1.5.0-BETA1",
// release "1.5.0", pre "BETA1", test (a suffix: a test build, not the release; Decisions 6: whatever the suffix says, a
// beta or a release candidate, the page calls it a test build), dev (a development build, "DEV") }, or null (not
// DUB-1's). The update protocol's identity (FM-1_8XXYYZZ) can't tell them apart
export function versionOf(s) {
  const m = /^DUB-1 (?:(\d+\.\d+\.\d+)(?:-([A-Za-z0-9.-]+))?|(DEV))$/.exec(s || "");
  if (!m) return null;
  return m[3] ? { text: "DEV", release: "", pre: "", test: false, dev: true }
    : { text: m[1] + (m[2] ? "-" + m[2] : ""), release: m[1], pre: m[2] || "", test: !!m[2], dev: false };
}

// the FM-1 among the MIDI ports (fm1ota.js's names): the first pair whose device answers HELLO as a DUB-1 1.5
export async function findDevice(access) {
  const outs = [...access.outputs.values()], ins = [...access.inputs.values()];
  for (const input of ins) {
    if (input.state === "disconnected" || !PORT_RE.test(input.name || "")) continue;
    const output = outs.find((o) => o.name === input.name) || (outs.length === 1 ? outs[0] : null);
    if (!output) continue;
    try { if (input.open) await input.open(); if (output.open) await output.open(); } catch (_) { continue; }
    const dev = new Device(new MidiLink(input, output));
    try {
      dev.info = await dev.hello(1);
      return dev;
    } catch (_) { dev.link.close(); }
  }
  throw new SetError("notfound", "No DUB-1 1.5 answered. Check the USB cable (a data cable), close other apps that use " +
    "MIDI, and that the FM-1 runs DUB-1 1.5 or later." + (ins.length ? ` MIDI inputs: ${ins.map((i) => i.name).join(", ")}.` : ""));
}

// the set protocol over a link ({ send(bytes) -> bool, read(ms) -> Promise<bytes|null>, drain(), lost })
export class Device {
  constructor(link) { this.link = link; this.seq = 0; this.info = null; }

  // one request, its answer { cmd, body }: sent again when unanswered (a frame lost or garbled either way)
  async request(cmd, body = [], tries = DELAY.tries) {
    this.seq = (this.seq + 1) & 0x7f;
    const pkt = sxFrame(cmd, this.seq, body);
    for (let t = 0; t < tries; t++) {
      this.link.drain();
      if (!this.link.send(pkt)) throw new SetError("lost", "The FM-1 was disconnected.");
      const end = Date.now() + DELAY.reply;
      for (let left; (left = end - Date.now()) > 0;) {
        const a = sxParse(await this.link.read(left));
        if (!a) { if (Date.now() >= end) break; continue; }
        if (a.seq === this.seq && (a.cmd === cmd || a.cmd === BUSY || a.cmd === NAK)) return a;
      }
      if (this.link.lost) throw new SetError("lost", "The FM-1 was disconnected.");
    }
    throw new SetError("lost", "The FM-1 doesn't answer. Is it a DUB-1 1.5, still connected?");
  }
  refused(a, what = "") {
    if (a.cmd === BUSY) return new SetError("busy", BUSY_TEXT[a.body[1]] || "The FM-1 is busy.", a.body[1]);
    if (a.cmd === NAK) {
      const why = a.body[1], detail = u16(a.body, 2);
      return new SetError("refused", (what ? what + ": " : "") + (WHY_TEXT[why] || `refused (${why})`) +
        (why === 5 ? ` (its format ${detail}, this DUB-1's ${this.info ? this.info.format : "?"})` : ""), why, detail);
    }
    return new SetError("lost", "An answer out of step.");
  }
  // a request that must be ACKed: its body; BUSY / NAK thrown
  async ok(cmd, body = [], what = "") {
    const a = await this.request(cmd, body);
    if (a.cmd !== cmd) throw this.refused(a, what);
    return a.body;
  }

  async hello(tries) {
    const a = await this.request(HELLO, [], tries);
    const b = a.body;
    if (a.cmd !== HELLO || b.length < 32 || b[0] !== 1) throw new SetError("notfound", "Not a DUB-1 1.5 set protocol.");
    return this.info = { proto: b[0], chunkIn: u16(b, 1), chunkOut: u16(b, 3), format: u16(b, 5), max: u16(b, 9),
      sets: b[11], playing: !!(b[12] & 1), asking: !!(b[12] & 2), restoring: !!(b[12] & 8),
      version: text(b.subarray(16, 48)) };   // (the version 16 bytes before Decisions 4's, 32 since)
  }

  // [{ kind, index, length, pieces, flags, name }]: WORK (the pieces as they are now: live) first, then each set stored
  async list() {
    const b = await this.ok(LIST);
    const out = [];
    for (let i = 0; i < b[0]; i++) {
      const e = b.subarray(1 + 22 * i, 23 + 22 * i);
      out.push({ kind: e[0], index: e[1], length: u16(e, 2), pieces: e[4], flags: e[5], name: text(e.subarray(6, 22)) });
    }
    return out;
  }

  // WORK as it is now (kind 1) or a stored set (2, index 0..7): its bytes, checked (a container). WORK is made at each
  // chunk: a knob turned, or a set's next piece, between chunks while the music plays fails the whole's CRC: again
  async read(kind, index, progress = () => {}, tries = 3) {
    for (let t = 0; ; t++) {
      const out = [];
      let total = 1;
      while (out.length < total) {
        const off = out.length;
        const b = await this.ok(READ, [kind, index, off & 255, off >> 8, CHUNK_OUT & 255, CHUNK_OUT >> 8]);
        if (u16(b, 0) !== off) throw new SetError("lost", "An answer out of step.");
        total = u16(b, 2);
        for (let i = 4; i < b.length; i++) out.push(b[i]);
        progress(out.length, total);
      }
      const bytes = Uint8Array.from(out);
      try { parse(bytes); return bytes; } catch (e) {
        if (!(e instanceof Refused) || t + 1 >= tries)
          throw new SetError("lost", `What came back doesn't read (${e.message})` +
            (kind === WORK ? ": the pieces kept changing while the FM-1 played. Try again, or stop it first." : "."));
      }
    }
  }

  // the whole: WORK and every set stored, one BACKUP file ({ bytes, objs, list }); progress(done, total) in bytes
  async backup(progress = () => {}) {
    const ls = await this.list();
    const total = ls.reduce((n, e) => n + e.length, 0);
    let before = 0;
    const objs = [];
    for (const e of ls) {
      const bytes = await this.read(e.kind, e.index, (d) => progress(before + d, total));
      objs.push({ kind: e.kind, index: e.index, bytes });
      before += e.kind === WORK ? e.length : bytes.length;
      progress(before, total);
    }
    return { bytes: bundle(objs, this.info ? this.info.version : "", 1), objs, list: ls };
  }

  // the FM-1 asking: onAsk() once, then STATUS polled till it's answered (onTick(seconds left) as it waits): the state.
  // (beta2) op: the question's; another asked already (LOAD IT NOW?, after a store's yes into the set loaded): yes.
  // this.live after it: the set stored is the set loaded (STATUS's flags: the FM-1 keeps playing the old one)
  async waitAnswer(onAsk = () => {}, onTick = () => {}, op = 0) {
    onAsk();
    const t0 = Date.now(), end = t0 + DELAY.ask;
    this.live = false;
    while (Date.now() < end) {
      const b = await this.ok(STATUS);
      this.live = b.length > 21 && !!(b[21] & 3);
      if (op && b[0] !== op) return S_YES;
      if (b[1] !== S_ASKING) return b[1];
      onTick(Math.max(0, ASK_S - Math.floor((Date.now() - t0) / 1000)));
      await new Promise((r) => setTimeout(r, DELAY.poll));
    }
    return S_TIMEOUT;
  }

  // a set container (bytes) to set index (0..7): BEGIN, DATA, COMMIT, the FM-1's answer: the state (S_*)
  async sendSet(index, data, opts = {}) { return this.sendObj(SET, index, data, opts); }

  // (kind SET) as sendSet; (kind WORK, index 0) a restore's pieces. In a restore's yes the FM-1 keeps it at once:
  // COMMIT answers S_YES (or S_FAILED), not asked
  async sendObj(kind, index, data, { onAsk, onTick, progress = () => {} } = {}) {
    const crc = crc32(data), n = data.length;
    await this.ok(BEGIN, [kind, index, n & 255, n >> 8, crc & 255, (crc >>> 8) & 255, (crc >>> 16) & 255, crc >>> 24], "Sending");
    for (let off = 0; off < n; off += CHUNK_IN) {
      const end = Math.min(off + CHUNK_IN, n);
      const b = await this.ok(DATA, [off & 255, off >> 8, ...data.subarray(off, end)], "Sending");
      if (u16(b, 4) !== end) throw new SetError("lost", `The FM-1 has ${u16(b, 4)} bytes, not ${end}.`);
      progress(end, n);
    }
    const a = await this.request(COMMIT);
    if (a.cmd === BUSY && a.body[1] === 2) {     // (COMMIT's answer lost, sent again: asking already)
      const s = await this.ok(STATUS);
      if (s[0] !== COMMIT || s[3] !== index) throw new SetError("busy", BUSY_TEXT[2], 2);
    } else if (a.cmd === NAK && a.body[1] === 10) {   // (a restore's, kept at once, its answer lost: nothing open now)
      const s = await this.ok(STATUS);
      if (s[0] === COMMIT && s[2] === kind && s[3] === index && s[1] !== S_ASKING) return s[1];
      throw this.refused(a, "The set");
    } else if (a.cmd !== COMMIT) {
      throw this.refused(a, "The set");
    } else if (a.body[1] !== S_ASKING) {
      return a.body[1];                          // (kept in a restore's yes, or failed)
    }
    return this.waitAnswer(onAsk, onTick, COMMIT);
  }

  // (Decisions 4) a whole backup (a BACKUP file's bytes) back on the FM-1: every set it holds into its own number and
  // its pieces into the pieces, after one question (RESTORE BACKUP? n sets + the pieces). Everything checked here first
  // (each a set or the pieces, names that fit, a format this FM-1 reads). onItem(i, n, what) before each is sent,
  // progress(done, total) in bytes. { state (the question's answer), done: [{ kind, index, name, state }] (each kept) }.
  // (Decisions 5) It stops at the first that isn't kept (the FM-1's flash failed, or anything else): the FM-1 ends the
  // restore itself on a flash failure, and is told CANCEL anyway; a SetError("stopped") then carries .done (those kept
  // before), .failed (the one that wasn't: { kind, index, name, why, state (S_*: its COMMIT's), err (or what was
  // thrown) }) and .left (the ones never sent)
  async restore(data, { onAsk, onTick, onItem = () => {}, progress = () => {} } = {}) {
    const plan = restorePlan(data, this.info ? this.info.format : 0);
    const a = await this.request(RESTORE, [plan.mask, plan.work ? 1 : 0]);
    if (a.cmd !== RESTORE) throw this.refused(a, "Restore");
    const st = await this.waitAnswer(onAsk, onTick);
    const done = [];
    if (st !== S_YES) return { state: st, done };
    const items = [...plan.sets, ...(plan.work ? [plan.work] : [])];
    const total = items.reduce((t, o) => t + o.bytes.length, 0);
    let before = 0;
    for (const [i, o] of items.entries()) {
      onItem(i, items.length, o);
      let s, err = null;
      try {
        s = await this.sendObj(o.kind, o.index, o.bytes, { progress: (d) => progress(before + d, total) });
      } catch (e) {
        err = e;
      }
      if (err || s !== S_YES) {                  // (stopped here: the rest never sent)
        if (!(err && err.code === "lost")) {
          try { await this.request(CANCEL); } catch (_) { /* (gone: the FM-1 ends it after 10 s) */ }
        }
        const what = o.kind === SET ? `set ${o.index + 1}` : "the pieces";
        const why = err ? err.message : STATE_TEXT[s] || `state ${s}`;
        const e = new SetError("stopped", `The restore stopped at ${what}: ${why}.`);
        e.done = done;
        e.failed = { kind: o.kind, index: o.index, name: o.name, why, state: err ? null : s, err };
        e.left = items.slice(i + 1).map((x) => ({ kind: x.kind, index: x.index, name: x.name }));
        throw e;
      }
      done.push({ kind: o.kind, index: o.index, name: o.name, state: s });
      before += o.bytes.length;
    }
    return { state: st, done };
  }

  // APPLY (load into the pieces) or DELETE of set index: asked on the FM-1; { state, name }
  async askOp(cmd, index, { onAsk, onTick } = {}) {
    let a = await this.request(cmd, [SET, index]), b = a.body;
    if (a.cmd === BUSY && a.body[1] === 2) {
      b = await this.ok(STATUS);
      if (b[0] !== cmd || b[3] !== index) throw new SetError("busy", BUSY_TEXT[2], 2);
    } else if (a.cmd !== cmd) {
      throw this.refused(a);
    }
    return { state: await this.waitAnswer(onAsk, onTick), name: text(b.subarray(5, 21)) };
  }

  // the transfer dropped, a question waiting answered no
  async cancel() { return this.ok(CANCEL); }
}

// a .dub1 file as the set to send (dub1_set.py's as_set): a SET as it is, a WORK (a backup of the pieces) made a set
// (its kind; nothing else changes), its name changed if asked; every name checked as the FM-1 checks it (names.h:
// never cut on its screen). Throws SetError("file")
export function asSet(data, newName = null) {
  let c;
  try { c = parse(data); } catch (e) { throw new SetError("file", `Not a .dub1 file DUB-1 reads (${e.message}).`); }
  const h = c.header;
  if (h.kind === BACKUP) throw new SetError("file", "A whole backup: choose one of its sets (or its pieces) to send.");
  if (h.kind !== WORK && h.kind !== SET) throw new SetError("file", `Not a set (kind ${h.kind}).`);
  let changed = h.kind !== SET;
  h.kind = SET;
  for (const ch of c.chunks) {
    if (ch.id === "META" && newName !== null) {
      try { ch.name = nameField(newName); changed = true; } catch (e) { throw new SetError("file", e.message); }
    }
    if ((ch.id === "META" || ch.id === "PIEC") && nameFits(ch.name) < 0)
      throw new SetError("file", `The name "${text(ch.name)}" is wider than the FM-1's header shows (87 px), or too long.`);
  }
  if (newName !== null && !c.chunks.some((ch) => ch.id === "META")) throw new SetError("file", "This file has no name to change.");
  return changed ? build(c) : data;
}

// (Decisions 4) what a whole backup puts back: { sets: [{ kind, index, bytes, name, pieces, format }] (by number),
// work (the pieces, or null), mask (set n+1: bit n) }; each read and its names checked as a send's (asSet), none made
// for a newer DUB-1 than format (the FM-1's: 0 unknown). Throws SetError("file"), or ("refused", why 5)
export function restorePlan(data, format = 0) {
  let objs;
  try { objs = unbundle(data); } catch (e) {
    throw new SetError("file", `Not a whole backup DUB-1 reads (${e.message}): Restore everything takes the file "Back up everything" made.`);
  }
  const sets = [];
  let work = null, mask = 0;
  for (const o of objs) {
    const one = o.kind === SET && o.index < NSETS && !(mask >> o.index & 1), pieces = o.kind === WORK && !work;
    if (!one && !pieces) continue;
    asSet(o.bytes);                              // (its names: as a send checks them)
    const info = describe(o.bytes);
    const it = { kind: o.kind, index: one ? o.index : 0, bytes: o.bytes, name: info.name, pieces: info.pieces.length,
      format: info.format };
    if (format && info.format > format)
      throw new SetError("refused", `${one ? "Set " + (o.index + 1) : "The pieces"}: ${WHY_TEXT[5]} (its format ` +
        `${info.format}, this DUB-1's ${format})`, 5, info.format);
    if (one) { sets.push(it); mask |= 1 << o.index; } else work = it;
  }
  if (!sets.length && !work) throw new SetError("file", "This backup holds nothing to restore.");
  sets.sort((x, y) => x.index - y.index);
  return { sets, work, mask };
}

// what a .dub1 file holds, for the page: { kind, kindName, format, version, name, pieces: [{ slot, recipe, recipeName,
// name, kind }], size, objs (a backup's: [{ kind, index, bytes, info }]) }. Throws SetError("file")
export function describe(data) {
  let c;
  try { c = parse(data); } catch (e) { throw new SetError("file", `Not a .dub1 file DUB-1 reads (${e.message}).`); }
  const h = c.header, meta = c.chunks.find((ch) => ch.id === "META");
  const out = { kind: h.kind, kindName: { 1: "the pieces (a backup of them)", 2: "a set", 6: "a whole backup" }[h.kind] ||
    `kind ${h.kind}`, format: h.format, version: meta ? text(meta.fw) : "", name: meta ? text(meta.name) : "",
    pieces: [], size: data.length, setlen: 0 };
  const glob = c.chunks.find((ch) => ch.id === "GLOB");
  if (glob) out.setlen = glob.setlen;
  for (const ch of c.chunks) {
    if (ch.id !== "PIEC") continue;
    out.pieces.push({ slot: ch.index, recipe: ch.recipe, recipeName: RECIPES[ch.recipe] || `recipe ${ch.recipe + 1}`,
      name: text(ch.name), kind: ["whole", "the recipe", "the recipe and changes"][ch.kind] || "" });
  }
  if (h.kind === BACKUP) {
    try { out.objs = unbundle(data).map((o) => ({ ...o, info: describe(o.bytes) })); } catch (e) {
      throw new SetError("file", `A backup that doesn't read (${e.message}).`);
    }
  }
  return out;
}

// a file name for a download: "dub1-set-3-Echo-Set.dub1", "dub1-backup-20261030-2215.dub1"
export function fileName(what, n = 0, label = "") {
  const d = new Date(), p = (x) => String(x).padStart(2, "0");
  const stamp = `${d.getFullYear()}${p(d.getMonth() + 1)}${p(d.getDate())}-${p(d.getHours())}${p(d.getMinutes())}`;
  const safe = label.replace(/[^A-Za-z0-9]+/g, "-").replace(/^-|-$/g, "");
  return what === "set" ? `dub1-set-${n}${safe ? "-" + safe : ""}.dub1` : what === "pieces" ? `dub1-pieces-${stamp}.dub1`
    : `dub1-backup-${stamp}.dub1`;
}
