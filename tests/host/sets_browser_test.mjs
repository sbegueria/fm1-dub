// SPDX-License-Identifier: GPL-3.0-only
// (1.5.0, step 5) the Sets page (web/dub1_sets.html) end to end, in a headless Chrome driven over its DevTools protocol
// (no packages: Node's own fetch and WebSocket, as emu_browser_test.mjs), its Web MIDI a fake port named "FM-1" wired to
// the host simulator (build/host/amb_host's "sxpipe", tests/host/hostlink.mjs): what the page sends goes to the
// simulator's set protocol, its answers come back to the page as MIDI in. The simulator's buttons are pressed from
// here when the page says the FM-1 asks. Connect (the list, eight sets), a file chosen and shown (its pieces), sent
// (the question on the page; GLO: kept), the list again, a whole backup and a set downloaded (the bytes read back
// here), load (GLO), delete answered no (HOME), (Decisions 4) restore everything from that backup (one question, GLO:
// set 1 and the pieces back), the page in Japanese and back, and while playing: a send and a restore refused in the
// page's words.
//   node tests/host/sets_browser_test.mjs [chrome] [SHOTDIR]    (SHOTDIR: the page's screenshots, PNG)
import { spawn } from "child_process";
import fs from "fs";
import os from "os";
import path from "path";
import { HostLink, HOST } from "./hostlink.mjs";
import { unbundle, parse } from "../../web/dub1container.js";
import { Edit } from "../../web/dub1cues.js";

const chrome = process.argv[2] || "chromium";
const shots = process.argv[3];
const HTTP = 20000 + (process.pid % 10000), CDP = 30000 + (process.pid % 10000);
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
let fails = 0;
const check = (ok, what) => { console.log(`  ${ok ? "ok  " : "FAIL"} ${what}`); if (!ok) fails++; };
if (!fs.existsSync(HOST)) { console.log("no build/host/amb_host: run host/build_host.sh"); process.exit(1); }

const tmp = fs.mkdtempSync(path.join(os.tmpdir(), "sets-browser-"));
const five = path.join(tmp, "five.dub1"), three = path.join(tmp, "three.dub1");
// (step 6) the page served as the site has it (tools/make_pages.py): sets/ with its two modules, the ready-made sets
// and their list beside it
const site = path.join(tmp, "site"), sdir = path.join(site, "sets");
fs.mkdirSync(sdir, { recursive: true });
for (const f of ["dub1_sets.html", "dub1sets.js", "dub1container.js", "dub1cues.js", "dub1cuetables.js", "dub1makeset.js"]) fs.copyFileSync(path.join("web", f), path.join(sdir, f));
const readyList = JSON.parse(fs.readFileSync("sets/sets.json", "utf8"));
for (const o of readyList) fs.copyFileSync(path.join("sets", o.file), path.join(sdir, o.file));
fs.copyFileSync("sets/sets.json", path.join(sdir, "sets.json"));
const server = spawn("python3", ["-m", "http.server", String(HTTP), "--bind", "127.0.0.1"], { cwd: site, stdio: "ignore" });
const profile = path.join(tmp, "profile");
const browser = spawn(chrome, ["--headless=new", `--remote-debugging-port=${CDP}`, `--user-data-dir=${profile}`,
  "--no-first-run", "--no-default-browser-check", "--hide-scrollbars", "about:blank"], { stdio: "ignore" });
let link = null;
const done = async (code) => {
  const gone = new Promise((r) => browser.once("exit", r));
  browser.kill();
  server.kill();
  if (link) await link.close();
  await Promise.race([gone, sleep(5000)]);
  try { fs.rmSync(tmp, { recursive: true, force: true, maxRetries: 5, retryDelay: 200 }); } catch {}
  process.exit(code);
};
setTimeout(() => { console.log("  FAIL timed out"); done(1); }, 300000);

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
async function shot(name) {                      /* the whole page (its height), 640 px wide */
  if (!shots) return;
  const h = await js("Math.ceil(document.documentElement.scrollHeight)");
  const r = await cdp("Page.captureScreenshot", { format: "png", captureBeyondViewport: true,
    clip: { x: 0, y: 0, width: 640, height: h, scale: 1 } });
  fs.mkdirSync(shots, { recursive: true });
  fs.writeFileSync(path.join(shots, name), Buffer.from(r.data, "base64"));
}
async function shotEl(name, id) {               /* (step 7b) one part of the page: its box */
  if (!shots) return;
  const b = await js(`(() => { const r = document.getElementById(${JSON.stringify(id)}).getBoundingClientRect();
    return [r.left + scrollX, r.top + scrollY, r.width, r.height]; })()`);
  const r = await cdp("Page.captureScreenshot", { format: "png", captureBeyondViewport: true,
    clip: { x: Math.max(0, b[0] - 8), y: Math.max(0, b[1] - 8), width: Math.min(640, b[2] + 16), height: b[3] + 16, scale: 1 } });
  fs.mkdirSync(shots, { recursive: true });
  fs.writeFileSync(path.join(shots, name), Buffer.from(r.data, "base64"));
}
const same = (a, b) => a.length === b.length && a.every((x, i) => x === b[i]);
const txt = (id) => js(`document.getElementById(${JSON.stringify(id)}).textContent`);

