// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
//
// The FM-1 USB-MIDI update protocol (M-UPGRADE compatible) over WebMIDI.
// Step 1: the running firmware reads parts of the package and stages the update
// loader. Step 2: the loader reads the whole image and writes it. In both steps
// the device sends cmd 0x30 read requests on the logical image and we answer.
//
// Stalls in step 2, as tools/fm1_install.py (its docstring, "Stalls", says why): no request for 6 s -> the
// last answer again (once); no request for 17 s (Felucca's loader gives a read up after 3 x 5 s and waits in its
// main loop) -> a handshake: the loader there -> the upgrade command again (its new session reads the package
// again from the start; the sectors already written are only compared); not there, or its port gone -> wait up
// to 30 s for a loader on USB-MIDI and start it again. At most 3 times. Another loader (M-VAVE's, in the stock
// package): no answer sent again, and 180 s before the handshake. The session's log (each request, its timing,
// the gaps, the stalls, the end) is kept in memory: updater.logText().

const HS_QUERY = [0xF0, 0x00, 0x32, 0x45, 0x00, 0x00, 0x00, 0x40, 0x7F, 0xF7];
const UPGRADE = [0xF0, 0x22, 0x24, 0x35, 0x7F, 0xF7];
const MAXDATA = 512;
const LOADER_MARK = "FELUCCA-LOADER-1";
// ms; the tests shorten them (new Updater(access, log, { timing: {...} }))
export const OTA_TIMING = { start: 2000, reply: 10, poll: 1000, hs: 1000, loader: 3000, reboot: 3000, retry: 1000,
  idleCheck: 8000, idleWrite: 180000, waitLoader: 30000, waitReboot: 40000,
  resend: 6000, quiet: 17000, reconnect: 30000, gap: 1000, resumes: 3 };
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const IS_OTA = (id) => /^ota-/i.test(id.model);

// does the image carry Felucca's own update loader (its marker in the loader's first LZ4 literals)?
export function hasFeluccaLoader(image) {
  const m = Array.from(LOADER_MARK, (c) => c.charCodeAt(0));
  outer: for (let i = 0; i + m.length <= image.length; i++) {
    if (image[i] !== m[0]) continue;
    for (let k = 1; k < m.length; k++) if (image[i + k] !== m[k]) continue outer;
    return true;
  }
  return false;
}

// errors carry a code the pages translate: notfound, model, stopped, noloader, lost, noreturn,
// mismatch (detail: the identity the device reports), badreq; and stall (step 2 stopped or lost, not back:
// detail "lost" or "stopped")
function fail(code, msg, detail = "") { const e = new Error(msg); e.code = code; e.detail = detail; return e; }

export function pack7(data) {
  const out = []; let acc = 0, nb = 0;
  for (const b of data) {
    acc |= b << nb; nb += 8;
    while (nb >= 7) { out.push(acc & 0x7F); acc >>>= 7; nb -= 7; }
  }
  if (nb) out.push(acc & 0x7F);
  return out;
}

export function unpack7(s) {
  const out = []; let acc = 0, nb = 0;
  for (const b of s) {
    acc |= (b & 0x7F) << nb; nb += 7;
    while (nb >= 8) { out.push(acc & 0xFF); acc >>>= 8; nb -= 8; }
  }
  return out;
}

function response(addr, data, fl = 0) {
  const len = data.length;
  const body = [0x00, 0x59, 0x30, (len + 8) & 0xFF, ((len + 8) >> 8) & 0xFF, 0, fl,
    addr & 0xFF, (addr >>> 8) & 0xFF, (addr >>> 16) & 0xFF, (addr >>> 24) & 0xFF,
    len & 0xFF, (len >> 8) & 0xFF, 0, ...data];
  let s = 0;
  for (let i = 6; i < body.length; i++) s += body[i];
  body.push(~s & 0xFF);
  return [0xF0, ...pack7(body), 0xF7];
}

