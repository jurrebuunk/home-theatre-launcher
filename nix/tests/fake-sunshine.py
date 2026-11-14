# A tiny stand-in for a GameStream / Sunshine host: serverinfo over HTTP, and
# over HTTPS (client certificate required) serverinfo, applist, appasset and
# cancel. /test/* endpoints let the test script set and read its state.
# Usage: fake-sunshine.py NAME UUID HTTP_PORT [HTTPS_PORT SERVER_CERT SERVER_KEY CLIENT_CERT]
import http.server
import json
import ssl
import struct
import sys
import threading
import zlib

name, uuid, http_port = sys.argv[1], sys.argv[2], int(sys.argv[3])
https_port = int(sys.argv[4]) if len(sys.argv) > 4 else 0
STATE = {"current": 0, "cancels": 0, "assets": 0}
APPS = [(101, "Desktop"), (102, "Steam Big Picture"), (103, "Ember Coast")]


def png():
    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))
    raw = b"".join(b"\0" + b"\x8a\xa1\xa8" * 3 for _ in range(4))
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", 3, 4, 8, 2, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(raw)) + chunk(b"IEND", b""))


class Handler(http.server.BaseHTTPRequestHandler):
    def log_message(self, *args):
        pass

    def send(self, body, kind="application/xml"):
        self.send_response(200)
        self.send_header("Content-Type", kind)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        path = self.path.split("?")[0]
        secure = isinstance(self.connection, ssl.SSLSocket)
        if path == "/serverinfo":
            current = STATE["current"] if secure else 0
            self.send((f'<?xml version="1.0" encoding="utf-8"?><root status_code="200">'
                       f"<hostname>{name}</hostname><uniqueid>{uuid}</uniqueid>"
                       f"<HttpsPort>{https_port or 47984}</HttpsPort><mac>00:11:22:33:44:55</mac>"
                       f"<PairStatus>{1 if secure else 0}</PairStatus><currentgame>{current}</currentgame>"
                       f"<state>SUNSHINE_SERVER_FREE</state></root>").encode())
        elif secure and path == "/applist":
            apps = "".join(f"<App><IsHdrSupported>0</IsHdrSupported><AppTitle>{n}</AppTitle><ID>{i}</ID></App>"
                           for i, n in APPS)
            self.send(f'<root status_code="200">{apps}</root>'.encode())
        elif secure and path == "/appasset":
            STATE["assets"] += 1
            self.send(png(), "image/png")
        elif secure and path == "/cancel":
            STATE["current"] = 0
            STATE["cancels"] += 1
            self.send(b'<root status_code="200"><cancel>1</cancel></root>')
        elif path == "/test/state":
            self.send(json.dumps(STATE).encode(), "application/json")
        elif path.startswith("/test/run/"):
            STATE["current"] = int(path.rsplit("/", 1)[1])
            self.send(b"ok", "text/plain")
        else:
            self.send_response(404)
            self.end_headers()


def serve(server):
    threading.Thread(target=server.serve_forever, daemon=True).start()


serve(http.server.ThreadingHTTPServer(("0.0.0.0", http_port), Handler))
if https_port:
    ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    ctx.load_cert_chain(sys.argv[5], sys.argv[6])
    ctx.verify_mode = ssl.CERT_REQUIRED  # only the paired client (moonlight's certificate)
    ctx.load_verify_locations(sys.argv[7])
    https = http.server.ThreadingHTTPServer(("0.0.0.0", https_port), Handler)
    https.socket = ctx.wrap_socket(https.socket, server_side=True)
    serve(https)
threading.Event().wait()
