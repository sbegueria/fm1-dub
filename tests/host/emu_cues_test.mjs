// SPDX-License-Identifier: GPL-3.0-only
// (1.5.0, step 7b) A set edited on the Sets page (web/dub1cues.js), heard in the emulator (build/emu/amb.wasm, in Node):
// loaded as "Hear a set" (and the cue editor's "Hear it") loads one (web_set_load: the firmware's own decoder, as the
// FM-1 loads a set), PLAY, and its cues play on their bars: the kick muted by a cue at bar 3 (from PLAY) and back at bar
// 5, a chord throw held 2 bars from bar 6; nothing before.
//   node tests/host/emu_cues_test.mjs build/emu-cues/amb.wasm   (DUB1_CUES=1 EMU_OUT=build/emu-cues web/emu/build.sh)
//   node tests/host/emu_cues_test.mjs build/emu/amb.wasm off     ((1.5.0) the page's, DUB1_CUES 0: the cues loaded, none
//                                                                  played: no mute, no throw, on any bar)
import fs from "fs";
import * as C from "../../web/dub1cues.js";
import { asSet } from "../../web/dub1sets.js";

const wasm = fs.readFileSync(process.argv[2] || "build/emu-cues/amb.wasm"), OFF = process.argv[3] === "off";
let mem, failed = 0;
const ok = (c, what) => { console.log(`${what.padEnd(76)} ${c ? "ok" : "FAIL"}`); if (!c) failed++; };
const wasi = new Proxy({ clock_time_get: (id, p, out) => { new DataView(mem.buffer).setBigUint64(out, 0n, true); return 0; },
  fd_write: (fd, iov, n, out) => { new DataView(mem.buffer).setUint32(out, 0, true); return 0; } }, { get: (t, k) => k in t ? t[k] : () => 0 });
const { instance } = await WebAssembly.instantiate(wasm, { wasi_snapshot_preview1: wasi, env: new Proxy({}, { get: () => () => 0 }) });
const ex = instance.exports; mem = ex.memory;
if (ex._initialize) ex._initialize();
ex.web_master(2800); ex.web_boot();
const B_PLAY = 10, ms = (n) => { for (let i = 0; i < n; i++) ex.web_render(44); };   // (44 frames ~ 1 ms)
const peek = (i) => ex.web_peek(i), bars = () => peek(0) / 16;
const untilBar = (b) => { for (let g = 0; g < 400000 && peek(0) < b * 16; g++) ex.web_render(44); };
const tap = (m) => { ex.web_buttons(1 << m); ms(80); ex.web_buttons(0); ms(80); };

ms(1500);
if (peek(2)) tap(B_PLAY);
ms(300);
ok(!peek(2), "the emulator booted, stopped");
// the Winter Set, edited: Hoarfrost's kick muted from bar 3 to bar 5, a chord throw held 2 bars at bar 6 (from PLAY)
const e = new C.Edit(new Uint8Array(fs.readFileSync("sets/winter-set.dub1")));
e.add(0, { pos: C.pos(C.BAR, 3, 0), op: C.MUTE, arg: 3, val: 1, len: 0 });
e.add(0, { pos: C.pos(C.BAR, 5, 0), op: C.MUTE, arg: 3, val: 0, len: 0 });
e.add(0, { pos: C.pos(C.BAR, 6, 0), op: C.MOVE, arg: 0, val: 0, len: 32 });
const bytes = asSet(e.bytes());
new Uint8Array(mem.buffer, ex.web_set_buf(), bytes.length).set(bytes);
const rc = ex.web_set_load(bytes.length);
ok(rc >= 0 && peek(5) === 0, `loaded as Hear a set loads it (${rc}): Hoarfrost playing`);
ms(300);
ok(peek(4) === 7, "Hoarfrost's cues: its 4 and the 3 added");
tap(B_PLAY);
ok(peek(2) === 1, "PLAY");
if (OFF) {                                       // (the cues off: bar by bar to 9, nothing muted or held)
  let quiet = true;
  for (let b = 1; b <= 9; b++) { untilBar(b); ms(100); quiet = quiet && peek(1) === 0 && peek(6) === 0; }
  ok(quiet, "the cues off: bars 1 to 9, no mute, no throw held");
  tap(B_PLAY);
  console.log(failed ? `emu cues off: ${failed} FAILED` : "emu cues off: all ok");
  process.exit(failed ? 1 : 0);
}
untilBar(2); ms(200);
ok(peek(1) === 0 && peek(6) === 0, "bar 2: nothing yet");
untilBar(3); ms(100);
ok((peek(1) & 8) === 8, `bar 3: the kick muted by its cue (at ${bars().toFixed(2)})`);
untilBar(4); ms(100);
ok((peek(1) & 8) === 8, "bar 4: still");
untilBar(5); ms(100);
ok((peek(1) & 8) === 0, "bar 5: back");
untilBar(6); ms(100);
ok(peek(6) === 1, "bar 6: the chord throw held");
untilBar(8); ms(100);
ok(peek(6) === 0, "bar 8: let go, after its 2 bars");
tap(B_PLAY);
ok(!peek(2) && peek(1) === 0, "stopped");
console.log(failed ? `emu cues: ${failed} FAILED` : "emu cues: all ok");
process.exit(failed ? 1 : 0);
