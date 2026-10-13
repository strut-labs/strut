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
        b = subprocess.run([STRUT, str(po), "-o", str(exe)], capture_output=True, text=True, timeout=180)
        if b.returncode:
            print("compile FAILED:", b.stdout, b.stderr); sys.exit(1)
        failed = False
        for reactor in (False, True):
            e = dict(os.environ); e.pop("STRUT_HTTP_REACTOR", None)
            if reactor: e["STRUT_HTTP_REACTOR"] = "1"
            p = subprocess.Popen([str(exe)], env=e, start_new_session=True,
                                 stdout=subprocess.PIPE, stderr=subprocess.PIPE)
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
                        v = c.recv(4096)  # timeout/reset is a failure, never successful EOF
                        if not v: break
                        data += v
                stdout, stderr = p.communicate(timeout=8)
                rc = p.returncode
                head, body = data.split(b"\r\n\r\n", 1)
                ok = (head.split(b"\r\n", 1)[0] == b"HTTP/1.1 200 OK"
                      and body == b"abcdefghij" and rc == 0 and not stderr)
                if stdout: print(stdout.decode(errors="replace"), end="")
                if stderr: print(stderr.decode(errors="replace"), file=sys.stderr, end="")
                print("[%s] partial-body drained + full response + exit=%r -> %s"
                      % ("reactor" if reactor else "legacy", rc, "PASS" if ok else "FAIL"))
                if not ok: failed = True
            finally:
                if p.poll() is None:
                    try: os.killpg(p.pid, signal.SIGKILL)
                    except Exception: pass
                stdout, stderr = p.communicate(timeout=5)
                if stderr: print(stderr.decode(errors="replace"), file=sys.stderr, end="")
        sys.exit(1 if failed else 0)

if __name__ == "__main__":
    main()
