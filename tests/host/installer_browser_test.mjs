// SPDX-License-Identifier: GPL-3.0-only
// (1.5.0, Decisions 5) the installer page's "Check this FM-1" (web/amb_installer.html) in a headless Chrome driven over
// its DevTools protocol, as sets_browser_test.mjs: the page put together as tools/make_pages.py puts it (fm1pkg.js and
// fm1ota.js inlined, its metadata; the Sets page's dub1sets.js and dub1container.js in sets/ beside it), its Web MIDI a
// fake FM-1 written here that answers the update protocol's identity (FM-1_8XXYYZZ) and, if told to, the set protocol's
// HELLO with a version string. 1.4.3: nothing to back up, HELLO never asked; 1.5.0-BETA1: a test build, not the release (Decisions 6);
// 1.5.0: the release; a 1.5.0 that doesn't answer HELLO: the digits alone, said so; a development build (DEV); stock
// FM-1_015; then in Japanese. (The package the page fetches isn't there: the page says so; the check doesn't need it.)
//   node tests/host/installer_browser_test.mjs [chrome]
import { spawn } from "child_process";
import fs from "fs";
import os from "os";
import path from "path";

const chrome = process.argv[2] || "chromium";
const HTTP = 21000 + (process.pid % 9000), CDP = 31000 + (process.pid % 9000);
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
let fails = 0;
const check = (ok, what) => { console.log(`  ${ok ? "ok  " : "FAIL"} ${what}`); if (!ok) fails++; };

// the site's install/ and sets/ as make_pages.py makes them (make_site.py's strip_module, its META)
const tmp = fs.mkdtempSync(path.join(os.tmpdir(), "installer-browser-"));
const site = path.join(tmp, "site");
fs.mkdirSync(path.join(site, "install"), { recursive: true });
fs.mkdirSync(path.join(site, "sets"), { recursive: true });
const strip = (s) => s.replace(/^export\s+/gm, "").replace(/^import .*?;\n/gm, "");
const lib = strip(fs.readFileSync("web/fm1pkg.js", "utf8")) + "\n" + strip(fs.readFileSync("web/fm1ota.js", "utf8"));
const meta = JSON.stringify({ version: "1.5.0", product: "FM-1_8010500", pkg: "../firmware/dub-1.5.0.fwsc",
                              sets: "../sets/dub1sets.js" });
const page = fs.readFileSync("web/amb_installer.html", "utf8");
fs.writeFileSync(path.join(site, "install", "index.html"), page.replace("/*LIB*/", () => lib).replace("/*META*/", () => meta));
for (const f of ["dub1sets.js", "dub1container.js", "dub1cues.js", "dub1cuetables.js"])
  fs.copyFileSync(path.join("web", f), path.join(site, "sets", f));
// a package for the install's check: synthetic, its identity FM-1_8010500 in the marker bytes (fm1pkg.js's productOf)
{
  const raw = new Uint8Array(0x2000 + 20).map((_, i) => (i * 7) & 0xff), id = "FM-1_8010500";
  for (let i = 0; i < 20; i++) raw[i * 48 + 47] = i < id.length ? (id.charCodeAt(i) + i + 1) & 0xff : 0x7d;
  fs.mkdirSync(path.join(site, "firmware"));
  fs.writeFileSync(path.join(site, "firmware", "dub-1.5.0.fwsc"), raw);
}

const server = spawn("python3", ["-m", "http.server", String(HTTP), "--bind", "127.0.0.1"], { cwd: site, stdio: "ignore" });
const profile = path.join(tmp, "profile");
const browser = spawn(chrome, ["--headless=new", `--remote-debugging-port=${CDP}`, `--user-data-dir=${profile}`,
  "--no-first-run", "--no-default-browser-check", "--hide-scrollbars", "about:blank"], { stdio: "ignore" });
const done = async (code) => {
  const gone = new Promise((r) => browser.once("exit", r));
  browser.kill();
  server.kill();
  await Promise.race([gone, sleep(5000)]);
  try { fs.rmSync(tmp, { recursive: true, force: true, maxRetries: 5, retryDelay: 200 }); } catch {}
  process.exit(code);
};
setTimeout(() => { console.log("  FAIL timed out"); done(1); }, 120000);

