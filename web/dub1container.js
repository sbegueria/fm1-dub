// SPDX-License-Identifier: GPL-3.0-only
// DUB-1's container (1.5.0): the bytes of a stored set, the working project (WORK) and a .dub1 file. The JS twin of
// firmware/src/app/container.c and tools/dub1_container.py: the same shape, read and written the same (the twins'
// test, tests/host/container_twins.sh). Only the codec: what the fields mean is the firmware's (project.c).
//   header  "D1ST", u16 version 1, u16 header length 32, u32 total, u32 proj format, u16 kind (1 WORK, 2 SET, 3 SONG,
//           4 SOUNDS, 5 INBOX), u16 pieces, u32 flags, u32 reserved x 2
//   chunks  4-byte id, u32 length, payload, zeros to a multiple of 4 (an unknown id kept as it is): META 44 (name[16],
//           writer version[16], source, migrated, saves); GLOB (u8 piece leds screen values trans nsir nsk setlen, u16 pool,
//           u16 npar, i8 sir[], i16 par[]); PIEC (u8 index kind recipe screen pings_ok flags, u16 nparams pool npings,
//           u32 seed, name[16], u8 pings[], FULL i16 par[] / DELTA (u16 index, i16 value)[] / RECIPE none); END 4 (u32
//           CRC-32, zlib's, of every byte before it). Little-endian; the shape strict.
//   (step 7) CUES: u8 index, u8 count (1..64), u16 flags, then count cues of 8 bytes sorted by pos: u16 pos (part << 14 |
//           bar << 4 | step), u8 op, u8 arg, i16 val, u16 len
export const FULL = 0, RECIPE = 1, DELTA = 2, HDR = 32, VERSION = 1;
export const KINDS = { 1: "WORK", 2: "SET", 3: "SONG", 4: "SOUNDS", 5: "INBOX", 6: "BACKUP" };
// (1.5.0, step 5) a BACKUP (kind 6): the web page's whole backup in one file, WORK and every stored set: META (the
// writer's version), then an "OBJ " chunk an object (u8 kind, u8 index, u16 0, the object's own container whole). The
// device never takes one (bundle, unbundle; tools/dub1_container.py's the same, byte for byte)
export const BACKUP = 6;

const CRC_T = (() => {
  const t = new Uint32Array(256);
  for (let n = 0; n < 256; n++) {
    let c = n;
    for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1;
    t[n] = c >>> 0;
  }
  return t;
})();
export function crc32(b, n = b.length) {             // zlib's
  let c = 0xffffffff;
  for (let i = 0; i < n; i++) c = CRC_T[(c ^ b[i]) & 255] ^ (c >>> 8);
  return (c ^ 0xffffffff) >>> 0;
}

export class Refused extends Error {}
const pad = (n) => (4 - (n % 4)) % 4;
const id4 = (b, at) => String.fromCharCode(b[at], b[at + 1], b[at + 2], b[at + 3]);

// bytes (Uint8Array) -> { header, chunks: [{ id, ... }], crc }; throws Refused if it doesn't read
export function parse(data) {
  const b = data instanceof Uint8Array ? data : new Uint8Array(data);
  const dv = new DataView(b.buffer, b.byteOffset, b.byteLength);
  if (b.length < HDR + 12) throw new Refused("short");
  if (id4(b, 0) !== "D1ST") throw new Refused("magic");
  const header = {
    version: dv.getUint16(4, true), hlen: dv.getUint16(6, true), total: dv.getUint32(8, true),
    format: dv.getUint32(12, true), kind: dv.getUint16(16, true), npieces: dv.getUint16(18, true),
    flags: dv.getUint32(20, true), rsv: [dv.getUint32(24, true), dv.getUint32(28, true)],
  };
  if (header.version !== VERSION) throw new Refused("version " + header.version);
  if (header.hlen !== HDR || header.total !== b.length || b.length % 4) throw new Refused("header");
  const chunks = [];
  let at = HDR, end;
  for (;;) {
    if (b.length - at < 8) throw new Refused("no END");
    const id = id4(b, at), len = dv.getUint32(at + 4, true);
    if (id === "END ") {
      if (len !== 4 || b.length - at !== 12) throw new Refused("END");
      end = at;
      break;
    }
    if (len > b.length - at - 8 || len + pad(len) > b.length - at - 8) throw new Refused("chunk length");
    for (let i = 0; i < pad(len); i++) if (b[at + 8 + len + i]) throw new Refused("padding");
    chunks.push(chunk(id, b.subarray(at + 8, at + 8 + len)));
    at += 8 + len + pad(len);
  }
  const crc = dv.getUint32(end + 8, true);
  if (crc32(b, end) !== crc) throw new Refused("CRC");
  return { header, chunks, crc };
}

