#!/usr/bin/env python3
"""Strong self-capture cycle control (current runtime).

Negative control for the ownership distinction behind Defect C: a route handler that
captures its own server STRONGLY (by value) forms the documented reference-counting cycle
(state -> routes -> handler -> captured server -> state). The server still functions
correctly (shutdown returns 200/bye, exit 0), but the cycle is not reclaimed at exit.

Behavior:
  - always: the strong-capture server responds and exits 0 (ownership semantics unchanged);
  - when LeakSanitizer is active (ASAN_OPTIONS contains detect_leaks=1): exit must be nonzero
    and the report must mention LeakSanitizer, confirming the strong cycle is retained and is
    NOT magically collected.

This deliberately leaks; it is the counterpart to the weak-capture explicit-stop gate, which
must be LeakSanitizer-clean. Legacy and reactor. Bounded and failure-propagating."""
import os
import signal
import socket
import subprocess
import sys
import tempfile
import time
from pathlib import Path

STRUT = os.environ.get("STRUT") or ("/home/nick/relgcc/strut"
                                    if not Path("build/strut").exists() else "build/strut")
LSAN = "detect_leaks=1" in os.environ.get("ASAN_OPTIONS", "")
SRC = r'''function main() -> int : NetworkError {
    app := http_server();
    app.get("/shutdown", (http_request request) => { app.stop(); return http_text("bye"); });
    app.listen("127.0.0.1", %PORT%);
    return 0;
}
'''


def main():
    with socket.socket() as s:
        s.bind(("127.0.0.1", 0)); port = s.getsockname()[1]
    with tempfile.TemporaryDirectory(prefix="strut-strongcycle-") as td:
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
                c.settimeout(5)
                c.sendall(b"GET /shutdown HTTP/1.1\r\nHost: localhost\r\n\r\n")
                data = b""
                while True:
                    v = c.recv(4096)
                    if not v: break
                    data += v
                c.close()
                stdout, stderr = p.communicate(timeout=10)
                rc = p.returncode
                head, body = (data.split(b"\r\n\r\n", 1) + [b""])[:2]
                served = head.split(b"\r\n", 1)[0] == b"HTTP/1.1 200 OK" and body == b"bye"
                if LSAN:
                    ok = served and rc != 0 and b"LeakSanitizer" in stderr
                    mode = "LSan-detects-cycle"
                else:
                    ok = served and rc == 0
                    mode = "serves+exit0"
                print("[%s] strong-capture %s (rc=%s) -> %s"
                      % ("reactor" if reactor else "legacy", mode, rc, "PASS" if ok else "FAIL"))
                if not ok: failed = True
            finally:
                if p.poll() is None:
                    try:
                        if os.name == "posix":
                            os.killpg(os.getpgid(p.pid), signal.SIGKILL)
                        else:
                            p.kill()
                    except Exception:
                        try: p.kill()
                        except Exception: pass
                try:
                    p.communicate(timeout=5)
                except subprocess.TimeoutExpired:
                    pass
        sys.exit(1 if failed else 0)

if __name__ == "__main__":
    main()
