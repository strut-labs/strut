#!/usr/bin/env python3
"""Explicit-stop-during-admission-drain certification (current runtime). Server: max_requests=2
with POST /upload (echoes request.body) and GET /shutdown (app.stop()). One connection holds an
incomplete POST body in waiting_body while normal draining may begin; a SECOND connection calls
/shutdown. Explicit cancellation must take precedence over the normal drain: the shutdown gets
200/bye, the admitted partial-body first request is interrupted (EOF), and the server exits 0.
Legacy and reactor. Bounded and failure-propagating."""
import os, signal, socket, subprocess, sys, tempfile, time
from pathlib import Path

STRUT = os.environ.get("STRUT") or ("/home/nick/relgcc/strut"
                                    if not Path("build/strut").exists() else "build/strut")
SRC = r'''function main() -> int : NetworkError {
    app := http_server();
    app.post("/upload", (http_request request) => { b := request.body; return http_text("ok"); });
    app.get("/shutdown", (http_request request) => { app.stop(); return http_text("bye"); });
    app.listen("127.0.0.1", %PORT%, 2);
    return 0;
}
'''


def main():
    with socket.socket() as s:
        s.bind(("127.0.0.1", 0)); port = s.getsockname()[1]
    with tempfile.TemporaryDirectory(prefix="strut-exstop-") as td:
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
                    try: c1 = socket.create_connection(("127.0.0.1", port), timeout=2); break
                    except OSError:
                        if time.monotonic() > dl: raise
                c1.settimeout(3)
                c1.sendall(b"POST /upload HTTP/1.1\r\nHost: localhost\r\nContent-Length: 10\r\n\r\n12345")
                # legacy closes an incomplete-body first request at its ~5s read/shutdown
                # deadline, not instantly; extend the client recv window past that so EOF is
                # separable from a stuck server (timeout -> FAIL).
                c1.settimeout(12)
                time.sleep(0.3)
                c2 = socket.create_connection(("127.0.0.1", port), timeout=2)
                c2.settimeout(3)
                c2.sendall(b"GET /shutdown HTTP/1.1\r\nHost: localhost\r\n\r\n")
                r2 = b""; shutdown_eof = False; shutdown_failed = False
                while True:
                    try:
                        v = c2.recv(4096)
                    except socket.timeout:
                        shutdown_failed = True; break
                    except Exception:
                        shutdown_failed = True; break
                    if v == b"": shutdown_eof = True; break
                    r2 += v
                r1 = b""; partial_eof = False; partial_bad = False
                while True:
                    try:
                        v = c1.recv(4096)
                    except socket.timeout:
                        partial_bad = True; break   # read timed out -> not closed -> FAIL
                    except Exception as exc:
                        partial_bad = True; break   # unexpected socket error -> report
                    if v == b"": partial_eof = True; break
                    r1 += v
                c1.close(); c2.close()
                stdout, stderr = p.communicate(timeout=8)
                rc = p.returncode
                head, body = r2.split(b"\r\n\r\n", 1)
                ok = (head.split(b"\r\n", 1)[0] == b"HTTP/1.1 200 OK" and body == b"bye"
                      and shutdown_eof and not shutdown_failed and partial_eof and not partial_bad
                      and r1 == b"" and rc == 0 and not stderr)
                if stdout: print(stdout.decode(errors="replace"), end="")
                if stderr: print(stderr.decode(errors="replace"), file=sys.stderr, end="")
                print("[%s] shutdown-200+EOF, partial-first-EOF, exit0 -> %s"
                      % ("reactor" if reactor else "legacy", "PASS" if ok else "FAIL"))
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
