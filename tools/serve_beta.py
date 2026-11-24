#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The Sets page and the emulator on this computer, for a beta's device test (tools/serve_beta.sh starts it):
127.0.0.1 only (Chrome allows Web MIDI with SysEx there, as on https; it still asks). Laid out as the site
(tools/make_pages.py) so the page finds what it finds there:

  PORT      /dub1_sets.html (and /sets/: the same page, as the site has it), its modules (web/); beside it the ready-made
            sets (sets/: sets.json, the .dub1 files), as the site's sets/; /emu/ the built emulator (build/emu: its
            amb.wasm), so the page's "Hear it" (../emu/#hear: this origin's storage) and its emulator links work
  PORT + 1  the emulator (build/emu), as before; its ready-made sets at /sets/ (its ../sets/sets.json)

Every answer "Cache-Control: no-store" (beta2: Chrome kept beta1's dub1container.js, and the page silently lost data).
Any python3 (the threads: socketserver's mixin, not 3.7's ThreadingHTTPServer).
  tools/serve_beta.py [PORT]
"""
import os
import posixpath
import socketserver
import sys
import threading
import urllib.parse
from http.server import HTTPServer, SimpleHTTPRequestHandler

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
WEB, SETS, EMU = (os.path.join(ROOT, d) for d in ("web", "sets", os.path.join("build", "emu")))


def under(base, rel):
    """rel (a URL path's parts) under base, never above it"""
    parts = [p for p in rel.split("/") if p and p not in (".", "..")]
    return os.path.join(base, *parts)


def page_map(rel):
    """(PORT) a URL path to a file: /emu/ the emulator; /sets/ as /; web/ first, then sets/"""
    if rel == "/emu" or rel.startswith("/emu/"):
        return under(EMU, rel[4:])
    if rel == "/sets" or rel.startswith("/sets/"):
        rel = rel[5:] or "/"
    if rel in ("", "/", "/index.html"):
        return os.path.join(WEB, "dub1_sets.html")
    p = under(WEB, rel)
    return p if os.path.exists(p) else under(SETS, rel)


class Handler(SimpleHTTPRequestHandler):
    extensions_map = dict(SimpleHTTPRequestHandler.extensions_map, **{
        ".wasm": "application/wasm", ".js": "text/javascript", ".mjs": "text/javascript",
        ".json": "application/json", ".dub1": "application/octet-stream", ".md": "text/plain; charset=utf-8"})
    mapper = None                                # (the server's: a URL path to a file)

    def translate_path(self, path):
        rel = posixpath.normpath(urllib.parse.unquote(path.split("?", 1)[0].split("#", 1)[0]))
        return self.mapper(rel if rel != "." else "/")   # (a folder: its index.html, as http.server does)

    def end_headers(self):
        self.send_header("Cache-Control", "no-store")
        SimpleHTTPRequestHandler.end_headers(self)

    def log_message(self, *a):                   # (quiet: the terminal shows the two addresses only)
        pass


class Server(socketserver.ThreadingMixIn, HTTPServer):
    daemon_threads = True
    allow_reuse_address = True


def serve(port, mapper):
    h = type("H%d" % port, (Handler,), {"mapper": staticmethod(mapper)})
    s = Server(("127.0.0.1", port), h)
    threading.Thread(target=s.serve_forever, daemon=True).start()
    return s


def main(argv):
    port = int(argv[1]) if len(argv) > 1 else 8000
    emu = os.path.isfile(os.path.join(EMU, "amb.wasm"))
    serve(port, page_map)
    if emu:
        serve(port + 1, lambda rel: under(SETS, rel[5:]) if rel.startswith("/sets/") else under(EMU, rel))
    print("the Sets page:  http://127.0.0.1:%d/dub1_sets.html   (Japanese: http://127.0.0.1:%d/dub1_sets.html?lang=ja)"
          % (port, port))
    if emu:
        print("the emulator:   http://127.0.0.1:%d/   (and http://127.0.0.1:%d/emu/, beside the page)" % (port + 1, port))
    else:
        print("(no build/emu/amb.wasm: the Sets page only; its emulator links and \"Hear it\" find nothing)")
    print("(open them in Chrome or Edge; Ctrl-C stops)")
    sys.stdout.flush()
    try:
        threading.Event().wait()
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main(sys.argv)
