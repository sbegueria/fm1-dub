// SPDX-License-Identifier: GPL-3.0-only
// (1.5.0) web/dub1container.js in Node, for the twins' test (tests/host/container_twins.sh):
//   node container_twin.mjs emit DIR N | dump FILE | recode IN OUT | names < LINES (each a name: its width, or "no") |
//   bundle OUT K:I:FILE .. | unbundle FILE (step 5: a BACKUP)
import fs from "fs";
import { parse, build, dump, sample, Refused, nameFits, bundle, unbundle, crc32 } from "../../web/dub1container.js";
const [cmd, a, b] = process.argv.slice(2);
if (cmd === "emit") {
  for (let k = 0; k < +b; k++) fs.writeFileSync(`${a}/j${String(k).padStart(3, "0")}.dub1`, sample(k));
} else if (cmd === "dump") {
  try {
    process.stdout.write(dump(parse(fs.readFileSync(a))));
  } catch (e) {
    if (!(e instanceof Refused)) throw e;
    console.log("REFUSED " + e.message);
    process.exit(1);
  }
} else if (cmd === "recode") {
  fs.writeFileSync(b, build(parse(fs.readFileSync(a))));
} else if (cmd === "bundle") {                  // (step 5) bundle OUT K:I:FILE ..: a BACKUP, its META "DUB-1 TEST"
  const objs = process.argv.slice(4).map((x) => {
    const [k, i, ...f] = x.split(":");
    return { kind: +k, index: +i, bytes: new Uint8Array(fs.readFileSync(f.join(":"))) };
  });
  fs.writeFileSync(a, bundle(objs, "DUB-1 TEST", 2));
} else if (cmd === "unbundle") {                // (step 5) a line an object: kind, index, length, CRC
  try {
    for (const o of unbundle(new Uint8Array(fs.readFileSync(a))))
      console.log(`${o.kind} ${o.index} ${o.bytes.length} ${crc32(o.bytes).toString(16).padStart(8, "0")}`);
  } catch (e) {
    if (!(e instanceof Refused)) throw e;
    console.log("REFUSED " + e.message);
    process.exit(1);
  }
} else if (cmd === "names") {
  const all = fs.readFileSync(0), out = [];
  let at = 0;
  for (let i = 0; i < all.length; i++) {
    if (all[i] !== 10) continue;
    const ln = all.subarray(at, i), f = new Uint8Array(16);
    at = i + 1;
    if (ln.length > 16) { out.push("no"); continue; }
    f.set(ln);
    const w = nameFits(f);
    out.push(w < 0 ? "no" : String(w));
  }
  process.stdout.write(out.join("\n") + (out.length ? "\n" : ""));
} else {
  console.log("emit DIR N | dump FILE | recode IN OUT | names");
  process.exit(2);
}
