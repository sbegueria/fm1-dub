// SPDX-License-Identifier: GPL-3.0-only
// The browser emulator end to end, in a headless Chromium driven over its DevTools protocol (no packages: Node's own
// fetch and WebSocket): the page served, switched on, a fake Web MIDI controller ("Fake Keys") put in its place
// before the page loads. It checks that MIDI in reaches the firmware (a note on channel 1 adds it to the scale, the
// screen says so).
//   node tests/host/emu_browser_test.mjs build/emu [chromium] [shot.png]
import { spawn } from "child_process";
import fs from "fs";
import os from "os";
import path from "path";

const dir = process.argv[2] || "build/emu";
const chrome = process.argv[3] || "chromium";
const shot = process.argv[4];
const HTTP = 8765, CDP = 9333;
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
let fails = 0;
const check = (ok, what) => { console.log(`  ${ok ? "ok  " : "FAIL"} ${what}`); if (!ok) fails++; };

const server = spawn("python3", ["-m", "http.server", String(HTTP), "--bind", "127.0.0.1", "-d", dir], { stdio: "ignore" });
const profile = fs.mkdtempSync(path.join(os.tmpdir(), "emu-browser-"));
const browser = spawn(chrome, ["--headless=new", `--remote-debugging-port=${CDP}`, `--user-data-dir=${profile}`,
  "--autoplay-policy=no-user-gesture-required", "--no-first-run", "--no-default-browser-check", "about:blank"], { stdio: "ignore" });
const done = async (code) => {   /* (the browser gone before its profile is removed: it writes into it as it quits) */
  const gone = new Promise((r) => browser.once("exit", r));
  browser.kill();
  server.kill();
  await Promise.race([gone, sleep(5000)]);
  try { fs.rmSync(profile, { recursive: true, force: true, maxRetries: 5, retryDelay: 200 }); } catch {}
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
async function until(expr, ms = 10000) {
  for (const t0 = Date.now(); Date.now() - t0 < ms; await sleep(100))
    if (await js(expr)) return true;
  return false;
}

// the fake controller: an input the test plays
const FAKE = `(() => {
  const input = { id: "in1", name: "Fake Keys", onmidimessage: null };
  navigator.requestMIDIAccess = async () => ({ inputs: new Map([["in1", input]]), outputs: new Map(), onstatechange: null });
  window.__in = (b) => input.onmidimessage && input.onmidimessage({ data: new Uint8Array(b), target: input });
})();`;

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
  await cdp("Page.navigate", { url: `http://127.0.0.1:${HTTP}/index.html` });
  check(await until(`document.readyState === "complete" && !!document.getElementById("go")`), "the page loads");

  await js(`document.getElementById("go").click()`);
  check(await until(`document.getElementById("status").textContent.startsWith("On")`, 20000), "switched on (the wasm in its worklet)");
  await js(`document.getElementById("midi-on").click()`);
  check(await until(`document.getElementById("midi-status").textContent.includes("Fake Keys")`), "Connect MIDI finds the input");

  // MIDI in: a note on channel 1 (G, 55) into the scale; the screen says it
  // (every line the screen says, kept: the chord's own come and go between two looks)
  await sleep(1500);
  await js(`(() => { const t = document.getElementById("said-text"); window.__said = [];
                     new MutationObserver(() => window.__said.push(t.textContent)).observe(t, { childList: true, characterData: true, subtree: true }); })()`);
  await js(`window.__in([0x90, 55, 100])`);
  check(await until(`document.getElementById("midi-last").textContent.startsWith("90 37 64")`), "MIDI in shown: 90 37 64");
  // (the screen's "ADDED G", or fm1-reader's words for it, "added G", when the build has the reader)
  check(await until(`window.__said.some((t) => /(added|removed) G/i.test(t))`),
        "the firmware took it: " + JSON.stringify(await js(`window.__said.filter((t) => /G$/.test(t))`)));

  if (shot) {
    const r = await cdp("Page.captureScreenshot", { format: "png", clip: { x: 0, y: 0, width: 800, height: 560, scale: 1 } });
    fs.writeFileSync(shot, Buffer.from(r.data, "base64"));
  }
} catch (e) {
  console.log("  FAIL " + (e && e.message || e));
  fails++;
}
console.log(fails ? "emu browser: FAILED" : "emu browser: ok");
done(fails ? 1 : 0);
