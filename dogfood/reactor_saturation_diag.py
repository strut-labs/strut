#!/usr/bin/env python3
"""Bounded Windows/Linux diagnostic for reactor saturation rejection.

Starts a reactor server with max_connections=2, occupies both slots with incomplete
requests, then probes with a complete request and prints the raw response, timing,
socket errors and the server's stderr/exit. Platform-portable (no /proc, no killpg).
Exit code is always 0; this is a diagnostic, not a gate."""
import os, socket, subprocess, sys, tempfile, time
from pathlib import Path

STRUT = os.environ.get("STRUT") or ("/home/nick/relgcc/strut"
                                    if not Path("build/strut").exists() else "build/strut")
SRC = '''function main() -> void : NetworkError {
    app := http_server();
    app.timeouts(5000, 5000, 5000, 3000);
    app.limits(1024, 4096, 16, 2);
    app.get("/", (http_request request) => { return http_text("ok"); });
    app.listen("127.0.0.1", %PORT%);
}
'''


def free_port():
    with socket.socket() as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


def main():
    print("platform=%s strut=%s reactor=%s" % (sys.platform, STRUT, os.environ.get("STRUT_HTTP_REACTOR")))
    port = free_port()
    with tempfile.TemporaryDirectory(prefix="satdiag-") as td:
        p = Path(td) / "p.p"
        p.write_text(SRC.replace("%PORT%", str(port)))
        exe = Path(td) / "p"
        b = subprocess.run([STRUT, str(p), "-o", str(exe)], capture_output=True, text=True, timeout=180)
        if b.returncode:
            print("COMPILE_FAIL", b.stderr[-500:]); return
        env = dict(os.environ)
        env.pop("STRUT_HTTP_REACTOR", None)
        if os.environ.get("STRUT_HTTP_REACTOR") == "1":
            env["STRUT_HTTP_REACTOR"] = "1"
        proc = subprocess.Popen([str(exe)], env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        try:
            dl = time.monotonic() + 8
            while True:
                try:
                    s = socket.create_connection(("127.0.0.1", port), timeout=2); s.close(); break
                except OSError:
                    if time.monotonic() > dl:
                        print("SERVER_NEVER_LISTENED"); return
            time.sleep(0.5)
            blockers = []
            for _ in range(2):
                c = socket.create_connection(("127.0.0.1", port), timeout=5)
                c.sendall(b"GET / HTTP/1.1\r\nHost: localhost\r\n")  # incomplete
                blockers.append(c); time.sleep(0.2)
            for attempt in range(6):
                t0 = time.monotonic()
                probe = socket.create_connection(("127.0.0.1", port), timeout=3)
                probe.settimeout(3)
                probe.sendall(b"GET / HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n")
                data = b""
                err = None
                try:
                    while True:
                        v = probe.recv(4096)
                        if not v:
                            break
                        data += v
                except Exception as e:
                    err = "%s: %s" % (type(e).__name__, e)
                probe.close()
                print("probe %d: elapsed=%.3f bytes=%d err=%s first_line=%r" % (
                    attempt, time.monotonic() - t0, len(data), err, data[:40]))
                time.sleep(0.2)
            for c in blockers:
                c.close()
            time.sleep(0.5)
            alive = proc.poll() is None
            print("server alive=%s exit=%s" % (alive, proc.poll()))
        finally:
            if proc.poll() is None:
                proc.terminate()
                try:
                    proc.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    proc.kill(); proc.wait()
            try:
                out, err = proc.communicate(timeout=3)
            except Exception:
                out, err = "", ""
            if err:
                print("SERVER_STDERR:", err[-800:])


if __name__ == "__main__":
    main()