function chunk(id, p) {
  const dv = new DataView(p.buffer, p.byteOffset, p.byteLength);
  if (id === "META") {
    if (p.length !== 44) throw new Refused("META");
    return { id, name: p.slice(0, 16), fw: p.slice(16, 32), source: dv.getUint32(32, true),
      migrated: dv.getUint32(36, true), saves: dv.getUint32(40, true) };
  }
  if (id === "GLOB") {
    if (p.length < 12) throw new Refused("GLOB");
    const nsir = p[5], nsk = p[6], ns = nsir * nsk, npar = dv.getUint16(10, true);
    if (ns > 64 || p.length !== 12 + ns + 2 * npar) throw new Refused("GLOB");
    const sir = [], par = [];
    for (let i = 0; i < ns; i++) sir.push(dv.getInt8(12 + i));
    for (let i = 0; i < npar; i++) par.push(dv.getInt16(12 + ns + 2 * i, true));
    return { id, piece: p[0], leds: p[1], screen: p[2], values: p[3], trans: p[4], nsir, nsk, setlen: p[7],
      pool: dv.getUint16(8, true), sir, par };
  }
  if (id === "PIEC") {
    if (p.length < 32) throw new Refused("PIEC");
    const kind = p[1], nparams = dv.getUint16(6, true), npings = dv.getUint16(10, true);
    if (kind > DELTA) throw new Refused("PIEC kind");
    const nd = kind === FULL ? nparams : kind === DELTA ? 2 * nparams : 0;
    if ((kind === RECIPE && nparams) || p.length !== 32 + npings + 2 * nd) throw new Refused("PIEC");
    const at = 32 + npings, par = [];
    for (let i = 0; i < nparams && kind !== RECIPE; i++) {
      if (kind === FULL) par.push(dv.getInt16(at + 2 * i, true));
      else par.push([dv.getUint16(at + 4 * i, true), dv.getInt16(at + 4 * i + 2, true)]);
    }
    return { id, index: p[0], kind, recipe: p[2], screen: p[3], pings_ok: p[4], flags: p[5], pool: dv.getUint16(8, true),
      seed: dv.getUint32(12, true), name: p.slice(16, 32), pings: p.slice(32, 32 + npings), par };
  }
  if (id === "CUES") {
    if (p.length < 4) throw new Refused("CUES");
    const count = p[1];
    if (count < 1 || count > 64 || p.length !== 4 + 8 * count) throw new Refused("CUES");
    const cues = [];
    for (let i = 0; i < count; i++) {
      const at = 4 + 8 * i;
      cues.push([dv.getUint16(at, true), p[at + 2], p[at + 3], dv.getInt16(at + 4, true), dv.getUint16(at + 6, true)]);
      if (i && cues[i][0] < cues[i - 1][0]) throw new Refused("CUES order");
    }
    return { id, index: p[0], flags: dv.getUint16(2, true), cues };
  }
  return { id, data: p.slice() };
}

class Out {
  constructor() { this.b = []; }
  u8(v) { this.b.push(v & 255); }
  u16(v) { this.u8(v); this.u8(v >>> 8); }
  u32(v) { this.u16(v & 0xffff); this.u16(v >>> 16); }
  bytes(a) { for (const x of a) this.b.push(x & 255); }
  id(s) { for (let i = 0; i < 4; i++) this.b.push(s.charCodeAt(i) & 255); }
}