let ws, seq = 0;
const pending = new Map();
function cdp(method, params = {}) {
  const id = ++seq;
  ws.send(JSON.stringify({ id, method, params }));
  return new Promise((res, rej) => pending.set(id, { res, rej }));
}
async function js(expr) {
  const r = await cdp("Runtime.evaluate", { expression: expr, awaitPromise: true, returnByValue: true });
  if (r.exceptionDetails) throw new Error(r.exceptionDetails.exception?.description || r.exceptionDetails.text);
  return r.result.value;
}
async function until(expr, ms = 15000) {
  for (const t0 = Date.now(); Date.now() - t0 < ms; await sleep(100))
    if (await js(expr)) return true;
  return false;
}

// the fake FM-1: one port, "FM-1"; window.__fm1 = { id: "FM-1_8010500", hello: "DUB-1 1.5.0" | null } says what it
// answers; window.__hellos counts the HELLOs it was asked. Both the update protocol's Link (onmidimessage) and the set
// protocol's MidiLink (addEventListener) listen
const FAKE = `(() => {
  const ls = new Set();
  const pack7 = (d) => { const o = []; let a = 0, n = 0; for (const b of d) { a |= b << n; n += 8; while (n >= 7) { o.push(a & 0x7f); a >>>= 7; n -= 7; } } if (n) o.push(a & 0x7f); return o; };
  const input = { id: "in", name: "FM-1", state: "connected", open: async () => {}, onmidimessage: null,
                  addEventListener: (t, f) => ls.add(f), removeEventListener: (t, f) => ls.delete(f) };
  const deliver = (b) => setTimeout(() => {
    const ev = { data: new Uint8Array(b), target: input };
    if (input.onmidimessage) input.onmidimessage(ev);
    ls.forEach((f) => f(ev));
  }, 5);
  window.__hellos = 0;
  const output = { id: "out", name: "FM-1", state: "connected", open: async () => {}, send: (b) => {
    const f = window.__fm1 || {};
    b = Array.from(b);
    if (b.length === 10 && b[1] === 0x00 && b[2] === 0x32 && b[3] === 0x45) {   // (the identity handshake)
      const d = [0x00, 0x59, 0x11, 0, 0, 0];
      for (let i = 0; i < 27; i++) d.push(i < f.id.length ? f.id.charCodeAt(i) : 0);
      d.push(0);
      deliver([0xf0, ...pack7(d), 0xf7]);
    } else if (b[1] === 0x7d && b[2] === 0x44 && b[3] === 0x31 && b[4] === 0x01) {   // (the set protocol's HELLO)
      window.__hellos++;
      if (!f.hello) return;
      const body = [1, 0, 2, 0, 1, 37, 0, 0, 0, 0x00, 0x3f, 8, 0, 0, 0, 0];
      for (let i = 0; i < 32; i++) body.push(i < f.hello.length ? f.hello.charCodeAt(i) : 0);
      const fr = [0x7d, 0x44, 0x31, 0x01, b[5], ...pack7(body)];
      let s = 0;
      for (let i = 3; i < fr.length; i++) s += fr[i];
      deliver([0xf0, ...fr, s & 0x7f, 0xf7]);
    }
  } };
  Object.defineProperty(navigator, "clipboard", { value: { writeText: async (t) => { window.__clip = t; } } });   // (Copy the log)
  navigator.requestMIDIAccess = async (o) => ({ sysex: !!(o && o.sysex), inputs: new Map([["in", input]]),
                                                outputs: new Map([["out", output]]), onstatechange: null });
})();`;

async function checkWith(fm1, want, what) {
  await js(`window.__fm1 = ${JSON.stringify(fm1)}; window.__hellos = 0; document.getElementById("checkres").textContent = ""; document.getElementById("check").click()`);
  const ok = await until(`(() => { const t = document.getElementById("checkres").textContent;
    return t && !/Checking|確認しています/.test(t) && !document.getElementById("check").disabled; })()`, 20000);
  const res = await js(`document.getElementById("checkres").textContent`), hellos = await js("window.__hellos");
  check(ok && want.every((re) => re.test(res)) && (fm1.asked === undefined || (hellos > 0) === fm1.asked),
        `${what}: "${res.slice(0, 150)}" (HELLO asked ${hellos})`);
}

