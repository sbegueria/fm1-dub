// SPDX-License-Identifier: GPL-3.0-only
// (1.5.0, step 5) the host simulator (build/host/amb_host's "sxpipe") as an FM-1 for the Node tests of the Sets page
// (tests/host/dub1_sets_test.mjs, sets_browser_test.mjs): tools/dub1_set.py's HostLink in JS
import { spawn } from "child_process";
import fs from "fs";
import path from "path";

export const HOST = "build/host/amb_host";
// the simulator as an FM-1: frames in as "sx F0 .. F7" lines, its answers out as "sxout F0 .. F7"; cmd() runs a script
// line there (the buttons pressed so), resolved when its echo comes back
export class HostLink {
  constructor(script, dir, onFrame = null) {
    const file = path.join(dir, "start.amb");
    fs.writeFileSync(file, script + "sxpipe\n");
    this.q = []; this.waiter = null; this.acks = []; this.log = []; this.gone = false;
    this.p = spawn(HOST, [file, dir], { stdio: ["pipe", "pipe", "inherit"] });
    this.p.on("exit", () => { this.gone = true; });
    this.ready = new Promise((res) => { this.onReady = res; });
    let buf = "";
    this.p.stdout.on("data", (d) => {
      buf += d.toString("latin1");
      let i;
      while ((i = buf.indexOf("\n")) >= 0) {
        const ln = buf.slice(0, i); buf = buf.slice(i + 1);
        if (ln.startsWith("sxout ")) {
          const f = ln.slice(6).split(" ").map((x) => parseInt(x, 16));
          if (onFrame) { onFrame(f); continue; }  // (the browser test: straight to the page)
          this.q.push(f);
          if (this.waiter) { const w = this.waiter; this.waiter = null; w(); }
        } else if (ln === "sxpipe") this.onReady();
        else if (ln.startsWith("> ")) { const a = this.acks.find((x) => x.line === ln.slice(2)); if (a) { this.acks.splice(this.acks.indexOf(a), 1); a.res(true); } }
        else if (ln !== "sxdone") this.log.push(ln);
      }
    });
  }
  get lost() { return this.gone; }
  line(s) { if (this.gone) return false; this.p.stdin.write(s + "\n"); return true; }
  send(bytes) { return this.line("sx " + bytes.map((b) => b.toString(16).padStart(2, "0").toUpperCase()).join(" ")); }
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
  cmd(s) { return new Promise((res) => { this.acks.push({ line: s, res }); this.line(s); setTimeout(() => res(false), 30000); }); }
  close() { try { this.p.stdin.end(); } catch {} return new Promise((r) => { if (this.gone) r(); else this.p.on("exit", r); setTimeout(r, 5000); }); }
}