function parseRequest(pkt) {
  if (pkt[0] !== 0xF0 || pkt[pkt.length - 1] !== 0xF7) return null;
  const u = unpack7(pkt.slice(1, -1));
  if (u.length !== 15 || u[0] !== 0 || u[1] !== 0x59 || u[2] !== 0x30) return null;
  let s = 0;
  for (let i = 6; i < 14; i++) s += u[i];
  if ((~s & 0xFF) !== u[14]) return null;
  return { fl: u[6], addr: (u[7] | (u[8] << 8) | (u[9] << 16) | (u[10] << 24)) >>> 0, len: u[11] | (u[12] << 8) | (u[13] << 16) };
}

function parseIdentity(pkt) {
  const d = unpack7(pkt.slice(1, -1));
  if (d.length !== 34 || d[0] !== 0 || d[1] !== 0x59 || d[2] !== 0x11) return null;
  const txt = String.fromCharCode(...d.slice(6, 33)).replace(/\0+$/, "");
  const m = /^([^_]+)_(\d+)$/.exec(txt);
  return m ? { text: txt, model: m[1], version: parseInt(m[2], 10) } : null;
}

// one MIDI in/out pair with a SysEx queue
class Link {
  constructor(input, output) {
    this.input = input; this.output = output; this.q = []; this.waiter = null;
    input.onmidimessage = (ev) => {
      if (ev.data[0] !== 0xF0) return;
      this.q.push(Array.from(ev.data));
      if (this.waiter) { const w = this.waiter; this.waiter = null; w(); }
    };
  }
  get lost() { return this.input.state === "disconnected" || this.output.state === "disconnected"; }
  // false when the port is gone (unplugged): WebMIDI throws on a disconnected output
  send(bytes) {
    try { this.output.send(bytes); return true; } catch (_) { return false; }
  }
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
  close() { this.input.onmidimessage = null; }
}

async function handshake(link, tries = 3, ms = 1000) {
  link.drain();
  for (let t = 0; t < tries; t++) {
    if (!link.send(HS_QUERY)) return null;
    const end = Date.now() + ms;
    while (Date.now() < end) {
      const p = await link.read(end - Date.now());
      if (!p) break;
      const id = parseIdentity(p);
      if (id) return id;
    }
  }
  return null;
}

export class Updater {
  // log(text): the page's step lines; opts.timing: OTA_TIMING's values to change (the tests)
  constructor(access, log, opts = {}) {
    this.access = access;
    this.log = log || (() => {});
    this.T = { ...OTA_TIMING, ...(opts.timing || {}) };
    this.t0 = Date.now();
    this.lines = [];
    this.lastReq = null;
    this.reqs = 0;
    this.ev(`fm1ota ${new Date().toISOString()}  ${typeof navigator !== "undefined" ? navigator.userAgent : ""}`);
  }

  // the session's log: each request (address, length, the time since the one before), the gaps, the stalls, the end
  ev(msg) { this.lines.push(`${((Date.now() - this.t0) / 1000).toFixed(3).padStart(9)}  ${msg}`); }
  req(addr, len) {
    const now = Date.now(), dt = this.lastReq === null ? 0 : now - this.lastReq;
    this.lastReq = now;
    this.reqs++;
    this.ev(`req #${String(this.reqs).padEnd(5)} 0x${addr.toString(16).padStart(8, "0")} +${String(len).padEnd(4)} ` +
            `${dt.toFixed(1).padStart(8)} ms${dt >= this.T.gap ? `  GAP ${(dt / 1000).toFixed(1)} s` : ""}`);
  }
  logText() { return this.lines.join("\n") + "\n"; }

  inputNames() { return [...this.access.inputs.values()].filter((i) => i.state !== "disconnected").map((i) => i.name); }

  // every in/out pair with the same port name that answers the handshake
  async find(filter) {
    const outs = [...this.access.outputs.values()];
    for (const input of this.access.inputs.values()) {
      if (input.state === "disconnected") continue;
      if (!/fm-1|felucca|ota|composite|sinco|usb-midi/i.test(input.name || "")) continue;   // never probe other gear
      const output = outs.find((o) => o.name === input.name) || (outs.length === 1 ? outs[0] : null);
      if (!output) continue;
      try { await input.open(); await output.open(); } catch (_) { continue; }
      const link = new Link(input, output);
      const id = await handshake(link, 2, this.T.hs);
      if (id && (!filter || filter(id))) return { link, id, name: input.name };
      link.close();
    }
    return null;
  }