try {
  let target;
  for (let i = 0; i < 50 && !target; i++) {
    await sleep(200);
    try { target = await (await fetch(`http://127.0.0.1:${CDP}/json/new?about:blank`, { method: "PUT" })).json(); } catch {}
  }
  ws = new WebSocket(target.webSocketDebuggerUrl);
  await new Promise((r) => ws.addEventListener("open", r));
  ws.addEventListener("message", (e) => {
    const m = JSON.parse(e.data);
    if (m.id && pending.has(m.id)) { (m.error ? pending.get(m.id).rej : pending.get(m.id).res)(m.error || m.result); pending.delete(m.id); }
  });
  await cdp("Page.enable");
  await cdp("Runtime.enable");
  await cdp("Page.addScriptToEvaluateOnNewDocument", { source: FAKE });
  await cdp("Page.navigate", { url: `http://127.0.0.1:${HTTP}/install/` });
  check(await until(`document.readyState === "complete" && !!document.getElementById("check")`), "the installer page loads");
  await sleep(300);

  await checkWith({ id: "FM-1_8010403", hello: null, asked: false }, [/DUB-1 1\.4\.3: it has no sets yet/], "1.4.3");
  await checkWith({ id: "FM-1_8010500", hello: "DUB-1 1.5.0-BETA1", asked: true },
                  [/runs DUB-1 1\.5\.0-BETA1, a test build of 1\.5\.0, not the release/, /Sets page/],
                  "1.5.0-BETA1");
  check(await js(`!!document.querySelector("#checkres a[href]")`), "... the Sets page linked");
  await checkWith({ id: "FM-1_8010500", hello: "DUB-1 1.5.0", asked: true }, [/runs DUB-1 1\.5\.0, the release:/], "1.5.0");
  await checkWith({ id: "FM-1_8010500", hello: null, asked: true },
                  [/runs DUB-1 1\.5\.0 \(its full version not read: a test build and the release look the same this way\)/],
                  "1.5.0 not answering HELLO");
  await checkWith({ id: "FM-1_800", hello: "DUB-1 DEV", asked: true }, [/development build of DUB-1 \(DEV\)/], "a development build");
  await checkWith({ id: "FM-1_015", hello: null, asked: false }, [/runs FM-1_015, not DUB-1/], "stock");
  await js(`document.getElementById("lang").click()`);
  check(await until(`document.documentElement.lang === "ja"`), "日本語");
  await checkWith({ id: "FM-1_8010500", hello: "DUB-1 1.5.0-BETA1", asked: true },
                  [/DUB-1 1\.5\.0-BETA1 です（1\.5\.0 のテスト版で、正式版ではありません）/], "日本語: 1.5.0-BETA1");
  await checkWith({ id: "FM-1_8010500", hello: "DUB-1 1.5.0", asked: true }, [/DUB-1 1\.5\.0 です（正式版）/], "日本語: 1.5.0");
  check(await js(`/ハブを通さず/.test(document.querySelector("[data-t=direct]").textContent)`), "日本語: before installing, no hub");
  await js(`document.getElementById("lang").click()`);
  check(await until(`document.documentElement.lang === "en"`), "English again");
  check(await js(`/not through a hub/.test(document.querySelector("[data-t=direct]").textContent) &&
                  /switched on/.test(document.querySelector("[data-t=direct]").textContent)`),
        "before installing: straight into the computer, not through a hub; keep it switched on");
  check(await js(`document.getElementById("logbox").hidden && document.getElementById("help").hidden`),
        "no log button, no recovery links before an install");
  // an install whose step 1 gets no answer (this fake FM-1 doesn't serve the update): stopped, nothing written; the
  // session's log offered ("Copy the log"; headless, the clipboard may refuse: then the log is shown)
  check(await until(`!document.getElementById("go").disabled`), "the package loads: Install enabled");
  await js(`window.__fm1 = { id: "FM-1_8010500", hello: null }; document.getElementById("go").click()`);
  const stopped = await until(`!document.getElementById("logbox").hidden`, 30000);
  check(stopped && /Stopped\. Unplug the USB cable, plug it straight into the computer/.test(await js(`document.getElementById("alert").textContent`))
        && await js(`document.getElementById("help").hidden`), "step 1 unanswered: stopped, the log offered, no recovery links (nothing written)");
  await js(`document.getElementById("copylog").click()`);
  check(await until(`/Copied/.test(document.getElementById("copied").textContent)`, 5000), "Copy the log: copied");
  const logText = await js(`window.__clip || ""`);
  check(/step 1 \(check\) on FM-1_8010500/.test(logText) && /no request for \d/.test(logText) && /\n *[\d.]+  end\n?$/.test(logText),
        "... the log: the device, the step, the silence, the end");
} catch (e) {
  console.log("  FAIL " + (e && e.stack || e));
  fails++;
}
console.log(fails ? "installer browser: FAILED" : "installer browser: ok");
done(fails ? 1 : 0);
