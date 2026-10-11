#!/usr/bin/env python3
"""Process-safe legacy/reactor matrix probe. Picks an isolated port per run, bakes it into the
fixture (derived from reactor_ws_probe.p with the ADMISSION-NEUTRAL limit 32), compiles the
current runtime via the CLI, launches the server in its own process group, verifies liveness,
and records /plain, /ws(upgrade), /missing and /ws(no-upgrade) responses under both legacy and
reactor modes. Any stale process is killed; the new process must own the listener."""
import os, re, signal, socket, subprocess, sys, tempfile, time
from pathlib import Path

root = Path(__file__).resolve().parent
TEMPLATE = (root / "reactor_ws_probe.p").read_text()
F = re.compile(r'\s*app\.listen\("127\.0\.0\.1", [0-9]+, 32\);')

def free_port():
    with socket.socket() as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]

def connect(port, deadline):
    while time.monotonic() < deadline:
        try:
            return socket.create_connection(("127.0.0.1", port), timeout=2)
        except OSError:
            time.sleep(0.03)
    raise TimeoutError("server never accepted")

def request(port, req, deadline):
    c = connect(port, deadline)
    with c:
        c.sendall(req)
        data = b""
        c.settimeout(4)
        while True:
            try:
                v = c.recv(4096)
            except socket.timeout:
                break
            if not v:
                break
            data += v
    return data

def run_matrix(exe, port):
    """Each (mode, request) row uses a FRESH server process: fully isolated, so a crash or
    keep-alive state in one row cannot contaminate the next."""
    env0 = dict(os.environ); env0.pop("STRUT_HTTP_REACTOR", None)
    reqs = {
        "plain": b"GET /plain HTTP/1.1\r\nHost: localhost\r\n\r\n",
        "ws_up": (b"GET /ws HTTP/1.1\r\nHost: localhost\r\nConnection: Upgrade\r\n"
                  b"Upgrade: websocket\r\nSec-WebSocket-Version: 13\r\n"
                  b"Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n\r\n"),
        "missing": b"GET /missing HTTP/1.1\r\nHost: localhost\r\n\r\n",
        "ws_noup": b"GET /ws HTTP/1.1\r\nHost: localhost\r\n\r\n",
    }
    out = {}
    for reactor in (False, True):
        out["reactor" if reactor else "legacy"] = {}
        for key, rq in reqs.items():
            e = dict(env0)
            if reactor:
                e["STRUT_HTTP_REACTOR"] = "1"
            p = subprocess.Popen([str(exe)], env=e, start_new_session=True,
                                 stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            try:
                deadline = time.monotonic() + 6
                r = request(port, rq, deadline)
                body = r[:64].decode(errors="replace").replace("\r\n", " | ")
                accept = b"Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=" in r
                out["reactor" if reactor else "legacy"][key] = (body, accept)
            finally:
                os.killpg(p.pid, signal.SIGKILL)
                p.wait()
    return out

def main():
    cc = os.environ.get("CXX", "g++")
    with tempfile.TemporaryDirectory(prefix="strut-rws-") as td:
        td = Path(td)
        port = free_port()
        src = td / "probe.p"
        src.write_text(F.sub('app.listen("127.0.0.1", %d, 32);' % port, TEMPLATE))
        exe = td / "probe"
        strut = os.environ.get("STRUT") or ("build/strut" if Path("build/strut").exists() else "/home/nick/relgcc/strut")
        b = subprocess.run([strut, str(src), "-o", str(exe)], capture_output=True, text=True)
        if b.returncode != 0:
            print("compile FAILED:", b.stderr[-800:]); sys.exit(1)
        m = run_matrix(exe, port)
        for mode, rows in m.items():
            print("[%s]" % mode)
            for k, (body, accept) in rows.items():
                print("  %-9s %s%s" % (k, body, "  [accept-key]" if accept else ""))
        mtx = m
        assert mtx["legacy"]["plain"][0].startswith("HTTP/1.1 200"), mtx
        assert mtx["reactor"]["plain"][0].startswith("HTTP/1.1 200"), mtx
        assert mtx["legacy"]["ws_up"][0].startswith("HTTP/1.1 101") and mtx["legacy"]["ws_up"][1], mtx
        assert mtx["reactor"]["ws_up"][0].startswith("HTTP/1.1 101") and mtx["reactor"]["ws_up"][1], mtx
        assert mtx["legacy"]["missing"][0].startswith("HTTP/1.1 404"), mtx
        assert mtx["reactor"]["missing"][0].startswith("HTTP/1.1 404"), mtx
        assert mtx["legacy"]["ws_noup"][0].startswith("HTTP/1.1 400"), mtx
        assert mtx["reactor"]["ws_noup"][0].startswith("HTTP/1.1 400"), mtx
        print("matrix assertions PASS (legacy + reactor: /plain 200, /ws-upgrade 101 + accept-key, /missing 404, /ws-no-upgrade 400)")
        return 0

if __name__ == "__main__":
    sys.exit(main())