// the fake Web MIDI: one port, "FM-1"; what the page sends goes to __sx (a binding: to the simulator), __in(bytes) is
// MIDI in from it
const FAKE = `(() => {
  const ls = new Set();
  const input = { id: "in", name: "FM-1", state: "connected", open: async () => {},
                  addEventListener: (t, f) => ls.add(f), removeEventListener: (t, f) => ls.delete(f) };
  const output = { id: "out", name: "FM-1", state: "connected", open: async () => {},
                   send: (b) => window.__sx(Array.from(b).join(" ")) };
  navigator.requestMIDIAccess = async (o) => ({ sysex: !!(o && o.sysex), inputs: new Map([["in", input]]),
                                                outputs: new Map([["out", output]]), onstatechange: null });
  window.__in = (b) => { const ev = { data: new Uint8Array(b), target: input }; ls.forEach((f) => f(ev)); };
})();`;

try {
  // (setsdelete: the sets the FM-1 comes with, stored at its first boot, deleted: the empty store this test was made for)
  const script = "wait 600\ntap PLAY\nwait 300\nsetsdelete\n" + ["name 0 Winter", "name 1 Night Drive",
    "name -1 Echo Set", "slotrecipe 2 7", `dub1set ${five} 5`, "name -1 Three", `dub1set ${three} 3 noglob`, "name -1 -"].join("\n") + "\n";
  link = new HostLink(script, tmp, (f) => { if (ws) cdp("Runtime.evaluate", { expression: `window.__in([${f.join(",")}])` }); });
  await link.ready;

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
    else if (m.method === "Runtime.bindingCalled" && m.params.name === "__sx")
      link.send(m.params.payload.split(" ").map(Number));
  });
  await cdp("Page.enable");
  await cdp("Runtime.enable");
  await cdp("DOM.enable");
  await cdp("Runtime.addBinding", { name: "__sx" });
  await cdp("Emulation.setDeviceMetricsOverride", { width: 640, height: 900, deviceScaleFactor: 1, mobile: false });
  await cdp("Page.addScriptToEvaluateOnNewDocument", { source: FAKE });
  await cdp("Page.navigate", { url: `http://127.0.0.1:${HTTP}/sets/dub1_sets.html` });
  check(await until(`document.readyState === "complete" && !!document.getElementById("connect")`), "the page loads");
  await sleep(300);
  await shot("sets_0_start.png");

  // connect: the FM-1 found, the list (the pieces now, eight sets, empty)
  await js(`document.getElementById("connect").click()`);
  check(await until(`!document.getElementById("dev").hidden && document.querySelectorAll("#sets tbody tr").length === 9`),
        "Connect: the FM-1 found, the pieces now and eight sets listed");
  check(/DUB-1 .*Stopped/.test(await txt("conn")), "its version, stopped: " + await txt("conn"));

  // a file chosen: its pieces shown before anything is sent
  const doc = await cdp("DOM.getDocument");
  const fileNode = await cdp("DOM.querySelector", { nodeId: doc.root.nodeId, selector: "#file" });
  await cdp("DOM.setFileInputFiles", { nodeId: fileNode.nodeId, files: [five] });
  check(await until(`!document.getElementById("preview").hidden && document.querySelectorAll("#preview li").length === 5`),
        "a file chosen: its five pieces shown");
  const pv = await txt("preview");
  check(/Echo Set/.test(pv) && /Winter/.test(pv) && /Haze/.test(pv), "its name and pieces: " + pv.slice(0, 90));
  check(await js(`document.getElementById("name").value`) === "Echo Set" && !(await js(`document.getElementById("send").disabled`)),
        "its name in the field, fits; Send ready");
  await js(`(() => { const n = document.getElementById("name"); n.value = "Woodward Dub"; n.dispatchEvent(new Event("input")); })()`);
  check(await js(`document.getElementById("send").disabled`) && /Too wide/.test(await txt("namefit")),
        "a name too wide (Woodward Dub): said, Send off");
  await js(`(() => { const n = document.getElementById("name"); n.value = "Echo Set"; n.dispatchEvent(new Event("input")); })()`);
  await shot("sets_1_file.png");

  // sent to set 1: the question on the page while the FM-1 asks; GLO there: kept
  await js(`document.getElementById("slot").value = "0"; document.getElementById("send").click()`);
  check(await until(`!document.getElementById("ask").hidden`), "Send: the page says the FM-1 asks");
  check(/STORE SET 1\? Echo Set/.test(await txt("ask-q")) && /GLO/.test(await txt("ask")), "the question and how to answer: " + await txt("ask-q"));
  await sleep(1300);
  await shot("sets_2_asking.png");
  await link.cmd("tap GLO");
  check(await until(`/Kept as set 1/.test(document.getElementById("status").textContent)`), "GLO on the FM-1: kept, said");
  check(await until(`document.getElementById("ask").hidden && /Echo Set/.test(document.querySelector("#sets tbody tr:nth-child(2)").textContent)`),
        "the list again: set 1, Echo Set");
  await shot("sets_3_kept.png");

  // a whole backup and a set downloaded: the bytes read back here
  await js(`window.dub1LastSave = null; document.getElementById("backup").click()`);
  check(await until(`window.dub1LastSave && /^dub1-backup-/.test(window.dub1LastSave.name)`), "Back up everything: a file");
  const whole = path.join(tmp, "whole.dub1");
  {
    const b = new Uint8Array(await js(`Array.from(window.dub1LastSave.bytes)`));
    const o = unbundle(b);
    check(o.length === 2 && o[1].index === 0 && o[1].bytes.length === fs.statSync(five).size &&
          o[1].bytes.every((x, i) => x === fs.readFileSync(five)[i]), "the backup: the pieces and set 1, as sent");
    fs.writeFileSync(whole, b);
  }
  await js(`window.dub1LastSave = null; [...document.querySelectorAll("#sets button")].find((b) => /Download set 1/.test(b.getAttribute("aria-label"))).click()`);
  check(await until(`window.dub1LastSave && window.dub1LastSave.name === "dub1-set-1-Echo-Set.dub1"`), "Download set 1: dub1-set-1-Echo-Set.dub1");
  check(parse(new Uint8Array(await js(`Array.from(window.dub1LastSave.bytes)`))).header.kind === 2, "its bytes a set");

  // load set 1 (GLO), then delete it answered no (HOME)
  await js(`[...document.querySelectorAll("#sets button")].find((b) => /Load set 1/.test(b.getAttribute("aria-label"))).click()`);
  check(await until(`!document.getElementById("ask").hidden && /LOAD SET 1\\? Echo Set/.test(document.getElementById("ask-q").textContent)`),
        "Load: LOAD SET 1? Echo Set");
  await link.cmd("tap GLO");
  check(await until(`/loaded into the pieces/.test(document.getElementById("status").textContent)`), "GLO: loaded");
  await link.cmd("expect setlen 5");
  await js(`[...document.querySelectorAll("#sets button")].find((b) => /Delete set 1/.test(b.getAttribute("aria-label"))).click()`);
  check(await until(`!document.getElementById("ask").hidden && /DELETE SET 1/.test(document.getElementById("ask-q").textContent)`), "Delete: asked");
  await link.cmd("tap HOME");
  check(await until(`/answered no on the FM-1/.test(document.getElementById("status").textContent)`), "HOME: not deleted, said");

  // (Decisions 4) restore everything from the backup made above: what it puts back shown first; one question (on the
  // page as on the FM-1); GLO once: set 1 and the pieces back (the pieces' names as they were then)
  await link.cmd("name 0 Changed");
  const rNode = await cdp("DOM.querySelector", { nodeId: doc.root.nodeId, selector: "#restorefile" });
  await cdp("DOM.setFileInputFiles", { nodeId: rNode.nodeId, files: [whole] });
  check(await until(`!document.getElementById("restoreview").hidden && !document.getElementById("restore").disabled`),
        "a backup chosen to restore: shown, Restore ready");
  const rv = await txt("restoreview");
  check(/Set 1: Echo Set, 5 pieces, replacing Echo Set/.test(rv) && /The pieces/.test(rv), "what it puts back: " + rv.slice(0, 120));
  await js(`document.getElementById("restore").click()`);
  check(await until(`!document.getElementById("ask").hidden && /RESTORE BACKUP\\? 1 set \\+ the pieces/.test(document.getElementById("ask-q").textContent)`),
        "Restore everything: one question, RESTORE BACKUP? 1 set + the pieces");
  await sleep(800);
  await shot("sets_5_restore_asking.png");
  await link.cmd("tap GLO");
  check(await until(`/Restored: 1 set and the pieces/.test(document.getElementById("status").textContent)`, 20000),
        "GLO once: restored, said: " + await txt("status"));
  await link.cmd("expectname 0 Winter");
  await link.cmd("workcheck");
  await shot("sets_6_restored.png");

  // (Decisions 4) in Japanese, as the installer: a click away, everything said again in it
  await js(`document.getElementById("lang").click()`);
  check(await until(`document.documentElement.lang === "ja" && document.querySelector("h1").textContent === "セット"`), "日本語: the page");
  check(/接続しました/.test(await txt("conn")) && /復元しました/.test(await txt("status")) &&
        /セット 1/.test(await txt("sets")) && (await txt("restore")) === "すべて復元", "日本語: the list, the status, the buttons");
  await shot("sets_7_ja.png");
  await js(`document.getElementById("lang").click()`);
  check(await until(`document.documentElement.lang === "en" && /Restored/.test(document.getElementById("status").textContent)`), "back to English");

  // (Decisions 5) a restore that fails on the flash (its power lost before set 1 comes): stopped there, the page says
  // which failed, that nothing was restored before it, and that the pieces were never sent
  await js(`document.getElementById("restore").click()`);
  check(await until(`!document.getElementById("ask").hidden`), "Restore everything again: asked");
  await link.cmd("powercut 1");
  await link.cmd("tap GLO");
  check(await until(`/stopped at set 1/.test(document.getElementById("alert").textContent)`, 20000) &&
        /Restored before it: nothing/.test(await txt("alert")) && /Not sent: the pieces/.test(await txt("alert")),
        "the flash failing: stopped, said: " + await txt("alert"));
  await shot("sets_8_restore_stopped.png");
  for (const l of ["reboot", "wait 300", "tap PLAY", "wait 300", "expect playing 0"])
    await link.cmd(l);

  // (step 6) the ready-made sets: listed (sets.json), each to download; the Winter Set sent from the list: its pieces
  // shown first, its name in the field; GLO: kept as set 3, then loaded (GLO), its five pieces in the simulator
  check(await until(`!document.getElementById("ready").hidden && document.querySelectorAll("#readylist li").length === ${readyList.length}`),
        "the ready-made sets listed: " + readyList.map((o) => o.name).join(", "));
  check(/Winter Set/.test(await txt("readylist")) && /Hoarfrost/.test(await txt("readylist")) &&
        await js(`document.querySelector("#readylist a").getAttribute("href")`) === readyList[0].file, "each with its pieces and a download");
  await shot("sets_9_ready.png");
  await js(`[...document.querySelectorAll("#readylist button")][0].click()`);
  check(await until(`/winter-set.dub1/.test(document.getElementById("preview").textContent) &&
                     document.querySelectorAll("#preview li").length === 5 && document.getElementById("name").value === "Winter Set"`),
        "Send to the FM-1: the Winter Set in the form, its five pieces shown: " + (await txt("preview")).slice(0, 90));
  // (the user, 2026-11-22) a new file: Into the first empty set, nothing said; a set already there chosen: said beside it
  check(await js(`(() => { const s = document.getElementById("slot"); return /\\(empty\\)/.test(s.options[s.selectedIndex].text) &&
                   [...s.options].findIndex((o) => /\\(empty\\)/.test(o.text)) === s.selectedIndex &&
                   document.getElementById("slotwarn").hidden; })()`), "a new file: Into the first empty set");
  await js(`(() => { const s = document.getElementById("slot"); s.value = "0"; s.dispatchEvent(new Event("change")); })()`);
  check(!(await js(`document.getElementById("slotwarn").hidden`)) && /replaces .+ on the FM-1/.test(await txt("slotwarn")),
        "a set already there chosen: said: " + await txt("slotwarn"));
  await js(`document.getElementById("slot").value = "2"; document.getElementById("send").click()`);
  check(await until(`!document.getElementById("ask").hidden && /STORE SET 3\\? Winter Set/.test(document.getElementById("ask-q").textContent)`),
        "sent: STORE SET 3? Winter Set");
  await link.cmd("tap GLO");
  check(await until(`/Kept as set 3/.test(document.getElementById("status").textContent)`), "GLO: kept as set 3");
  await js(`[...document.querySelectorAll("#sets button")].find((b) => /Load set 3/.test(b.getAttribute("aria-label"))).click()`);
  check(await until(`!document.getElementById("ask").hidden`), "Load set 3: asked");
  await link.cmd("tap GLO");
  check(await until(`/loaded into the pieces/.test(document.getElementById("status").textContent)`), "GLO: loaded");
  for (const l of ["expect setlen 5", "expectname 0 Hoarfrost", "expectname 4 Thaw", "workcheck"])
    await link.cmd(l);
  // (beta2, the user's) the Winter Set sent again into set 3, the set loaded: kept, and said the FM-1 plays the old one
  // till it's loaded again; the FM-1 then asks LOAD IT NOW? (HOME: no)
  await link.cmd("wait 3300");
  await js(`document.getElementById("slot").value = "2"; document.getElementById("send").click()`);
  check(await until(`!document.getElementById("ask").hidden && /STORE SET 3\\? Winter Set/.test(document.getElementById("ask-q").textContent)`),
        "sent again into set 3 (the set loaded): asked");
  await link.cmd("tap GLO");
  check(await until(`/keeps playing the old one until it's loaded again/.test(document.getElementById("status").textContent)`),
        "GLO: kept, the set loaded: " + await txt("status"));
  await shot("sets_9b_kept_live.png");
  for (const l of ["wait 3300", "expect asking 48", "expect again 1", "tap HOME", "wait 3300", "expect outcome 0",
                   "expectname 0 Hoarfrost"])
    await link.cmd(l);

  // (1.5.0, the user's, 2026-11-22) the cues kept but off: no Cues section, no Edit cues beside the sets, the pieces
  // now or the ready-made sets; the page again with ?cues=1 (the tests', ours): the editor there, connected again
  const cueUi = `[!document.getElementById("cues").hidden, [...document.querySelectorAll("#sets button, #readylist button")]
    .filter((b) => /Edit cues/.test(b.textContent)).length]`;
  check(JSON.stringify(await js(cueUi)) === "[false,0]", "the cues hidden: no Cues section, no Edit cues button");
  await cdp("Page.navigate", { url: `http://127.0.0.1:${HTTP}/sets/dub1_sets.html?cues=1` });
  check(await until(`document.readyState === "complete" && !!document.getElementById("connect")`), "?cues=1: the page again");
  await sleep(300);
  await js(`document.getElementById("connect").click()`);
  check(await until(`!document.getElementById("dev").hidden && document.querySelectorAll("#sets tbody tr").length === 9 &&
                     document.querySelectorAll("#readylist li").length === ${readyList.length}`), "connected again");
  const nSets = await js(`[...document.querySelectorAll("#sets button")].filter((b) => /^Load set/.test(b.getAttribute("aria-label"))).length`);
  check(JSON.stringify(await js(cueUi)) === `[true,${1 + nSets + readyList.length}]`,
        `?cues=1: the Cues section, Edit cues beside the pieces now, the ${nSets} sets and the ready-made ones`);
  {                                              // (the backup chosen to restore again, as before the page was left)
    const d = await cdp("DOM.getDocument");
    const n = await cdp("DOM.querySelector", { nodeId: d.root.nodeId, selector: "#restorefile" });
    await cdp("DOM.setFileInputFiles", { nodeId: n.nodeId, files: [whole] });
    check(await until(`!document.getElementById("restoreview").hidden && !document.getElementById("restore").disabled`),
          "the backup chosen to restore again");
  }

  // (step 7b) the cue editor: set 3 (the Winter Set) read from the FM-1 into it (Edit cues); Black Ice's cues: one
  // added (the kick muted at the main's bar 17), moved past the main (said: not heard), back; a value the FM-1 wouldn't
  // take refused in the form; one deleted; the page in Japanese; downloaded (the bytes read here: the cue there); sent
  // to set 4 (the Send form, the FM-1 asks: GLO), read back into the editor with it; Hear it (the set handed to the
  // emulator in this browser's storage)
  const sel = (id, v, ev = "change") => js(`(() => { const s = document.getElementById(${JSON.stringify(id)}); s.value = ${JSON.stringify(String(v))};
    s.dispatchEvent(new Event(${JSON.stringify(ev)})); })()`);
  const rows = () => js(`[...document.querySelectorAll("#cuetable tbody tr")].map((r) => r.textContent)`);
  await js(`[...document.querySelectorAll("#sets button")].find((b) => /Edit the cues of Set 3/.test(b.getAttribute("aria-label"))).click()`);
  check(await until(`!document.getElementById("cueed").hidden && /Set 3 on the FM-1/.test(document.getElementById("cuehead").textContent)`),
        "Edit cues beside set 3: read from the FM-1 into the editor: " + await txt("cuehead"));
  check(/5 pieces, 39 cues/.test(await txt("cuehead")) && (await rows()).length === 4 && /Intro, bar 1, 16th 1/.test((await rows())[0]),
        "Hoarfrost's 4 cues listed, by place: " + (await rows())[0]);
  check(await js(`document.querySelectorAll("#cuetl svg line.mark").length`) === 4, "its song drawn, a mark a cue");
  await sel("cuepiece", 1);
  check(await until(`document.querySelectorAll("#cuetable tbody tr").length === 9`), "Black Ice: 9 cues");
  await shotEl("cues_1_open.png", "cues");
  await js(`document.getElementById("cueadd").click()`);
  check(await until(`!document.getElementById("cueform").hidden`), "Add a cue: the form");
  await sel("cf-op", 0);
  await sel("cf-arg", 3);
  await sel("cf-valb", 1);
  await sel("cf-part", 1);
  await sel("cf-bar", 17, "input");
  await sel("cf-step", 0);
  check(!(await js(`document.getElementById("cf-ok").disabled`)) && (await txt("cf-why")) === "", "KICK muted, main bar 17: ready to add");
  await shotEl("cues_2_form.png", "cues");
  await js(`document.getElementById("cf-ok").click()`);
  check(await until(`document.querySelectorAll("#cuetable tbody tr").length === 10`), "added: 10 cues");
  let rs = await rows(), at = rs.findIndex((r) => /Main, bar 17, 16th 1/.test(r) && /KICK muted/.test(r));
  check(at === 0 && /Cue 1 added/.test(await txt("cuemsg")), "in its place (the first: Black Ice's others are later), said: " + await txt("cuemsg"));
  await js(`document.querySelectorAll("#cuetable tbody tr")[0].querySelector("button").click()`);
  await sel("cf-bar", 400, "input");
  check(/past the main \(296 bars\)/.test(await txt("cf-why")) && !(await js(`document.getElementById("cf-ok").disabled`)),
        "moved to the main's bar 400: allowed, said not heard: " + await txt("cf-why"));
  await js(`document.getElementById("cf-ok").click()`);
  check(await until(`document.querySelectorAll("#cuetable .out").length === 1 && document.querySelectorAll("#cuetl svg line.drop").length === 1`),
        "listed in its new place, marked not heard, dashed on the song");
  await shotEl("cues_3_notheard.png", "cues");
  rs = await rows();
  await js(`document.querySelectorAll("#cuetable tbody tr")[${rs.findIndex((r) => /bar 400/.test(r))}].querySelector("button").click()`);
  await sel("cf-bar", 17, "input");
  await js(`document.getElementById("cf-ok").click()`);
  check(await until(`/Main, bar 17, 16th 1/.test(document.querySelector("#cuetable tbody tr").textContent)`), "and back to bar 17");
  await js(`document.getElementById("cueadd").click()`);
  await sel("cf-op", 7);
  await sel("cf-arg", 15);
  await sel("cf-val", 101, "input");
  check(await js(`document.getElementById("cf-ok").disabled`) && /from 0 to 100/.test(await txt("cf-why")),
        "Motion to 101: refused in the form: " + await txt("cf-why"));
  await sel("cf-val", 80, "input");
  check(/^80 \(0…100\)/.test(await txt("cf-valt")) && !(await js(`document.getElementById("cf-ok").disabled`)), "Motion to 80: fine");
  await sel("cf-len", 2, "input");
  check(/= 2 bars/.test(await txt("cf-lent")), "over 2 bars");
  await shotEl("cues_4_ride.png", "cues");
  await js(`document.getElementById("cf-cancel").click()`);
  rs = await rows();
  await js(`[...document.querySelectorAll("#cuetable tbody tr")][${rs.length - 1}].querySelectorAll("button")[1].click()`);
  check(await until(`document.querySelectorAll("#cuetable tbody tr").length === 9`) && /Cue 10 deleted/.test(await txt("cuemsg")),
        "Black Ice's last deleted: 9 cues");
  await js(`document.getElementById("lang").click()`);
  check(await until(`document.querySelector("#cuetable thead").textContent.includes("場所") && /メイン、17 小節目/.test(document.querySelector("#cuetable tbody").textContent)`),
        "日本語: the editor");
  await shotEl("cues_5_ja.png", "cues");
  await js(`document.getElementById("lang").click()`);
  await until(`document.documentElement.lang === "en"`);
  await js(`window.dub1LastSave = null; document.getElementById("cuedl").click()`);
  check(await until(`window.dub1LastSave && window.dub1LastSave.name === "dub1-Winter-Set-cues.dub1"`), "Download the set: dub1-Winter-Set-cues.dub1");
  const edited = new Uint8Array(await js(`Array.from(window.dub1LastSave.bytes)`));
  {
    const e2 = new Edit(edited), bi = e2.pieces[1].cues;
    check(e2.pieces.map((p) => p.cues.length).join() === "4,9,8,12,6" && bi[0].pos === ((1 << 14) | (16 << 4)) && bi[0].op === 0 &&
          bi[0].arg === 3 && bi[0].val === 1, "its bytes: Black Ice's first cue the kick muted at main 16.0, its last gone");
  }
  await js(`document.getElementById("cuesend").click()`);
  check(await until(`!document.getElementById("sendform").hidden && document.getElementById("name").value === "Winter Set" &&
                     /dub1-Winter-Set-cues.dub1/.test(document.getElementById("preview").textContent)`), "Send to the FM-1: in the Send form");
  await js(`document.getElementById("slot").value = "3"; document.getElementById("send").click()`);
  check(await until(`!document.getElementById("ask").hidden && /STORE SET 4\\? Winter Set/.test(document.getElementById("ask-q").textContent)`),
        "the FM-1 asks: STORE SET 4? Winter Set");
  await link.cmd("tap GLO");
  check(await until(`/Kept as set 4/.test(document.getElementById("status").textContent)`), "GLO: kept as set 4");
  await js(`[...document.querySelectorAll("#sets button")].find((b) => /Edit the cues of Set 4/.test(b.getAttribute("aria-label"))).click()`);
  check(await until(`/Set 4 on the FM-1/.test(document.getElementById("cuehead").textContent)`) &&
        same(await js(`Array.from(window.dub1CueEditor().orig)`), edited), "set 4 read back: the edited set, byte for byte");
  await js(`localStorage.removeItem("dub1-hear"); document.getElementById("cuehear").click()`);
  check(await until(`!!localStorage.getItem("dub1-hear")`) && /emulator opens/.test(await txt("cuemsg")) &&
        same(Array.from(atob(JSON.parse(await js(`localStorage.getItem("dub1-hear")`)).b64), (c) => c.charCodeAt(0)), edited),
        "Hear it: the set handed to the emulator (this browser's storage), said");

  // (1.5.0) Make a set: set 3 (the Winter Set) read from the FM-1 and a file added (ten rows, eight ticked); three kept
  // (Hoarfrost, Aurora, the file's Winter), Aurora moved up to the top with the keyboard's buttons (the focus kept),
  // renamed (a name too wide refused first), the set named, Mode Song; downloaded (its pieces, names, cues with their
  // pieces), in Japanese, heard (handed to the emulator), sent (the Send form: STORE SET 5? GLO: kept), started again
  const mkRows = () => js(`document.querySelectorAll("#mk-list li").length`);
  const mkRow = (i, sel2) => `document.querySelectorAll("#mk-list li")[${i}].querySelector(${JSON.stringify(sel2)})`;
  check(!(await js(`document.getElementById("mk-srcrow").hidden`)) && await js(`document.getElementById("mk-conn").hidden`) &&
        JSON.stringify(await js(`[...document.getElementById("mk-src").options].map((o) => o.value)`)) ===
        JSON.stringify(["w", "s0", "s2", "s3", ...readyList.map((o, k) => "r" + k)]), "Make a set: its sources (the pieces now, sets 1, 3, 4, the ready-made)");
  await sel("mk-src", "s2");
  await js(`document.getElementById("mk-add").click()`);
  check(await until(`document.querySelectorAll("#mk-list li").length === 5 && /5 pieces added from Set 3/.test(document.getElementById("mk-msg").textContent)`),
        "set 3 read from the FM-1: its five pieces, said: " + await txt("mk-msg"));
  check(await js(`document.getElementById("mk-name").value`) === "Winter Set" && await js(`document.getElementById("mk-mode").value`) === "2" &&
        await js(`document.getElementById("mk-trans").value`) === "2", "the set's name, Mode Set and Transition Mix: the first's");
  {
    const d = await cdp("DOM.getDocument");
    const n = await cdp("DOM.querySelector", { nodeId: d.root.nodeId, selector: "#mk-file" });
    await cdp("DOM.setFileInputFiles", { nodeId: n.nodeId, files: [five] });
  }
  check(await until(`document.querySelectorAll("#mk-list li").length === 10`) &&
        await js(`[...document.querySelectorAll("#mk-list input[type=checkbox]")].filter((c) => c.checked).length`) === 8 &&
        /8 pieces in the set/.test(await txt("mk-count")), "a file added: ten rows, eight ticked (a set's most)");
  for (const i of [1, 2, 4, 6, 7])
    await js(`${mkRow(i, "input[type=checkbox]")}.click()`);
  check(/3 pieces in the set/.test(await txt("mk-count")) &&
        (await js(`[...document.querySelectorAll("#mk-list li:not(.off) input[type=text]")].map((i) => i.value).join("|")`)) === "Hoarfrost|Aurora|Winter",
        "three kept: Hoarfrost, Aurora, the file's Winter");
  for (let k = 0; k < 3; k++)
    await js(`(() => { const b = document.querySelector("#mk-list li:nth-child(${4 - k}) button[data-mv='-1']"); b.focus(); b.click(); })()`);
  check((await js(`[...document.querySelectorAll("#mk-list li:not(.off) input[type=text]")].map((i) => i.value).join("|")`)) === "Aurora|Hoarfrost|Winter" &&
        await js(`document.activeElement.getAttribute("aria-label")`) === "Move Aurora down" && /Aurora moved to place 1/.test(await txt("mk-msg")),
        "Aurora up three times: first, the focus on its Down (Up off at the top), said: " + await txt("mk-msg"));
  const typeIn = (expr, v) => js(`(() => { const n = ${expr}; n.value = ${JSON.stringify(v)}; n.dispatchEvent(new Event("input")); })()`);
  await typeIn(mkRow(0, "input[type=text]"), "Woodward Dub");
  check(await js(`document.getElementById("mk-dl").disabled`) && await js(`${mkRow(0, "input[type=text]")}.getAttribute("aria-invalid")`) === "true" &&
        /Too wide/.test(await js(`${mkRow(0, ".fit")}.textContent`)), "a piece's name too wide (Woodward Dub): marked, said, the buttons off");
  await typeIn(mkRow(0, "input[type=text]"), "Aurora Dub");
  await typeIn(`document.getElementById("mk-name")`, "My Mix");
  await sel("mk-mode", 1);
  check(!(await js(`document.getElementById("mk-dl").disabled`)) && /^Fits/.test(await txt("mk-namefit")), "renamed, the set named My Mix, Mode Song: ready");
  await shotEl("make_1.png", "make");
  if (shots) {                                   // (a phone's width: the rows wrapped, no scrolling sideways)
    await cdp("Emulation.setDeviceMetricsOverride", { width: 375, height: 800, deviceScaleFactor: 1, mobile: true });
    await sleep(200);
    check(await js(`document.documentElement.scrollWidth <= 375`), "a phone's width (375 px): nothing wider than the page");
    await shotEl("make_1_phone.png", "make");
    await cdp("Emulation.setDeviceMetricsOverride", { width: 640, height: 900, deviceScaleFactor: 1, mobile: false });
    await sleep(200);
  }
  await js(`window.dub1LastSave = null; document.getElementById("mk-dl").click()`);
  check(await until(`window.dub1LastSave && window.dub1LastSave.name === "dub1-set-My-Mix.dub1"`), "Download the set: dub1-set-My-Mix.dub1");
  const mine = new Uint8Array(await js(`Array.from(window.dub1LastSave.bytes)`));
  {
    const e3 = new Edit(mine);
    check(e3.name === "My Mix" && e3.pieces.map((p) => p.name).join("|") === "Aurora Dub|Hoarfrost|Winter" && e3.mode === 1 && e3.trans === 2 &&
          e3.pieces.map((p) => p.cues.length).join() === "12,4,0" && !e3.problems.length,
          "its bytes: My Mix, Aurora Dub, Hoarfrost, Winter; Song, Mix; the cues with their pieces (12, 4, 0)");
  }
  await js(`document.getElementById("lang").click()`);
  check(await until(`document.getElementById("h-make").textContent === "セットを作る" && /セットのピース: 3 個/.test(document.getElementById("mk-count").textContent)`) &&
        (await js(`${mkRow(0, "button[data-mv='1']")}.getAttribute("aria-label")`)) === "Aurora Dub を下へ" &&
        (await js(`${mkRow(0, "input[type=text]")}.value`)) === "Aurora Dub", "日本語: Make a set, what's typed kept");
  await shotEl("make_2_ja.png", "make");
  await js(`document.getElementById("lang").click()`);
  await until(`document.documentElement.lang === "en"`);
  await js(`localStorage.removeItem("dub1-hear"); document.getElementById("mk-hear").click()`);
  check(await until(`!!localStorage.getItem("dub1-hear")`) && same(Array.from(atob(JSON.parse(await js(`localStorage.getItem("dub1-hear")`)).b64), (c) => c.charCodeAt(0)), mine),
        "Hear it: the set handed to the emulator");
  await js(`document.getElementById("mk-send").click()`);
  check(await until(`!document.getElementById("sendform").hidden && document.getElementById("name").value === "My Mix" &&
                     document.querySelectorAll("#preview li").length === 3`), "Send to the FM-1: in the Send form, its three pieces shown");
  await js(`document.getElementById("slot").value = "4"; document.getElementById("send").click()`);
  check(await until(`!document.getElementById("ask").hidden && /STORE SET 5\\? My Mix/.test(document.getElementById("ask-q").textContent)`),
        "the FM-1 asks: STORE SET 5? My Mix");
  await link.cmd("tap GLO");
  check(await until(`/Kept as set 5/.test(document.getElementById("status").textContent)`), "GLO: kept as set 5");
  await link.cmd("sethas 4 My Mix");
  await js(`document.getElementById("mk-clear").click()`);
  check(await until(`document.getElementById("mk-ed").hidden`) && await js(`window.dub1MakeSet().rows.length`) === 0, "Start again: no pieces");

  // playing: the list read, a send refused in the page's words (the last answer's outcome on the FM-1's screen gone
  // first: 3 s, a button meanwhile only takes it away)
  await link.cmd("wait 3200");
  await link.cmd("tap PLAY");
  await sleep(300);
  await js(`document.getElementById("refresh").click()`);
  check(await until(`/playing/.test(document.getElementById("conn").textContent)`), "the list read while playing, said so");
  await js(`document.getElementById("slot").value = "3"; document.getElementById("send").click()`);
  check(await until(`/stop it first/.test(document.getElementById("alert").textContent)`), "a send while playing: " + await txt("alert"));
  await shot("sets_4_playing.png");
  await js(`document.getElementById("alert").textContent = ""; document.getElementById("restore").click()`);
  check(await until(`/stop it first/.test(document.getElementById("alert").textContent)`), "a restore while playing: refused");
  await link.cmd("tap PLAY");
  await link.cmd("wait 100");
  const bad = link.log.filter((l) => l.includes("FAIL"));
  check(!bad.length && link.log.some((l) => l.includes("ok setlen == 5")), "the simulator's checks held" + (bad.length ? ": " + bad.join("; ") : ""));
} catch (e) {
  console.log("  FAIL " + (e && e.stack || e));
  fails++;
}
console.log(fails ? "sets browser: FAILED" : "sets browser: ok");
done(fails ? 1 : 0);