function payload(c) {
  const o = new Out();
  if (c.id === "META") {
    o.bytes(c.name); o.bytes(c.fw); o.u32(c.source); o.u32(c.migrated); o.u32(c.saves);
  } else if (c.id === "GLOB") {
    for (const k of ["piece", "leds", "screen", "values", "trans", "nsir", "nsk", "setlen"]) o.u8(c[k]);
    o.u16(c.pool); o.u16(c.par.length);
    for (const v of c.sir) o.u8(v);
    for (const v of c.par) o.u16(v);
  } else if (c.id === "PIEC") {
    for (const k of ["index", "kind", "recipe", "screen", "pings_ok", "flags"]) o.u8(c[k]);
    o.u16(c.par.length); o.u16(c.pool); o.u16(c.pings.length); o.u32(c.seed);
    o.bytes(c.name); o.bytes(c.pings);
    for (const v of c.par) {
      if (c.kind === DELTA) { o.u16(v[0]); o.u16(v[1]); } else o.u16(v);
    }
  } else if (c.id === "CUES") {
    o.u8(c.index); o.u8(c.cues.length); o.u16(c.flags);
    for (const q of c.cues) { o.u16(q[0]); o.u8(q[1]); o.u8(q[2]); o.u16(q[3]); o.u16(q[4]); }
  } else {
    o.bytes(c.data);
  }
  return o.b;
}

// { header, chunks } -> Uint8Array (the total and END's CRC worked out here)
export function build(c) {
  const h = c.header, o = new Out();
  o.id("D1ST"); o.u16(VERSION); o.u16(HDR); o.u32(0); o.u32(h.format); o.u16(h.kind); o.u16(h.npieces);
  o.u32(h.flags); o.u32(h.rsv[0]); o.u32(h.rsv[1]);
  for (const ch of c.chunks) {
    const p = payload(ch);
    o.id(ch.id); o.u32(p.length); o.bytes(p);
    for (let i = 0; i < pad(p.length); i++) o.u8(0);
  }
  o.id("END "); o.u32(4);
  const total = o.b.length + 4;
  const b = new Uint8Array(total);
  b.set(o.b);
  new DataView(b.buffer).setUint32(8, total, true);
  new DataView(b.buffer).setUint32(total - 4, crc32(b, total - 12), true);   // (END's own 8 bytes not in it)
  return b;
}

// (step 5) objects [{ kind, index, bytes }] as one BACKUP file (each a container: Refused if one isn't); fw the writer's
// version (the FM-1's, from HELLO); source META's (0 device, 1 web, 2 host script)
export function bundle(objs, fw = "", source = 1) {
  const f = new Uint8Array(16);
  for (let i = 0; i < Math.min(16, fw.length); i++) f[i] = fw.charCodeAt(i) & 255;
  const chunks = [{ id: "META", name: new Uint8Array(16), fw: f, source, migrated: 0, saves: 0 }];
  let format = 0;
  for (const o of objs) {
    format = Math.max(format, parse(o.bytes).header.format);
    const d = new Uint8Array(4 + o.bytes.length);
    d[0] = o.kind; d[1] = o.index; d.set(o.bytes, 4);
    chunks.push({ id: "OBJ ", data: d });
  }
  return build({ header: { format, kind: BACKUP, npieces: objs.length, flags: 0, rsv: [0, 0] }, chunks });
}
// (step 5) a BACKUP file's objects: [{ kind, index, bytes }], each read (Refused if the file or one isn't)
export function unbundle(data) {
  const c = parse(data), out = [];
  if (c.header.kind !== BACKUP) throw new Refused(`not a backup (kind ${c.header.kind})`);
  for (const ch of c.chunks) {
    if (ch.id !== "OBJ ") continue;
    if (ch.data.length < 4 + HDR + 12) throw new Refused("OBJ");
    const bytes = ch.data.slice(4);
    parse(bytes);
    out.push({ kind: ch.data[0], index: ch.data[1], bytes });
  }
  if (out.length !== c.header.npieces) throw new Refused("OBJ count");
  return out;
}

