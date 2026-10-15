#!/usr/bin/env python3
"""Static server for threaded Emscripten/Axmol WASM.

Serves Cross-Origin-Opener-Policy + Cross-Origin-Embedder-Policy so the
document can become crossOriginIsolated and SharedArrayBuffer works.
"""
from __future__ import annotations

import argparse
import mimetypes
import os
import sys
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer


# Ensure .wasm is application/wasm even on older Python mime maps.
mimetypes.add_type("application/wasm", ".wasm")
mimetypes.add_type("application/javascript", ".js")
mimetypes.add_type("application/octet-stream", ".data")


class WasmHandler(SimpleHTTPRequestHandler):
    extensions_map = {
        **getattr(SimpleHTTPRequestHandler, "extensions_map", {}),
        ".wasm": "application/wasm",
        ".js": "application/javascript",
        ".mjs": "application/javascript",
        ".data": "application/octet-stream",
        ".html": "text/html",
        ".css": "text/css",
        ".json": "application/json",
        ".png": "image/png",
        ".jpg": "image/jpeg",
        ".jpeg": "image/jpeg",
        ".svg": "image/svg+xml",
        ".ico": "image/x-icon",
    }

    def end_headers(self) -> None:
        # Required for SharedArrayBuffer / pthread WASM (crossOriginIsolated).
        self.send_header("Cross-Origin-Opener-Policy", "same-origin")
        self.send_header("Cross-Origin-Embedder-Policy", "require-corp")
        # CORP lets same-origin subresources satisfy COEP require-corp.
        self.send_header("Cross-Origin-Resource-Policy", "same-origin")
        # Avoid stale html/js during iterative rebuilds.
        self.send_header("Cache-Control", "no-cache")
        super().end_headers()

    def log_message(self, fmt: str, *args) -> None:
        sys.stderr.write("%s - %s\n" % (self.address_string(), fmt % args))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--root",
        default="/workspace/PelicanBike/build_wasm/bin/PelicanBike",
        help="Directory to serve (default: PelicanBike WASM bin)",
    )
    parser.add_argument("--host", default="0.0.0.0")
    parser.add_argument("--port", type=int, default=8088)
    args = parser.parse_args()

    root = os.path.abspath(args.root)
    if not os.path.isdir(root):
        print(f"error: root directory not found: {root}", file=sys.stderr)
        return 1

    os.chdir(root)
    handler = partial(WasmHandler, directory=root)
    httpd = ThreadingHTTPServer((args.host, args.port), handler)
    print(
        f"Serving {root} on http://{args.host}:{args.port}/ "
        f"(COOP=same-origin, COEP=require-corp, CORP=same-origin)",
        flush=True,
    )
    print(f"Open: http://127.0.0.1:{args.port}/PelicanBike.html", flush=True)
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print("\nShutting down.", flush=True)
    finally:
        httpd.server_close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
