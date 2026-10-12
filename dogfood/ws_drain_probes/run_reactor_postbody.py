#!/usr/bin/env python3
"""Incomplete-POST-body admission-drain regression (current runtime). Server: max_requests=1,
POST /echo returns request.body. The client delivers headers + a PARTIAL body, admission
exhaustion becomes active, then the remainder is sent; the first request must not be interrupted
and the full body must round-trip with a 200 before the server exits 0. Both modes."""
import os, socket, subprocess, sys, tempfile, time
from pathlib import Path

STRUT = os.environ.get("STRUT") or ("/home/nick/relgcc/strut"
                                    if not Path("build/strut").exists() else "build/strut")
SRC = r'''function main() -> int : NetworkError {
    app := http_server();
    app.post("/echo", (http_request request) => {
        return http_text(request.body);
    });
    app.listen("127.0.0.1", %PORT%, 1);
    return 0;
}
'''

def main():
    with socket.socket() as s:
        s.bind(("127.0.0.1", 0)); port = s.getsockname()[1]
    with tempfile.TemporaryDirectory(prefix="strut-postbody-") as td:
        td = Path(td)
        po = td / "p.p"; po.write_text(SRC.replace("%PORT%", str(port)))
        exe = td / "p"
        b = subprocess.run([STRUT, str(po), "-o", str(exe)], capture_output=True, text=True)
        if b.returncode:
            print("compile FAILED:", b.stderr[-500:]); sys.exit(1)
        failed = False
        for reactor in (False, True):
            e = dict(os.environ); e.pop("STRUT_HTTP_REACTOR", None)
            if reactor: e["STRUT_HTTP_REACTOR"] = "1"
            p = subprocess.Popen([str(exe)], env=e, start_new_session=True)
            try:
                dl = time.monotonic() + 6
                while True:
                    try: c = socket.create_connection(("127.0.0.1", port), timeout=2); break
                    except OSError:
                        if time.monotonic() > dl: raise
                with c:
                    c.sendall(b"POST /echo HTTP/1.1\r\nHost: localhost\r\nContent-Length: 10\r\n\r\nabc")
                    time.sleep(0.5)
                    c.sendall(b"defghij")
                    c.settimeout(3); data = b""
                    while True:
                        try: v = c.recv(4096)
                        except Exception: break
                        if not v: break
                        data += v
                rc = None; dl = time.monotonic() + 5
                while time.monotonic() < dl:
                    if p.poll() is not None: rc = p.returncode; break
                    time.sleep(0.05)
                ok = (b"HTTP/1.1 200" in data and b"abcdefghij" in data and rc == 0)
                print("[%s] partial-body drained + full response + exit=%r -> %s"
                      % ("reactor" if reactor else "legacy", rc, "PASS" if ok else "FAIL"))
                if not ok: failed = True
            finally:
                if p.poll() is None:
                    try: os.killpg(p.pid, signal.SIGKILL)
                    except Exception: pass
                p.wait()
        sys.exit(1 if failed else 0)

if __name__ == "__main__":
    main()
