// SPDX-License-Identifier: GPL-3.0-only
// The browser emulator end to end, in a headless Chromium driven over its DevTools protocol (no packages: Node's own
// fetch and WebSocket): the page served, switched on, a fake Web MIDI controller ("Fake Keys") put in its place
// before the page loads. It checks that MIDI in reaches the firmware (a note on channel 1 adds it to the scale, the
// screen says so). (1.5.0, step 5) And the saved objects: in IndexedDB, what an older page left in localStorage moved
// there once (seeded before the page loads); a set heard first (Hear a set: a .dub1 file loaded into the pieces).
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

const profile = fs.mkdtempSync(path.join(os.tmpdir(), "emu-browser-"));
// (step 6) served as the site has it (tools/make_pages.py): emu/, and sets/ beside it with the ready-made sets
const site = path.join(profile, "site");
fs.mkdirSync(path.join(site, "sets"), { recursive: true });
fs.symlinkSync(path.resolve(dir), path.join(site, "emu"));
const readySets = fs.existsSync("sets/sets.json") ? JSON.parse(fs.readFileSync("sets/sets.json", "utf8")) : [];
for (const o of readySets) fs.copyFileSync(path.join("sets", o.file), path.join(site, "sets", o.file));
if (readySets.length) fs.copyFileSync("sets/sets.json", path.join(site, "sets", "sets.json"));
const server = spawn("python3", ["-m", "http.server", String(HTTP), "--bind", "127.0.0.1"], { cwd: site, stdio: "ignore" });
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

