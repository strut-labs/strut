#!/usr/bin/env python3
"""Reactor parameter-propagation regression (end-to-end, handler-dependent oracle).

The handler accepts the WebSocket upgrade ONLY when request.params["id"] == "abc123"; otherwise
it returns without accepting. Therefore a valid HTTP 101 proves the captured route parameter
reached the handler. Runs the same compiled fixture (current compiler, isolated ports,
process-group cleanup) in legacy and reactor modes for both positive and negative ids."""
import os, re, signal, socket, subprocess, sys, tempfile, threading, time
from pathlib import Path

STRUT = os.environ.get("STRUT") or (
    max([(p.stat().st_mtime, str(p)) for p in (Path("build/strut"), Path("/home/nick/relgcc/strut"))
         if p.exists()])[1]
)

SRC = r'''function main() -> int : NetworkError {
    app := http_server();
    app.websocket("/ws/:id", (http_request request, websocket socket) => {
        p := request.params["id"];
        if (p != "abc123") { return; }
        try { socket.accept(); } catch (NetworkError e) { }
        return;
    });
    app.listen("127.0.0.1", %PORT%, 4);
    return 0;
}
'''


def free_port():
    with socket.socket() as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


def probe(exe, port, reactor, ident):
    env = dict(os.environ)
    env.pop("STRUT_HTTP_REACTOR", None)
    if reactor:
        env["STRUT_HTTP_REACTOR"] = "1"
    p = subprocess.Popen([str(exe)], env=env, start_new_session=True,
                         stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    try:
        deadline = time.monotonic() + 6
        while True:
            try:
                c = socket.create_connection(("127.0.0.1", port), timeout=2)
                break
            except OSError:
                if time.monotonic() > deadline:
                    raise
        with c:
            req = ("GET /ws/%s HTTP/1.1\r\nHost: localhost\r\nConnection: Upgrade\r\n"
                   "Upgrade: websocket\r\nSec-WebSocket-Version: 13\r\n"
                   "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n\r\n") % ident
            c.sendall(req.encode())
            data = b""
            c.settimeout(2)
            while True:
                try:
                    v = c.recv(4096)
                except socket.timeout:
                    break
                if not v:
                    break
                data += v
        if b"HTTP/1.1 101" in data or b"101 Switching" in data:
            return "101"
        if data.startswith(b"HTTP/1.1 403"):
            return "403"
        return "other:" + data[:24].decode(errors="replace")
    finally:
        try:
            os.killpg(p.pid, signal.SIGKILL)
        except Exception:
            pass
        p.wait()


def main():
    with tempfile.TemporaryDirectory(prefix="strut-param-") as td:
        td = Path(td)
        port = free_port()
        po = td / "p.p"
        po.write_text(SRC.replace("%PORT%", str(port)))
        exe = td / "p"
        b = subprocess.run([STRUT, str(po), "-o", str(exe)], capture_output=True, text=True)
        if b.returncode != 0:
            print("compile FAILED:", b.stderr[-600:]); sys.exit(1)
        checks = [
            ("legacy-positive", False, "abc123", "101"),
            ("legacy-negative", False, "nope", "403"),
            ("reactor-positive", True, "abc123", "101"),
            ("reactor-negative", True, "nope", "403"),
        ]
        failed = False
        for name, reactor, ident, expect in checks:
            got = probe(exe, port, reactor, ident)
            ok = (got == expect)
            print("%s: %s (expected %s) %s" % (name, got, expect, "PASS" if ok else "FAIL"))
            if not ok:
                failed = True
        if failed:
            print("reactor parameter propagation regression FAILED"); sys.exit(1)
    print("reactor parameter propagation ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())