// SPDX-License-Identifier: GPL-3.0-only
// The web installer's update protocol (web/fm1ota.js) against a simulated FM-1 on a fake Web MIDI, as
// tests/install_test.py does for tools/fm1_install.py: a whole install; the write's stalls (an answer lost, the
// loader giving its session up, the loader off USB and back, the loader gone for good); the session's log.
//   node tests/host/installer_ota_test.mjs
import { Updater, pack7, unpack7, hasFeluccaLoader } from "../../web/fm1ota.js";

const HS = [0xF0, 0x00, 0x32, 0x45, 0x00, 0x00, 0x00, 0x40, 0x7F, 0xF7].join();
const UPG = [0xF0, 0x22, 0x24, 0x35, 0x7F, 0xF7].join();
const FIN_CHECK = 0xE0000000, FIN_WRITE = 0xF0000000;
const TIMING = { start: 10, reply: 0, poll: 50, hs: 100, loader: 10, reboot: 10, retry: 20, idleCheck: 400,
  idleWrite: 400, waitLoader: 2000, waitReboot: 2000, resend: 150, quiet: 400, reconnect: 1000, gap: 100, resumes: 3 };
let fails = 0;
const check = (ok, what) => { console.log(`  ${ok ? "ok  " : "FAIL"} ${what}`); if (!ok) fails++; };

// one FM-1 on the fake MIDI access; its stalls as in install_test.py's FakeFM1 (each once, at that many answers)
class FakeFM1 {
  constructor(image, o = {}) {
    Object.assign(this, { image, identity: "FM-1_015", afterWrite: "FM-1_900", dropAt: null, quietAt: null,
      replugAt: null, replugDelay: 300, unplugAfter: null, stallAfter: null }, o);
    this.served = this.bad = this.upgrades = this.dropped = 0;
    this.inputs = new Map(); this.outputs = new Map(); this.gen = 0;
    this.boot(this.identity, o.name || "FM-1");
  }
  boot(identity, name) {
    for (const p of [...this.inputs.values(), ...this.outputs.values()]) p.state = "disconnected";
    this.inputs.clear(); this.outputs.clear();
    const gen = ++this.gen;
    this.identity = identity; this.waiting = null; this.queue = [];
    const dev = this;
    this.input = { name, state: "connected", open: async () => {}, onmidimessage: null };
    this.output = { name, state: "connected", open: async () => {},
      send(b) { if (this.state === "disconnected") throw new Error("gone"); setTimeout(() => { if (dev.gen === gen) dev.rx(Array.from(b)); }, 1); } };
    this.inputs.set("in" + gen, this.input); this.outputs.set("out" + gen, this.output);
  }
  unplug() { this.input.state = this.output.state = "disconnected"; this.inputs.clear(); this.outputs.clear(); this.gen++; }
  tx(b) { const i = this.input; setTimeout(() => { if (i.onmidimessage && i.state !== "disconnected") i.onmidimessage({ data: new Uint8Array(b) }); }, 1); }
  rx(d) {
    const s = d.join();
    if (s === HS) {
      const t = [...this.identity].map((c) => c.charCodeAt(0));
      this.tx([0xF0, ...pack7([0, 0x59, 0x11, 0, 0, 0, ...t, ...new Array(28 - t.length).fill(0)]), 0xF7]);
    } else if (s === UPG) {
      this.upgrades++;
      this.queue = this.identity.startsWith("ota-")
        ? [...Array.from({ length: 6 }, (_, k) => [k * 512, 512]), [FIN_WRITE, 8]]
        : [[0, 64], [0x40, 160], [0x1000, 512], [FIN_CHECK, 8]];
      this.next();
    } else if (this.waiting) {
      if (this.dropAt !== null && this.served === this.dropAt) { this.dropAt = null; this.dropped++; return; }
      const u = unpack7(d.slice(1, -1)), [addr, n] = this.waiting;
      const want = addr >= FIN_CHECK ? [...Buffer.from("success\0")] : Array.from(this.image.subarray(addr, addr + n));
      if (u.length !== 15 + want.length || want.some((b, i) => u[14 + i] !== b)) this.bad++;
      this.waiting = null;
      this.served++;
      if (this.unplugAfter && this.served >= this.unplugAfter) { this.unplug(); return; }
      if (this.stallAfter && this.served >= this.stallAfter) return;   // stops asking for good, still on USB
      if (this.quietAt !== null && this.served >= this.quietAt) { this.quietAt = null; this.queue = []; return; }
      if (this.replugAt !== null && this.served >= this.replugAt) {
        this.replugAt = null; this.unplug();
        if (this.replugDelay !== null) setTimeout(() => this.boot(this.identity, "Felucca Update"), this.replugDelay);
        return;
      }
      if (addr === FIN_CHECK) setTimeout(() => this.boot("ota-FM-1_900", "Felucca Update"), 50);
      else if (addr === FIN_WRITE) setTimeout(() => this.boot(this.afterWrite, "Felucca"), 50);
      else this.next();
    }
  }
  next() {
    if (!this.queue.length) return;
    const [addr, n] = this.waiting = this.queue.shift();
    const u = [0, 0x59, 0x30, 0, 0, 0, 0, addr & 0xFF, (addr >>> 8) & 0xFF, (addr >>> 16) & 0xFF, (addr >>> 24) & 0xFF, n & 0xFF, n >> 8, 0];
    let sum = 0;
    for (let i = 6; i < 14; i++) sum += u[i];
    u.push(~sum & 0xFF);
    this.tx([0xF0, ...pack7(u), 0xF7]);
  }
}

