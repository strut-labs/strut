#!/usr/bin/env python3
"""Reactor saturation accounting regression (current runtime).

Verifies the reactor saturation-rejection contract and its connection accounting:
  - admitted connections occupy the configured connection budget;
  - a connection over the budget receives HTTP/1.1 503;
  - completing the rejected 503 does NOT free an admitted slot (rejection-only
    connections must not decrement the admitted-work counter);
  - a further connection is still rejected while both admitted slots are occupied;
  - releasing one admitted connection permits a new admission;
  - a slow-reading rejected peer does not stall the reactor;
  - repeated saturation does not leak resources (fd/thread return).

Runs legacy and reactor modes with a fresh server process and dynamically selected
ports. Compilation/assertion failures propagate as a nonzero exit."""
import os
import signal
import socket
import subprocess
import sys
import tempfile
import threading
import time
from pathlib import Path

STRUT = os.environ.get("STRUT") or ("/home/nick/relgcc/strut"
                                    if not Path("build/strut").exists() else "build/strut")

SRC = '''function main() -> void : NetworkError {
    app := http_server();
    app.timeouts(30000, 30000, 30000, 3000);
    app.limits(1024, 4096, 16, 2);
    app.get("/", (http_request request) => { return http_text("ok"); });
    app.listen("127.0.0.1", %PORT%);
}
'''


def free_port():
    with socket.socket() as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


def full_request(port, timeout=5.0):
    c = socket.create_connection(("127.0.0.1", port), timeout=timeout)
    c.settimeout(timeout)
    c.sendall(b"GET / HTTP/1.1\r\nHost: localhost\r\n\r\n")
    data = b""
    try:
        while True:
            v = c.recv(4096)
            if not v:
                break
            data += v
    except socket.timeout:
        data += b"<TIMEOUT>"
    finally:
        c.close()
    return data


def open_blocker(port):
    c = socket.create_connection(("127.0.0.1", port), timeout=5)
    c.settimeout(5)
    c.sendall(b"GET / HTTP/1.1\r\nHost: localhost\r\n")  # incomplete: holds an admitted slot
    return c


def fast_status(port, timeout=2.0):
    """One request with Connection: close; return the status code promptly (no keep-alive wait)."""
    try:
        c = socket.create_connection(("127.0.0.1", port), timeout=timeout)
    except OSError:
        return "ERR"
    try:
        c.settimeout(timeout)
        c.sendall(b"GET / HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n")
        line = b""
        while b"\r\n" not in line:
            v = c.recv(1)
            if not v:
                break
            line += v
        for code in (b" 503 ", b" 200 ", b" 404 ", b" 403 ", b" 400 "):
            if code in line:
                return code.strip().decode()
        return "OTHER"
    except Exception:
        return "ERR"
    finally:
        c.close()


def status_code(data):
    head = data.split(b"\r\n", 1)[0]
    for code in (b" 503 ", b" 200 ", b" 404 ", b" 403 ", b" 400 "):
        if code in head + b" ":
            return code.strip().decode()
    return "OTHER"


def sample(pid):
    try:
        root = Path(f"/proc/{pid}")
        fds = len(list((root / "fd").iterdir()))
        threads = 0
        for line in (root / "status").read_text().splitlines():
            if line.startswith("Threads:"):
                threads = int(line.split()[1])
        return (fds, threads)
    except (FileNotFoundError, PermissionError, ProcessLookupError):
        return None


def run_mode(reactor):
    port = free_port()
    with tempfile.TemporaryDirectory(prefix="strut-satacct-") as td:
        td = Path(td)
        po = td / "p.p"
        po.write_text(SRC.replace("%PORT%", str(port)))
        exe = td / "p"
        b = subprocess.run([STRUT, str(po), "-o", str(exe)], capture_output=True, text=True, timeout=180)
        if b.returncode:
            print("compile FAILED:", b.stdout, b.stderr)
            sys.exit(1)
        env = dict(os.environ)
        env.pop("STRUT_HTTP_REACTOR", None)
        if reactor:
            env["STRUT_HTTP_REACTOR"] = "1"
        p = subprocess.Popen([str(exe)], env=env, start_new_session=True,
                             stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        mode = "reactor" if reactor else "legacy"
        try:
            # Wait for the listener to come up without leaving a counted probe connection:
            # a bare connect-and-close races the slot accounting, so poll TCP readiness then
            # settle before the test's own connections.
            deadline = time.monotonic() + 8
            while True:
                try:
                    probe = socket.create_connection(("127.0.0.1", port), timeout=2)
                    probe.close()
                    break
                except OSError:
                    if time.monotonic() > deadline:
                        raise
            time.sleep(0.5)
            blk1 = open_blocker(port)
            blk2 = open_blocker(port)
            time.sleep(0.3)
            r3 = full_request(port)          # over budget -> 503
            r4 = full_request(port)          # reject close must NOT free a slot -> 503
            blk1.close(); time.sleep(0.4)    # release one admitted slot
            r5 = full_request(port)          # admission now possible -> 200
            blk2.close()
            ok_core = status_code(r3) == "503" and status_code(r4) == "503" and status_code(r5) == "200"

            # Slow-reading rejected peer must not stall the reactor: occupy both slots,
            # then keep a rejected peer open without reading it, then confirm the server
            # still answers a fresh (admitted) request once a slot frees.
            blk1 = open_blocker(port)
            blk2 = open_blocker(port)
            time.sleep(0.2)
            slow = socket.create_connection(("127.0.0.1", port), timeout=5)
            slow.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 64)
            slow.sendall(b"GET / HTTP/1.1\r\nHost: localhost\r\n\r\n")
            blk1.close(); time.sleep(0.4)
            r6 = full_request(port)          # 200 while a rejected peer is still open
            ok_slow = status_code(r6) == "200"
            slow.close(); blk2.close()

            # Repeated saturation must not leak descriptors/threads.
            baseline = sample(p.pid)
            for _ in range(30):
                a = open_blocker(port); c = open_blocker(port)
                rejected = False
                for _ in range(50):
                    if fast_status(port) == "503":
                        rejected = True
                        break
                    time.sleep(0.02)
                a.close(); c.close()
                if not rejected:
                    raise RuntimeError("repeated saturation did not reject with 503")
                time.sleep(0.03)
            time.sleep(0.5)
            after = sample(p.pid)
            ok_res = (baseline is None or after is None or
                      (after[0] <= baseline[0] + 4 and after[1] <= baseline[1] + 2))

            ok = ok_core and ok_slow and ok_res
            print("[%s] core(r3=%s r4=%s r5=%s) slow=%s resources=%s -> %s" % (
                mode, status_code(r3), status_code(r4), status_code(r5),
                ok_slow, ok_res, "PASS" if ok else "FAIL"))
            if not ok:
                print("  detail: baseline=%s after=%s r6=%s" % (baseline, after, r6[:40]))
            return ok
        finally:
            if p.poll() is None:
                try:
                    os.killpg(p.pid, signal.SIGKILL)
                except Exception:
                    pass
                p.wait()


def main():
    results = [run_mode(False), run_mode(True)]
    sys.exit(0 if all(results) else 1)


if __name__ == "__main__":
    main()