// (names.h's) a name as the device takes it: up to NAME_CHARS letters, numbers, spaces and a few marks, not blank in
// front, no wider than NAME_PX by the header font's widths (NAME_W, ' '..'~', the digits tabular; tools/gen_name_w.py
// checks the table is the firmware's): a name the page accepts is never cut on the device
export const NAME_PX = 87, NAME_CHARS = 12;
export const NAME_W = [3, 5, 5, 10, 8, 12, 10, 3, 5, 5, 6, 7, 4, 6, 4, 7, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 5, 4, 7, 7, 7, 7,
  12, 9, 8, 8, 8, 8, 7, 8, 8, 4, 8, 8, 7, 9, 9, 8, 8, 8, 8, 8, 8, 8, 8, 12, 8, 8, 7, 6, 7, 6, 7, 7, 3, 7, 8, 7, 8, 7,
  5, 7, 7, 4, 4, 7, 3, 11, 7, 8, 8, 8, 5, 7, 5, 7, 7, 10, 7, 7, 6, 5, 3, 5, 8];
const NAME_MARKS = " -.'&!?/+#():,_";
const nameChar = (c) => (c >= 48 && c <= 57) || (c >= 65 && c <= 90) || (c >= 97 && c <= 122) ||
  NAME_MARKS.includes(String.fromCharCode(c));
// a name field (16 bytes: the name, then NULs) as the device takes it: its width in pixels, or -1
export function nameFits(field) {
  let w = 0, end = false;
  for (let i = 0; i < field.length; i++) {
    const c = field[i];
    if (!c) end = true;
    else if (end || i >= NAME_CHARS || !nameChar(c)) return -1;
    else w += NAME_W[c - 32];
  }
  return (field.length && field[0] === 32) || w > NAME_PX ? -1 : w;
}
// a name as its 16-byte field; throws Refused if the device would not take it
export function nameField(text) {
  const f = new Uint8Array(16);
  if (text.length > 16) throw new Refused(`the name "${text}": too long`);
  for (let i = 0; i < text.length; i++) f[i] = text.charCodeAt(i) & 255;
  for (let i = 0; i < text.length; i++) if (text.charCodeAt(i) > 255) f[i] = 1;
  if (nameFits(f) < 0) throw new Refused(`the name "${text}": up to ${NAME_CHARS} letters, numbers, spaces and ` +
    `${NAME_MARKS.trim()}, no wider than ${NAME_PX} px`);
  return f;
}

export const name = (field) => {                     // a name field (16 bytes, NUL-padded) as text
  let s = "";
  for (const c of field) { if (!c) break; s += String.fromCharCode(c); }
  return s;
};

const hex = (a) => Array.from(a, (x) => (x & 255).toString(16).padStart(2, "0")).join("");
const lst = (a) => a.join(",") || "-";
// the canonical dump: one line a chunk (container_test.c's and dub1_container.py's, the same)
export function dump(c) {
  const h = c.header, out = [`D1ST version=${h.version} hlen=${h.hlen} total=${h.total} format=${h.format} ` +
    `kind=${h.kind} npieces=${h.npieces} flags=${h.flags} rsv=${h.rsv[0]},${h.rsv[1]}`];
  for (const ch of c.chunks) {
    if (ch.id === "META") {
      out.push(`META name=${hex(ch.name)} fw=${hex(ch.fw)} source=${ch.source} migrated=${ch.migrated} saves=${ch.saves}`);
    } else if (ch.id === "GLOB") {
      out.push(`GLOB piece=${ch.piece} leds=${ch.leds} screen=${ch.screen} values=${ch.values} trans=${ch.trans} ` +
        `nsir=${ch.nsir} nsk=${ch.nsk} setlen=${ch.setlen} pool=${ch.pool} npar=${ch.par.length} sir=${lst(ch.sir)} par=${lst(ch.par)}`);
    } else if (ch.id === "PIEC") {
      const data = ch.kind === DELTA ? ch.par.map((p) => `${p[0]}:${p[1]}`).join(",") || "-" : lst(ch.par);
      out.push(`PIEC index=${ch.index} kind=${ch.kind} recipe=${ch.recipe} screen=${ch.screen} pings_ok=${ch.pings_ok} ` +
        `flags=${ch.flags} nparams=${ch.par.length} pool=${ch.pool} npings=${ch.pings.length} seed=${ch.seed} ` +
        `name=${hex(ch.name)} pings=${hex(ch.pings) || "-"} data=${data}`);
    } else if (ch.id === "CUES") {
      out.push(`CUES index=${ch.index} count=${ch.cues.length} flags=${ch.flags} cues=${ch.cues.map((q) => q.join(":")).join(",")}`);
    } else {
      out.push(`CHNK id=${hex(Array.from(ch.id, (x) => x.charCodeAt(0)))} len=${ch.data.length} data=${hex(ch.data) || "-"}`);
    }
  }
  out.push(`END crc=${c.crc.toString(16).padStart(8, "0")}`);
  return out.join("\n") + "\n";
}