const image = new Uint8Array(0x2000).map((_, i) => (i * 7) & 0xFF);
image.set([..."FELUCCA-LOADER-1"].map((c) => c.charCodeAt(0)), 0x1800);
const plain = image.slice(); plain.fill(0, 0x1800, 0x1810);

async function install(o, img = image) {
  const dev = new FakeFM1(img, o), steps = [];
  const up = new Updater(dev, () => {}, { timing: TIMING });
  let rc = "ok", err = null;
  const t0 = Date.now();
  try { await up.install(img, "FM-1_900", (k, a) => steps.push([k, a])); } catch (e) { rc = e.code; err = e; }
  return { dev, up, rc, err, steps, log: up.logText(), took: Date.now() - t0 };
}

check(hasFeluccaLoader(image) && !hasFeluccaLoader(plain), "the Felucca loader's marker found (and not in a package without it)");

let r = await install({});
const reqs = r.log.match(/req #\d+ +0x[0-9a-f]{8} \+\d+ +[\d.]+ ms/g) || [];
check(r.rc === "ok" && r.dev.bad === 0 && r.dev.upgrades === 2 && reqs.length === r.dev.served && /0xf0000000/.test(reqs.at(-1))
      && /step 1 \(check\)/.test(r.log) && /step 2 \(write\)/.test(r.log) && /done: FM-1_900/.test(r.log),
      `a whole install, and its log: every request (${reqs.length}), the steps, the end`);

r = await install({ dropAt: 6 });
check(r.rc === "ok" && r.dev.dropped === 1 && r.dev.upgrades === 2 && /the last answer again/.test(r.log) && !/resume 1/.test(r.log)
      && r.steps.some(([k]) => k === "stall"), "an answer lost -> sent again after the pause, the write goes on");

r = await install({ quietAt: 7 });
check(r.rc === "ok" && r.dev.bad === 0 && r.dev.upgrades === 3 && /handshake on the same port: ota-FM-1_900/.test(r.log)
      && /resume 1/.test(r.log) && r.steps.some(([k, a]) => k === "resume" && a === 1) && /GAP/.test(r.log),
      "the loader goes quiet mid-write -> a handshake, the upgrade again, the write finishes; the gap logged");

r = await install({ replugAt: 8 });
check(r.rc === "ok" && r.dev.bad === 0 && r.dev.upgrades === 3 && /the port is gone/.test(r.log) && /MIDI inputs: \(none\)/.test(r.log)
      && /found ota-FM-1_900 on Felucca Update/.test(r.log) && /resume 1/.test(r.log),
      "the loader drops off USB and comes back -> found again, the write finishes");

r = await install({ replugAt: 8, replugDelay: null });
check(r.rc === "stall" && r.err.detail === "lost" && /did not come back within 1 s/.test(r.err.message)
      && /the loader did not come back/.test(r.log) && r.took >= 1000 && r.took < 4000,
      `the loader gone for good -> waits for it, then "stall" (${r.took} ms), the log says so`);

const dev = new FakeFM1(image, { identity: "ota-FM-1_900", name: "Felucca Update", stallAfter: 3 });
const up = new Updater(dev, () => {}, { timing: TIMING });
let code = null;
try { await up.resume(image, () => {}); } catch (e) { code = e.code; }
check(code === "stall" && dev.upgrades === 1 + TIMING.resumes && /gave up after 3 resumes/.test(up.logText()),
      "a loader that keeps stopping -> 3 resumes, then \"stall\"");

r = await install({ dropAt: 6 }, plain);
check(r.rc === "ok" && !/the last answer again/.test(r.log) && /another loader/.test(r.log) && /resume 1/.test(r.log),
      "another loader (the stock package's): no answer sent again; after the long idle, a restart");

console.log(fails ? `INSTALLER OTA TESTS FAILED (${fails})` : "installer ota tests passed");
process.exit(fails ? 1 : 0);
