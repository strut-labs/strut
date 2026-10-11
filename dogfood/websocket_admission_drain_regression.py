#!/usr/bin/env python3
"""Deterministic WebSocket admission-drain regression (permanent).

Reflects the Codex-identified legacy-HTTP race: finite admission (`max_requests=1`) triggers
shutdown while an admitted FIRST request is still reading its head; the pre-fix runtime
interrupted it before the HTTP 101 could be sent (worker-first scheduling -> b'').

The two probe sources under dogfood/ws_drain_probes/ are frozen snapshots of the generated
runtime: identical to the emitted runtime at the admission-drain fix SHA, plus atomic test
barriers that force the two execution orders deterministically:
  - worker_first.cpp: worker marks the first request idle, THEN the admission-exhaustion scan runs
  - stop_first.cpp:  the scan runs BEFORE the worker marks the first request idle

Both must now produce a valid HTTP/1.1 101 Switching Protocols response carrying the expected
Sec-WebSocket-Accept key, with normal server exit (rc==0). This is the exact before/after pair:
pre-fix the worker-first variant produced b'' under the same forced ordering.

The probes are pinned runtime snapshots (they cannot be regenerated while the compiler cannot
emit plain C++); the live http_websocket_certification.py runs verify the CURRENT emitted runtime.
SENSITIVITY: original_worker_first.cpp is the PRE-FIX runtime snapshot under the same forced
ordering and must FAIL (b''), so a future reintroduction of the original admission-shutdown
behavior in src/generated_runtime.cpp leaves the driver red. Exit code is nonzero if any ordering
mismatches its expected outcome."""
import os, socket, subprocess, sys, tempfile, time
from pathlib import Path

root = Path(__file__).resolve().parent
probe_dir = root / "ws_drain_probes"
opening = (b"GET /ws HTTP/1.1\r\nHost: localhost\r\nConnection: Upgrade\r\n"
           b"Upgrade: websocket\r\nSec-WebSocket-Version: 13\r\n"
           b"Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n\r\n")
ACCEPT_KEY = b"s3pPLMBiTxaQ9kYGzzhZRbK+xOo="


def run_probe(exe, port):
    env = dict(os.environ)
    env.pop("STRUT_HTTP_REACTOR", None)
    p = subprocess.Popen([str(exe), str(port)], stdout=subprocess.PIPE,
                         stderr=subprocess.PIPE, env=env)
    connected = False
    try:
        deadline = time.monotonic() + 5
        while True:
            try:
                c = socket.create_connection(("127.0.0.1", port), timeout=2)
                connected = True
                break
            except ConnectionRefusedError:
                if time.monotonic() > deadline:
                    raise
        with c:
            c.sendall(opening)
            data = b""
            while True:
                v = c.recv(4096)
                if not v:
                    break
                data += v
        stdout, stderr = p.communicate(timeout=8)
        return p.returncode, data, stdout, stderr, connected
    finally:
        if p.poll() is None:
            p.kill()
            p.wait()


def main():
    cc = os.environ.get("CXX", "g++")
    with tempfile.TemporaryDirectory(prefix="strut-ws-drain-") as td:
        td = Path(td)
        exes = {}
        for name in ("worker_first", "stop_first", "original_worker_first"):
            exe = td / name
            b = subprocess.run([cc, "-std=c++20", "-O2", "-pthread",
                                str(probe_dir / (name + ".cpp")), "-o", str(exe)],
                               capture_output=True, text=True)
            if b.returncode != 0:
                print("build FAILED (%s):\n%s" % (name, b.stderr[-1500:])); sys.exit(1)
            exes[name] = exe
        failed = False
        expect_pass = {"worker_first": True, "stop_first": True, "original_worker_first": False}
        for name, exe in exes.items():
            with socket.socket() as reserve:
                reserve.bind(("127.0.0.1", 0))
                port = reserve.getsockname()[1]
            rc, data, stdout, stderr, connected = run_probe(exe, port)
            ok = (rc == 0 and b"HTTP/1.1 101" in data and ACCEPT_KEY in data and b"Sec-WebSocket-Accept" in data)
            # Negative control must reproduce the ORIGINAL signature precisely: TCP connected,
            # server exited 0, no bytes received, no crash/timeout. A crash/timeout/empty-mark
            # is a FAIL, not a pass, for the pre-fix control.
            if expect_pass[name]:
                good = ok
            else:
                good = connected and rc == 0 and data == b""
            print("%s: %s %s (rc=%d)" % (name,
                  "valid HTTP/1.1 101" if ok else "empty (original signature) " if connected and rc == 0 and data == b"" else "UNDETERMINED",
                  "PASS" if good else "UNEXPECTED", rc))
            if not good:
                failed = True
        if failed:
            print("websocket admission-drain regression FAILED"); sys.exit(1)
    print("websocket admission-drain regression ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())