// a random container of every kind of chunk (the twins' test), from a seed
export function sample(seed) {
  let s = (seed * 2654435761 + 12345) >>> 0 || 1;
  const r = (n) => { s ^= s << 13; s >>>= 0; s ^= s >>> 17; s ^= s << 5; s >>>= 0; return s % n; };
  const nm = () => {
    const C = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 -.'&!", b = new Uint8Array(16);
    const n = r(13);
    for (let i = 0; i < n; i++) b[i] = C.charCodeAt(r(C.length));
    return b;
  };
  const chunks = [], u8 = (n) => Uint8Array.from({ length: n }, () => r(256));
  if (r(4)) chunks.push({ id: "META", name: nm(), fw: Uint8Array.from([0x31, 0x2e, 0x35, 0x2e, 0x30, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]),
    source: r(3), migrated: r(3) ? 0 : r(0x7fffffff), saves: r(1000) });
  if (r(3)) {
    const nsir = r(5), nsk = r(5);
    chunks.push({ id: "GLOB", piece: r(256), leds: r(256), screen: r(256), values: r(256), trans: r(256), nsir, nsk,
      setlen: r(2), pool: r(65536), sir: Array.from({ length: nsir * nsk }, () => r(256) - 128),
      par: Array.from({ length: r(200) }, () => r(65536) - 32768) });
  }
  const np = r(9);
  for (let k = 0; k < np + (r(3) === 0 ? 1 : 0); k++) {
    if (r(5) === 0) chunks.push({ id: "X" + String.fromCharCode(65 + r(26)) + "YZ", data: u8(r(40)) });
    const kind = r(3), n = kind === RECIPE ? 0 : r(200);
    const par = Array.from({ length: n }, () => (kind === FULL ? r(65536) - 32768 : [r(65536), r(65536) - 32768]));
    chunks.push({ id: "PIEC", index: r(8), kind, recipe: r(8), screen: r(256), pings_ok: r(2), flags: r(256),
      pool: r(65536), seed: r(2) ? 0 : r(0x7fffffff), name: nm(), pings: u8(r(100)), par });
    if (r(3) === 0) {                                // (step 7) a cue list, its positions sorted
      const n = 1 + (r(4) ? r(12) : r(64)), cues = [];
      let pos = r(4) ? r(4096) : r(65536);
      for (let i = 0; i < n; i++) {
        pos += r(3) ? r(40) : 0;
        cues.push([Math.min(pos, 65535), r(256), r(256), r(65536) - 32768, r(2) ? r(64) : r(65536)]);
      }
      chunks.push({ id: "CUES", index: r(8), flags: r(8) ? 0 : r(65536), cues });
    }
  }
  return build({ header: { version: 1, hlen: 32, total: 0, format: 8 + r(40), kind: 1 + r(5), npieces: np, flags: r(4),
    rsv: r(4) ? [0, 0] : [r(0x7fffffff), r(0x7fffffff)] }, chunks });
}
