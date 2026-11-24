#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""DUB-1's container (1.5.0): the bytes of a stored set, the working project (WORK) and a .dub1 file.

The Python twin of firmware/src/app/container.c (and web/dub1container.js): the same shape, read and written the
same. Only the codec: what the fields mean (a piece's values, their format) is the firmware's (project.c).

  header  "D1ST", u16 version 1, u16 header length 32, u32 total length, u32 proj format, u16 kind
          (1 WORK, 2 SET, 3 SONG, 4 SOUNDS, 5 INBOX), u16 pieces, u32 flags, u32 reserved x 2
  chunks  4-byte id, u32 length, payload, zeros to a multiple of 4; an unknown id is kept as it is
          META 44  name[16], writer version[16], u32 source, u32 migrated, u32 saves
          GLOB     u8 piece leds screen values trans nsir nsk setlen, u16 pool, u16 npar, i8 sir[nsir*nsk], i16 par[npar]
          PIEC     u8 index kind recipe screen pings_ok flags, u16 nparams pool npings, u32 seed, name[16],
                   u8 pings[npings], FULL i16 par[nparams] / DELTA (u16 index, i16 value) x nparams / RECIPE none
          CUES     (step 7) u8 index, u8 count (1..64), u16 flags, then count cues of 8 bytes sorted by pos: u16 pos
                   (part << 14 | bar << 4 | step), u8 op, u8 arg, i16 val, u16 len
          END 4    u32 CRC-32 (zlib) of every byte before it
All little-endian; the shape strict (a known chunk's length exact, padding zero): what reads, writes back the same.

  (1.5.0, step 5) A BACKUP (kind 6): the web page's and dub1_set.py's whole backup in one file, WORK and every stored
  set: META (the writer's version), then an "OBJ " chunk an object: u8 kind, u8 index, u16 0, the object's own
  container whole (its own CRC). Made and read here and in the JS twin only: the device never takes one (bundle,
  unbundle).

  A name (a piece's, the set's) as the device takes it: name_fits (firmware/src/app/names.h's twin: the characters, up to
  12, and no wider than the header's tag holds, by the header font's widths, NAME_W; tools/gen_name_w.py checks the
  table is the firmware's).

  dub1_container.py dump FILE           the canonical dump (container_test's and the JS twin's, line for line)
  dub1_container.py recode IN OUT       read and written again
  dub1_container.py emit DIR N          N sample containers (DIR/p000.dub1 ..), for the twins' test
  dub1_container.py names < LINES       each line a name: its width if the device takes it, else "no" (the names' twins)
  dub1_container.py bundle OUT K:I:FILE ..   a BACKUP of those objects (kind K, index I), its META "DUB-1 TEST"
  dub1_container.py unbundle FILE       a BACKUP's objects, a line each: kind, index, length, CRC (the twins' test)
"""
import random
import struct
import sys
import zlib

MAGIC = b"D1ST"
VERSION = 1
HDR = 32
FULL, RECIPE, DELTA = 0, 1, 2
KINDS = {1: "WORK", 2: "SET", 3: "SONG", 4: "SOUNDS", 5: "INBOX", 6: "BACKUP"}
BACKUP = 6


# (names.h's) the widest name in pixels, the characters at most, and the header font's widths ' '..'~' (the digits tabular)
NAME_PX = 87
NAME_CHARS = 12
NAME_W = [3, 5, 5, 10, 8, 12, 10, 3, 5, 5, 6, 7, 4, 6, 4, 7, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 5, 4, 7, 7, 7, 7,
          12, 9, 8, 8, 8, 8, 7, 8, 8, 4, 8, 8, 7, 9, 9, 8, 8, 8, 8, 8, 8, 8, 8, 12, 8, 8, 7, 6, 7, 6, 7, 7, 3, 7, 8, 7, 8, 7,
          5, 7, 7, 4, 4, 7, 3, 11, 7, 8, 8, 8, 5, 7, 5, 7, 7, 10, 7, 7, 6, 5, 3, 5, 8]
NAME_MARKS = " -.'&!?/+#():,_"


def name_fits(field):
    """a name field (bytes: the name, then NULs to its end) as the device takes it: its width in pixels, or None"""
    field, w, end = bytes(field), 0, False
    for i, c in enumerate(field):
        if not c:
            end = True
        elif end or i >= NAME_CHARS or not (c < 128 and (chr(c).isalnum() or chr(c) in NAME_MARKS)):
            return None
        else:
            w += NAME_W[c - 32]
    if (field and field[0] == 0x20) or w > NAME_PX:
        return None
    return w


def name_field(text):
    """a name as its 16-byte field (Refused if the device would not take it)"""
    f = text.encode("latin-1", "replace")
    if len(f) > 16 or name_fits(f + bytes(16 - len(f))) is None:
        raise Refused("the name %r: up to %d letters, numbers, spaces and %s, no wider than %d px" % (
            text, NAME_CHARS, NAME_MARKS.strip(), NAME_PX))
    return f + bytes(16 - len(f))


class Refused(Exception):
    """not a container this reader takes (shape, version, CRC)"""


def _pad(n):
    return (4 - n % 4) % 4


def parse(data):
    """bytes -> {'header': {...}, 'chunks': [...]} (each chunk a dict with 'id'); Refused if it doesn't read"""
    data = bytes(data)
    if len(data) < HDR + 12:
        raise Refused("short")
    if data[:4] != MAGIC:
        raise Refused("magic")
    version, hlen, total, fmt, kind, npieces, flags, r0, r1 = struct.unpack_from("<HHIIHHIII", data, 4)
    if version != VERSION:
        raise Refused("version %d" % version)
    if hlen != HDR or total != len(data) or len(data) % 4:
        raise Refused("header")
    header = dict(version=version, hlen=hlen, total=total, format=fmt, kind=kind, npieces=npieces, flags=flags,
                  rsv=[r0, r1])
    chunks, at = [], HDR
    while True:
        if len(data) - at < 8:
            raise Refused("no END")
        cid, ln = data[at:at + 4], struct.unpack_from("<I", data, at + 4)[0]
        if cid == b"END ":
            if ln != 4 or len(data) - at != 12:
                raise Refused("END")
            end = at
            break
        if ln > len(data) - at - 8 or ln + _pad(ln) > len(data) - at - 8:
            raise Refused("chunk length")
        pay = data[at + 8:at + 8 + ln]
        if any(data[at + 8 + ln:at + 8 + ln + _pad(ln)]):
            raise Refused("padding")
        chunks.append(_chunk(cid, pay))
        at += 8 + ln + _pad(ln)
    crc = struct.unpack_from("<I", data, end + 8)[0]
    if zlib.crc32(data[:end]) & 0xFFFFFFFF != crc:
        raise Refused("CRC")
    return dict(header=header, chunks=chunks, crc=crc)


def _chunk(cid, p):
    if cid == b"META":
        if len(p) != 44:
            raise Refused("META")
        source, migrated, saves = struct.unpack_from("<III", p, 32)
        return dict(id=cid, name=p[:16], fw=p[16:32], source=source, migrated=migrated, saves=saves)
    if cid == b"GLOB":
        if len(p) < 12:
            raise Refused("GLOB")
        piece, leds, screen, values, trans, nsir, nsk, setlen, pool, npar = struct.unpack_from("<8BHH", p, 0)
        ns = nsir * nsk
        if ns > 64 or len(p) != 12 + ns + 2 * npar:
            raise Refused("GLOB")
        return dict(id=cid, piece=piece, leds=leds, screen=screen, values=values, trans=trans, nsir=nsir, nsk=nsk,
                    setlen=setlen, pool=pool, sir=list(struct.unpack_from("<%db" % ns, p, 12)),
                    par=list(struct.unpack_from("<%dh" % npar, p, 12 + ns)))
    if cid == b"PIEC":
        if len(p) < 32:
            raise Refused("PIEC")
        index, kind, recipe, screen, pings_ok, flags, nparams, pool, npings, seed = struct.unpack_from("<6BHHHI", p, 0)
        if kind > DELTA:
            raise Refused("PIEC kind")
        nd = nparams if kind == FULL else 2 * nparams if kind == DELTA else 0
        if (kind == RECIPE and nparams) or len(p) != 32 + npings + 2 * nd:
            raise Refused("PIEC")
        d = dict(id=cid, index=index, kind=kind, recipe=recipe, screen=screen, pings_ok=pings_ok, flags=flags,
                 nparams=nparams, pool=pool, seed=seed, name=p[16:32], pings=p[32:32 + npings])
        v = struct.unpack_from("<%dh" % nd, p, 32 + npings)
        d["par"] = list(v) if kind == FULL else [(v[2 * i] & 0xFFFF, v[2 * i + 1]) for i in range(nparams)] \
            if kind == DELTA else []
        return d
    if cid == b"CUES":
        if len(p) < 4:
            raise Refused("CUES")
        index, count, flags = struct.unpack_from("<BBH", p, 0)
        if not 1 <= count <= 64 or len(p) != 4 + 8 * count:
            raise Refused("CUES")
        cues = [struct.unpack_from("<HBBhH", p, 4 + 8 * i) for i in range(count)]
        if any(cues[i][0] < cues[i - 1][0] for i in range(1, count)):
            raise Refused("CUES order")
        return dict(id=cid, index=index, flags=flags, cues=[list(q) for q in cues])
    return dict(id=cid, data=p)


def _payload(c):
    cid = c["id"]
    if cid == b"META":
        return c["name"] + c["fw"] + struct.pack("<III", c["source"], c["migrated"], c["saves"])
    if cid == b"GLOB":
        ns = c["nsir"] * c["nsk"]
        return struct.pack("<8BHH", c["piece"], c["leds"], c["screen"], c["values"], c["trans"], c["nsir"], c["nsk"],
                           c["setlen"], c["pool"], len(c["par"])) + struct.pack("<%db" % ns, *c["sir"]) + \
            struct.pack("<%dh" % len(c["par"]), *c["par"])
    if cid == b"PIEC":
        kind = c["kind"]
        if kind == DELTA:
            flat = [v for i, x in c["par"] for v in (i - 65536 if i >= 32768 else i, x)]
        else:
            flat = list(c["par"]) if kind == FULL else []
        n = len(c["par"])
        return struct.pack("<6BHHHI", c["index"], kind, c["recipe"], c["screen"], c["pings_ok"], c["flags"], n,
                           c["pool"], len(c["pings"]), c["seed"]) + c["name"] + bytes(c["pings"]) + \
            struct.pack("<%dh" % len(flat), *flat)
    if cid == b"CUES":
        return struct.pack("<BBH", c["index"], len(c["cues"]), c["flags"]) + \
            b"".join(struct.pack("<HBBhH", *q) for q in c["cues"])
    return c["data"]


def build(c):
    """{'header': {...}, 'chunks': [...]} -> bytes (the total and END's CRC worked out here)"""
    h = c["header"]
    body = b""
    for ch in c["chunks"]:
        p = _payload(ch)
        body += ch["id"] + struct.pack("<I", len(p)) + p + bytes(_pad(len(p)))
    total = HDR + len(body) + 12
    out = MAGIC + struct.pack("<HHIIHHIII", VERSION, HDR, total, h["format"], h["kind"], h["npieces"], h["flags"],
                              h["rsv"][0], h["rsv"][1]) + body + b"END " + struct.pack("<I", 4)
    return out + struct.pack("<I", zlib.crc32(out[:-8]) & 0xFFFFFFFF)   # (END's own 8 bytes not in it)


def bundle(objs, fw="", source=2):
    """(step 5) objects [(kind, index, bytes)] as one BACKUP file (each object a container: Refused if one isn't); fw:
    the writer's version (the FM-1's, from HELLO); source: META's (0 device, 1 web, 2 host script)"""
    chunks = [dict(id=b"META", name=bytes(16), fw=fw.encode("latin-1")[:16].ljust(16, b"\0"), source=source,
                   migrated=0, saves=0)]
    fmt = 0
    for kind, index, data in objs:
        fmt = max(fmt, parse(data)["header"]["format"])
        chunks.append(dict(id=b"OBJ ", data=bytes([kind, index, 0, 0]) + bytes(data)))
    return build(dict(header=dict(format=fmt, kind=BACKUP, npieces=len(objs), flags=0, rsv=[0, 0]), chunks=chunks))


def unbundle(data):
    """(step 5) a BACKUP file's objects: [(kind, index, bytes)], each read (Refused if the file or one isn't)"""
    c = parse(data)
    if c["header"]["kind"] != BACKUP:
        raise Refused("not a backup (kind %d)" % c["header"]["kind"])
    out = []
    for ch in c["chunks"]:
        if ch["id"] == b"OBJ ":
            d = ch["data"]
            if len(d) < 4 + HDR + 12:
                raise Refused("OBJ")
            parse(d[4:])
            out.append((d[0], d[1], bytes(d[4:])))
    if len(out) != c["header"]["npieces"]:
        raise Refused("OBJ count")
    return out


def name(field):
    """a name field (16 bytes, NUL-padded) as text"""
    return field.split(b"\0", 1)[0].decode("latin-1")


def dump(c):
    """the canonical dump: one line a chunk (container_test.c's and dub1container.js's, the same)"""
    h = c["header"]
    out = ["D1ST version=%d hlen=%d total=%d format=%d kind=%d npieces=%d flags=%d rsv=%d,%d" % (
        h["version"], h["hlen"], h["total"], h["format"], h["kind"], h["npieces"], h["flags"], h["rsv"][0],
        h["rsv"][1])]
    lst = lambda v: ",".join(str(x) for x in v) or "-"
    for ch in c["chunks"]:
        cid = ch["id"]
        if cid == b"META":
            out.append("META name=%s fw=%s source=%d migrated=%d saves=%d" % (
                ch["name"].hex(), ch["fw"].hex(), ch["source"], ch["migrated"], ch["saves"]))
        elif cid == b"GLOB":
            out.append("GLOB piece=%d leds=%d screen=%d values=%d trans=%d nsir=%d nsk=%d setlen=%d pool=%d npar=%d sir=%s"
                       " par=%s" % (ch["piece"], ch["leds"], ch["screen"], ch["values"], ch["trans"], ch["nsir"],
                                    ch["nsk"], ch["setlen"], ch["pool"], len(ch["par"]), lst(ch["sir"]), lst(ch["par"])))
        elif cid == b"PIEC":
            data = ",".join("%d:%d" % p for p in ch["par"]) if ch["kind"] == DELTA else lst(ch["par"])
            out.append("PIEC index=%d kind=%d recipe=%d screen=%d pings_ok=%d flags=%d nparams=%d pool=%d npings=%d "
                       "seed=%d name=%s pings=%s data=%s" % (
                           ch["index"], ch["kind"], ch["recipe"], ch["screen"], ch["pings_ok"], ch["flags"],
                           len(ch["par"]), ch["pool"], len(ch["pings"]), ch["seed"], ch["name"].hex(),
                           bytes(ch["pings"]).hex() or "-", data or "-"))
        elif cid == b"CUES":
            out.append("CUES index=%d count=%d flags=%d cues=%s" % (
                ch["index"], len(ch["cues"]), ch["flags"], ",".join("%d:%d:%d:%d:%d" % tuple(q) for q in ch["cues"])))
        else:
            out.append("CHNK id=%s len=%d data=%s" % (cid.hex(), len(ch["data"]), ch["data"].hex() or "-"))
    out.append("END crc=%08x" % c["crc"])
    return "\n".join(out) + "\n"


def _rnd_name(r):
    chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 -.'&!"
    s = "".join(r.choice(chars) for _ in range(r.randrange(13))).encode()
    return s + bytes(16 - len(s))


def sample(seed):
    """a random container of every kind of chunk (the twins' test)"""
    r = random.Random(seed)
    chunks = []
    if r.random() < 0.75:
        chunks.append(dict(id=b"META", name=_rnd_name(r), fw=b"1.5.0".ljust(16, b"\0"), source=r.randrange(3),
                           migrated=r.choice([0, r.randrange(1 << 32)]), saves=r.randrange(1000)))
    if r.random() < 0.67:
        nsir, nsk = r.randrange(5), r.randrange(5)
        chunks.append(dict(id=b"GLOB", piece=r.randrange(256), leds=r.randrange(256), screen=r.randrange(256),
                           values=r.randrange(256), trans=r.randrange(256), nsir=nsir, nsk=nsk, setlen=r.randrange(2),
                           pool=r.randrange(65536), sir=[r.randrange(-128, 128) for _ in range(nsir * nsk)],
                           par=[r.randrange(-32768, 32768) for _ in range(r.randrange(200))]))
    np = r.randrange(9)
    for _ in range(np + (r.random() < 0.3)):
        if r.random() < 0.2:
            chunks.append(dict(id=b"X" + bytes([65 + r.randrange(26)]) + b"YZ",
                               data=bytes(r.randrange(256) for _ in range(r.randrange(40)))))
        kind = r.randrange(3)
        n = 0 if kind == RECIPE else r.randrange(200)
        par = [r.randrange(-32768, 32768) for _ in range(n)] if kind == FULL else \
            [(r.randrange(65536), r.randrange(-32768, 32768)) for _ in range(n)] if kind == DELTA else []
        chunks.append(dict(id=b"PIEC", index=r.randrange(8), kind=kind, recipe=r.randrange(8), screen=r.randrange(256),
                           pings_ok=r.randrange(2), flags=r.randrange(256), pool=r.randrange(65536),
                           seed=r.choice([0, r.randrange(1 << 32)]), name=_rnd_name(r),
                           pings=bytes(r.randrange(256) for _ in range(r.randrange(100))), par=par))
        if r.random() < 0.33:                    # (step 7) a cue list, its positions sorted
            n = 1 + (r.randrange(12) if r.random() < 0.75 else r.randrange(64))
            pos, cues = r.randrange(4096) if r.random() < 0.75 else r.randrange(65536), []
            for _ in range(n):
                pos += r.randrange(40) if r.random() < 0.67 else 0
                cues.append([min(pos, 65535), r.randrange(256), r.randrange(256), r.randrange(-32768, 32768),
                             r.randrange(64) if r.random() < 0.5 else r.randrange(65536)])
            chunks.append(dict(id=b"CUES", index=r.randrange(8), flags=0 if r.random() < 0.875 else r.randrange(65536),
                               cues=cues))
    h = dict(version=1, hlen=32, total=0, format=8 + r.randrange(40), kind=1 + r.randrange(5), npieces=np,
             flags=r.randrange(4), rsv=[r.randrange(1 << 32), r.randrange(1 << 32)] if r.random() < 0.25 else [0, 0])
    return build(dict(header=h, chunks=chunks))


def main(argv):
    if len(argv) == 3 and argv[1] == "dump":
        try:
            sys.stdout.write(dump(parse(open(argv[2], "rb").read())))
        except Refused as e:
            print("REFUSED %s" % e)
            return 1
        return 0
    if len(argv) == 4 and argv[1] == "recode":
        open(argv[3], "wb").write(build(parse(open(argv[2], "rb").read())))
        return 0
    if len(argv) == 4 and argv[1] == "emit":
        for k in range(int(argv[3])):
            open("%s/p%03d.dub1" % (argv[2], k), "wb").write(sample(k))
        return 0
    if len(argv) == 2 and argv[1] == "namelist":   # (the names' twins' test: edge cases, then random ones)
        r = random.Random(5)
        pool = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 -.'&!?/+#():,_*;=@~\xe9\x7f"
        out = [b"", b"Night Drive", b"Dub Chamber", b"Echospace 12", b"Woodward Dub", b" Fog", b"WWWWW", b"WWWWWW",
               b"WWWWWWW", b"WWWWWWWW", b"iiiiiiiiiiii", b"iiiiiiiiiiiii"]
        for k in range(3000):
            n = r.randrange(18) if k % 3 else r.randrange(9)
            p = pool if k % 4 else pool[:62] + " "
            out.append(bytes(ord(r.choice(p)) for _ in range(n)))
        sys.stdout.buffer.write(b"".join(x + b"\n" for x in out))
        return 0
    if len(argv) >= 3 and argv[1] == "bundle":
        objs = []
        for a in argv[3:]:
            k, i, f = a.split(":", 2)
            objs.append((int(k), int(i), open(f, "rb").read()))
        open(argv[2], "wb").write(bundle(objs, "DUB-1 TEST"))
        return 0
    if len(argv) == 3 and argv[1] == "unbundle":
        try:
            for k, i, d in unbundle(open(argv[2], "rb").read()):
                print("%d %d %d %08x" % (k, i, len(d), zlib.crc32(d) & 0xFFFFFFFF))
        except Refused as e:
            print("REFUSED %s" % e)
            return 1
        return 0
    if len(argv) == 2 and argv[1] == "names":
        for ln in sys.stdin.buffer.read().split(b"\n")[:-1]:
            w = name_fits(ln + bytes(16 - len(ln))) if len(ln) <= 16 else None
            print("no" if w is None else w)
        return 0
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv))