  async waitFor(filter, ms, tick) {
    const end = Date.now() + ms;
    let seen = null;
    while (Date.now() < end) {
      const names = this.inputNames().join(", ") || "(none)";
      if (names !== seen) { this.ev(`MIDI inputs: ${names}`); seen = names; }   // what came and went on USB
      const dev = await this.find(filter);
      if (dev) { this.ev(`found ${dev.id.text} on ${dev.name}`); return dev; }
      if (tick) tick();
      await sleep(this.T.retry);
    }
    return null;
  }

  // serve read requests until the device asks for finishAddr; stops early when the port disappears (lost: true)
  // instead of waiting out idleMs. resendMs: that long without a request -> the last answer again (once);
  // tick(ms since the last request) while it is that quiet. -> { served, finished, lost, last }
  async serve(link, image, finishAddr, idleMs, onProgress, resendMs = null, tick = null) {
    let served = 0, last = Date.now(), answer = null, resent = false;
    for (;;) {
      const pkt = await link.read(this.T.poll);
      if (!pkt) {
        const quiet = Date.now() - last;
        if (link.lost) { this.ev(`the port is gone, ${(quiet / 1000).toFixed(1)} s after the last request`); return { served, finished: false, lost: true, last }; }
        if (quiet > idleMs) { this.ev(`no request for ${(quiet / 1000).toFixed(1)} s`); return { served, finished: false, lost: false, last }; }
        if (resendMs !== null && quiet >= resendMs) {
          if (answer && !resent) {
            resent = true;
            this.ev(`no request for ${(quiet / 1000).toFixed(1)} s: the last answer again`);
            link.send(answer);
          }
          if (tick) tick(quiet);
        }
        continue;
      }
      const r = parseRequest(pkt);
      if (!r) continue;
      last = Date.now(); resent = false;
      this.req(r.addr, r.len);
      if (r.addr === 0xE0000000 || r.addr === 0xF0000000) {
        answer = response(r.addr, [...new TextEncoder().encode("success"), 0]);
        link.send(answer);
        if (r.addr === finishAddr) { this.ev(`finish 0x${r.addr.toString(16)} answered`); return { served, finished: true, lost: false, last }; }
        continue;
      }
      if (r.len > MAXDATA || r.addr + r.len > image.length) {
        this.ev(`bad request ${r.addr.toString(16)}+${r.len}`);
        throw fail("badreq", `bad request ${r.addr.toString(16)}+${r.len}`);
      }
      await sleep(this.T.reply);
      answer = response(r.addr, Array.from(image.subarray(r.addr, r.addr + r.len)), r.fl);
      if (!link.send(answer)) {
        this.ev("the answer could not be sent: the port is gone");
        return { served, finished: false, lost: true, last };
      }
      served++;
      if (onProgress) onProgress(served, r.addr);
    }
  }

  // full install: running firmware -> loader -> new firmware
  async install(image, product, onStep) {
    const step = onStep || (() => {});
    this.ev(`install ${product}, ${image.length} B, ${hasFeluccaLoader(image) ? "Felucca loader" : "another loader"}`);
    this.ev(`MIDI inputs: ${this.inputNames().join(", ") || "(none)"}`);
    const dev = await this.find((id) => !IS_OTA(id));
    if (!dev) { this.ev("FM-1 not found"); throw fail("notfound", "FM-1 not found (USB cable, and no other app using it?)"); }
    const [model] = product.split("_");
    if (dev.id.model !== model) { dev.link.close(); throw fail("model", `the device is ${dev.id.text}, the package is for ${product}`); }
    step("start", dev.id.text);
    this.ev(`step 1 (check) on ${dev.id.text}, ${dev.name}`);
    dev.link.send(UPGRADE);
    await sleep(this.T.start);
    const s1 = await this.serve(dev.link, image, 0xE0000000, this.T.idleCheck, (n) => step("verify", n));
    dev.link.close();
    if (!s1.finished) throw fail(s1.lost ? "lost" : "stopped", `the device ${s1.lost ? "was disconnected" : "stopped"} after ${s1.served} requests: nothing was written`);
    step("loader");
    await sleep(this.T.loader);
    const ota = await this.waitFor(IS_OTA, this.T.waitLoader);
    if (!ota) { this.ev("no loader"); throw fail("noloader", "the update loader did not appear"); }
    await this.write(ota, image, step, product);
    step("reboot");
    await sleep(this.T.reboot);
    const back = await this.waitFor((id) => !IS_OTA(id), this.T.waitReboot);
    if (!back) { this.ev("the FM-1 did not come back after the write"); throw fail("noreturn", "the device did not come back: power-cycle it"); }
    back.link.close();
    if (back.id.text !== product) { this.ev(`mismatch: ${back.id.text}`); throw fail("mismatch", `installed, but the device reports ${back.id.text}`, back.id.text); }
    this.ev(`done: ${back.id.text}`);
    step("done", back.id.text);
    return back.id.text;
  }