// an older page's saved objects (localStorage, base64): the frame store's region, 4 KB of it erased (0xFF)
const SEED = `(() => { try { if (location.port === "${HTTP}" && !sessionStorage.getItem("seeded")) {
  sessionStorage.setItem("seeded", "1");
  localStorage.setItem("dub-emu-store", JSON.stringify({ 2: btoa(String.fromCharCode(...new Uint8Array(4096).fill(255))) }));
} } catch (e) {} })();`;

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
  await cdp("Page.addScriptToEvaluateOnNewDocument", { source: SEED });
  await cdp("DOM.enable");
  await cdp("Page.navigate", { url: `http://127.0.0.1:${HTTP}/emu/index.html` });
  check(await until(`document.readyState === "complete" && !!document.getElementById("go")`), "the page loads");

  // (beta2, the user's) the Sets page's "Hear it" before the emulator is on: shown, loaded by itself as it's switched on
  const b64 = fs.readFileSync("tests/host/cues-edited.dub1").toString("base64");
  await js(`localStorage.setItem("dub1-hear", JSON.stringify({ name: "Winter Set", from: "a test", b64: "${b64}", t: 1 })); location.hash = "#hear"`);
  check(await until(`/From the Sets page \\(a test\\): .Winter Set., 5 pieces.* loads when you switch on$/.test(document.getElementById("set-info").textContent)`),
        "Hear it (#hear) while off: shown, loads when switched on: " + await js(`document.getElementById("set-info").textContent`));
  await js(`document.getElementById("go").click()`);
  check(await until(`/^Set loaded into the pieces: Winter Set/.test(document.getElementById("status").textContent)`, 20000),
        "switched on (the wasm in its worklet), the set loaded by itself: " + await js(`document.getElementById("status").textContent`));
  check(!/switch on/.test(await js(`document.getElementById("set-info").textContent`)), "the box no longer says it waits");
  await js(`history.replaceState(null, "", location.pathname)`);
  await js(`document.getElementById("midi-on").click()`);
  check(await until(`document.getElementById("midi-status").textContent.includes("Fake Keys")`), "Connect MIDI finds the input");

  // MIDI in: a note on channel 1 (G, 55) into the scale; the screen says it
  // (every line the screen says, kept: the chord's own come and go between two looks. Each line from its own mutation
  // record, the text node it put in: several SAYs in one batch replace the text before the observer runs, and the
  // text as it is then is only the last of them)
  await sleep(1500);
  await js(`(() => { const t = document.getElementById("said-text"); window.__said = [];
                     new MutationObserver((rs) => { for (const r of rs) { for (const n of r.addedNodes) window.__said.push(n.textContent);
                                                                          if (r.type === "characterData") window.__said.push(r.target.textContent); } })
                       .observe(t, { childList: true, characterData: true, subtree: true }); })()`);
  await js(`window.__in([0x90, 55, 100])`);
  check(await until(`document.getElementById("midi-last").textContent.startsWith("90 37 64")`), "MIDI in shown: 90 37 64");
  // (the screen's "ADDED G", or fm1-reader's words for it, "added G", when the build has the reader)
  check(await until(`window.__said.some((t) => /(added|removed) G/i.test(t))`),
        "the firmware took it: " + JSON.stringify(await js(`window.__said.filter((t) => /G$/.test(t))`)));

  // (step 5) the store: IndexedDB, the older page's moved over once, localStorage emptied of it
  check(await js(`window.emuStore.mode`) === "IndexedDB" && await js(`window.emuStore.moved`) &&
        await js(`localStorage.getItem("dub-emu-store")`) === null, "the store in IndexedDB, localStorage's moved there once");
  // (step 5) a set heard first: a backup's pieces (a WORK) loaded into the pieces as a set
  {
    const doc = await cdp("DOM.getDocument");
    const n = await cdp("DOM.querySelector", { nodeId: doc.root.nodeId, selector: "#set-file" });
    await cdp("DOM.setFileInputFiles", { nodeId: n.nodeId, files: [path.resolve("tests/host/work-1.5.0-step4.dub1")] });
    check(await until(`/Fixture Set.*8 pieces: Basic, Dub Room/.test(document.getElementById("set-info").textContent) &&
                       !document.getElementById("set-load").disabled`), "Hear a set: the file's pieces shown");
    await js(`window.__said = []; document.getElementById("set-load").click()`);
    check(await until(`/Set loaded into the pieces: Fixture Set/.test(document.getElementById("status").textContent)`),
          "loaded: " + await js(`document.getElementById("status").textContent`));
    check(await until(`window.__said.some((t) => /set loaded/i.test(t))`), "the firmware said it: SET LOADED");
    // (step 6) a ready-made set chosen from the list (the site's sets/), heard: loaded into the pieces
    if (readySets.length) {
      check(await until(`!document.getElementById("set-readyrow").hidden &&
                         document.getElementById("set-ready").options.length === ${readySets.length + 1}`),
            "the ready-made sets listed: " + readySets.map((o) => o.name).join(", "));
      await js(`(() => { const s = document.getElementById("set-ready"); s.value = ${JSON.stringify(readySets[0].file)};
                s.dispatchEvent(new Event("change")); })()`);
      check(await until(`/${readySets[0].name}.*${readySets[0].pieces[0]}/.test(document.getElementById("set-info").textContent) &&
                         !document.getElementById("set-load").disabled`), "a ready-made set chosen: its pieces shown");
      await js(`document.getElementById("set-load").click()`);
      check(await until(`/Set loaded into the pieces: ${readySets[0].name}/.test(document.getElementById("status").textContent)`),
            "loaded: " + await js(`document.getElementById("status").textContent`));
    }
  }
  // (step 7b) the Sets page's cue editor's "Hear it": its set handed over in this browser's storage, the page at #hear;
  // (beta2) on already: loaded by itself, no button
  {
    await js(`document.getElementById("status").textContent = ""; localStorage.setItem("dub1-hear", JSON.stringify({ name: "Winter Set", from: "a test", b64: "${b64}", t: 2 })); location.hash = "#hear"`);
    check(await until(`/From the Sets page \\(a test\\): .Winter Set., 5 pieces/.test(document.getElementById("set-info").textContent) &&
                       !document.getElementById("set-load").disabled`), "Hear it (#hear): the cue editor's set shown");
    check(await until(`/Set loaded into the pieces: Winter Set/.test(document.getElementById("status").textContent)`),
          "on: loaded by itself: " + await js(`document.getElementById("status").textContent`));
  }
  check(await until(`new Promise((res) => { const r = indexedDB.open("dub-emu"); r.onsuccess = () => {
          const g = r.result.transaction("objects").objectStore("objects").get(2);
          g.onsuccess = () => { res(!!g.result && g.result.byteLength === 69632 * 4); r.result.close(); }; g.onerror = () => res(false); }; })`),
        "WORK saved with the set: the frame store's region (272 KB) in IndexedDB");

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