  // step 2 with the loader found by find()/waitFor(); throws "stall" when it stops and doesn't come back.
  // step("stall", seconds since the last request) while waiting, step("resume", n) when the write starts again
  async write(ota, image, step, product = null) {
    const felucca = hasFeluccaLoader(image);
    const quiet = felucca ? this.T.quiet : this.T.idleWrite, resend = felucca ? this.T.resend : null;
    let link = ota.link, name = ota.name, total = 0, resumes = 0, reached = 0;
    this.ev(`step 2 (write) on ${ota.id.text}, ${name}; ${felucca ? "Felucca" : "another"} loader; package ${image.length} B`);
    const prog = (n, addr) => {                          // step("write", percent of the package read)
      reached = Math.max(reached, addr);
      step("write", Math.min(99, Math.floor((addr * 100) / image.length)));
    };
    for (;;) {
      link.send(UPGRADE);
      this.ev("upgrade command sent");
      await sleep(this.T.start);
      const s = await this.serve(link, image, 0xF0000000, quiet, prog, resend, (q) => step("stall", Math.floor(q / 1000)));
      total += s.served;
      if (s.finished) { link.close(); return s; }
      const why = s.lost ? "lost" : "stopped", pct = Math.floor((reached * 100) / image.length);
      if (resumes >= this.T.resumes) {
        link.close();
        this.ev(`gave up after ${resumes} resumes`);
        throw fail("stall", `the loader ${s.lost ? "was disconnected" : "stopped answering"} again at ${pct}% (${total} requests), after ${resumes} resumes`, why);
      }
      let back = null;
      if (!s.lost) {                                     // still on USB: is the loader there?
        const id = await handshake(link, 2, this.T.hs);
        this.ev(`handshake on the same port: ${id ? id.text : "no answer"}`);
        if (id && IS_OTA(id)) back = { link, id, name };
      }
      if (!back) {                                       // gone, or mute: wait for it on USB
        link.close();
        this.ev(`waiting up to ${this.T.reconnect / 1000} s for the loader on USB`);
        const since = () => step("stall", Math.floor((Date.now() - s.last) / 1000));
        since();
        back = await this.waitFor((id) => IS_OTA(id) || id.text === product, this.T.reconnect, since);
        if (back && !IS_OTA(back.id)) {                  // the write had finished after all
          back.link.close();
          this.ev(`the FM-1 is back running ${back.id.text}: the write had finished`);
          return { served: total, finished: true };
        }
      }
      if (!back) {
        this.ev("the loader did not come back");
        throw fail("stall", `the loader ${s.lost ? "was disconnected" : "stopped answering"} at ${pct}% (${total} requests) and did not come back within ${this.T.reconnect / 1000} s`, why);
      }
      resumes++;
      this.ev(`resume ${resumes}: ${back.id.text} on ${back.name}, the write starts again`);
      step("resume", resumes);
      link = back.link; name = back.name;
    }
  }

  // resume: the device is already in update mode (loader) -> true when the write finished (throws "stall" when it
  // stops and doesn't come back), false when no loader is found
  async resume(image, onStep, product = null) {
    const ota = await this.find(IS_OTA);
    if (!ota) { this.ev("no loader to resume"); return false; }
    this.ev(`resume an unfinished write: ${ota.id.text} on ${ota.name}`);
    const s = await this.write(ota, image, onStep || (() => {}), product);
    this.ev("done (resumed)");
    return s.finished;
  }
}
